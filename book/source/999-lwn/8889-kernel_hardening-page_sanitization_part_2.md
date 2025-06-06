# Page sanitization, part 2

上周的 “安全”页面介绍了最近提出的一些补丁，这些补丁可以在释放内核内存时清除内存，从而 “净化 ”内核内存。当时，依赖于 sanitize_mem 启动参数的第二版补丁在释放时无条件清除内存，受到了普遍欢迎。不过，也许人们还没来得及看。在过去的一周里，有许多人提出了反对意见，而开发人员 Larry Highsmith 对此大多给予了激烈的回应。从很多方面看，这都像是 “如何不与内核社区合作”的又一堂课。

基本问题是，在内存被释放后，数据仍会长期存在于内存中。有时，这些数据包含密码、加密密钥、机密文件等，但在一般情况下，内核不可能知道哪些页面是敏感页面。通过在删除内存时清除内存，可以减少这些潜在敏感数据的寿命。一篇研究论文描述了一些实验，这些实验表明内存值在 Linux 系统上会持续几天甚至几周。内核中泄露内存信息的漏洞可能会将这些值泄露给攻击者。 

因此，Highsmith 提议添加一项内存清除功能，该功能早已成为 PaX 安全项目内核补丁的一部分。在回收内存时清除内存显然会对性能产生影响，但由于内存是在分配时清除的（以避免明显的信息泄露），因此影响可能没有乍看起来那么大。正如 Arjan van de Ven 所指出的那样：

```
如果我们释放置零，就不需要把分配归零。虽然这有点争议，但它确实意味着至少有一部分成本只是时间上的转移，这意味着希望不会太糟糕......
```

Peter Zijlstra 对缓存效应表示担忧： “分配时为 0 有缓存热度的优势，我们要使用内存，为什么不分配呢？[释放时清零只会导致额外的缓存驱逐，而没有任何好处"。不过，van de Ven 描述了他如何看待缓存受到的影响，并得出结论： “不要误解我的意思，我并不是说零点空闲更好，我只是想指出零点分配的'优势'并不像人们有时认为的那么大......”

但有些人，比如艾伦-考克斯，认为对性能的影响无关紧要： “如果你需要这种数据抹除，那么性能影响基本上无关紧要，安全才是第一位的。” Zijlstra 和其他人担心的是所有内核用户付出的代价，即使是那些没有启用 sanitize_mem 的用户。他指出，即使在未启用该功能的情况下，补丁也会增加额外的函数调用和分支。有人建议将提议的代码与现有实现进行比较，但这正是对话开始偏离轨道的地方。

Highsmith 显然对讨论的方向感到沮丧，但他没有后退，而是大加抨击。当然，在这个话题中也有一些挑衅，Zijlstra 的 "真的，过你自己的生活，去解决真正的错误吧。不要为了打飞机的权利而让我们的内核变慢。"这样的评论当然无济于事。但 Highsmith 需要认识到，他才是那个试图在内核中添加一些东西的人，所以 “举证 ”的责任在他身上。相反，他居高临下的态度似乎表明，他觉得自己是在向内核社区赠送礼物，而他们却愚钝得无法理解。

内核贡献者的一个重要特征是与社区的其他成员合作良好：回答问题、回应代码审查建议等。如果做不到这一点，不管补丁的技术价值如何，它们都会被忽略。当有人建议在特定的内核分配中使用 kzfree() 来清除内存，然后释放敏感数据时，海史密斯做出了回应：

```
这是没有希望的，而且 kzfree 已经坏了。就像我在之前的回复中说的，请自己测试看看结果。不管是谁写的，他都忽略了 SLAB/SLUB 是如何工作的，如果 kzfree 以前在内核中的某个地方使用过，那么它早就应该被注意到了。
```

由于 Highsmith 是在回应 SLAB 维护者 Pekka Enberg 的建议，因此这种回应--即使是真实的--可能也不是正确的方法。恩伯格等人特别询问了 kzfree() 中的问题，但海德史密斯的回答既居高临下又含糊其辞。当恩伯格和英戈-莫尔纳（Ingo Molnar）试图找出问题所在时，海史密斯又开始大谈 SLOB 内存分配器。

此外，莫尔纳还指出，一些相同的敏感值在内核堆栈中的寿命可能很长：

```
触及任何加密路径（或内核中的其他敏感数据）并将其泄漏到内核堆栈的长期任务可能会无限期地将敏感信息保留在堆栈中（尤其是在意外深入堆栈上下文的情况下），直至任务退出。这些信息将在释放和清除原始敏感数据后继续存在。
```

海史密斯并没有认识到这是一个需要解决的额外领域，而是继续他的咆哮：
```
但你和其他含糊其辞的小集团却只发表了一些毫无用处的评论、赤裸裸的不文明回应、明显的误导企图、毫无根据的批评等等。自从上次我读了杰里-刘易斯（Jerry Lewis）未发行的电影剧本之后，我再也没见过比这更多的谬论了。
```

总的来说，根据启动时间标记释放内存的想法是合理的。包括 Cox 和 Rik van Riel 在内的几位内核黑客都表示有兴趣增加这一功能。不过，如果主要支持者把时间都花在争吵和谩骂上，那么合并的可能性似乎不大。

Highsmith 还提出了一套更新的补丁，只在特定的敏感区域（tty 缓冲区管理、802.11 密钥处理和加密 API）使用 kzfree()，但 Linus Torvalds 并没有对此留下特别深刻的印象。他认为没有必要使用 kzfree()，简单的 memset() 就足够了。Torvalds 不一定相信这些补丁的必要性，也不一定相信 Highsmith 对审查的回应：

还有一些人对补丁提出了其他技术上的抱怨，尤其是 Crypto API 补丁中到处使用 kzfree()。Crypto API 维护者 Herbert Xu 指出： “元数据清零是无偿的。总的来说，这些补丁看起来是勉强创建的--就好像这样做是一种恩惠。

目前尚不清楚事情的后续发展。Highsmith 在给 Torvalds 的回信中似乎可能要签下自己的名字：“下次再出现与我所评论的某些攻击途径密切相关的内核漏洞时，能够参考这些回复将是非常有用的”。海史密斯的沮丧是有一定道理的，但他需要明白，这样做对他（或内核）没有任何好处。

内核贡献者，尤其是新贡献者，需要认识到社区中至少有和他们一样聪明的人。在这种情况下，其中一些开发者可能不像海史密斯那样关注安全问题，但这并不会降低他们对内核的理解，也不会降低他们希望看到内核打上补丁以提高安全性的兴趣。如果这项功能在某些环境中非常有用，但却被淘汰，那将是非常不幸的。

