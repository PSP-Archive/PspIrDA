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

#ifndef _IRCOMM_H
#define _IRCOMM_H

#include <inttypes.h>

#include "irttp.h"

#define IRCOMM_LSAP_SEL_VAL		2

#define IRCOMM_CTRL_SERVICE_TYPE	0x00

extern IrIAS_Node ircomm_ias_node;

typedef struct IrCOMM_Control {
  uint8_t id;
  uint8_t len;
  uint8_t value[0];
} IrCOMM_Control;

typedef struct IrCOMM_Head {
  IrTTP_Head irttp;
  uint8_t clen;
} IrCOMM_Head;

typedef struct IrCOMM_Frame {
  IrCOMM_Head head;
  uint8_t info[0];
} IrCOMM_Frame;

void ircomm2_serve(IrLAP_Context *context_p,
		   int16_t (*put_data_cb)(uint8_t *data, uint16_t size,
					  uint8_t *resp_buf, uint16_t resp_buf_size,
					  uint8_t *terminate_p,
					  void *user_data),
		   void *user_data);

#endif /* _IRCOMM_H */
