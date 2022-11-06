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

#include <stdio.h>

#ifdef IRDA_DEBUG
int irda_debug;
extern FILE* debug_fd;
#endif
#ifdef IRDA_DUMP
int irda_dump;
extern FILE* dump_fd;
#endif

void dump( unsigned char data, int send_or_receive);


#ifdef IRDA_DEBUG
#  define DEBUG(n, format, args...) if (irda_debug >= (n)) fprintf( debug_fd,"%s(): " format, __FUNCTION__ , ##args)
#else
#  define DEBUG(n, format, args...)
#endif

#ifdef IRDA_DUMP
#  define DUMP(data, send_or_receive) if (irda_dump == 1) dump( data, send_or_receive)
#else
#  define DUMP(data, send_or_receive)
#endif

#define MAXPATH 1024
#define MAXNAME 256
#define MAX_ENTRY 256

