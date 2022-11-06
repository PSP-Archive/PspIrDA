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

/* 08-12-06 MA modified reply and irlmp2_handle_ctl_frame call to handle optionnal info */
/* 19-02-07 MA added Irlmp support and device name definitions */
/* 19-02-07 MA improved IrIAP parameters retrieval */

#include <string.h>
#include "pspirda.h"
#include "irlmp.h"

static uint8_t device_name[]={
  'P', 'S', 'P'
};

static uint8_t IrLMP_support[]={
  0x01, 0x00, 0x00
}; /* IrLMP version, IAS support, LM-MUX suport */


IrIAS_Node devicename_ias_node={
  "Device",
  "DeviceName",
  IRIAP_VALUE_TYPE_USER_STRING,
  0,
  0,
  device_name
};

IrIAS_Node IrLMP_support_ias_node={
  "Device",
  "IrLMPSupport",
  IRIAP_VALUE_TYPE_OCTET_SEQUENCE,
  0,
  0,
  IrLMP_support
};

static uint16_t iap_name_equal(char *name, uint8_t *encoded, uint16_t size)
{
  uint16_t i, j;

  if(size<1)
    return 0;

  i=0;
  while(i<encoded[0] && name[i]) {
    j=i+1;
    if(j>=size || name[i]!=encoded[j])
      break;
    i=j;
  }

  if(i<encoded[0] || name[i])
    return 0;

  return i+1;
}

static void iap_reply_getvaluebyclass(IrLAP_Context *context_p, IrLMP_Frame *req_p,
				      uint8_t status, IrIAS_Node *node_p)
{
  IrLAP_Small_Frame frame;
  IrLMP_Frame *resp_p=(IrLMP_Frame *)&frame;
  uint16_t n;

  resp_p->head.dlsap_sel=req_p->head.slsap_sel & ~IRLMP_DLSAP_C_MASK;
  resp_p->head.slsap_sel=req_p->head.dlsap_sel & ~IRLMP_DLSAP_C_MASK;
  resp_p->u.ctl.opcode=req_p->u.ctl.opcode | IRIAP_CTL_LST_MASK;

  n=0;

  /* status */
  resp_p->u.ctl.info[n++]=status;

  if(status==IRIAP_RESP_GVBCL_SUCCESS) {
     /* len */
    resp_p->u.ctl.info[n++]=0;
    resp_p->u.ctl.info[n++]=1;
    
    /* obj_id */
    resp_p->u.ctl.info[n++]=(node_p->obj_id)>>8;
    resp_p->u.ctl.info[n++]=(node_p->obj_id);
    
    /* value */
    resp_p->u.ctl.info[n++]=(node_p->value_type);
    
    switch(node_p->value_type) {
      
      case IRIAP_VALUE_TYPE_INTEGER:
      resp_p->u.ctl.info[n++]=0;
      resp_p->u.ctl.info[n++]=0;
      resp_p->u.ctl.info[n++]=(node_p->integer)>>8;
      resp_p->u.ctl.info[n++]=(node_p->integer);
      break;
      
      case IRIAP_VALUE_TYPE_USER_STRING: /* TODO check implementation */
      resp_p->u.ctl.info[n++]=IRLMP_CHARSET_ASCII;
      resp_p->u.ctl.info[n++]=sizeof(node_p->name);
      memcpy(resp_p->u.ctl.info + n, node_p->name, sizeof(node_p->name));
      n+=sizeof(node_p->name);
      break;
      
      case IRIAP_VALUE_TYPE_OCTET_SEQUENCE: /* TODO check implementation */
      resp_p->u.ctl.info[n++]=sizeof(node_p->name)>>8;
      resp_p->u.ctl.info[n++]=sizeof(node_p->name);
      memcpy(resp_p->u.ctl.info + n, node_p->name, sizeof(node_p->name));
      n+=sizeof(node_p->name);
      break;
    }
  }
    
  irlap2_send_i_frame(context_p, (IrLAP_Frame *)&frame, sizeof(IrLMP_Head)+sizeof(IrLMP_Ctl)+n);
}

static void iap_handle_getvaluebyclass(IrLAP_Context *context_p, IrLMP_Frame *req_p, uint16_t size, IrIAS_Node **ias_nodes_p)
{
  IrIAS_Node **node_p;
  uint16_t n, i;

  n=size-sizeof(IrLMP_Head)-sizeof(IrLMP_Ctl);
  i=0;
  
  if((i=iap_name_equal(devicename_ias_node.class_name, req_p->u.ctl.info, n)) &&
     (i=iap_name_equal(devicename_ias_node.attr_name, req_p->u.ctl.info+i, n-i))) {
    iap_reply_getvaluebyclass(context_p, req_p, IRIAP_RESP_GVBCL_SUCCESS, &devicename_ias_node);
    return;
  }
    
  for(node_p=ias_nodes_p; *node_p; node_p++) {
    n=size-sizeof(IrLMP_Head)-sizeof(IrLMP_Ctl);
    i=0;
    if((i=iap_name_equal((*node_p)->class_name, req_p->u.ctl.info, n)) &&
       (i=iap_name_equal((*node_p)->attr_name, req_p->u.ctl.info+i, n-i))) {
      iap_reply_getvaluebyclass(context_p, req_p, IRIAP_RESP_GVBCL_SUCCESS, *node_p);
      return;
    }
  }

  iap_reply_getvaluebyclass(context_p, req_p, IRIAP_RESP_GVBCL_NOSUCHATTR, 0);
}

static void reply(IrLAP_Context *context_p, IrLMP_Frame *req_p, int16_t info_size, uint8_t unsupported)
{
  IrLAP_Small_Frame frame;
  IrLMP_Frame *resp_p=(IrLMP_Frame *)&frame;

  if(unsupported)
    resp_p->u.ctl.info[0]=IRLMP_RESP_UNSUPPORTED;
  else
    resp_p->u.ctl.info[0]=0;

  resp_p->u.ctl.info[1]=0;

  irlmp2_reply(context_p, req_p, resp_p, sizeof(IrLMP_Head)+sizeof(IrLMP_Ctl)+info_size);
}

void irlmp2_reply(IrLAP_Context *context_p, IrLMP_Frame *req_p, IrLMP_Frame *resp_p, uint16_t size)
{
  if(req_p->head.dlsap_sel & IRLMP_DLSAP_C_MASK) {
    resp_p->head.dlsap_sel=req_p->head.slsap_sel | IRLMP_DLSAP_C_MASK;
    resp_p->head.slsap_sel=req_p->head.dlsap_sel & ~IRLMP_DLSAP_C_MASK;
    resp_p->u.ctl.opcode=req_p->u.ctl.opcode | IRLMP_OP_A_MASK;
  } else {
    resp_p->head.dlsap_sel=req_p->head.slsap_sel & ~IRLMP_DLSAP_C_MASK;
    resp_p->head.slsap_sel=req_p->head.dlsap_sel & ~IRLMP_DLSAP_C_MASK;
    if(!(req_p->head.dlsap_sel & ~IRLMP_DLSAP_C_MASK))
      resp_p->u.ctl.opcode=req_p->u.ctl.opcode | IRIAP_CTL_LST_MASK;
  }

  irlap2_send_i_frame(context_p, (IrLAP_Frame *)resp_p, size);
}

void irlmp2_handle_ctl_frame(IrLAP_Context *context_p, IrLMP_Frame *req_p, int16_t info_size)
{
  switch(req_p->u.ctl.opcode) {
  case IRLMP_OP_CONNECT:
    reply(context_p, req_p, info_size, 0);
    break;
  case IRLMP_OP_DISCONNECT:
    reply(context_p, req_p, info_size, 0);
    break;
  case IRLMP_OP_ACCESS_MODE:
    reply(context_p, req_p, info_size, 1);
    break;
  }
}

int16_t irlmp2_receive_frame(IrLAP_Context *context_p, IrLAP_Frame *frame_p,
			     IrLAP_Frame *last_resp_p, uint16_t last_resp_size,
			     uint8_t service_hint1,
			     IrIAS_Node **ias_nodes_p)
{
  IrLMP_Frame *req_p;
  int16_t n;
  int16_t n_info;

  req_p=(IrLMP_Frame *)frame_p;

  if((n=irlap2_receive_i_frame(context_p, frame_p,
			       last_resp_p, last_resp_size,
			       irlmp_get_dev_info, service_hint1))<0) {
    DEBUG( 4,  "Error irlap2_receive_i_frame return code\n");
    return -1;
  }

  do {
    if(n<sizeof(IrLMP_Head)+
       ((req_p->head.dlsap_sel & IRLMP_DLSAP_C_MASK) ? sizeof(IrLMP_Ctl) : 0))
      continue;

    if(req_p->head.dlsap_sel & ~IRLMP_DLSAP_C_MASK)
      return n;

    if(req_p->head.dlsap_sel & IRLMP_DLSAP_C_MASK) {
      /* link control frame */
      n_info=n-sizeof(IrLMP_Head)-sizeof(IrLMP_Ctl);
      irlmp2_handle_ctl_frame(context_p, req_p, n_info);
    } else {
      /* data frame */
      switch(req_p->u.ctl.opcode & ~IRIAP_CTL_LST_MASK) {
      case IRLMP_OP_LM_GETVALUEBYCLASS:
	      iap_handle_getvaluebyclass(context_p, req_p, n, ias_nodes_p);
	      break;
      default:
	      reply(context_p, req_p, 1, 1);
	      break;
      }
    }
  } while((n=irlap2_receive_i_frame(context_p, frame_p,
				    0, 0,
				    irlmp_get_dev_info, service_hint1))>=0);

  DEBUG( 4,  "Error end of irlmp2_receive_frame reached\n");
  return -1;
}
