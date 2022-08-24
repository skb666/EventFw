#### Embedded LINK

MCU端存在的问题：
1. None

Host端存在的问题
1. Keil中从仿真状态退出，会造成错误。 检查magic为0xAAAAAAAA时，等一段时间。
2. MCU重启时的处理。（软重启和硬重启都测试一下） 检查magic为0xAAAAAAAA时，等一段时间。
