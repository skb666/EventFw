# Test说明
------
### 说明
1-0 从一个任务Give，满负荷向另一个任务，同时从0.8ms中断中和高优先级任务中，发送纯事件。
1-1 从一个任务Give，满负荷向另一个任务，同时从0.8ms中断中和高优先级任务中，向任务句柄发送纯事件。
测试直接发送任务句柄的情况。看看哈希的开销到底有多大（测试结束，F429 2.8us）
2-0 从一个任务Give，满负荷向另一个任务Value，同时从0.8ms中断中和高优先级任务Middle和High中，发布订阅的纯事件。
2-1 从一个任务Give，满负荷向状态机Sm，同时从0.8ms中断中和高优先级任务High与Middle中，发布订阅的纯事件。
2-2 从一个任务Give，满负荷向状态机Sm和另一个任务Value，同时从0.8ms中断中，和高优先级任务High与Middle中，发布订阅的纯事件。
3 从一个任务Give，满负荷向另一个任务Value，同时从0.8ms中断中，和高优先级任务High与Middle中，发送流事件。
4 从一个任务Give，满负荷向数据库Value，同时从0.8ms中断中，和高优先级任务High与Middle中，发送数据，从另一个任务读取。
5-0 从一个任务Give，满负荷向另一个任务Value，同时从0.8ms中断中，和高优先级任务High与Middle中，发送两个事件，一个是任务特定接收的事件，一个不是。
5-1 从一个任务Give，满负荷向另一个任务Value，同时从0.8ms中断中，和高优先级任务High与Middle中，发布两个事件，一个是任务特定接收的事件，一个不是。
6 从一个任务Give，满负荷向另一个任务Value，同时从0.8ms中断中，和高优先级任务High与Middle中，发送值事件。
7 从一个任务Give，满负荷向另一个任务Value，同时从0.8ms中断中，和高优先级任务High与Middle中，发布值事件，同时Value订阅1ms周期事件。
9 测试task_delay_no_event。

以上的任务优先级：High > Reactor > Sm > Value > Middle > Give = GiveSame
High任务发送的周期为1ms，Middle发送周期为2ms。