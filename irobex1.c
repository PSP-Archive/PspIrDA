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

#include <string.h>

#include "irobex.h"
#include "pspirda.h"

static uint16_t expand_unicode(uint8_t *buf, uint16_t size)
{
  uint16_t n, i, j;

  i=size<<1;
  j=size;

  n=i;
  buf[n++]=0;
  buf[n++]=0;

  while(j>0) {
    buf[--i]=buf[--j];
    buf[--i]=0;
  }

  return n;
}

static uint8_t obex_connect(IrLAP_Context *context_p, IrOBEX_Frame *resp_p,
                             uint8_t dlsap_sel, uint16_t *peer_mtu_size_p)
{
  IrOBEX_Frame req;

  req.head.code=IROBEX_REQ_CODE_CONNECT | IROBEX_REQ_FINAL_MASK;
  req.head.len[0]=(IROBEX_HEAD_SIZE+sizeof(IrOBEX_Connect))>>8;
  req.head.len[1]=IROBEX_HEAD_SIZE+sizeof(IrOBEX_Connect);

  req.u.connect.version=IROBEX_VERSION;
  req.u.connect.flags=0;
  req.u.connect.mtu[0]=IROBEX_MTU>>8;
  req.u.connect.mtu[1]=IROBEX_MTU;

  if(irttp1_send_receive_i_frame(context_p,
                 (IrTTP_Frame *)&req, sizeof(IrOBEX_Head)+sizeof(IrOBEX_Connect),
                 IROBEX_LSAP_SEL_VAL, dlsap_sel,
                 (IrLAP_Frame *)resp_p)<sizeof(IrOBEX_Head)+sizeof(IrOBEX_Connect)) {
    DEBUG( 2,  "Error connect : trying to continue!\n");
  } /* WAS return 0; */
  
  if(resp_p->head.code!=(IROBEX_RESP_CODE_OK | IROBEX_RESP_FINAL_MASK)) {
    DEBUG( 2, "Error response code to connection request : trying to continue! %d\n", resp_p->head.code );
    *peer_mtu_size_p = IROBEX_MTU;
  } /* WAS return 0 */
  else {
    *peer_mtu_size_p=
    (uint16_t)resp_p->u.connect.mtu[0]<<8 |
    (uint16_t)resp_p->u.connect.mtu[1];  
  }
  
  if(*peer_mtu_size_p>IROBEX_MTU)
    *peer_mtu_size_p=IROBEX_MTU;
  if(*peer_mtu_size_p<IROBEX_MIN_MTU) {
    DEBUG( 2, "Error invalid peer MTU : %d\n", *peer_mtu_size_p);
    return 0;
  }

  return 1;
}

static uint8_t obex_disconnect(IrLAP_Context *context_p, IrOBEX_Frame *resp_p,
                                uint8_t dlsap_sel)
{
  IrOBEX_Frame req;

  req.head.code=IROBEX_REQ_CODE_DISCONNECT | IROBEX_REQ_FINAL_MASK;
  req.head.len[0]=IROBEX_HEAD_SIZE>>8;
  req.head.len[1]=IROBEX_HEAD_SIZE;

  return irttp1_send_receive_i_frame(context_p,
               (IrTTP_Frame *)&req, sizeof(IrOBEX_Head),
               IROBEX_LSAP_SEL_VAL, dlsap_sel,
               (IrLAP_Frame *)resp_p)>=0;
}

static uint8_t obex_put(IrLAP_Context *context_p, IrOBEX_Frame *resp_p,
                    uint8_t dlsap_sel, uint16_t peer_mtu_size,
                    int16_t (*get_name_cb)(char *name, uint16_t size, void *user_data),
                    int16_t (*get_data_cb)(uint8_t *data, uint16_t size, void *user_data),
                    void *user_data)
{
  IrLAP_Frame frame;
  IrOBEX_Frame *req_p=(IrOBEX_Frame *)&frame;
  uint16_t n;
  int16_t i;
  uint8_t eof;

  n=0;

  if(get_name_cb) {
    if((i=(*get_name_cb)(req_p->u.info+3, (peer_mtu_size-5)>>1, user_data))<0) {
      DEBUG( 2, "Error get_data_cb return value 1\n");
      return 0;
    }
    i=expand_unicode(req_p->u.info+3, i)+3;

    req_p->u.info[0]=IROBEX_TYPE_UNICODE_TEXT | IROBEX_ID_NAME;
    req_p->u.info[1]=i>>8;
    req_p->u.info[2]=i;
    n+=i;
    
    /* TODO add file size */
  }

  eof=!get_data_cb;
  while(!eof || n>0) {
    if(!eof && (i=peer_mtu_size-n-3)>0) {
      if((i=(*get_data_cb)(req_p->u.info+n+3, i, user_data))<0) {
        DEBUG( 2, "Error get_data_cb return value 2\n");
        return 0;
      }

      if(i==0)
        eof=1;

      i+=3;
      if(!eof)
        req_p->u.info[n]=IROBEX_TYPE_BYTE_SEQUENCE | IROBEX_ID_BODY;
      else
        req_p->u.info[n]=IROBEX_TYPE_BYTE_SEQUENCE | IROBEX_ID_END_OF_BODY;
      req_p->u.info[n+1]=i>>8;
      req_p->u.info[n+2]=i;
      n+=i;
    }

    if(!eof)
      req_p->head.code=IROBEX_REQ_CODE_PUT;
    else
      req_p->head.code=IROBEX_REQ_CODE_PUT | IROBEX_REQ_FINAL_MASK;
    req_p->head.len[0]=(IROBEX_HEAD_SIZE+n)>>8;
    req_p->head.len[1]=IROBEX_HEAD_SIZE+n;

    if(irttp1_send_receive_i_frame(context_p,
             (IrTTP_Frame *)req_p, sizeof(IrOBEX_Head)+n,
             IROBEX_LSAP_SEL_VAL, dlsap_sel,
            (IrLAP_Frame *)resp_p)<sizeof(IrOBEX_Head)) {
      DEBUG( 2, "Error irttp1_send_receive_i_frame return value\n");
      return 0;
    }

    if(resp_p->head.code!=(IROBEX_ACK_CODE(req_p->head.code) | IROBEX_RESP_FINAL_MASK)) {
      DEBUG( 2, "Error head code : req %02X resp %02X\n", req_p->head.code, resp_p->head.code);
      /*WAS return 0;*/
    }

    n=0;
  }

  return 1;
}

uint8_t irobex_send(IrLAP_Context *context_p,
                    IrLAP_Device_Addr addr_p,
                    int16_t (*get_name_cb)(char *name, uint16_t size, void *user_data),
                    int16_t (*get_data_cb)(uint8_t *data, uint16_t size, void *user_data),
                    void *user_data)
{
  uint8_t success;
  IrLAP_Frame frame;
  uint8_t dlsap_sel;
  uint16_t peer_mtu_size;

  if(!irttp1_connect(context_p, &frame, addr_p,
                     IRLMP_HINT1_OBEX,
                     "OBEX", "IrDA:TinyTP:LsapSel",
                     IROBEX_LSAP_SEL_VAL, &dlsap_sel)) {
    DEBUG( 2, "Error irttp1_connect return value\n");
    return 0;
  }

  if(!obex_connect(context_p, (IrOBEX_Frame *)&frame,
                   dlsap_sel, &peer_mtu_size)) {
    DEBUG( 2, "Error obex_connect return value\n");
    return 0;
  }

  success=obex_put(context_p, (IrOBEX_Frame *)&frame,
                   dlsap_sel, peer_mtu_size,
                   get_name_cb, get_data_cb, user_data);

  if(!obex_disconnect(context_p,  (IrOBEX_Frame *)&frame,
                      dlsap_sel)) {
    DEBUG( 2, "Error obex_disconnect return value\n");
    success=0;
  }

  irlap1_disconnect(context_p,  &frame);

  return success;
}
