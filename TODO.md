# EventOS开发规划
---------
#### EventOS V0.2
+ 【完成】测试流事件
+ 【完成】测试事件的发布，订阅在任务中的使用。
+ 【完成】测试流事件满的情况。
+ 【完成】测试事件的广播机制。
+ eos_event_unsub
+ 【完成】eos_event_time_cancel
+ 【完成】eos_task_exit
+ 所有中断函数在中断里的测试
+ 整理开关中断功能
+ 发布V0.2。
--------------------------------------------------------
#### EventOS V0.3
将内核独立出来
--------------------------------------------------------
#### EventOS V0.4
+ 实现mutex。
+ 考虑是否将队列满修改为阻塞，或者可选（进入断言，阻塞）
+ eos_mutex_take
+ eos_mutex_release
+ eos_task_yield
+ eos_db_get_attribute
+ eos_db_set_attribute
+ eos_event_attribute_unblocked
+ eos_delay_no_event
+ eos_task_delete