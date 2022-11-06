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

/* 19-02-07 MA Changed irlap_send_frame call */
/* 19-02-07 MA Changed IrIAS ircomm structure */

#include <string.h>

#include "ircomm.h"

#define IRCOMM_CTRL_SERVICE_TYPE_VAL		0x01 /* 3-Wire */

IrIAS_Node ircomm_ias_node={
  "IrDA:IrCOMM",
  "IrDA:TinyTP:LsapSel",
  IRIAP_VALUE_TYPE_INTEGER,
  IRCOMM_LSAP_SEL_VAL,
  IRCOMM_LSAP_SEL_VAL
};

static IrCOMM_Control *set_ctrl_param_1(IrCOMM_Control *ctrl_p, uint8_t id, uint8_t value)
{
  ctrl_p->id=id;
  ctrl_p->len=1;
  ctrl_p->value[0]=value;

  return (IrCOMM_Control *)((uint8_t *)ctrl_p+sizeof(IrCOMM_Control)+ctrl_p->len);
}

static uint16_t append_ctrl_params(uint8_t *info_p)
{
  IrCOMM_Control *ctrl_p;

  ctrl_p=(IrCOMM_Control *)info_p;

  ctrl_p=set_ctrl_param_1(ctrl_p, IRCOMM_CTRL_SERVICE_TYPE, IRCOMM_CTRL_SERVICE_TYPE_VAL);

  return (uint8_t *)ctrl_p-info_p;
}

void ircomm2_serve(IrLAP_Context *context_p,
		   int16_t (*put_data_cb)(uint8_t *data, uint16_t size,
					  uint8_t *resp_buf, uint16_t resp_buf_size,
					  uint8_t *terminate_p,
					  void *user_data),
		   void *user_data)
{
  IrIAS_Node *ias_nodes[2];
  int16_t n, last_resp_size;
  IrLAP_Frame req_frame, resp_frame, *last_resp_p;
  IrCOMM_Frame *req_p, *resp_p;
  uint8_t terminate;

  ias_nodes[0]=&ircomm_ias_node;
  ias_nodes[1]=0;

  req_p=(IrCOMM_Frame *)&req_frame;
  resp_p=(IrCOMM_Frame *)&resp_frame;

  last_resp_p=0;
  last_resp_size=0;

  terminate=0;

  for(;;) {
    if((n=irttp2_receive_frame(context_p, &req_frame,
			       last_resp_p, last_resp_size,
			       IRLMP_HINT1_COMM,
			       ias_nodes))<0)
      return;

    if(req_p->head.irttp.irlmp.dlsap_sel!=IRCOMM_LSAP_SEL_VAL)
      continue;

    if(n>=sizeof(IrCOMM_Head) &&
       n>=sizeof(IrCOMM_Head)+req_p->head.clen) {
      n-=sizeof(IrCOMM_Head)+req_p->head.clen;

      if(req_p->head.clen)
	      resp_p->head.clen=append_ctrl_params(resp_p->info);
      else
	      resp_p->head.clen=0;

      if(n>0) {
	      if((last_resp_size=(*put_data_cb)(req_p->info+req_p->head.clen, n,
					  resp_p->info+resp_p->head.clen, sizeof(IrLAP_Frame)-sizeof(IrCOMM_Head)-resp_p->head.clen,
					  &terminate,
					  user_data))<0)
	      return;
      } else
	      last_resp_size=0;

      if(last_resp_size>0 || resp_p->head.clen) {
	      last_resp_p=&resp_frame;
	      last_resp_size+=sizeof(IrCOMM_Head)+resp_p->head.clen;
	      irttp2_reply(context_p, (IrTTP_Frame *)req_p, (IrTTP_Frame *)last_resp_p, last_resp_size);
	      continue;
      }
    }

    last_resp_p=0;
    last_resp_size=0;

    resp_frame.head.addr=context_p->conn_addr;
    resp_frame.head.ctrl=IRLAP_CTRL_S_RR | IRLAP_CTRL_P_F_MASK | context_p->vr<<IRLAP_CTRL_NR_BIT_POS;

    irlap_send_frame(context_p, (IrLAP_Frame *)&resp_frame, sizeof(IrLAP_Head));

    if(terminate)
      return;
  }
}
