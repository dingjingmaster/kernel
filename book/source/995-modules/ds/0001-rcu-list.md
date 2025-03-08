# RCU 链表

RCU英文全称为Read-Copy-Update(读-拷贝-更新)，是一种同步机制。RCU记录所有指向共享数据的指针的使用者，当要修改该共享数据时，首先创建一个副本，在副本中修改。所有读访问线程都离开读临界区之后，指针指向新的修改后副本的指针，并且删除旧数据。通过RCU保护的链表仍然可以使用标准的链表元素。只有在遍历链表、修改和删除链表元素时，必须调用标准函数的RCU变体。函数名称很容易记住：在标准函数之后附加`_rcu`后缀。

RCU只解决了读者和写者之间的并发问题，但不能保证多个写者不会同时修改链表，导致数据损坏。

`struct rcu_head` 是RCU机制中的一个结构体，主要用于**延迟释放**需要RCU保护的数据结构。它的核心作用是记录RCU回调函数，当RCU读者完成访问之后，由内核调度调用该回调，以执行清理或释放操作

> 源码位置 /include/linux/rculist.h

```c
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/rculist.h>
#include <linux/spinlock.h>

struct Book
{
    int id;
    char name[64];
    char author[64];
    int borrow;
    struct list_head node;
    struct rcu_head rcu;
};

static LIST_HEAD(books);
static spinlock_t booksLock;    // RCU只解决了读者和写者之间的并发问题，此自旋锁解决写者之间并发问题

/**
 * @brief preempt_count() == 0 才可以抢占； > 0 表示当前代码在不可抢占的上下文中，抢占被延迟到 preempt_count 归零后执行
 *
 *
 * @see preempt_diable(); preempt_count +1，防止当前任务被抢占
 * @see preempt_enable(); preempt_count -1, 如果归0，可能触发任务切换
 */
static void book_reclaim_callback(struct rcu_head *rcu)
{
    struct Book *b = container_of(rcu, struct Book, rcu);

    pr_info("callback free : %lx, preempt_count : %d\n", (unsigned long)b, preempt_count());

    kfree(b);
}

static void add_book(int id, const char *name, const char *author)
{
    struct Book *b = kzalloc(sizeof(struct Book), GFP_KERNEL);
    if (!b) {
        return;
    }

    b->id = id;
    strncpy(b->name, name, sizeof(b->name));
    strncpy(b->author, author, sizeof(b->author));
    b->borrow = 0;

    spin_lock(&booksLock);
    list_add_rcu(&b->node, &books);
    spin_unlock(&booksLock);
}

static int borrow_book(int id, int async)
{
    struct Book *b = NULL;
    struct Book *new_b = NULL;
    struct Book *old_b = NULL;

    rcu_read_lock();

    list_for_each_entry(b, &books, node) {
        if (b->id == id) {
            if(b->borrow) {
                rcu_read_unlock();
                return -1;
            }
            old_b = b;
            break;
        }
    }

    if(!old_b) {
        rcu_read_unlock();
        return -1;
    }

    new_b = kzalloc(sizeof(struct Book), GFP_ATOMIC);
    if (!new_b) {
        rcu_read_unlock();
        return -1;
    }

    memcpy(new_b, old_b, sizeof(struct Book));
    new_b->borrow = 1;

    spin_lock(&booksLock);
    list_replace_rcu(&old_b->node, &new_b->node);
    spin_unlock(&booksLock);

    rcu_read_unlock();

    if (async) {
        // 所有正在执行的RCU读端临界区完成后，通过回调函数安全的释放或回收资源。call_rcu 是非阻塞的，适用于高性能场景。
        call_rcu(&old_b->rcu, book_reclaim_callback);
    }
    else {
        // 等待所有已经开始的RCU读端临界区完成，从而确保在更新共享数据时的安全性。
        synchronize_rcu();
        kfree(old_b);
    }

    pr_info("borrow success %d, preempt_count : %d\n", id, preempt_count());

    return 0;
}

static int is_borrowed_book(int id)
{
    struct Book *b;

    rcu_read_lock();
    list_for_each_entry_rcu(b, &books, node) {
        if(b->id == id) {
            rcu_read_unlock();
            return b->borrow;
        }
    }
    rcu_read_unlock();

    pr_err("not exist book\n");

    return -1;
}

static int return_book(int id, int async)
{
    struct Book *b = NULL;
    struct Book *new_b = NULL;
    struct Book *old_b = NULL;

    rcu_read_lock();

    list_for_each_entry(b, &books, node) {
        if(b->id == id) {
            if(!b->borrow) {
                rcu_read_unlock();
                return -1;
            }

            old_b = b;
            break;
        }
    }

    if(!old_b) {
        rcu_read_unlock();
        return -1;
    }

    new_b = kzalloc(sizeof(struct Book), GFP_ATOMIC);
    if(!new_b) {
        rcu_read_unlock();
        return -1;
    }

    memcpy(new_b, old_b, sizeof(struct Book));
    new_b->borrow = 0;

    spin_lock(&booksLock);
    list_replace_rcu(&old_b->node, &new_b->node);
    spin_unlock(&booksLock);

    rcu_read_unlock();

    if (async) {
        call_rcu(&old_b->rcu, book_reclaim_callback);
    }
    else {
        synchronize_rcu();
        kfree(old_b);
    }

    pr_info("return success %d, preempt_count : %d\n", id, preempt_count());

    return 0;
}

static void delete_book(int id, int async)
{
    struct Book *b;

    spin_lock(&booksLock);
    list_for_each_entry(b, &books, node) {
        if(b->id == id) {
            list_del_rcu(&b->node);
            spin_unlock(&booksLock);
            if (async) {
                call_rcu(&b->rcu, book_reclaim_callback);
            }
            else {
                synchronize_rcu();
                kfree(b);
            }
            return;
        }
    }
    spin_unlock(&booksLock);

    pr_err("not exist book\n");
}

static void print_book(int id)
{
    struct Book *b;

    rcu_read_lock();
    list_for_each_entry_rcu(b, &books, node) {
        if(b->id == id) {
            pr_info("id : %d, name : %s, author : %s, borrow : %d, addr : %lx\n", b->id, b->name, b->author, b->borrow, (unsigned long)b);
            rcu_read_unlock();
            return;
        }
    }
    rcu_read_unlock();

    pr_err("not exist book\n");
}


static void test_example(int async)
{
    add_book(0, "book1", "jb");
    add_book(1, "book2", "jb");

    print_book(0);
    print_book(1);

    pr_info("book1 borrow : %d\n", is_borrowed_book(0));
    pr_info("book2 borrow : %d\n", is_borrowed_book(1));

    borrow_book(0, async);
    borrow_book(1, async);

    print_book(0);
    print_book(1);

    return_book(0, async);
    return_book(1, async);

    print_book(0);
    print_book(1);

    delete_book(0, async);
    delete_book(1, async);

    print_book(0);
    print_book(1);
}

static int __init list_rcu_example_init(void)
{
    spin_lock_init(&booksLock);

    test_example(0);
    test_example(1);

    return 0;
}

static void __exit list_rcu_example_exit(void)
{
    return;
}

module_init(list_rcu_example_init);
module_exit(list_rcu_example_exit);
MODULE_LICENSE("GPL");
```


