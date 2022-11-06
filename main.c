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

/* 19-02-07 MA minor code cleanups and GUI improvements */
/* 19-02-07 MA improved exit callback */

#include <oslib/oslib.h>

#include "main.h"
#include "pspirda.h"
#include "irphy.h"
#include "irobex.h"
#include "ircomm.h"



/* Define the module info section */
PSP_MODULE_INFO("PspIrDA 0.0.2", 0, 1, 1);

/* Define the main thread's attribute value */
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

/* files descriptors */
static SceUID receive_fd = -1;
static SceUID send_fd = -1;
/* files paths */
char filetosend[MAXPATH];
char receptiondir[MAXPATH];


#if IRDA_DEBUG
FILE* debug_fd;
#endif
#if IRDA_DUMP
FILE* dump_fd;
int send;
#endif

int done;

/* Exit callback */
int exit_callback(int arg1, int arg2, void *common)
{
  end_main();
  return 0;
}

/* Callback thread */
int CallbackThread(SceSize args, void *argp)
{
  int cbid;

  cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
  sceKernelRegisterExitCallback(cbid);

  sceKernelSleepThreadCB();

  return 0;
}

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void)
{
  int thid = 0;

  thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
  if(thid >= 0)
  {
    sceKernelStartThread(thid, 0, 0);
  }

  return thid;
}

void print_header()
{
  oslPrintf(" ------------------------------------------------------------------\n");
  oslPrintf(" ------------ PspIrDA 0.0.2 -- IrDA Obex file transfer ------------\n");
  oslPrintf(" ------------------------------------------------------------------\n");
}

void print_main_menu()
{
  oslCls();

  print_header();

  oslPrintf("\n");
  oslPrintf("-----Main menu-----\n");
  oslPrintf("\n");
  oslPrintf("SQUARE   : receive a file\n");
  oslPrintf("CROSS    : send a file\n");
  oslPrintf("TRIANGLE : exit program\n");
}

void print_send_menu()
{
  oslCls();

  print_header();

  oslPrintf("\n");
  oslPrintf("-----Send menu-----\n");
  oslPrintf("\n");
  oslPrintf("RIGHT    : next file or directory\n");
  oslPrintf("LEFT     : previous file or directory\n");
  oslPrintf("CROSS    : select file or enter directory\n");
  oslPrintf("TRIANGLE : abort\n");
}

void print_receive_menu()
{
  oslCls();

  print_header();

  oslPrintf("\n");
  oslPrintf("---Receive menu---\n");
  oslPrintf("\n");
  oslPrintf("RIGHT    : next directory\n");
  oslPrintf("LEFT     : previous directory\n");
  oslPrintf("CIRCLE   : enter directory\n");
  oslPrintf("CROSS    : select directory\n");
  oslPrintf("TRIANGLE : abort\n");
}

#if IRDA_DUMP
void dump( unsigned char data, int send_or_receive)
{
  if(send_or_receive != send) {
    send = send_or_receive;
    if( send == 1) {
      fprintf( dump_fd, "\n-->\n");
    }
    else {
      fprintf( dump_fd, "\n<--\n");
    }
  }
  
  fprintf( dump_fd, "%02X ", data);
}
#endif

void cd_parent_dir( char* dir)
{
  char* p_char;
  
  if ( strcmp( dir, "ms0:/") == 0 )
    return;
    
  p_char = &dir[strlen( dir )];
  
  /* remove last "/" */
  p_char--;
  *p_char = 0;
  
  p_char--;
  while( (*p_char) != '/') {
    *p_char = 0;
    p_char--;
  }
}

void get_name( char* path, char* name)
{
  char* p_char;
  int path_len;
  int name_len;
  int i_len;
  
  name_len = 0;
  path_len = strlen( path );
  p_char = &path[path_len];
  
  /* determine name length */
  while( (*p_char) != '/') {
    name_len++;
    p_char--;
  }
  
  /* copy name */
  for ( i_len=0; i_len<name_len; i_len++) {
    name[i_len] = path[path_len - name_len + i_len + 1 ];
  }
}

static int16_t get_name_cb(char *name, uint16_t size, void *user_data)
{
  char file_name[MAXNAME];
  
  get_name( filetosend, file_name);
  strncpy( name, file_name, size);
  name[size-1]=0;
  
  return strlen(name);
}

static int16_t get_data_cb(uint8_t *data, uint16_t size, void *user_data)
{
  int n;
  n = sceIoRead( send_fd, data, size);
  return n;
}

static uint8_t put_name_cb(char *name, void *user_data)
{
  char new_path[MAXPATH];
  char old_path[MAXPATH];

  SceIoStat file_stat;

  strcpy( new_path, receptiondir);
  strcat( new_path, name);
  strcpy( old_path, receptiondir);
  strcat( old_path, "rec_file");

  if ( sceIoClose(receive_fd) < 0 ) {
    DEBUG( 1, "Error closing file %s\n", old_path);
    return 0;
  }
  while ( sceIoGetstat( new_path, &file_stat) >= 0 ) {
    strcat( new_path, "_");
  }
  if ( sceIoRename( old_path, new_path) < 0 ) {
    DEBUG( 1, "Error renaming %s to %s\n", old_path, new_path);
    return 0;
  }
  if (!(receive_fd = sceIoOpen( new_path, PSP_O_WRONLY|PSP_O_CREAT, 0777))) {
    DEBUG( 1, "Error opening file\n");
    return 0;
  }

  oslPrintf_xy( 0, 120, "Receiving : %s\n", name);

  return 1;
}

static uint8_t put_data_cb(uint8_t *data, uint16_t size, void *user_data)
{

  if(sceIoWrite(receive_fd, data, size) < size) {
    DEBUG( 1, "Error writing\n");
  }

  return 1;
}

int obex_receive(IrLAP_Context *ctx_p, int try)
{
  int i;
  int success;

  char full_path[MAXPATH];

  success = 0;

  strcpy( full_path, receptiondir);
  strcat( full_path, "rec_file");

  oslCls();
  print_header();

  oslPrintf_xy( 0, 100, "Reception in : %s\n", receptiondir);
  oslPrintf_xy( 0, 110, "Please wait...\n");

  /* Open temporary file  */
  if (!(receive_fd = sceIoOpen( full_path, PSP_O_WRONLY|PSP_O_CREAT, 0777))) {
    DEBUG( 1, "Error opening file\n");
    return 0;
  }

  for( i=0; i<try; i++ ) {
    oslPrintf_xy( 0, 110,"Waiting: %d/%d\n", i+1, try);

    irlap_init_context(ctx_p);

    success = irobex_receive(ctx_p, put_name_cb, put_data_cb, 0);

    if(success) {
      break;
    }
  }

  /* Close received file  */
  if ( sceIoClose(receive_fd) < 0 ) {
    DEBUG( 1, "Error closing file\n");
    return 0;
  }

  return success;
}

int obex_send(IrLAP_Context *ctx_p)
{
  int success;

  success = 0;

  irlap_init_context(ctx_p);

  oslCls();
  print_header();

  oslPrintf_xy( 0, 100, "Sending file %s\n", filetosend);
  oslPrintf_xy( 0, 110, "Please wait...\n");

  if (!(send_fd = sceIoOpen( filetosend, PSP_O_RDONLY, 0777))) {
    DEBUG( 1, "Error opening file\n");
    return 0;
  }
  
  success = irobex_send( ctx_p, 0, get_name_cb, get_data_cb, 0);
  
  if ( sceIoClose(send_fd) < 0 ) {
    DEBUG( 1, "Error closing file\n");
    return 0;
  }
  
  return success;
}

int choose_file()
{
  SceIoDirent files[MAX_ENTRY];
  char path[MAXPATH];
  
  SceUID dir_fd;
  int number_files, cur_file;
  int button_pressed;
  int button_value;
  int enter_dir;
  int choice_done;

  enter_dir = 1;
  button_pressed = 0;
  choice_done = 0;
  
  strcpy( path, "ms0:/");
  
  while(enter_dir) {
    
    /*opening current directory*/
    dir_fd = sceIoDopen( path );
    if (dir_fd<0){
      DEBUG( 1, "Error opening directory : %s\n", path);
      return 0;
    }
    number_files = 0;
    while( number_files < MAX_ENTRY ){
      memset(&files[number_files], 0, sizeof(SceIoDirent)); 
      
      if(sceIoDread(dir_fd, &files[number_files])<=0)
        break;
      
      if( strcmp( files[number_files].d_name, ".") == 0 ) {
        continue;
      }
      
      if(FIO_S_ISDIR(files[number_files].d_stat.st_mode)){
        strcat(files[number_files].d_name, "/");
      }
      number_files++;
    }
    
    cur_file = 0;
    choice_done = 0;
    while(!choice_done){
      
      if( cur_file < 0 )
        cur_file=0;
      else if ( cur_file >= number_files )
        cur_file=number_files-1;
        
      /*oslPrintf_xy( 0, 100, "                                                                                ");*/
      print_send_menu();
      
      if(FIO_S_ISDIR(files[cur_file].d_stat.st_mode)) {
        oslPrintf_xy( 0, 100, "Directory : %s\n", files[cur_file].d_name);
      }
      else {
        oslPrintf_xy( 0, 100, "File      : %s , %d bytes\n", files[cur_file].d_name, (int) files[cur_file].d_stat.st_size);
      }
      
      button_pressed = 0;
      
      while(!button_pressed){
        
        button_value = oslWaitKey();
        
        switch (button_value) {
          /* Previous file */
          case OSL_KEY_LEFT:
          button_pressed = 1;
          cur_file--;
          break;
          /* Next file */
          case OSL_KEY_RIGHT:
          button_pressed = 1;
          cur_file++;
          break;
          /* Enter directory or select file */
          case OSL_KEY_CROSS:
          if (strcmp( files[cur_file].d_name, "../") == 0) {
            cd_parent_dir( path );
            enter_dir = 1;
          }
          else {
              
            if(FIO_S_ISDIR(files[cur_file].d_stat.st_mode)) {
              strcat( path, files[cur_file].d_name);
              enter_dir = 1;
            }
            else {
              strcpy( filetosend, path);
              strcat( filetosend, files[cur_file].d_name);
              enter_dir = 0;
            }
          }
          choice_done = 1;
          button_pressed = 1;
          break;
          /* Abort */
          case OSL_KEY_TRIANGLE:
            return 0;
          break;
        }
      }
    }
    sceIoDclose(dir_fd);
  }
  
  return 1;
}

int choose_dir()
{
  SceIoDirent dirs[MAX_ENTRY];
  char path[MAXPATH];
  
  SceUID dir_fd;
  int number_dir, cur_dir;
  int button_pressed;
  int button_value;
  int enter_dir;
  int choice_done;

  enter_dir = 1;
  button_pressed = 0;
  choice_done = 0;
  
  strcpy( path, "ms0:/");
  
  while(enter_dir) {
    
    /*opening current directory*/
    dir_fd = sceIoDopen( path );
    if (dir_fd<0){
      DEBUG( 1, "Error opening directory\n");
      return 0;
    }
    
    number_dir = 0;
    while( number_dir < MAX_ENTRY ){
      memset(&dirs[number_dir], 0, sizeof(SceIoDirent));
      if( sceIoDread(dir_fd, &dirs[number_dir])<=0 )
        break;
      
      if( FIO_S_ISDIR(dirs[number_dir].d_stat.st_mode) ){
        strcat(dirs[number_dir].d_name, "/");
        number_dir++;
      }
    }
    
    cur_dir = 0;
    choice_done = 0;
    while(!choice_done){
      
      if( cur_dir < 0 )
        cur_dir=0;
      else if ( cur_dir >= number_dir )
        cur_dir=number_dir-1;
        
      /*oslPrintf_xy( 0, 100, "                                                                                ");*/
      print_receive_menu();
      oslPrintf_xy( 0, 100, "Directory : %s", dirs[cur_dir].d_name);
      
      button_pressed = 0;
      
      while(!button_pressed){
        
        button_value = oslWaitKey();
        
        switch (button_value) {
          /* Previous file */
          case OSL_KEY_LEFT:
          button_pressed = 1;
          cur_dir--;
          break;
          
          /* Next file */
          case OSL_KEY_RIGHT:
          button_pressed = 1;
          cur_dir++;
          break;
          
          /* Enter directory  */
          case OSL_KEY_CIRCLE:
          if (strcmp( dirs[cur_dir].d_name, "../") == 0) {
            cd_parent_dir( path );
          }
          else {
            strcat( path, dirs[cur_dir].d_name);
          }
          enter_dir = 1;
          choice_done = 1;
          button_pressed = 1;
          break;
          
          /* Select directory */
          case OSL_KEY_CROSS:
          if (strcmp( dirs[cur_dir].d_name, "../") != 0) {
            strcpy( receptiondir, path);
            if (strcmp( dirs[cur_dir].d_name, "./") != 0) {
              strcat( receptiondir, dirs[cur_dir].d_name);
            }
            enter_dir = 0;
          }
          choice_done = 1;
          button_pressed = 1;
          break;
          /* Abort */
          case OSL_KEY_TRIANGLE:
            return 0;
          break;
        }
      }
    }
    sceIoDclose(dir_fd);
  }
  
  return 1;
}

void start_main()
{
  sceCtrlSetSamplingCycle(0);
  sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

  oslInit(1);
  oslInitGfx(OSL_PF_8888, 0);
  oslInitConsole();
  oslStartDrawing();
  oslSetKeyAutorepeatMask(OSL_KEYMASK_TRIANGLE|OSL_KEYMASK_CIRCLE |
                          OSL_KEYMASK_CROSS|OSL_KEYMASK_SQUARE);

  #if IRDA_DEBUG
  irda_debug = IRDA_DEBUG;
  debug_fd = fopen( "debug.log", "w");
  #endif
  #if IRDA_DUMP
  irda_dump = IRDA_DUMP;
  dump_fd = fopen( "dump.log", "w");
  send = 0;
  #endif

  irphy_reset();

}

void end_main()
{
  #if IRDA_DEBUG
  fclose( debug_fd );
  #endif
  #if IRDA_DUMP
  fclose( dump_fd );
  #endif
  
  oslEndGfx();
  oslQuit();

}

int main(int argc, char **argv)
{
  IrLAP_Context ctx;

  int button_value;

  SetupCallbacks();

  start_main();

  done = 0;
  while(!done)
  {
    print_main_menu();

    button_value = oslWaitKey();

    switch ( button_value ) {
      
      case OSL_KEY_SQUARE:
      print_receive_menu();
      if(choose_dir())
        obex_receive( &ctx, 100);
      break;
      
      case OSL_KEY_CROSS:
      print_send_menu();
      if(choose_file())
        obex_send( &ctx);
      break;
      
      case OSL_KEY_TRIANGLE:
      done =1;
      break;
    }
  }

 end_main();

  return 0;
}
