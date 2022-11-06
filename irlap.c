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

  Copyright (C) 2006-2007 Mathieu Albinet, mathieu17@gmail.com
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  o Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  o Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  o The names of the authors may not be used to endorse or promote
    products derived from this software without specific prior written
    permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* 19-02-07 MA added initial irlap parameters negotiation */

#include <string.h>
#include "pspirda.h"
#include "irlap.h"
#include "irphy.h"

#define IRLAP_TIMEOUT1			500
#define IRLAP_TIMEOUT2			20

#define IRLAP_PSEUDO_RANDOM_DEV_ADDR	0x18021967

#define IRLAP_PAR_NUMBER 7

#define IRLAP_PAR_BAUD_RATE_VAL		0x02 /*9600 bps (0x06 19200; ...) */
#define IRLAP_PAR_MAX_TURN_TIME_VAL	0x01 /* 500 ms */
#define IRLAP_PAR_DATA_SIZE_VAL		((IRLAP_DATA_SIZE-1)>>5)
#define IRLAP_PAR_WINDOW_SIZE_VAL	0x01 /* 1 frame */
#define IRLAP_PAR_ADD_BOFS_VAL		0x80 /* 0 BOFs */
#define IRLAP_PAR_MIN_TURN_TIME_VAL	0x80 /* 0 ms */
#define IRLAP_PAR_DISC_TIME_VAL		0x01 /* 3 s */

#define IRLAP_XBOF_COUNT		10

#define IRLAP_XBOF			0xFF
#define IRLAP_BOF			0xC0
#define IRLAP_EOF			0xC1
#define IRLAP_CE			0x7D
#define IRLAP_TRANS			0x20
#define IRLAP_FCS_INIT			0xFF
#define IRLAP_FCS_GOOD0			0xB8
#define IRLAP_FCS_GOOD1			0xF0

#define IRLAP_ADD_BOFS_0  0
#define IRLAP_ADD_BOFS_1  1
#define IRLAP_ADD_BOFS_2  2
#define IRLAP_ADD_BOFS_3  3
#define IRLAP_ADD_BOFS_5  5
#define IRLAP_ADD_BOFS_12  12
#define IRLAP_ADD_BOFS_24  24
#define IRLAP_ADD_BOFS_48  48


static uint8_t most_sign_bit(uint8_t byte)
{
  uint8_t i_bit;
  
  i_bit = 0;
  
   while(byte!=0) {
    i_bit++;
    byte = byte>>1;
  }

  return i_bit-1;
}

static inline void send_byte(uint8_t v)
{
  if(v==IRLAP_BOF || v==IRLAP_EOF || v==IRLAP_CE) {
    irphy_send(IRLAP_CE);
    irphy_send(v^IRLAP_TRANS);
  } else
    irphy_send(v);
}

static IrLAP_Neg_Param *set_neg_param_1(IrLAP_Neg_Param *param_p, uint8_t id, uint8_t value)
{
  param_p->id=id;
  param_p->len=1;
  param_p->value[0]=value;

  return (IrLAP_Neg_Param *)((uint8_t *)param_p+sizeof(IrLAP_Neg_Param)+param_p->len);
}

void get_neg_params(IrLAP_Context *context_p, uint8_t *info_p)
{
  IrLAP_Neg_Param *param_p;
  uint8_t i_param;
  
  param_p=(IrLAP_Neg_Param *)info_p;

  for( i_param=0; i_param<IRLAP_PAR_NUMBER; i_param++) {
    switch(param_p->id) {
      case IRLAP_PAR_BAUD_RATE:
      context_p->baud_rate &= param_p->value[0];
      break;
      case IRLAP_PAR_DATA_SIZE:
      context_p->data_size &= param_p->value[0];
      break;
      case IRLAP_PAR_ADD_BOFS:
      switch (most_sign_bit(param_p->value[0])) {
        case 0:
        context_p->add_bofs=IRLAP_ADD_BOFS_48;
        break;
        case 1:
        context_p->add_bofs=IRLAP_ADD_BOFS_24;
        break;
        case 2:
        context_p->add_bofs=IRLAP_ADD_BOFS_12;
        break;
        case 3:
        context_p->add_bofs=IRLAP_ADD_BOFS_5;
        break;
        case 4:
        context_p->add_bofs=IRLAP_ADD_BOFS_3;
        break;
        case 5:
        context_p->add_bofs=IRLAP_ADD_BOFS_2;
        break;
        case 6:
        context_p->add_bofs=IRLAP_ADD_BOFS_1;
        break;
        case 7:
        context_p->add_bofs=IRLAP_ADD_BOFS_0;
        break;
        default:
        context_p->add_bofs=IRLAP_XBOF_COUNT;
        break;
      }      
      break;
    }
    param_p = (IrLAP_Neg_Param *)((uint8_t *)param_p+sizeof(IrLAP_Neg_Param)+1);
  }
}

uint16_t irlap_append_neg_params(IrLAP_Context *context_p, uint8_t *info_p)
{
  IrLAP_Neg_Param *param_p;

  param_p=(IrLAP_Neg_Param *)info_p;

  param_p=set_neg_param_1(param_p, IRLAP_PAR_BAUD_RATE, context_p->baud_rate);
  param_p=set_neg_param_1(param_p, IRLAP_PAR_MAX_TURN_TIME, IRLAP_PAR_MAX_TURN_TIME_VAL);
  param_p=set_neg_param_1(param_p, IRLAP_PAR_DATA_SIZE, context_p->data_size);
  param_p=set_neg_param_1(param_p, IRLAP_PAR_WINDOW_SIZE, IRLAP_PAR_WINDOW_SIZE_VAL);
  param_p=set_neg_param_1(param_p, IRLAP_PAR_ADD_BOFS, IRLAP_PAR_ADD_BOFS_VAL);
  param_p=set_neg_param_1(param_p, IRLAP_PAR_MIN_TURN_TIME, IRLAP_PAR_MIN_TURN_TIME_VAL);
  param_p=set_neg_param_1(param_p, IRLAP_PAR_DISC_TIME, IRLAP_PAR_DISC_TIME_VAL);

  return (uint8_t *)param_p-info_p;
}

uint8_t irlap_matching_dest_addr(IrLAP_Context *context_p, IrLAP_Device_Addr addr_p)
{
  return
    memcmp(addr_p, context_p->dev_addr, sizeof(IrLAP_Device_Addr))==0 ||
    (addr_p[0]==0xFF && addr_p[1]==0xFF && addr_p[2]==0xFF && addr_p[3]==0xFF);
}

void irlap_init_context(IrLAP_Context *context_p)
{
  context_p->dev_addr[0]=(uint8_t)IRLAP_PSEUDO_RANDOM_DEV_ADDR;
  context_p->dev_addr[1]=(uint8_t)(IRLAP_PSEUDO_RANDOM_DEV_ADDR>>8);
  context_p->dev_addr[2]=(uint8_t)(IRLAP_PSEUDO_RANDOM_DEV_ADDR>>16);
  context_p->dev_addr[3]=(uint8_t)(IRLAP_PSEUDO_RANDOM_DEV_ADDR>>24);
  context_p->conn_addr=IRLAP_ADDR_NULL;
  context_p->vr=0;
  context_p->vs=0;
  context_p->baud_rate=IRLAP_PAR_BAUD_RATE_VAL;
  context_p->data_size=IRLAP_PAR_DATA_SIZE_VAL;
  context_p->add_bofs=IRLAP_XBOF_COUNT;
}

void irlap_send_frame(IrLAP_Context *context_p, IrLAP_Frame *frame_p, uint16_t size)
{
  uint16_t i;
  uint8_t fcs0, fcs1, v;

  for(i=0; i<context_p->add_bofs; i++)
    irphy_send(IRLAP_XBOF);

  irphy_send(IRLAP_BOF);

  fcs0=IRLAP_FCS_INIT;
  fcs1=IRLAP_FCS_INIT;

  for(i=0; i<size; i++) {
    v=((uint8_t *)frame_p)[i];
    send_byte(v);

    v^=fcs0;
    fcs0=fcs1;
    fcs1=v;

    fcs1^=fcs1<<4;
    fcs0^=fcs1>>4;
    fcs0^=fcs1<<3;
    fcs1^=fcs1>>5;
  }

  send_byte(~fcs0);
  send_byte(~fcs1);

  irphy_send(IRLAP_EOF);
}

int16_t irlap_receive_frame(IrLAP_Frame *frame_p)
{
  int16_t n;
  uint8_t fcs0, fcs1, v;

  do {
    if(!irphy_wait(IRLAP_TIMEOUT1))
      return -1;
  } while(irphy_receive()!=IRLAP_BOF);
  
  fcs0=IRLAP_FCS_INIT;
  fcs1=IRLAP_FCS_INIT;
  n=0;
  while(n<sizeof(IrLAP_Frame)) {
    if(!irphy_wait(IRLAP_TIMEOUT2))
      return -1;
    v=irphy_receive();

    if(v==IRLAP_BOF) {
      if(n>0)
	      return -1;
      continue;
    }

    if(v==IRLAP_EOF) {
      if(n<2 || fcs0!=IRLAP_FCS_GOOD0 || fcs1!=IRLAP_FCS_GOOD1)
	      return -1;
      return n-2;
    }

    if(v==IRLAP_CE) {
      if(!irphy_wait(IRLAP_TIMEOUT2))
	      return -1;
      v=irphy_receive() ^ IRLAP_TRANS;
    }

    ((uint8_t *)frame_p)[n++]=v;

    v^=fcs0;
    fcs0=fcs1;
    fcs1=v;

    fcs1^=fcs1<<4;
    fcs0^=fcs1>>4;
    fcs0^=fcs1<<3;
    fcs1^=fcs1>>5;
  }

  return -1;
}
