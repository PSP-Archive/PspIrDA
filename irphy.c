/*
  Copyright (C) 2006 Mathieu Albinet, mathieu17@gmail.com
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

/* 19-02-07 MA code cleanups */

#include <sys/time.h>

#include <pspsdk.h>
#include <psprtc.h>
#include <pspiofilemgr.h>


#include "pspirda.h"

static unsigned char DataWaiting;
static int isDataWaiting;

static int irda_fd=-1;

static unsigned long TicksPerMicroSec;

void irphy_reset(void)
{
  DataWaiting = 0;
  isDataWaiting = 0;

  TicksPerMicroSec = (u32) (sceRtcGetTickResolution() * 0.000001);

  irda_fd = sceIoOpen("irda0:", PSP_O_RDWR | PSP_O_NBLOCK | PSP_O_NOWAIT, 0);
  if (irda_fd<0){
    DEBUG( 6, "Error opening irda\n");
  }
}

void irphy_send(uint8_t v)
{
  int n;

  if((n=sceIoWrite(irda_fd, &v, 1))<0) {
    DEBUG( 6, "Error write\n");
  }

  DUMP( (unsigned char) v, 1);
}

uint8_t irphy_wait(int16_t timeout)
{
  unsigned char v;
  
  unsigned long long CurrentTick;
  unsigned long long FinalTick;
  
  int n;
  
  if(sceRtcGetCurrentTick(&CurrentTick) != 0) {
    DEBUG( 6, "Error get current tick");
    return 0;
  }
  
  if ( timeout >= 0 ) {
    FinalTick = CurrentTick + (timeout*TicksPerMicroSec*1000);
  }
  else {
    FinalTick = 0xffffffff;
  }
  
  do {
    if((n=sceIoRead( irda_fd, &v, 1))<0) {
      DEBUG( 6, "Read error\n");
    }
    
    if( n == 1 ) {
      isDataWaiting = 1;
      DataWaiting = v;
      return 1;
    }
    
    if(sceRtcGetCurrentTick(&CurrentTick) != 0) {
      DEBUG( 6, "Error get current tick");
      return 0;
    }
  } while( CurrentTick < FinalTick );
  
  DataWaiting = 0;
  isDataWaiting = 0;

  return 0;
}

uint8_t irphy_receive(void)
{
  uint8_t v;
  int n;

  if( isDataWaiting == 1 ) {
    v = DataWaiting;
  }
  else {
    if((n=sceIoRead( irda_fd, &v, 1))<0) {
      DEBUG( 6, "Read error\n");
    }
  }

  DUMP( (unsigned char) v, 0);

  return v;
}
