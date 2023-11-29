#ifndef CAN_MODULE_ID_USER_DEFINE
#define CAN_MODULE_ID_USER_DEFINE

/*
CAN id 11bit 0x000 => 0x7FF

ODD         Mother board
EVEN        Modules

//////////////////////////////////////////
0x0**       Reserved

0x1ix       Bootloader, i module, X 
    0       BL RX
		1       BL TX
		2       BL Info
		
0x2**       VC => reserved

0x3ix       ISOTP high priority, i module,
		
0x4ix       CAN high priority, i module, 
    8       Data
    
0x5ix       ISOTP high priority, i module, 
    0       uC AT RX
		1       uc AT TX
    2       DATA RX
		3       DATA TX
		
0x6ix       CAN low priority, i module, 
    0       Module Info
		1       Status
		
///////// ID MODULE //////////		
0x00        Mower
0x01        Digital Fence
0x02        Voice Control
0x03        FindMyLandroid
0x04        Radio-Link
0x05        RTK-GPS
*/

//********************************//
//*** CAN ID BOOTLOADER MODULI ***//
//********************************//
#define CAN_BL_ID_DIGITAL_FENCE_TX 	0x111
#define CAN_BL_ID_DIGITAL_FENCE_RX 	0x110

#define CAN_BL_ID_4G_TX            	0x131
#define CAN_BL_ID_4G_RX            	0x130

#define CAN_BL_ID_VC_TX             0x121
#define CAN_BL_ID_VC_RX             0x120

//#define CAN_BL_ID_RL_TX             0x141
//#define CAN_BL_ID_RL_RX             0x140

#define CAN_BL_ID_RTK_TX            0x151
#define CAN_BL_ID_RTK_RX            0x150

//********************************//

//*********************//
//*** CAN ID MODULI ***//
//*********************//
#define CAN_ID_4G_BL              0x132
#define CAN_ID_4G_INFO 						0x630

#define CAN_ID_DF_BL              0x112
#define CAN_ID_DF_INFO            0x610

#define CAN_ID_VC_BL							0x122
#define CAN_ID_VC_INFO            0x620

//#define CAN_ID_RL_BL							0x142
//#define CAN_ID_RL_INFO            0x640

#define CAN_ID_RTK_BL							0x152
#define CAN_ID_RTK_INFO           0x650

//********************************//

#define CAN_ID_DIGITAL_FENCE_DATA 0x418

#define CAN_ID_4G_GPS_POS         0x638

#define CAN_ID_RL_RSSI            0x648
//*********************//

//*********************//
//*** CAN ID MOWER  ***//
//*********************//
#define CAN_ID_MOWER_STATUS       0x601

//*****************//
//*** CAN ISOTP ***//
//*****************//
#define ISOTP_ID_4G_AT_CMD       0x531
#define ISOTP_ID_4G_AT_RSP       0x530

#define ISOTP_ID_RL_AT_CMD       0x541
#define ISOTP_ID_RL_AT_RSP       0x540

#define ISOTP_ID_4G_DATA_TX      0x533
#define ISOTP_ID_4G_DATA_RX      0x532

//#define ISOTP_ID_RL_DATA_TX      0x543
//#define ISOTP_ID_RL_DATA_RX      0x542

#define ISOTP_ID_RTK_DATA_TX     0x310 //0x551
#define ISOTP_ID_RTK_DATA_RX     0x300 //0x550

#define ISOTP_ID_VC_DATA_RX      0x202
#define ISOTP_ID_VC_DATA_TX      0x201


//*****************//

#endif


