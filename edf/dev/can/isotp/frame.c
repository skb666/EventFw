#include <stdio.h>

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "canModuleIdUserDefine.h"

#include "frame.h"
#include "isotp.h"
#include "version.h"


void isotpCb_RTK_Data( ISOTP_msgRx_type *IsotpMsg );

ISOTP_msgTx_type isotpMsg_dataTx = {
	.id = ISOTP_ID_RTK_DATA_TX,
	.flowControl.id  = ISOTP_ID_RTK_DATA_RX,
	.flowControl.enable = true,	
	.ISOTP_state = ISOTP_START_SEND,
};

ISOTP_msgRx_type isotpMsg_dataRx = {
	.id = ISOTP_ID_RTK_DATA_RX,
	.flowControl.send.id  = ISOTP_ID_RTK_DATA_TX,
	.flowControl.send.nBlock  = FC_BLOCK_COUNTER,
	.flowControl.send.delay  = FC_DELAY_BLOCK_MS,	
	.flowControl.received = false,
	.data = 0,		
	.malloc_len = 0,
	.frameCompled = isotpCb_RTK_Data,
};

ISOTP_msgRx_type *const isotpTable[] = {
	&isotpMsg_dataRx,
	(ISOTP_msgRx_type*) NULL,
};

void isotpCb_RTK_Data( ISOTP_msgRx_type *IsotpMsg ) 
{
	//frame_isotp_dataTx( IsotpMsg->data, IsotpMsg->data_len );
}

#define HELLO_WORLD "HELL\n\r"

void isotp_dataTx( uint8_t *sendbuff,uint16_t len )
{
		static uint32_t isotp_timer = 0;
		if( isotp_timer < osKernelGetTickCount() ) {
			//isotpMsg_dataTx.data = (uint8_t*)HELLO_WORLD;
			isotpMsg_dataTx.data = sendbuff;
			
			//isotpMsg_dataTx.data_len = sizeof(HELLO_WORLD);
			isotpMsg_dataTx.data_len = len;
			
			ISOTP_ret_id ret = ISOTP_sendFrame( &isotpMsg_dataTx );
			
			if( ret == ISOTP_ERROR || ret == ISOTP_TIMEOUT || ret == ISOTP_FLOW_TIMEOUT ) 
				isotp_timer = osKernelGetTickCount() + 200;
			else if( ret == ISOTP_OK )
				isotp_timer = osKernelGetTickCount() + 200;
		}
		
}