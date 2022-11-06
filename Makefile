IRDA_SOURCES=\
	irlap.c irlap1.c irlap2.c\
	irlmp.c irlmp1.c irlmp2.c\
	irttp1.c irttp2.c irttp2_accept.c\
	irobex1.c irobex2.c\
	ircomm2.c

IRDA_HEADERS=\
	irphy.h irlap.h irlmp.h irttp.h\
	irobex.h ircomm.h main.h

TARGET = pspirda
OBJS=\
  $(IRDA_SOURCES:.c=.o)\
	irphy.o main.o

INCDIR = 
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
STDLIBS= -losl -lpng -lz -lpspsdk -lpspctrl -lpsppower -lpsprtc  -lpspgu -lm
LIBS = $(STDLIBS)$(YOURLIBS)

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PspIrDA 0.0.2
PSP_EBOOT_ICON = icon0.png
PSP_EBOOT_PIC1 = pic1.png

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
