# 目前已知的情报：

Execute Script: ``./record-trace -r 1 --frontend pin --follow -- executable``

Attach Script: ``./record-trace -r 1 --frontend pin --follow --pid xxxx``

Test Executable: ``follow_child_app1.cpp follow_child_app2.cpp``

* Multihread ( pthread ) attach & execute: pass
* Fork + pthread execute: pass
* Fork + pthread attach: pass
* Fork + execv + pthread execute: fail
    执行成功，但子进程所创立的线程没有形成独立的thread文件，其编号从0开始。这或许是因为execv阻断了pin追溯父进程信息的能力，**需要修改创建Thread文件的逻辑**
* Fork + execv + pthread attach: fail
    ``E: Wait for injector pid 25899 to exit failed: No child processes``
    我们从上一个实验中发现，父进程所fork出的子进程的pid与execv所替换的进程pid不同（这是与原生pin的行为相违背）。它创建了一个新的进程/线程，其pid为fork出的子进程+1。或许这导致了pin无法追踪execv所创立的新进程（但为什么execute过程没有这个问题呢？）**我们需要检查一下pin是如何追踪execv的**
    