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

/* 19-02-07 modified irlap_append_neg_params call, added initial irlap parameters negotiations  */
/* 19-02-07 MA Changed irlap_send_frame call */
/* 19-02-07 MA increased IrLap retry attemps to 255 */
/* 19-02-07 MA receive ready send to secondary and not broadcast*/

#include <string.h>
#include "pspirda.h"
#include "irlap.h"

#define IRLAP_RETRY_ATTEMPTS		255

#define IRLAP_ADDR_SECONDARY		(1<<1)

static void send_test_reply(IrLAP_Context *context_p, IrLAP_Device_Addr addr_p)
{
  struct {
    IrLAP_Head head;
    IrLAP_Test test;
  } resp;

  resp.head.addr=IRLAP_ADDR_NULL;
  resp.head.ctrl=IRLAP_CTRL_U_TEST_RESP | IRLAP_CTRL_P_F_MASK;
  memcpy(resp.test.dest, addr_p, sizeof(IrLAP_Device_Addr));
  memcpy(resp.test.source, context_p->dev_addr, sizeof(IrLAP_Device_Addr));

  irlap_send_frame(context_p, (IrLAP_Frame *)&resp, sizeof(IrLAP_Head)+sizeof(IrLAP_Test));
}

static void send_rr(IrLAP_Context *context_p)
{
  IrLAP_Head req;

  req.addr=IRLAP_ADDR_SECONDARY | IRLAP_ADDR_C_TEST;
  req.ctrl=IRLAP_CTRL_S_RR | IRLAP_CTRL_P_F_MASK | context_p->vr<<IRLAP_CTRL_NR_BIT_POS;

  irlap_send_frame(context_p, (IrLAP_Frame *)&req, sizeof(IrLAP_Head));
}

static int16_t wait_response(IrLAP_Context *context_p, IrLAP_Frame *resp_p,
			     uint8_t ctrl_mask, uint8_t ctrl_value,
			     uint8_t rr_abort)
{
  int16_t n;

  do {
    if((n=irlap_receive_frame(resp_p))<0) {
      DEBUG( 5,  "Error irlap_receive_frame return code\n");
      return -1;
    }

    if(resp_p->head.addr & IRLAP_ADDR_C_TEST) {
      /* in primary mode only respond to TEST commands */
      switch(resp_p->head.ctrl & IRLAP_CTRL_U_MASK) {
      case IRLAP_CTRL_U_TEST_CMD:
        if(n>=sizeof(IrLAP_Head)+sizeof(IrLAP_Test) &&
          irlap_matching_dest_addr(context_p, resp_p->u.test.dest))
          send_test_reply(context_p, resp_p->u.test.source);
      }
      continue;
    }

    if(resp_p->head.ctrl & IRLAP_CTRL_I_TEST) {
      /* U or S format */
      if(!(resp_p->head.ctrl & IRLAP_CTRL_S_TEST)) {
	      /* S format */
        switch(resp_p->head.ctrl & IRLAP_CTRL_S_MASK) {
        case IRLAP_CTRL_S_RR:
          if((resp_p->head.addr & ~IRLAP_ADDR_C_TEST)==IRLAP_ADDR_SECONDARY) {
              context_p->vs=(resp_p->head.ctrl>>IRLAP_CTRL_NR_BIT_POS)&IRLAP_CTRL_N_MASK;

            if(rr_abort)
              return -1;

          break;
          }
        }
      }
    } else {
      /* I format */
      if((resp_p->head.addr & ~IRLAP_ADDR_C_TEST)==IRLAP_ADDR_SECONDARY &&
          context_p->vr==((resp_p->head.ctrl>>IRLAP_CTRL_NS_BIT_POS)&IRLAP_CTRL_N_MASK)) {
          context_p->vs=(resp_p->head.ctrl>>IRLAP_CTRL_NR_BIT_POS)&IRLAP_CTRL_N_MASK;
          context_p->vr=(context_p->vr+1)&IRLAP_CTRL_N_MASK;
      }
    }
  } while((resp_p->head.ctrl & ctrl_mask)!=ctrl_value);

  return n;
}

uint8_t irlap1_connect(IrLAP_Context *context_p, IrLAP_Frame *resp_p,
		       IrLAP_Device_Addr addr_p,
		       uint16_t (*get_dev_info_cb)(uint8_t *dev_info_p, uint8_t user_data),
		       uint8_t get_dev_info_user_data)
{
  IrLAP_Small_Frame req;
  uint8_t attempt;
  int16_t n;
  uint16_t param_len;

  req.head.addr=IRLAP_ADDR_BROADCAST | IRLAP_ADDR_C_TEST;
  req.head.ctrl=IRLAP_CTRL_U_XID_CMD | IRLAP_CTRL_P_F_MASK;

  req.u.discovery.format=IRLAP_XID_FMT_DISCOVERY;
  
  memcpy(req.u.discovery.source, context_p->dev_addr, sizeof(IrLAP_Device_Addr));
  if(addr_p)
    memcpy(req.u.discovery.dest, addr_p, sizeof(IrLAP_Device_Addr));
  else
    memset(req.u.discovery.dest, 0xFF, sizeof(IrLAP_Device_Addr));
  req.u.discovery.flags=IRLAP_XID_FLAGS_SLOTS_1;
  req.u.discovery.slot=0;
  req.u.discovery.version=IRLAP_VERSION;

  if(get_dev_info_cb)
    param_len=(*get_dev_info_cb)(req.u.discovery.info, get_dev_info_user_data);
  else
    param_len=0;

  for(attempt=0; attempt<IRLAP_RETRY_ATTEMPTS; attempt++) {
    irlap_send_frame(context_p, (IrLAP_Frame *)&req, sizeof(IrLAP_Head)+sizeof(IrLAP_Discovery)+param_len);

    if((n=wait_response(context_p, resp_p, IRLAP_CTRL_U_MASK, IRLAP_CTRL_U_XID_RESP, 0))>=0)
      break;
  }
  if(attempt>=IRLAP_RETRY_ATTEMPTS) {
    DEBUG( 5,  "Error to much retry attemps 1\n");
    return 0;
  }

  req.head.addr=IRLAP_ADDR_BROADCAST | IRLAP_ADDR_C_TEST;
  req.head.ctrl=IRLAP_CTRL_U_SNRM_CMD | IRLAP_CTRL_P_F_MASK;

  req.u.snrm.conn_addr=IRLAP_ADDR_SECONDARY;
  memcpy(req.u.snrm.source, context_p->dev_addr, sizeof(IrLAP_Device_Addr));
  memcpy(req.u.snrm.dest, resp_p->u.discovery.source, sizeof(IrLAP_Device_Addr));

  param_len=irlap_append_neg_params(context_p, req.u.snrm.info);

  for(attempt=0; attempt<IRLAP_RETRY_ATTEMPTS; attempt++) {
    irlap_send_frame(context_p, (IrLAP_Frame *)&req, sizeof(IrLAP_Head)+sizeof(IrLAP_SNRM)+param_len);

    if((n=wait_response(context_p, resp_p, IRLAP_CTRL_U_MASK, IRLAP_CTRL_U_UA_RESP, 0))>=0)
      break;
  }
  if(attempt>=IRLAP_RETRY_ATTEMPTS) {
    DEBUG( 5,  "Error to much retry attemps 2\n");
    return 0;
  }

  context_p->vr=0;
  context_p->vs=0;
  get_neg_params(context_p, resp_p->u.ua.info);

  return 1;
}

uint8_t irlap1_disconnect(IrLAP_Context *context_p, IrLAP_Frame *resp_p)
{
  IrLAP_Small_Frame req;
  uint8_t attempt;

  req.head.addr=IRLAP_ADDR_SECONDARY | IRLAP_ADDR_C_TEST;
  req.head.ctrl=IRLAP_CTRL_U_DISC_CMD | IRLAP_CTRL_P_F_MASK;

  for(attempt=0; attempt<IRLAP_RETRY_ATTEMPTS; attempt++) {
    irlap_send_frame(context_p, (IrLAP_Frame *)&req, sizeof(IrLAP_Head));
    if(wait_response(context_p, resp_p, IRLAP_CTRL_U_MASK, IRLAP_CTRL_U_UA_RESP, 0)>=0)
      break;
  }
  if(attempt>=IRLAP_RETRY_ATTEMPTS) {
    DEBUG( 5,  "Error to much retry attemps\n");
    return 0;
  }

  return 1;
}

int16_t irlap1_send_receive_i_frame(IrLAP_Context *context_p,
				    IrLAP_Frame *req_p, uint16_t req_size,
				    IrLAP_Frame *resp_p)
{
  uint8_t send_req, new_vs, attempt;
  int16_t n;

  send_req=1;
  new_vs=context_p->vs;

  for(attempt=0; attempt<IRLAP_RETRY_ATTEMPTS; attempt++) {

    if(attempt>0)
      send_rr(context_p);

    if(send_req) {
      req_p->head.addr=IRLAP_ADDR_SECONDARY | IRLAP_ADDR_C_TEST;
      req_p->head.ctrl=IRLAP_CTRL_P_F_MASK |
                       context_p->vr<<IRLAP_CTRL_NR_BIT_POS |
                       context_p->vs<<IRLAP_CTRL_NS_BIT_POS;

      irlap_send_frame(context_p, req_p, req_size);

      new_vs=(context_p->vs+1)&IRLAP_CTRL_N_MASK;
    }

    if((n=wait_response(context_p, resp_p, IRLAP_CTRL_I_TEST, 0, 1))>=0)
      return n;

    if(context_p->vs==new_vs)
      send_req=0;
  }

  DEBUG( 5,  "Error end of irlap1_send_receive_i_frame reached\n");

  return -1;
}
