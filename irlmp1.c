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

/* 19-02-07 MA Added initialization of info array of lmp connect frame to 0 */
/* 19-02-07 MA Added LMP disconnect procedure (not used) */
/* 19-02-07 MA changed LMP connect frame size + 1 */
/* 19-02-07 MA Added LMP devicename negotiation */

#include <string.h>
#include "pspirda.h"
#include "irlmp.h"

static inline uint8_t iap_append_name(uint8_t *dest_p, char *name)
{
  uint8_t len;

  len=strlen(name);

  dest_p[0]=len;
  memcpy(dest_p+1, name, len);

  return len+1;
}

static uint8_t iap_getvaluebyclass_int_result(uint16_t *dest_p, uint8_t *info_p, uint16_t n)
{
  uint16_t i;

  i=0;

  if(i>=n ||
     info_p[i++]!=IRIAP_RESP_GVBCL_SUCCESS)
    return 0;

  /* len */
  if(i+1>=n ||
     (info_p[i]==0 && info_p[i+1]==0))
    return 0;

  /* seek beyond len & obj_id */
  i+=4;

  if(i+4>=n ||
     info_p[i++]!=IRIAP_VALUE_TYPE_INTEGER)
    return 0;

  /* assert MSBs are zero */
  if(info_p[i] || info_p[i+1])
    return 0;

  *dest_p=
    (uint16_t)info_p[i+2]<<8 |
    (uint16_t)info_p[i+3];

  return 1;
}

static uint8_t lmp_connect(IrLAP_Context *context_p, IrLMP_Frame *resp_p,
			   char *ias_class_name, char *ias_lsap_name,
			   uint8_t slsap_sel, uint8_t *dlsap_sel_p)
{
  IrLAP_Small_Frame frame;
  IrLMP_Frame *req_p=(IrLMP_Frame *)&frame;
  int16_t n;
  uint16_t dlsap_sel;

  /* LMP connect  */
  req_p->head.dlsap_sel=IRLMP_DLSAP_C_MASK;
  req_p->head.slsap_sel=slsap_sel;
  req_p->u.ctl.opcode=IRLMP_OP_CONNECT;
  req_p->u.ctl.info[0]=0;

  if((n=irlap1_send_receive_i_frame(context_p,
				    (IrLAP_Frame *)req_p, sizeof(IrLMP_Head)+sizeof(IrLMP_Ctl)+1,
				    (IrLAP_Frame *)resp_p))<sizeof(IrLMP_Head)+sizeof(IrLMP_Ctl)+1) {
    DEBUG( 4,  "Error irlap1_send_receive_i_frame LMP connect  return code\n");
    return 0;
  }

  if(resp_p->u.ctl.opcode!=(IRLMP_OP_CONNECT | IRLMP_OP_A_MASK)) {
    DEBUG( 4, "Error lmp connect response opcode : %d, trying to continue\n", resp_p->u.ctl.opcode );
    /*WAS return 0;*/
  }

  /* LMP  devicename  */
  req_p->head.dlsap_sel=0;
  req_p->head.slsap_sel=slsap_sel;
  req_p->u.ctl.opcode=IRLMP_OP_LM_GETVALUEBYCLASS | IRIAP_CTL_LST_MASK;

  n=iap_append_name(req_p->u.ctl.info, "Device");
  n+=iap_append_name(req_p->u.ctl.info+n, "DeviceName");

  if((n=irlap1_send_receive_i_frame(context_p,
				    (IrLAP_Frame *)req_p, sizeof(IrLMP_Head)+sizeof(IrLMP_Ctl)+n,
				    (IrLAP_Frame *)resp_p))<sizeof(IrLMP_Head)+sizeof(IrLMP_Ctl)+1) { /*WAS +0 */
    DEBUG( 4, "Error irlap1_send_receive_i_frame LMP devicename return code\n");
    return 0;
  }

  if(resp_p->u.ctl.opcode!=(IRLMP_OP_LM_GETVALUEBYCLASS | IRIAP_CTL_LST_MASK)) {
    DEBUG( 4, "Error lmp device name response opcode : %d\n", resp_p->u.ctl.opcode );
  }  /* LMP devicename not supported by peer but should!!!  */

  /* LMP IAS values */
  req_p->head.dlsap_sel=0;
  req_p->head.slsap_sel=slsap_sel;
  req_p->u.ctl.opcode=IRLMP_OP_LM_GETVALUEBYCLASS | IRIAP_CTL_LST_MASK;

  n=iap_append_name(req_p->u.ctl.info, ias_class_name);
  n+=iap_append_name(req_p->u.ctl.info+n, ias_lsap_name);

  if((n=irlap1_send_receive_i_frame(context_p,
				    (IrLAP_Frame *)req_p, sizeof(IrLMP_Head)+sizeof(IrLMP_Ctl)+n,
				    (IrLAP_Frame *)resp_p))<sizeof(IrLMP_Head)+sizeof(IrLMP_Ctl)+1) {
    DEBUG( 4, "Error irlap1_send_receive_i_frame IAS values return code\n");
    return 0;
  }

  if(resp_p->u.ctl.opcode!=(IRLMP_OP_LM_GETVALUEBYCLASS | IRIAP_CTL_LST_MASK)) {
    DEBUG( 4, "Error lmp values response opcode : %d\n", resp_p->u.ctl.opcode );
    return 0;
  }

  if(!iap_getvaluebyclass_int_result(&dlsap_sel, resp_p->u.ctl.info, n-sizeof(IrLMP_Head)-sizeof(IrLMP_Ctl))) {
    DEBUG( 4, "Error iap_getvaluebyclass_int_result LMP values return code\n");
    return 0;
  }
  *dlsap_sel_p=dlsap_sel;

  /* LMP disconnect */ /* Useless */
  /*req_p->head.dlsap_sel=IRLMP_DLSAP_C_MASK;
  req_p->head.slsap_sel=slsap_sel;
  req_p->u.ctl.opcode=IRLMP_OP_DISCONNECT;
  req_p->u.ctl.info[0]=1;

  if((n=irlap1_send_receive_i_frame(context_p,
				    (IrLAP_Frame *)req_p, sizeof(IrLMP_Head)+sizeof(IrLMP_Ctl)+1,
				    (IrLAP_Frame *)resp_p))<sizeof(IrLMP_Head)+sizeof(IrLMP_Ctl)+1) {
    DEBUG( 4, "Error irlap1_send_receive_i_frame LMP disconnect return code\n");
    return 0;
  }

  if(resp_p->u.ctl.opcode!=(IRLMP_OP_DISCONNECT | IRLMP_OP_A_MASK)) {
    DEBUG( 4, "Error lmp disconnect response opcode : %d, trying to continue\n", resp_p->u.ctl.opcode );
  } */ /* Disconnect not supported by peer, trying to continue  */

  return 1;
}

uint8_t irlmp1_connect(IrLAP_Context *context_p, IrLAP_Frame *resp_p,
		       IrLAP_Device_Addr addr_p,
		       uint8_t service_hint1,
		       char *ias_class_name, char *ias_lsap_name,
		       uint8_t slsap_sel, uint8_t *dlsap_sel_p)
{
  if(!irlap1_connect(context_p, resp_p, addr_p,
		     irlmp_get_dev_info, service_hint1)) {
    DEBUG( 4, "Error irlap1_connect return code\n");
    return 0;
  }

  return lmp_connect(context_p, (IrLMP_Frame *)resp_p,
		     ias_class_name, ias_lsap_name,
		     slsap_sel, dlsap_sel_p);
}

int16_t irlmp1_send_receive_i_frame(IrLAP_Context *context_p,
				    IrLMP_Frame *req_p, uint16_t req_size,
				    uint8_t slsap_sel, uint8_t dlsap_sel,
				    IrLAP_Frame *resp_p)
{
  req_p->head.dlsap_sel=dlsap_sel;
  req_p->head.slsap_sel=slsap_sel;

  return irlap1_send_receive_i_frame(context_p,
				     (IrLAP_Frame *)req_p, req_size,
				     resp_p);
}
