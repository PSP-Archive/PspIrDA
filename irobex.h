/*
  Copyright (C) 2002-2003 Gerd Rausch, BlauLogic (http://blaulogic.com)
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  3. Except as contained in this notice, neither the name of BlauLogic
     nor the name(s) of the author(s) may be used to endorse or promote
     products derived from this software without specific prior written
     permission.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHOR(S) OR BLAULOGIC BE LIABLE FOR ANY CLAIM,
  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
  OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
  THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/* 19-02-07 MA IrOBEX MTU set to 255 */

#ifndef _IROBEX_H
#define _IROBEX_H

#include <inttypes.h>

#include "irttp.h"

#define IROBEX_LSAP_SEL_VAL		1

#define IROBEX_VERSION			0x10
#define IROBEX_HEAD_SIZE		(sizeof(IrOBEX_Head)-sizeof(IrTTP_Head))
#define IROBEX_MTU          255/*(IRLAP_DATA_SIZE-sizeof(IrOBEX_Head)) TODO : 255 is the min in the protocol */
#define IROBEX_MIN_MTU			(IROBEX_HEAD_SIZE+16)

#define IROBEX_REQ_CODE_CONNECT		0x00
#define IROBEX_REQ_CODE_DISCONNECT	0x01
#define IROBEX_REQ_CODE_PUT		0x02
#define IROBEX_REQ_CODE_GET		0x03
#define IROBEX_REQ_FINAL_MASK		0x80

#define IROBEX_RESP_CODE_CONTINUE	0x10
#define IROBEX_RESP_CODE_OK		0x20
#define IROBEX_RESP_CODE_BAD_REQUEST	0x40
#define IROBEX_RESP_CODE_NOT_IMPL	0x51
#define IROBEX_RESP_FINAL_MASK		0x80

#define IROBEX_TYPE_UNICODE_TEXT	0x00
#define IROBEX_TYPE_BYTE_SEQUENCE	0x40
#define IROBEX_TYPE_ONE_BYTE		0x80
#define IROBEX_TYPE_FOUR_BYTES		0xC0
#define IROBEX_TYPE_MASK		0xC0

#define IROBEX_ID_COUNT			0x00
#define IROBEX_ID_NAME			0x01
#define IROBEX_ID_TYPE			0x02
#define IROBEX_ID_LENGTH		0x03
#define IROBEX_ID_TIME			0x04
#define IROBEX_ID_DESCRIPTION		0x05
#define IROBEX_ID_TARGET		0x06
#define IROBEX_ID_HTTP			0x07
#define IROBEX_ID_BODY			0x08
#define IROBEX_ID_END_OF_BODY		0x09

#define IROBEX_ACK_CODE(code)		((code & IROBEX_REQ_FINAL_MASK) ? IROBEX_RESP_CODE_OK : IROBEX_RESP_CODE_CONTINUE)

typedef struct IrOBEX_Head {
  IrTTP_Head irttp;
  uint8_t code;
  uint8_t len[2];
} IrOBEX_Head;

typedef struct IrOBEX_Connect {
  uint8_t version;
  uint8_t flags;
  uint8_t mtu[2];
} IrOBEX_Connect;

typedef struct IrOBEX_Frame {
  IrOBEX_Head head;
  union {
    IrOBEX_Connect connect;
    uint8_t info[0];
  } u;
} IrOBEX_Frame;

extern IrIAS_Node irobex_ias_node;

uint8_t irobex_send(IrLAP_Context *context_p,
		    IrLAP_Device_Addr addr_p,
		    int16_t (*get_name_cb)(char *name, uint16_t size, void *user_data),
		    int16_t (*get_data_cb)(uint8_t *data, uint16_t size, void *user_data),
		    void *user_data);

uint8_t irobex_receive(IrLAP_Context *context_p,
		       uint8_t (*put_name_cb)(char *name, void *user_data),
		       uint8_t (*put_data_cb)(uint8_t *data, uint16_t size, void *user_data),
		       void *user_data);

#endif /* _IROBEX_H */
