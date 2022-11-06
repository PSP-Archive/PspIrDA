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

/* 19-02-07 MA added IrOBEX IAS node IrDA:OBEX */

#include <string.h>

#include "irobex.h"

IrIAS_Node irobex_ias_node={
  "OBEX",
  "IrDA:TinyTP:LsapSel",
  IRIAP_VALUE_TYPE_INTEGER,
  IROBEX_LSAP_SEL_VAL,
  IROBEX_LSAP_SEL_VAL,
  0
};

IrIAS_Node irda_irobex_ias_node={
  "IrDA:OBEX",
  "IrDA:TinyTP:LsapSel",
  IRIAP_VALUE_TYPE_INTEGER,
  IROBEX_LSAP_SEL_VAL,
  IROBEX_LSAP_SEL_VAL,
  0
};

static void condense_unicode(uint8_t *buf, uint16_t size)
{
  uint16_t i, j;

  i=0;
  j=1;
  while(j<size) {
    buf[i]=buf[j];
    i++;
    j+=2;
  }

  if(i>0)
    i--;

  buf[i]=0;
}

static int8_t handle_put(uint8_t *source, uint16_t source_size,
                      uint8_t (*put_name_cb)(char *name, void *user_data),
                      uint8_t (*put_data_cb)(uint8_t *data, uint16_t size, void *user_data),
                      void *user_data)
{
  uint8_t have_msg;
  uint16_t i, len;
  uint8_t code;

  have_msg=0;
  i=0;
  while(i<source_size) {
    code=source[i++];

    switch(code & IROBEX_TYPE_MASK) {
    case IROBEX_TYPE_UNICODE_TEXT:
    case IROBEX_TYPE_BYTE_SEQUENCE:
      if(i+2<=source_size) {
        len=source[i]<<8 | source[i+1];
        if(len>3)
          len-=3;
        else
          len=0;
      } else
        len=0;
      i+=2;
      break;
    case IROBEX_TYPE_ONE_BYTE:
      len=1;
      break;
    case IROBEX_TYPE_FOUR_BYTES:
      len=4;
      break;
    default:
      len=0;
    }

    switch(code & IROBEX_TYPE_MASK) {
    case IROBEX_TYPE_UNICODE_TEXT:
      if(i+len<=source_size) {
        switch(code & ~IROBEX_TYPE_MASK) {
        case IROBEX_ID_NAME:
          if(put_name_cb) {
            condense_unicode(source+i, len);
            if(!(*put_name_cb)(source+i, user_data))
              return -1;
          }
          break;
        case IROBEX_ID_TYPE:
        
          break;
        case IROBEX_ID_LENGTH:
        
          break;
        case IROBEX_ID_TIME:
        
          break;
        case IROBEX_ID_DESCRIPTION:
        
          break;
        }
      }
      break;
    case IROBEX_TYPE_BYTE_SEQUENCE:
      if(i+len<=source_size) {
      switch(code & ~IROBEX_TYPE_MASK) {
        case IROBEX_ID_END_OF_BODY:
          have_msg=1;
          /* flow into next case */
        case IROBEX_ID_BODY:
          if(put_data_cb && !(*put_data_cb)(source+i, len, user_data))
            return -1;
          break;
        }
      }
      break;
    }
    i+=len;
  }

  return have_msg;
}

uint8_t irobex_receive(IrLAP_Context *context_p,
             uint8_t (*put_name_cb)(char *name, void *user_data),
             uint8_t (*put_data_cb)(uint8_t *data, uint16_t size, void *user_data),
             void *user_data)
{
  IrIAS_Node *ias_nodes[3];
  int8_t success;
  int16_t n;
  uint16_t last_resp_size;
  IrLAP_Frame frame, *last_resp_p;
  IrOBEX_Frame *req_p, resp;

  
  ias_nodes[0]=&irobex_ias_node;
  ias_nodes[1]=&irda_irobex_ias_node;
  ias_nodes[2]=0;

  req_p=(IrOBEX_Frame *)&frame;

  success=0;
  last_resp_p=0;
  last_resp_size=0;

  for(;;) {
    if((n=irttp2_receive_frame(context_p, &frame,
                               last_resp_p, last_resp_size,
                               IRLMP_HINT1_OBEX,
                               ias_nodes))<0)
      return success>0;

    if(n<sizeof(IrOBEX_Head) ||
       req_p->head.irttp.irlmp.dlsap_sel!=IROBEX_LSAP_SEL_VAL)
      continue;

    switch(req_p->head.code & ~IROBEX_REQ_FINAL_MASK) {
    case IROBEX_REQ_CODE_CONNECT:
      resp.head.code=IROBEX_ACK_CODE(req_p->head.code) | IROBEX_RESP_FINAL_MASK;
      resp.head.len[0]=(IROBEX_HEAD_SIZE+sizeof(IrOBEX_Connect))>>8;
      resp.head.len[1]=IROBEX_HEAD_SIZE+sizeof(IrOBEX_Connect);
      resp.u.connect.version=IROBEX_VERSION;
      resp.u.connect.flags=0;
  
      resp.u.connect.mtu[0]=IROBEX_MTU>>8;
      resp.u.connect.mtu[1]=IROBEX_MTU;
      last_resp_p=(IrLAP_Frame *)&resp;
      last_resp_size=sizeof(IrOBEX_Head)+sizeof(IrOBEX_Connect);
      irttp2_reply(context_p, (IrTTP_Frame *)req_p, (IrTTP_Frame *)last_resp_p, last_resp_size);
      break;
    case IROBEX_REQ_CODE_DISCONNECT:
      resp.head.code=IROBEX_ACK_CODE(req_p->head.code) | IROBEX_RESP_FINAL_MASK;
      resp.head.len[0]=IROBEX_HEAD_SIZE>>8;
      resp.head.len[1]=IROBEX_HEAD_SIZE;
      last_resp_p=(IrLAP_Frame *)&resp;
      last_resp_size=sizeof(IrOBEX_Head);
      irttp2_reply(context_p, (IrTTP_Frame *)req_p, (IrTTP_Frame *)last_resp_p, last_resp_size);

      if(success)
        return success>0;
      break;
    case IROBEX_REQ_CODE_PUT:
      if(!success)
        success=handle_put(req_p->u.info, n-sizeof(IrOBEX_Head),
                            put_name_cb, put_data_cb, user_data);
      resp.head.code=IROBEX_ACK_CODE(req_p->head.code) | IROBEX_RESP_FINAL_MASK;
      resp.head.len[0]=IROBEX_HEAD_SIZE>>8;
      resp.head.len[1]=IROBEX_HEAD_SIZE;
      last_resp_p=(IrLAP_Frame *)&resp;
      last_resp_size=sizeof(IrOBEX_Head);
      irttp2_reply(context_p, (IrTTP_Frame *)req_p, (IrTTP_Frame *)last_resp_p, last_resp_size);
      break;
    default:
      resp.head.code=IROBEX_RESP_CODE_NOT_IMPL | IROBEX_RESP_FINAL_MASK;
      resp.head.len[0]=IROBEX_HEAD_SIZE>>8;
      resp.head.len[1]=IROBEX_HEAD_SIZE;
      last_resp_p=(IrLAP_Frame *)&resp;
      last_resp_size=sizeof(IrOBEX_Head);
      irttp2_reply(context_p, (IrTTP_Frame *)req_p, (IrTTP_Frame *)last_resp_p, last_resp_size);
    }
  }
}
