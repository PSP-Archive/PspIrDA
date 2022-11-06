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

/* 19-02-07 MA Do not transmit empty obex packet */

#include "irttp.h"
#include "pspirda.h"

static void reply_connect(IrLAP_Context *context_p, IrLMP_Frame *req_p)
{
  IrTTP_Connect_Frame resp;

  resp.rsvd=0;
  resp.credit=IRTTP_CREDITS;

  irlmp2_reply(context_p, req_p, (IrLMP_Frame *)&resp, sizeof(IrTTP_Connect_Frame));
}

void irttp2_reply(IrLAP_Context *context_p, IrTTP_Frame *req_p, IrTTP_Frame *resp_p, uint16_t size)
{
  resp_p->head.credit=IRTTP_CREDITS;
  irlmp2_reply(context_p, (IrLMP_Frame *)req_p, (IrLMP_Frame *)resp_p, size);
}

void irttp2_handle_ctl_frame(IrLAP_Context *context_p, IrLMP_Frame *req_p)
{
  switch(req_p->u.ctl.opcode) {
  case IRLMP_OP_CONNECT:
    reply_connect(context_p, req_p);
    break;
  default:
    irlmp2_handle_ctl_frame(context_p, req_p, 1); /* TODO replace 1 by correct value */
    break;
  }
}

int16_t irttp2_receive_frame(IrLAP_Context *context_p, IrLAP_Frame *frame_p,
               IrLAP_Frame *last_resp_p, uint16_t last_resp_size,
               uint8_t irlmp_service_hint1,
               IrIAS_Node **ias_nodes_p)
{
  IrLMP_Frame *req_p;
  int16_t n;

  req_p=(IrLMP_Frame *)frame_p;

  if((n=irlmp2_receive_frame(context_p, frame_p,
               last_resp_p, last_resp_size,
               irlmp_service_hint1,
               ias_nodes_p))<0) {
    DEBUG( 3,  "Error irlmp2_receive_frame return code\n");
    return -1;
  }

  do {
    /* empty obex packet */
    if(n==sizeof(IrTTP_Head)) {
      irttp2_reply(context_p, (IrTTP_Frame *)req_p, (IrTTP_Frame *)last_resp_p, sizeof(IrTTP_Head));
      continue;
    }
    
    if(req_p->head.dlsap_sel & IRLMP_DLSAP_C_MASK) {
      /* link control frame */
      irttp2_handle_ctl_frame(context_p, req_p);
    } else {
      /* data frame */
      return n;
    }
  } while((n=irlmp2_receive_frame(context_p, frame_p,
                    0, 0,
                    irlmp_service_hint1,
                    ias_nodes_p))>=0);

  DEBUG( 3,  "Error end of irttp2_receive_frame reached\n");
  return -1;
}
