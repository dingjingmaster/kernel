# ptrace函数

## 函数签名

```c
#include <sys/ptrace.h>

long ptrace(enum __ptrace_request op, pid_t pid, void *addr, void *data);
```

## 参数

- request: 指定ptrace的操作类型
- pid: 指定操作的目标进程PID
- addr: 操作地址，取决于request类型
- data: 数据或缓存区

### request

|值|说明|
|:--|:--|
|PTRACE_TRACEME|被调试进程调用，让父进程可以使用ptrace跟踪它|
|PTRACE_PEEKDATA/PTRACE_PEEKTEXT|从被调试进程的内存中读取数据(读取用户数据段)|
|PTRACE_PEEKUSER|读取寄存器(用户区域)中的内容|
|PTRACE_POKEDATA|向被调试进程的内存写入数据|
|PTRACE_POKEUSER|修改寄存器的值|
|PTRACE_CONT|让被调试的进程继续执行|
|PTRACE_SIGLESETP|单步执行(执行一条指令后暂停)|
|PTRACE_ATTACH|附加到一个正在运行的进程，对其进行跟踪|
|PTRACE_DETACH|断开对进程的跟踪，继续执行|
|PTRACE_GETREGS/PTRACE_SETREGS|获取/设置通用寄存器|
|PTRACE_SYSCALL|每次系统调用进入/退出时候暂停进程|
|PTRACE_KILL|杀死进程|
|PTRACE_GETFPREGS/PTRACE_SETFPREGS|获取/设置浮点寄存器|
|PTRACE_GET_THREAD_AREA/PTRACE_SET_THREAD_AREA||
