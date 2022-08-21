# EventOS项目说明

<a href='https://gitee.com/event-os/eventos/stargazers'><img src='https://gitee.com/event-os/eventos/badge/star.svg?theme=dark' alt='star'></img></a><a href='https://gitee.com/event-os/eventos/members'><img src='https://gitee.com/event-os/eventos/badge/fork.svg?theme=dark' alt='fork'></img></a>

邮箱：**event-os@outlook.com**，QQ群：**667432915**。

-------
### 一、EventOS是什么？
**EventOS**，是一个面向单片机、事件驱动的嵌入式开发平台。它主要有两大技术特色：一是事件驱动，二是超轻量。**EventOS**的目标，是开发一个企业级的嵌入式开发平台，以**事件总线**为核心，打造一个统一的嵌入式技术生态，涵盖RTOS内核、IO框架、设备框架、文件系统和中间件，为广大企业用户和嵌入式开发者们，提供搞可靠性的、高性能的、现代且高开发效率的嵌入式开发环境。

经过几个月艰苦的开发，**EventOS** V0.2终于能和大家见面，期间走了不少弯路。在V0.2中，**EventOS**实现了一个完整的嵌入式操作系统内核。为了加快**EventOS**开发的进度，**EventOS**内核中的e_kernel部分，是基于RT-Thread的内核，保留任务、定时器、信号量和线程锁，删除其他，整理简化而来。在此声明，e_kernel部分，遵循Apache-2.0开源协议，其版权属于RT-Thread开发团队。向RT-Thread团队的专业与开发精神，致以敬意和感谢！

### 二、EventOS的特色

#### 1. 强大的事件系统

**事件总线**为核心组件，灵活易用，是进行线程（状态机）间同步或者通信的主要手段，也是对EventOS分布式特性和跨平台开发进行支持的唯一手段。事件支持通知型，块事件、流事件、时间事件等不同的类型，支持**发布-订阅**和**直接发送**两种投递方式。

#### 2. 事件机制与数据库的紧密结合

在**EventOS**中，创新性的对事件机制与数据库，进行了充分的结合。二者的结合，衍生出了强大而友好的任务间通信机制，使用起来，远好于传统RTOS的邮箱或者消息队列。

#### 3. 最大限度的兼容性
**EventOS**会对常见的处理器架构，如ARM Cortex、RISC-V等进行支持，并对常见的RTOS如FreeRTOS等进行支持，最大限度的兼容当前的嵌入式开发生态，使每一个想尝试**EventOS**的网友，以最小的代价迁移到**EventOS**平台。

EventOS支持的硬件平台：
+ ARM Cortex-M0/M0+
+ ARM Cortex-M3
+ ARM Cortex-M4
+ ARM Cortex-M7

EventOS支持的软件平台：
+ FreeRTOS
+ uC/OS-II
+ uc/OS-III
+ embos

EventOS支持的IDE和编译器：
+ MDK KEIL
+ IAR
+ STM32CubeIDE
+ GCC

#### 4. 功能强大的软定时器和时间事件
**EventOS**不仅支持传统的软定时器，还以时间事件的形式，对软定时器功能，进行优雅且功能强大的实现。

#### 5. 简约的基础设施
**EventOS**的远期目标，定位于可跨平台开发，并具备分布式通信能力的RTOS，因为其基础设施，都需要具备分布式能力。目前，**EventOS**支持任务、定时器、事件、KV数据库和线程锁五种基础设施。
