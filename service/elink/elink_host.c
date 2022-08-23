/* include ------------------------------------------------------------------ */
#include "elink.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "exsh.h"

/* config ------------------------------------------------------------------- */
#define ELINK_DEBUG_SWD_EN                          1
#define ELINK_MCU_NAME                              "STM32F429IG"

/* private define ----------------------------------------------------------- */
#define elink_err(...) do {                                                    \
        printf("\033[1;31m");                                                  \
        printf(__VA_ARGS__);                                                   \
        printf("\033[0;0m\n");                                                 \
        fflush(stdout);                                                        \
    } while (0)

#define elink_info(...) do {                                                   \
        printf("\033[1;32m");                                                  \
        printf(__VA_ARGS__);                                                   \
        printf("\033[0;0m\n");                                                 \
        fflush(stdout);                                                        \
    } while (0)

#define EBLOCK_READ_BUFF_SIZE                       (4096)
#define ELINK_MQ_WRITE_SIZE                         (4096)
#define ELINK_MQ_READ_SIZE                          (4096)

/* private data structure --------------------------------------------------- */
enum
{
    ELINK_TIF_JTAG = 0,
    ELINK_TIF_SWD,
};

enum
{
    ELink_OK                = 0,
    ELink_LoadDllFail       = -1,
    ELink_DllNotFound       = -2,
};

typedef char (* func_is_open_t)(void);
typedef const char * (* func_open_t)(void);
typedef void (* func_close_t)(void);
typedef int (* func_get_sn_t)(void);
typedef int (* func_cmd_t)(const char *in, char *error, int buff_size);
typedef void (* func_set_speed_t)(uint32_t speed);
typedef int (* func_set_tif_t)(int interface);
typedef int (* func_read_memory_t)(uint32_t addr, uint32_t count, void *data);
typedef int (* func_set_memory_t)(uint32_t addr, uint32_t count, const void *data);

/* private data ------------------------------------------------------------- */
static func_is_open_t rtt_is_connected;
static func_is_open_t rtt_is_open;
static func_open_t rtt_open;
static func_close_t rtt_close;
static func_get_sn_t rtt_get_sn;
static func_get_sn_t rtt_connect;
static func_cmd_t rtt_cmd;
static func_set_speed_t rtt_set_speed;
static func_set_tif_t rtt_set_tif;
static func_set_memory_t rtt_set_memory;
static func_read_memory_t rtt_read_memory;

static void *lib = NULL;

static bool elink_started = false;
static osMessageQueueId_t mq_write;
static osMessageQueueId_t mq_read;
static osThreadId_t thread;

static uint32_t addr_elink;
static uint32_t state_host = ElinkState_Init;
static uint8_t *memory_tx;
static uint8_t *memory_rx;

/* private function --------------------------------------------------------- */
static int32_t elink_hw_init(void);
static int32_t elink_connect(void);
static int32_t elink_get_sn(void);
static char elink_is_connected(void);
static char elink_is_open(void);
static const char *elink_open(void);
static void elink_close(void);
static int32_t elink_cmd(const char *in, char *error, int32_t buff_size);
static int32_t elink_set_tif(int interface);
static int32_t elink_read_memory(uint32_t addr, uint32_t count, void *data);
static int32_t elink_set_memory(uint32_t addr, uint32_t count, const void *data);
static void elink_set_speed(uint32_t speed);

static void func_elink_host(void *para);

static void *loader_open(const char *lib_path);
static void *loader_function(void *lib, const char *function);
static void loader_close(void *lib);

static void _set_mcu_state(uint8_t state);
static void _set_mcu_buff_tx_tail(uint16_t tail);
static void _set_mcu_buff_rx_head(uint16_t head);

/* public function ---------------------------------------------------------- */
void elink_init(void)
{
    char error[256];
    elink_block_t eblock;
    
    int32_t ret_read = 0;

    char cmd_mcu_device[128];
    memset(cmd_mcu_device, 0, 128);
    strcat(cmd_mcu_device, "Device = ");
    strcat(cmd_mcu_device, ELINK_MCU_NAME);

    elink_info("elink init ...");
    elink_hw_init();
    elink_info("elink_hw_init end.");
    elink_started = true;

    // Connect to the elink MCU.
    if (elink_is_connected() == 0)
    {
        const char *str_ret = elink_open();
        if (str_ret != 0)
        {
            elink_err("elink Open Fail: %s.", str_ret);
            return;
        }
        elink_info("elink Open Success. Sn: %d.", elink_get_sn());

        int32_t ret_cmd = elink_cmd("ProjectFile = setting.ini", error, 256);
        if (error[0])
        {
            elink_err("elink Cmd. ret: %s.", error);
        }
        ret_cmd = elink_cmd(cmd_mcu_device, error, 256);
        if (error[0])
        {
            elink_err("elink Cmd. ret: %s.", error);
        }
        uint32_t tif;
        int32_t ret_tif;
#if (ELINK_DEBUG_SWD_EN == 0)
        ret_tif = elink_set_tif(ELINK_TIF_JTAG);
#else
        ret_tif = elink_set_tif(ELINK_TIF_SWD);
#endif
        elink_info("elink_set_tif. ret: %d.", ret_tif);
        elink_set_speed(50000);

        int32_t ret_connect = elink_connect();
        elink_info("elink Connect: %d.", ret_connect);
    }
    else
    {
        elink_info("elink is alrady open. Sn: %d.", elink_get_sn());
    }

    // Find the elink block in MCU.
    bool eblock_found = false;
    uint8_t buff_read[EBLOCK_READ_BUFF_SIZE];
    while (1)
    {
        for (uint32_t i = 0; i < 100; i ++)
        {
            ret_read = elink_read_memory(
                (0x20000000 + i * EBLOCK_READ_BUFF_SIZE),
                EBLOCK_READ_BUFF_SIZE,
                buff_read);

            for (uint32_t j = 0; j < (EBLOCK_READ_BUFF_SIZE - 16); j ++)
            {
                if (buff_read[j] != '^') continue;
                if (buff_read[j + 1] != '+') continue;
                if (buff_read[j + 2] != 'e') continue;
                if (buff_read[j + 3] != 'l') continue;

                if (strcmp((char *)&buff_read[j], "^+elink.block@$") == 0)
                {
                    addr_elink = (0x20000000 + i * 4096 + j);
                    eblock_found = true;
                    break;
                }
            }

            if (eblock_found)
            {
                break;
            }
        }

        if (eblock_found)
        {
            break;
        }

        elink_info("Elink block not found. Continue !\n");
        osDelay(10);
    }

    addr_elink -= 4;
    elink_info("Elink address 0x%08x.", addr_elink);

    // Check the magic is covered or not.
    uint32_t size_buff_tx, size_buff_rx;
    ret_read = elink_read_memory(addr_elink, sizeof(elink_block_t), &eblock);
    if (eblock.magic != 0xdeadbeef)
    {
        elink_err("elink block magic 0xdeadbeef is covered. Value: %08x.",
                    eblock.magic);
        goto exit;
    }
    elink_info("Elink magic checking end.");

    // Apply the memory tx.
    memory_tx = malloc(eblock.capacity_tx);
    memory_rx = malloc(eblock.capacity_rx);

    // Create two message queue for the writing and reading functions.
    mq_write = osMessageQueueNew(ELINK_MQ_WRITE_SIZE, 1, NULL);
    assert(mq_write != NULL);
    mq_read = osMessageQueueNew(ELINK_MQ_READ_SIZE, 1, NULL);
    assert(mq_read != NULL);
    elink_info("MQ creating end.");

    // Start the thread to send mq_write data and receive data into mq_read.
    thread = osThreadNew(func_elink_host, NULL, NULL);

    elink_info("Thread creating end.");

exit:
    return;
}

// For the host, writing is put data into the buffer rx.
uint16_t elink_write(void *data, uint16_t size)
{
    uint16_t ret = 0;
    osStatus_t ret_mq = osOK;

    uint8_t *buff = (uint8_t *)data;
    for (uint32_t i = 0; i < size; i ++)
    {
        ret_mq = osMessageQueuePut(mq_write, &buff[i], 0, 1000);
        if (ret_mq != osOK)
        {
            elink_err("osMessageQueuePut fails, err: %d.", (int32_t)ret_mq);
            goto exit;
        }

        ret ++;
    }

exit:
    return ret;
}

// For the host, reading is get data from the buffer tx.
uint16_t elink_read(void *data, uint16_t size)
{
    uint16_t ret = 0;
    osStatus_t ret_mq = osOK;

    uint8_t *buff = (uint8_t *)data;
    for (uint32_t i = 0; i < size; i ++)
    {
        ret_mq = osMessageQueueGet(mq_read, &buff[i], 0, 0);
        if (ret_mq != osOK)
        {
            goto exit;
        }

        ret ++;
    }

exit:
    return ret;
}

bool elink_running(void)
{
    return elink_started;
}

/* private function --------------------------------------------------------- */
static int32_t elink_hw_init(void)
{
    int32_t ret = ELink_OK;

    // TODO Search the JLinkARM.dll file in Disk C or D.

    lib = loader_open("C:\\Program Files\\SEGGER\\JLink\\JLinkARM.dll");
    if (NULL == lib)
    {
        elink_err("Loading JLINKARM.dll failures.");
        ret = ELink_LoadDllFail;
        goto exit;
    }
    else
    {
        elink_info("Loading JLINKARM.dll Success.");
    }

    // Functions in the JLINKARM.dll.
    rtt_is_connected = (func_is_open_t)loader_function(lib, "JLINKARM_IsConnected");
    rtt_is_open = (func_is_open_t)loader_function(lib, "JLINKARM_IsOpen");
    rtt_open = (func_open_t)loader_function(lib, "JLINKARM_Open");
    rtt_close = (func_close_t)loader_function(lib, "JLINKARM_Close");
    rtt_get_sn = (func_get_sn_t)loader_function(lib, "JLINKARM_GetSN");
    rtt_connect = (func_get_sn_t)loader_function(lib, "JLINKARM_Connect");
    rtt_cmd = (func_cmd_t)loader_function(lib, "JLINKARM_ExecCommand");
    rtt_set_speed = (func_set_speed_t)loader_function(lib, "JLINKARM_SetSpeed");
    rtt_set_tif = (func_set_tif_t)loader_function(lib, "JLINKARM_TIF_Select");
    rtt_set_memory = (func_set_memory_t)loader_function(lib, "JLINKARM_WriteMem");
    rtt_read_memory = (func_read_memory_t)loader_function(lib, "JLINKARM_ReadMem");

exit:
    return ret;
}

static int32_t elink_connect(void)
{
    return rtt_connect();
}

static int32_t elink_get_sn(void)
{
    return rtt_get_sn();
}

static char elink_is_connected(void)
{
    return rtt_is_connected();
}

static char elink_is_open(void)
{
    return rtt_is_open();
}

static const char *elink_open(void)
{
    return rtt_open();
}

static void elink_close(void)
{
    rtt_close();
    loader_close(lib);
}

static int32_t elink_cmd(const char *in, char *error, int32_t buff_size)
{
    return rtt_cmd(in, error, buff_size);
}

static int32_t elink_set_tif(int interface)
{
    return rtt_set_tif(interface);
}

static int32_t elink_read_memory(uint32_t addr, uint32_t count, void *data)
{
    return rtt_read_memory(addr, count, data);
}

static int32_t elink_set_memory(uint32_t addr, uint32_t count, const void *data)
{
    return rtt_set_memory(addr, count, data);
}

static void elink_set_speed(uint32_t speed)
{
    rtt_set_speed(speed);
}

/* dll file loader ---------------------------------------------------------- */
#include "libloaderapi.h"

static void *loader_open(const char *lib_path)
{
    HMODULE lib = LoadLibraryEx(lib_path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    // HMODULE lib = LoadLibraryEx(lib_path, NULL, NULL);
    return (void *)lib;
}

static void *loader_function(void *lib, const char *function)
{
    return (void *)GetProcAddress((HMODULE)lib, function);
}

static void loader_close(void *lib)
{
    FreeLibrary((HMODULE)lib);
}

/* Private function --------------------------------------------------------- */
static void func_elink_host(void *para)
{
    int32_t ret_read = 0;
    elink_block_t eblock;

    while (ex_shell_is_running())
    {
        ret_read = elink_read_memory(addr_elink, sizeof(elink_block_t), &eblock);
        // Check the magic is covered or not.
        if (eblock.magic != 0xdeadbeef)
        {
            elink_err("elink cb magic 0xdeadbeef is covered. Value: %08x.",
                        eblock.magic);
            break;
        }

        // If MCU is reset,
        if (ElinkState_Init == eblock.state)
        {
            _set_mcu_state(ElinkState_Running);
        }

        // Read data from buff tx and put data into message queue
        if (eblock.tx_read_lock == 0)
        {
            // If the buffer is empty, continue;
            if (eblock.head_tx == eblock.tail_tx)
            {
                continue;
            }

            // Get the buffer head and tail pointer.
            uint32_t addr_head = ((uint32_t)eblock.buff_tx + eblock.head_tx);
            uint32_t addr_tail = ((uint32_t)eblock.buff_tx + eblock.tail_tx);
            uint32_t size;
            if (eblock.head_tx > eblock.tail_tx)
            {
                size = (eblock.head_tx - eblock.tail_tx);
                ret_read = elink_read_memory(addr_tail, size, memory_tx);

                for (int32_t i = 0; i < size; i ++)
                {
                    osMessageQueuePut(mq_read, &memory_tx[i], 0, osWaitForever);
                }
            }
            else
            {
                uint32_t size_to_tail = (eblock.capacity_tx - eblock.tail_tx);
                elink_read_memory(addr_tail, size_to_tail, memory_tx);
                elink_read_memory((uint32_t)eblock.buff_tx, eblock.head_tx,
                                &memory_tx[size_to_tail]);
                size = (eblock.capacity_tx - eblock.tail_tx + eblock.head_tx);

                for (int32_t i = 0; i < size; i ++)
                {
                    osMessageQueuePut(mq_read, &memory_tx[i], 0, osWaitForever);
                }
            }

            _set_mcu_buff_tx_tail(eblock.head_tx);
        }

        // Read data from message queue and write data into buff rx
        if (eblock.rx_write_lock == 0)
        {
            // Calculate the free space size.
            uint32_t size_free = 0;
            if (eblock.head_rx >= eblock.tail_rx)
            {
                size_free = eblock.capacity_rx - (eblock.head_rx - eblock.tail_rx) - 1;
            }
            else
            {
                size_free = (eblock.tail_rx - eblock.head_rx) - 1;
            }

            uint32_t count_mem_rx = 0;
            for (uint32_t i = 0; i < size_free; i ++)
            {
                uint8_t byte;
                osStatus_t ret_mq = osMessageQueueGet(mq_write, &byte, 0, 0);
                if (ret_mq != osOK)
                {
                    break;
                }

                memory_rx[i] = byte;
                count_mem_rx ++;
            }

            // Calculate the size of two sections
            uint32_t size_sec0 = 0, size_sec1 = 0;
            if (size_free != 0 && count_mem_rx != 0)
            {
                uint8_t mem_sec0[size_free], mem_sec1[size_free];

                uint32_t capacity_sec0;
                if (eblock.head_rx >= eblock.tail_rx)
                {
                    capacity_sec0 = eblock.capacity_rx - eblock.head_rx;
                }
                else
                {
                    capacity_sec0 = size_free;
                }

                size_sec0 = capacity_sec0 > count_mem_rx ? count_mem_rx : capacity_sec0;
                size_sec1 = capacity_sec0 > count_mem_rx ? 0 : (count_mem_rx - capacity_sec0);
                memcpy(mem_sec0, memory_rx, size_sec0);
                memcpy(mem_sec1, &memory_rx[size_sec0], size_sec1);

                // Calculate the address of the buffer rx.
                uint32_t address_sec0 = ((uint32_t)eblock.buff_rx + eblock.head_rx);
                uint32_t address_sec1 = (uint32_t)eblock.buff_rx;

                // Copy the data into buffer rx
                if (size_sec0 != 0)
                {
                    elink_set_memory(address_sec0, size_sec0, mem_sec0);
                }
                if (size_sec1 != 0)
                {
                    elink_set_memory(address_sec1, size_sec1, mem_sec1);
                }
            
                // Write the buffer head
                uint16_t head = eblock.head_rx + (size_sec0 + size_sec1);
                head = (head % eblock.capacity_rx);
                _set_mcu_buff_rx_head(head);
            }
        }
    }

exit:
    _set_mcu_state(ElinkState_Stop);

    elink_close();

    elink_err("Exit the E-Terminal.");
    fflush(stdout);

    state_host = ElinkState_Stopped;

    elink_started = false;
}

static void _set_mcu_state(uint8_t state)
{
    uint32_t value = (uint32_t)state;
    elink_set_memory((addr_elink + 20), 4, &value);
}

static void _set_mcu_buff_tx_tail(uint16_t tail)
{
    uint32_t addr_tx_tail = addr_elink + sizeof(elink_block_t) - 4;
    elink_set_memory(addr_tx_tail, 2, &tail);
}

static void _set_mcu_buff_rx_head(uint16_t head)
{
    uint32_t addr_rx_head = addr_elink + sizeof(elink_block_t) - 2;
    elink_set_memory(addr_rx_head, 2, &head);
}
