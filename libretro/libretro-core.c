#include "libretro.h"
#include "libretro-core.h"
#include "mem.h"

#include "archdep.h"

#include "machine.h"
#include "c64mem.h"
#include "snapshot.h"
#include "autostart.h"

//CORE VAR
#ifdef _WIN32
char slash = '\\';
#else
char slash = '/';
#endif

bool retro_load_ok = false;

retro_log_printf_t log_cb;

char RETRO_DIR[512];

char DISKA_NAME[512]="\0";
char DISKB_NAME[512]="\0";
char TAPE_NAME[512]="\0";

int mapper_keys[29]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
char keys[4096];
char buf[24][4096];


// Our virtual time counter, increased by retro_run()
long microSecCounter=0;
int cpuloop=1;

#ifdef FRONTEND_SUPPORTS_RGB565
	uint16_t *Retro_Screen;
	uint16_t bmp[WINDOW_SIZE];
	uint16_t save_Screen[WINDOW_SIZE];
#else
	unsigned int *Retro_Screen;
	unsigned int bmp[WINDOW_SIZE];
	unsigned int save_Screen[WINDOW_SIZE];
#endif

//SOUND
short signed int SNDBUF[1024*2];
//FIXME: handle 50/60
int snd_sampler = 44100 / 50;

//PATH
char RPATH[512];

extern int SHOWKEY;

extern int app_init(void);
extern int app_free(void);
extern int app_render(int poll);

int CROP_WIDTH;
int CROP_HEIGHT;
int VIRTUAL_WIDTH;

int retroXS=0;
int retroYS=0;
int retroW=1024;
int retroH=768;
int retrow=1024;
int retroh=768;
int lastW=1024;
int lastH=768;

#include "vkbd.i"

unsigned vice_devices[5];

extern int RETROJOY;
extern int RETROTDE;
extern int RETRODSE;
extern int RETRODSEVOL;
extern int RETROSTATUS;
extern int RETRORESET;
extern int RETRODRVTYPE;
extern int RETROSIDMODL;
extern int RETROC64MODL;
extern int RETROUSERPORTJOY;
extern int RETROEXTPAL;
extern int RETROAUTOSTARTWARP;
extern int RETROBORDERS;
extern int RETROTHEME;
extern char RETROEXTPALNAME[512];
extern int retro_ui_finalized;
extern unsigned int cur_port;
extern void set_drive_type(int drive,int val);
extern void set_truedrive_emulation(int val);
extern void reset_mouse_pos();
extern uint8_t mem_ram[];
extern int g_mem_ram_size;

//VICE DEF BEGIN
#include "resources.h"
#include "sid.h"
#include "userport_joystick.h"
#if  defined(__VIC20__)
#include "c64model.h"
#include "vic20model.h"
#elif defined(__PLUS4__)
#include "c64model.h"
#include "plus4model.h"
#elif defined(__X128__)
#include "c64model.h"
#include "c128model.h"
#elif defined(__PET__)
#include "petmodel.h"
#elif defined(__CBM2__)
#include "cbm2model.h"
#else
#include "c64model.h"
#endif
//VICE DEF END

extern void emu_init(void);
extern void emu_uninit(void);
extern void vice_main_exit(void);
extern void emu_reset(void);

const char *retro_save_directory;
const char *retro_system_directory;
const char *retro_content_directory;
char retro_system_data_directory[512];

/*static*/ retro_input_state_t input_state_cb;
/*static*/ retro_input_poll_t input_poll_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;

#include "retro_strings.h"
#include "retro_files.h"
#include "retro_disk_control.h"
static dc_storage* dc;

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

static char CMDFILE[512];

int loadcmdfile(char *argv)
{
   int res=0;

   FILE *fp = fopen(argv,"r");

   if( fp != NULL )
   {
      if ( fgets (CMDFILE , 512 , fp) != NULL )
         res=1;
      fclose (fp);
   }

   return res;
}

int HandleExtension(char *path,char *ext)
{
   int len = strlen(path);

   if (len >= 4 &&
         path[len-4] == '.' &&
         path[len-3] == ext[0] &&
         path[len-2] == ext[1] &&
         path[len-1] == ext[2])
   {
      return 1;
   }

   return 0;
}

#include <ctype.h>

//Args for experimental_cmdline
static char ARGUV[64][1024];
static unsigned char ARGUC=0;

// Args for Core
static char XARGV[64][1024];
static const char* xargv_cmd[64];
int PARAMCOUNT=0;

extern int  skel_main(int argc, char *argv[]);
void parse_cmdline( const char *argv );

void Add_Option(const char* option)
{
   static int first=0;

   if(first==0)
   {
      PARAMCOUNT=0;
      first++;
   }

   sprintf(XARGV[PARAMCOUNT++],"%s",option);
}

int pre_main(const char *argv)
{
   int i=0;
   bool Only1Arg;

   if (strlen(argv) > strlen("cmd"))
   {
      if( HandleExtension((char*)argv,"cmd") || HandleExtension((char*)argv,"CMD"))
         i=loadcmdfile((char*)argv);
   }

   if(i==1)
   {
      parse_cmdline(CMDFILE);
      LOGI("Starting game from command line :%s\n",CMDFILE);
   }
   else
      parse_cmdline(argv);

   Only1Arg = (strcmp(ARGUV[0],CORE_NAME) == 0) ? 0 : 1;

   for (i = 0; i<64; i++)
      xargv_cmd[i] = NULL;


   if(Only1Arg)
   {  Add_Option(CORE_NAME);
      /*
         if (strlen(RPATH) >= strlen("crt"))
         if(!strcasecmp(&RPATH[strlen(RPATH)-strlen("crt")], "crt"))
         Add_Option("-cartcrt");
         */

#if defined(__VIC20__)
     if (strlen(RPATH) >= strlen(".20"))
       if (!strcasecmp(&RPATH[strlen(RPATH)-strlen(".20")], ".20"))
	 Add_Option("-cart2");

     if (strlen(RPATH) >= strlen(".40"))
       if (!strcasecmp(&RPATH[strlen(RPATH)-strlen(".40")], ".40"))
	 Add_Option("-cart4");

     if (strlen(RPATH) >= strlen(".60"))
       if (!strcasecmp(&RPATH[strlen(RPATH)-strlen(".60")], ".60"))
	 Add_Option("-cart6");

     if (strlen(RPATH) >= strlen(".a0"))
       if (!strcasecmp(&RPATH[strlen(RPATH)-strlen(".a0")], ".a0"))
	 Add_Option("-cartA");

     if (strlen(RPATH) >= strlen(".b0"))
       if (!strcasecmp(&RPATH[strlen(RPATH)-strlen(".b0")], ".b0"))
	 Add_Option("-cartB");
#endif

     if (strlen(RPATH) >= strlen(".prg"))
       if (!strcasecmp(&RPATH[strlen(RPATH)-strlen(".prg")], ".prg"))
         RETROTDE=0;

     if (strlen(RPATH) >= strlen(".d71"))
       if (!strcasecmp(&RPATH[strlen(RPATH)-strlen(".d71")], ".d71"))
         RETRODRVTYPE=1571;
       
     if (strlen(RPATH) >= strlen(".d81"))
       if (!strcasecmp(&RPATH[strlen(RPATH)-strlen(".d81")], ".d81"))
         RETRODRVTYPE=1581;


     Add_Option(RPATH/*ARGUV[0]*/);
   }
   else
   { // Pass all cmdline args
      for(i = 0; i < ARGUC; i++)
         Add_Option(ARGUV[i]);
   }

   for (i = 0; i < PARAMCOUNT; i++)
   {
      xargv_cmd[i] = (char*)(XARGV[i]);
      LOGI("%2d  %s\n",i,XARGV[i]);
   }

   skel_main(PARAMCOUNT,( char **)xargv_cmd);

   xargv_cmd[PARAMCOUNT - 2] = NULL;

   return 0;
}

void parse_cmdline(const char *argv)
{
   char *p,*p2,*start_of_word;
   int c,c2;
   static char buffer[512*4];
   enum states { DULL, IN_WORD, IN_STRING } state = DULL;

   strcpy(buffer,argv);
   strcat(buffer," \0");

   for (p = buffer; *p != '\0'; p++)
   {
      c = (unsigned char) *p; /* convert to unsigned char for is* functions */
      switch (state)
      {
         case DULL: /* not in a word, not in a double quoted string */
            if (isspace(c)) /* still not in a word, so ignore this char */
               continue;
            /* not a space -- if it's a double quote we go to IN_STRING, else to IN_WORD */
            if (c == '"')
            {
               state = IN_STRING;
               start_of_word = p + 1; /* word starts at *next* char, not this one */
               continue;
            }
            state = IN_WORD;
            start_of_word = p; /* word starts here */
            continue;
         case IN_STRING:
            /* we're in a double quoted string, so keep going until we hit a close " */
            if (c == '"')
            {
               /* word goes from start_of_word to p-1 */
               //... do something with the word ...
               for (c2 = 0,p2 = start_of_word; p2 < p; p2++, c2++)
                  ARGUV[ARGUC][c2] = (unsigned char) *p2;
               ARGUC++;

               state = DULL; /* back to "not in word, not in string" state */
            }
            continue; /* either still IN_STRING or we handled the end above */
         case IN_WORD:
            /* we're in a word, so keep going until we get to a space */
            if (isspace(c))
            {
               /* word goes from start_of_word to p-1 */
               //... do something with the word ...
               for (c2 = 0,p2 = start_of_word; p2 <p; p2++,c2++)
                  ARGUV[ARGUC][c2] = (unsigned char) *p2;
               ARGUC++;

               state = DULL; /* back to "not in word, not in string" state */
            }
            continue; /* either still IN_WORD or we handled the end above */
      }
   }
}

long GetTicks(void) {
   // NOTE: Cores should normally not depend on real time, so we return a
   // counter here
   // GetTicks() is used by vsyncarch_gettime() which is used by
   // * Vsync (together with sleep) to sync to 50Hz
   // * Mouse timestamps
   // * Networking
   // Returning a frame based msec counter could potentially break
   // networking but it's not something libretro uses at the moment.
   return microSecCounter;
}

void save_bkg(void)
{
   memcpy(save_Screen,Retro_Screen,PITCH*WINDOW_SIZE);
}

void restore_bgk(void)
{
   memcpy(Retro_Screen,save_Screen,PITCH*WINDOW_SIZE);
}

void Screen_SetFullUpdate(int scr)
{
   if(scr==0 ||scr>1)
      memset(&Retro_Screen, 0, sizeof(Retro_Screen));
   if(scr>0)
      memset(&bmp,0,sizeof(bmp));
}

int keyId(const char *val)
{
   int i=0;
   while (keyDesc[i]!=NULL)
   {
      if (!strcmp(keyDesc[i],val))
         return keyVal[i];
      i++;
   }
   return 0;
}

void retro_set_environment(retro_environment_t cb)
{
   static const struct retro_controller_description p1_controllers[] = {
      { "Vice Joystick", RETRO_DEVICE_VICE_JOYSTICK },
      { "Vice Keyboard", RETRO_DEVICE_VICE_KEYBOARD },
      { "Disconnected", RETRO_DEVICE_NONE },
   };
   static const struct retro_controller_description p2_controllers[] = {
      { "Vice Joystick", RETRO_DEVICE_VICE_JOYSTICK },
      { "Vice Keyboard", RETRO_DEVICE_VICE_KEYBOARD },
      { "Disconnected", RETRO_DEVICE_NONE },
   };
   static const struct retro_controller_description p3_controllers[] = {
      { "Vice Joystick", RETRO_DEVICE_VICE_JOYSTICK },
      { "Vice Keyboard", RETRO_DEVICE_VICE_KEYBOARD },
      { "Disconnected", RETRO_DEVICE_NONE },
   };
   static const struct retro_controller_description p4_controllers[] = {
      { "Vice Joystick", RETRO_DEVICE_VICE_JOYSTICK },
      { "Vice Keyboard", RETRO_DEVICE_VICE_KEYBOARD },
      { "Disconnected", RETRO_DEVICE_NONE },
   };
   static const struct retro_controller_description p5_controllers[] = {
      { "Vice Joystick", RETRO_DEVICE_VICE_JOYSTICK },
      { "Vice Keyboard", RETRO_DEVICE_VICE_KEYBOARD },
      { "Disconnected", RETRO_DEVICE_NONE },
   };

   static const struct retro_controller_info ports[] = {
      { p1_controllers, 3 }, // port 1
      { p2_controllers, 3 }, // port 2
      { p3_controllers, 3 }, // port 3
      { p4_controllers, 3 }, // port 4
      { p5_controllers, 3 }, // port 5
      { NULL, 0 }
   };

   struct retro_variable variables[] =
   {
      {
         "vice_vkbd_theme",
         "Virtual Keyboard Theme; classic|modern",
      },
      {
         "vice_statusbar",
         "Statusbar; disabled|enabled",
      },
      {
         "vice_drive_true_emulation",
         "Drive True Emulation; disabled|enabled",
      },
      {
         "vice_drive_sound_emulation",
         "Drive Sound Emulation; disabled|enabled",
      },
      {
         "vice_drive_sound_volume",
         "Drive Sound Volume; 10\%|20\%|30\%|40\%|50\%|60\%|70\%|80\%|90\%|100\%",
      },
      {
         "vice_drive8_type",
         "Drive8 Type; 1541|1542|1581|1540|1551|1570|1571|1573|2000|4000|2031|2040|3040|4040|1001|8050|8250",
      },
      {
         "vice_autostart_warp",
         "Autostart Warp; disabled|enabled",
      },
      {
         "vice_sid_model",
         "Sid Model; 6581F|8580F|6581R|8580R|8580RD",
      },
#if  defined(__VIC20__)
      {
         "vice_vic20_model",
         "VIC20 Model; VIC20MODEL_VIC20_PAL|VIC20MODEL_VIC20_NTSC|VIC20MODEL_VIC21|VIC20MODEL_UNKNOWN",
      },
#elif  defined(__PLUS4__)
      {
         "vice_plus4_model",
         "PLUS4 Model; PLUS4MODEL_C16_PAL|PLUS4MODEL_C16_NTSC|PLUS4MODEL_PLUS4_PAL|PLUS4MODEL_PLUS4_NTSC|PLUS4MODEL_V364_NTSC|PLUS4MODEL_232_NTSC|PLUS4MODEL_UNKNOWN",
      },
#elif  defined(__X128__)
      {
         "vice_c128_model",
         "C128 Model; C128MODEL_C128_PAL|C128MODEL_C128DCR_PAL|C128MODEL_C128_NTSC|C128MODEL_C128DCR_NTSC|C128MODEL_UNKNOWN",
      },
#elif  defined(__PET__)
      {
         "vice_pet_model",
         "PET Model; PETMODEL_2001|PETMODEL_3008|PETMODEL_3016|PETMODEL_3032|PETMODEL_3032B|PETMODEL_4016|PETMODEL_4032|PETMODEL_4032B|PETMODEL_8032|PETMODEL_8096|PETMODEL_8296|PETMODEL_SUPERPET|PETMODEL_UNKNOWN",
      },
#else
      {
         "vice_c64_model",
         "C64 Model; C64MODEL_C64_PAL|C64MODEL_C64C_PAL|C64MODEL_C64_OLD_PAL|C64MODEL_C64_NTSC|C64MODEL_C64C_NTSC|C64MODEL_C64_OLD_NTSC|C64MODEL_C64_PAL_N|C64MODEL_C64SX_PAL|C64MODEL_C64SX_NTSC|C64MODEL_C64_JAP|C64MODEL_C64_GS|C64MODEL_PET64_PAL|C64MODEL_PET64_NTSC|C64MODEL_ULTIMAX|C64MODEL_UNKNOWN",
      },
#endif
      {
         "vice_border",
         "Display borders; enabled|disabled",
      },
#if defined(__VIC20__)
      {
         "vice_vic20_external_palette",
         "External palette; none|mike-ntsc|mike-pal|colodore_vic|vice",
      },
#elif defined(__PLUS4__) 
      {
         "vice_plus4_external_palette",
         "External palette; none|yape-pal|yape-ntsc|colodore_ted",
      },
#elif defined(__PET__)
      {
         "vice_pet_external_palette",
         "External palette; none|amber|green|white",
      },
#else
      {
         "vice_external_palette",
         "External palette; none|pepto-pal|pepto-palold|pepto-ntsc-sony|pepto-ntsc|colodore|vice|c64hq|c64s|ccs64|frodo|godot|pc64|rgb|deekay|ptoing|community-colors",
      },
#endif
      {
         "vice_reset",
         "Reset type; autostart|soft|hard",
      },
	  {
         "vice_userport_joytype",
         "4-player adapter; none|Protovision_CGA|PET|Hummer|OEM|Hit|Kingsoft|Starbyte",
      },
      {
         "vice_joyport",
         "Controller0 port; port 2|port 1",
      },
      { "vice_mapper_select", buf[0] },
      { "vice_mapper_start", buf[1] },
      { "vice_mapper_y", buf[2] },
      { "vice_mapper_x", buf[3] },
      { "vice_mapper_b", buf[4] },
      { "vice_mapper_l", buf[5] },
      { "vice_mapper_r", buf[6] },
      { "vice_mapper_l2", buf[7] },
      { "vice_mapper_r2", buf[8] },
      { "vice_mapper_l3", buf[9] },
      { "vice_mapper_r3", buf[10] },

      { "vice_mapper_lu", buf[14] },
      { "vice_mapper_ld", buf[13] },
      { "vice_mapper_ll", buf[12] },
      { "vice_mapper_lr", buf[11] },

      { "vice_mapper_ru", buf[18] },
      { "vice_mapper_rd", buf[17] },
      { "vice_mapper_rl", buf[16] },
      { "vice_mapper_rr", buf[15] },
      
      { "vice_mapper_vkbd", buf[19] },
      { "vice_mapper_statusbar", buf[20] },
      { "vice_mapper_joyport_switch", buf[21] },
      { "vice_mapper_reset", buf[22] },
      { "vice_mapper_warp_mode", buf[23] },

      { NULL, NULL },
   };

   bool allowNoGameMode;
   int i;

   environ_cb = cb;

   cb( RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports );

   i=0;
   while (keyDesc[i]!=NULL)
   {
      if (i == 0)
         strcpy (keys, keyDesc[i]);
      else
         strcat (keys, keyDesc[i]);
      if (keyDesc[i+1]!=NULL)
         strcat (keys, "|");
      i++;
   }

   snprintf(buf[0],sizeof(buf[0]),"RetroPad SELECT; %s|%s","---", keys);
   snprintf(buf[1],sizeof(buf[1]),"RetroPad START; %s|%s","---", keys);
   
   snprintf(buf[2],sizeof(buf[2]), "RetroPad Y; %s|%s","RETROK_SPACE", keys);
   snprintf(buf[3],sizeof(buf[3]), "RetroPad X; %s|%s","RETROK_TAB", keys);
   snprintf(buf[4],sizeof(buf[4]), "RetroPad B; %s|%s","RETROK_F7", keys);
   snprintf(buf[5],sizeof(buf[5]), "RetroPad L; %s|%s","RETROK_F11", keys);
   snprintf(buf[6],sizeof(buf[6]), "RetroPad R; %s|%s","RETROK_F10", keys);
   snprintf(buf[7],sizeof(buf[7]), "RetroPad L2; %s|%s","RETROK_ESCAPE", keys);
   snprintf(buf[8],sizeof(buf[8]), "RetroPad R2; %s|%s","RETROK_RETURN", keys);
   snprintf(buf[9],sizeof(buf[9]), "RetroPad L3; %s|%s","RETROK_h", keys);
   snprintf(buf[10],sizeof(buf[10]), "RetroPad R3; %s|%s","RETROK_t", keys);
   
   snprintf(buf[14],sizeof(buf[14]),"RetroPad L-Up; %s|%s","RETROK_UP", keys);
   snprintf(buf[13],sizeof(buf[13]),"RetroPad L-Down; %s|%s","RETROK_DOWN", keys);
   snprintf(buf[12],sizeof(buf[12]),"RetroPad L-Left; %s|%s","RETROK_LEFT", keys);
   snprintf(buf[11],sizeof(buf[11]),"RetroPad L-Right; %s|%s","RETROK_RIGHT", keys);
   snprintf(buf[18],sizeof(buf[18]),"RetroPad R-Up; %s|%s","RETROK_y", keys);
   snprintf(buf[17],sizeof(buf[17]),"RetroPad R-Down; %s|%s","RETROK_n", keys);
   snprintf(buf[16],sizeof(buf[16]),"RetroPad R-Left; %s|%s","RETROK_l", keys);
   snprintf(buf[15],sizeof(buf[15]),"RetroPad R-Right; %s|%s","RETROK_r", keys);
   
   snprintf(buf[19],sizeof(buf[19]),"Hotkey: Toggle Virtual Keyboard; %s|%s","RETROK_F11", keys);
   snprintf(buf[20],sizeof(buf[20]),"Hotkey: Toggle Statusbar; %s|%s","RETROK_F10", keys);
   snprintf(buf[21],sizeof(buf[21]),"Hotkey: Switch Joyports; %s|%s","RETROK_RCTRL", keys);
   snprintf(buf[22],sizeof(buf[22]),"Hotkey: Reset; %s|%s","RETROK_END", keys);
   snprintf(buf[23],sizeof(buf[23]),"Hotkey: Warp Mode; %s|%s","RETROK_PAGEDOWN", keys);

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);

   allowNoGameMode = true;
   environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &allowNoGameMode);
}

static void update_variables(void)
{

   struct retro_variable var;

   var.key = "vice_vkbd_theme";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
         if (strcmp(var.value, "classic") == 0)RETROTHEME=0;
         if (strcmp(var.value, "modern") == 0)RETROTHEME=1;
   }

   var.key = "vice_statusbar";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(retro_ui_finalized){
         if (strcmp(var.value, "enabled") == 0)
            resources_set_int("SDLStatusbar", 1);
         if (strcmp(var.value, "disabled") == 0)
            resources_set_int("SDLStatusbar", 0);
      }
      else {
         if (strcmp(var.value, "enabled") == 0)RETROSTATUS=1;
         if (strcmp(var.value, "disabled") == 0)RETROSTATUS=0;
      }
   }

   var.key = "vice_autostart_warp";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(retro_ui_finalized){
         if (strcmp(var.value, "enabled") == 0)
            resources_set_int("AutostartWarp", 1);
         if (strcmp(var.value, "disabled") == 0)
            resources_set_int("AutostartWarp", 0);
      }
      else {
         if (strcmp(var.value, "enabled") == 0)RETROAUTOSTARTWARP=1;
         if (strcmp(var.value, "disabled") == 0)RETROAUTOSTARTWARP=0;
      }
   }

   var.key = "vice_drive8_type";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char str[100];
      int val;
      snprintf(str, sizeof(str), "%s", var.value);
      val = strtoul(str, NULL, 0);

      if(retro_ui_finalized)
         set_drive_type(8, val);
      else RETRODRVTYPE=val;
   }

   var.key = "vice_drive_true_emulation";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(retro_ui_finalized){
         if (strcmp(var.value, "enabled") == 0){
            set_truedrive_emulation(1);
            resources_set_int("VirtualDevices", 0);
         }
         if (strcmp(var.value, "disabled") == 0){
            set_truedrive_emulation(0);
            resources_set_int("VirtualDevices", 1);
         }
      }
      else  {
         if (strcmp(var.value, "enabled") == 0)RETROTDE=1;
         if (strcmp(var.value, "disabled") == 0)RETROTDE=0;
      }
   }

   var.key = "vice_drive_sound_emulation";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if(retro_ui_finalized){
         if (strcmp(var.value, "enabled") == 0){
     	    resources_set_int("DriveSoundEmulation", 1);
         }
         if (strcmp(var.value, "disabled") == 0){
            resources_set_int("DriveSoundEmulation", 0);
    	 }
      }
      else  {
         if (strcmp(var.value, "enabled") == 0)RETRODSE=1;
         if (strcmp(var.value, "disabled") == 0)RETRODSE=0;
      }
   }

   var.key = "vice_drive_sound_volume";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int val = atoi(var.value);
      val = val * 40;
      
      if(retro_ui_finalized)
         resources_set_int("DriveSoundEmulationVolume", val);
      else RETRODSEVOL=val;
   }


   var.key = "vice_sid_model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int eng=0,modl=0,sidmdl=0;

      if (strcmp(var.value, "6581F") == 0){eng=0;modl=0;}
      else if (strcmp(var.value, "8580F") == 0){eng=0;modl=1;}
      else if (strcmp(var.value, "6581R") == 0){eng=1;modl=0;}
      else if (strcmp(var.value, "8580R") == 0){eng=1;modl=1;}
      else if (strcmp(var.value, "8580RD") == 0){eng=1;modl=2;}

      sidmdl=((eng << 8) | modl) ;

      if(retro_ui_finalized)
        sid_set_engine_model(eng, modl);
      else RETROSIDMODL=sidmdl;
   }

#if  defined(__VIC20__)
   var.key = "vice_vic20_model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int modl=0;

      if (strcmp(var.value, "VIC20MODEL_VIC20_PAL") == 0)modl=VIC20MODEL_VIC20_PAL;
      else if (strcmp(var.value, "VIC20MODEL_VIC20_NTSC") == 0)modl=VIC20MODEL_VIC20_NTSC;
      else if (strcmp(var.value, "VIC20MODEL_VIC21") == 0)modl=VIC20MODEL_VIC21;
      else if (strcmp(var.value, "VIC20MODEL_UNKNOWN") == 0)modl=VIC20MODEL_UNKNOWN;

      if(retro_ui_finalized)
        vic20model_set(modl);
      else RETROC64MODL=modl;
   }
#elif  defined(__PLUS4__)
   var.key = "vice_plus4_model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int modl=0;

      if (strcmp(var.value, "PLUS4MODEL_C16_PAL") == 0)modl=PLUS4MODEL_C16_PAL;
      else if (strcmp(var.value, "PLUS4MODEL_C16_NTSC") == 0)modl=PLUS4MODEL_C16_NTSC;
      else if (strcmp(var.value, "PLUS4MODEL_PLUS4_PAL") == 0)modl=PLUS4MODEL_PLUS4_PAL;
      else if (strcmp(var.value, "PLUS4MODEL_PLUS4_NTSC") == 0)modl=PLUS4MODEL_PLUS4_NTSC;
      else if (strcmp(var.value, "PLUS4MODEL_V364_NTSC") == 0)modl=PLUS4MODEL_V364_NTSC;
      else if (strcmp(var.value, "PLUS4MODEL_232_NTSC") == 0)modl=PLUS4MODEL_232_NTSC;
      else if (strcmp(var.value, "PLUS4MODEL_UNKNOWN") == 0)modl=PLUS4MODEL_UNKNOWN;

      if(retro_ui_finalized)
        plus4model_set(modl);
      else RETROC64MODL=modl;
   }
#elif  defined(__X128__)
   var.key = "vice_c128_model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int modl=0;

      if (strcmp(var.value, "C128MODEL_C128_PAL") == 0)modl=C128MODEL_C128_PAL;
      else if (strcmp(var.value, "C128MODEL_C128DCR_PAL") == 0)modl=C128MODEL_C128DCR_PAL;
      else if (strcmp(var.value, "C128MODEL_C128_NTSC") == 0)modl=C128MODEL_C128_NTSC;
      else if (strcmp(var.value, "C128MODEL_C128DCR_NTSC") == 0)modl=C128MODEL_C128DCR_NTSC;
      else if (strcmp(var.value, "C128MODEL_UNKNOWN") == 0)modl=C128MODEL_UNKNOWN;
      if(retro_ui_finalized)
        c128model_set(modl);
      else RETROC64MODL=modl;
   }
#elif  defined(__PET__)
   var.key = "vice_pet_model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int modl=0;

      if (strcmp(var.value, "PETMODEL_2001") == 0)modl=PETMODEL_2001;
      else if (strcmp(var.value, "PETMODEL_3008") == 0)modl=PETMODEL_3008;
      else if (strcmp(var.value, "PETMODEL_3016") == 0)modl=PETMODEL_3016;
      else if (strcmp(var.value, "PETMODEL_3032") == 0)modl=PETMODEL_3032;
      else if (strcmp(var.value, "PETMODEL_3032B") == 0)modl=PETMODEL_3032B;
      else if (strcmp(var.value, "PETMODEL_4016") == 0)modl=PETMODEL_4016;
      else if (strcmp(var.value, "PETMODEL_4032") == 0)modl=PETMODEL_4032;
      else if (strcmp(var.value, "PETMODEL_4032B") == 0)modl=PETMODEL_4032B;
      else if (strcmp(var.value, "PETMODEL_8032") == 0)modl=PETMODEL_8032;
      else if (strcmp(var.value, "PETMODEL_8096") == 0)modl=PETMODEL_8096;
      else if (strcmp(var.value, "PETMODEL_8296") == 0)modl=PETMODEL_8296;
      else if (strcmp(var.value, "PETMODEL_SUPERPET") == 0)modl=PETMODEL_SUPERPET;
      else if (strcmp(var.value, "PETMODEL_UNKNOWN") == 0)modl=PETMODEL_UNKNOWN;
      if(retro_ui_finalized)
        petmodel_set(modl);
      else RETROC64MODL=modl;
   }
#elif  defined(__CBM2__)
   var.key = "vice_cbm2_model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int modl=0;

      if (strcmp(var.value, "CBM2MODEL_510_PAL") == 0)modl=CBM2MODEL_510_PAL;
      else if (strcmp(var.value, "CBM2MODEL_510_NTSC") == 0)modl=CBM2MODEL_510_NTSC;
      else if (strcmp(var.value, "CBM2MODEL_610_PAL") == 0)modl=CBM2MODEL_610_PAL;
      else if (strcmp(var.value, "CBM2MODEL_610_NTSC") == 0)modl=CBM2MODEL_610_NTSC;
      else if (strcmp(var.value, "CBM2MODEL_620_PAL") == 0)modl=CBM2MODEL_620_PAL;
      else if (strcmp(var.value, "CBM2MODEL_620_NTSC") == 0)modl=CBM2MODEL_620_NTSC;
      else if (strcmp(var.value, "CBM2MODEL_620PLUS_PAL") == 0)modl=CBM2MODEL_620PLUS_PAL;
      else if (strcmp(var.value, "CBM2MODEL_620PLUS_NTSC") == 0)modl=CBM2MODEL_620PLUS_NTSC;
      else if (strcmp(var.value, "CBM2MODEL_710_NTSC") == 0)modl=CBM2MODEL_710_NTSC;
      else if (strcmp(var.value, "CBM2MODEL_720_NTSC") == 0)modl=CBM2MODEL_720_NTSC;
      else if (strcmp(var.value, "CBM2MODEL_720PLUS_NTSC") == 0)modl=CBM2MODEL_720PLUS_NTSC;
      else if (strcmp(var.value, "CBM2MODEL_UNKNOWN") == 0)modl=CBM2MODEL_UNKNOWN;
      if(retro_ui_finalized)
        cbm2model_set(modl);
      else RETROC64MODL=modl;
   }
#else
   var.key = "vice_c64_model";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int modl=0;

      if (strcmp(var.value, "C64MODEL_C64_PAL") == 0)modl=C64MODEL_C64_PAL;
      else if (strcmp(var.value, "C64MODEL_C64C_PAL") == 0)modl=C64MODEL_C64C_PAL;
      else if (strcmp(var.value, "C64MODEL_C64_OLD_PAL") == 0)modl=C64MODEL_C64_OLD_PAL;
      else if (strcmp(var.value, "C64MODEL_C64_NTSC") == 0)modl=C64MODEL_C64_NTSC;
      else if (strcmp(var.value, "C64MODEL_C64C_NTSC") == 0)modl=C64MODEL_C64C_NTSC;
      else if (strcmp(var.value, "C64MODEL_C64_OLD_NTSC") == 0)modl=C64MODEL_C64_OLD_NTSC;
      else if (strcmp(var.value, "C64MODEL_C64_PAL_N") == 0)modl=C64MODEL_C64_PAL_N;
      else if (strcmp(var.value, "C64MODEL_C64SX_PAL") == 0)modl=C64MODEL_C64SX_PAL;
      else if (strcmp(var.value, "C64MODEL_C64SX_NTSC") == 0)modl=C64MODEL_C64SX_NTSC;
      else if (strcmp(var.value, "C64MODEL_C64_JAP") == 0)modl=C64MODEL_C64_JAP;
      else if (strcmp(var.value, "C64MODEL_C64_GS") == 0)modl=C64MODEL_C64_GS;
      else if (strcmp(var.value, "C64MODEL_PET64_PAL") == 0)modl=C64MODEL_PET64_PAL;
      else if (strcmp(var.value, "C64MODEL_PET64_NTSC") == 0)modl=C64MODEL_PET64_NTSC;
      else if (strcmp(var.value, "C64MODEL_ULTIMAX") == 0)modl=C64MODEL_ULTIMAX;
      else if (strcmp(var.value, "C64MODEL_UNKNOWN") == 0)modl=C64MODEL_UNKNOWN;

      if(retro_ui_finalized)
        c64model_set(modl);
      else RETROC64MODL=modl;
   }
#endif

   var.key = "vice_border";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int border=0; /* 0 : normal, 1: full, 2: debug, 3: none */
      if (strcmp(var.value, "enabled") == 0)border=0;
      else if (strcmp(var.value, "disabled") == 0)border=3;

      if(retro_ui_finalized)
#if defined(__VIC20__)
        resources_set_int("VICBorderMode", border);
#elif defined(__PLUS4__)
        resources_set_int("TEDBorderMode", border);
#else 
        resources_set_int("VICIIBorderMode", border);
#endif
      else RETROBORDERS=border;
   }

#if defined(__VIC20__)
   var.key = "vice_vic20_external_palette";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int extpal=-1;
      if (strcmp(var.value, "none") == 0)extpal=-1;
      else if (strcmp(var.value, "mike-ntsc") == 0)extpal=0;
      else if (strcmp(var.value, "mike-pal") == 0)extpal=1;
      else if (strcmp(var.value, "colodore_vic") == 0)extpal=2;
      else if (strcmp(var.value, "vice") == 0)extpal=3;

      if(retro_ui_finalized){
         if(extpal==-1)resources_set_int("VICExternalPalette", 0);
         else {
            resources_set_int("VICExternalPalette", 1);
            resources_set_string_sprintf("%sPaletteFile", var.value, "VIC");
         }
      } else {
         RETROEXTPAL=extpal;
         if(extpal!=-1){
            sprintf(RETROEXTPALNAME, "%s", var.value);
         }
      }
   }
#elif defined(__PLUS4__)
   var.key = "vice_plus4_external_palette";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int extpal=-1;
      if (strcmp(var.value, "none") == 0)extpal=-1;
      else if (strcmp(var.value, "yape-pal") == 0)extpal=0;
      else if (strcmp(var.value, "yape-ntsc") == 0)extpal=1;
      else if (strcmp(var.value, "colodore_ted") == 0)extpal=2;

      if(retro_ui_finalized){
         if(extpal==-1)resources_set_int("TEDExternalPalette", 0);
         else {
            resources_set_int("TEDExternalPalette", 1);
            resources_set_string_sprintf("%sPaletteFile", var.value, "TED");
         }
      } else {
         RETROEXTPAL=extpal;
         if(extpal!=-1){
            sprintf(RETROEXTPALNAME, "%s", var.value);
         }
      }
   }
#elif defined(__PET__)
   var.key = "vice_pet_external_palette";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int extpal=-1;
      if (strcmp(var.value, "none") == 0)extpal=-1;
      else if (strcmp(var.value, "amber") == 0)extpal=0;
      else if (strcmp(var.value, "green") == 0)extpal=1;
      else if (strcmp(var.value, "white") == 0)extpal=2;

      if(retro_ui_finalized){
         if(extpal==-1)resources_set_int("CrtcExternalPalette", 0);
         else {
            resources_set_int("CrtcExternalPalette", 1);
            resources_set_string_sprintf("%sPaletteFile", var.value, "Crtc");
         }
      } else {
         RETROEXTPAL=extpal;
         if(extpal!=-1){
            sprintf(RETROEXTPALNAME, "%s", var.value);
         }
      }
   }
#else
   var.key = "vice_external_palette";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int extpal=-1;
      if (strcmp(var.value, "none") == 0)extpal=-1;
      else if (strcmp(var.value, "pepto-pal") == 0)extpal=0;
      else if (strcmp(var.value, "pepto-palold") == 0)extpal=1;
      else if (strcmp(var.value, "pepto-ntsc-sony") == 0)extpal=2;
      else if (strcmp(var.value, "pepto-ntsc") == 0)extpal=3;
      else if (strcmp(var.value, "colodore") == 0)extpal=4;
      else if (strcmp(var.value, "vice") == 0)extpal=5;
      else if (strcmp(var.value, "c64hq") == 0)extpal=6;
      else if (strcmp(var.value, "c64s") == 0)extpal=7;
      else if (strcmp(var.value, "ccs64") == 0)extpal=8;
      else if (strcmp(var.value, "frodo") == 0)extpal=9;
      else if (strcmp(var.value, "godot") == 0)extpal=10;
      else if (strcmp(var.value, "pc64") == 0)extpal=11;
      else if (strcmp(var.value, "rgb") == 0)extpal=12;
      else if (strcmp(var.value, "deekay") == 0)extpal=13;
      else if (strcmp(var.value, "ptoing") == 0)extpal=14;
      else if (strcmp(var.value, "community-colors") == 0)extpal=15;

      if(retro_ui_finalized){
         if(extpal==-1)resources_set_int("VICIIExternalPalette", 0);
         else {
            resources_set_int("VICIIExternalPalette", 1);
            resources_set_string_sprintf("%sPaletteFile", var.value, "VICII");
         }
      } else {
         RETROEXTPAL=extpal;
         if(extpal!=-1){
            sprintf(RETROEXTPALNAME, "%s", var.value);
         }
      }
   }
#endif

   var.key = "vice_userport_joytype";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {

      int joyadaptertype=-1;
      if (strcmp(var.value, "none") == 0)joyadaptertype=-1;
      else if (strcmp(var.value, "Protovision_CGA") == 0)joyadaptertype=USERPORT_JOYSTICK_CGA;
      else if (strcmp(var.value, "PET") == 0)joyadaptertype=USERPORT_JOYSTICK_PET;
      else if (strcmp(var.value, "Hummer") == 0)joyadaptertype=USERPORT_JOYSTICK_HUMMER;
      else if (strcmp(var.value, "OEM") == 0)joyadaptertype=USERPORT_JOYSTICK_OEM;
      else if (strcmp(var.value, "Hit") == 0)joyadaptertype=USERPORT_JOYSTICK_HIT;
      else if (strcmp(var.value, "Kingsoft") == 0)joyadaptertype=USERPORT_JOYSTICK_KINGSOFT;
      else if (strcmp(var.value, "Starbyte") == 0)joyadaptertype=USERPORT_JOYSTICK_STARBYTE;

      if(retro_ui_finalized){
           if(joyadaptertype==-1)resources_set_int("UserportJoy", 0);
           else {
             resources_set_int("UserportJoy", 1);
             resources_set_int("UserportJoyType", joyadaptertype);
           }
      } else {
        RETROUSERPORTJOY=joyadaptertype;
      }
   }

   var.key = "vice_joyport";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "port 2") == 0)cur_port=2;
      else if (strcmp(var.value, "port 1") == 0)cur_port=1;
   }

   var.key = "vice_reset";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "autostart") == 0)
         RETRORESET=0;
      if (strcmp(var.value, "soft") == 0)
         RETRORESET=1;
      if (strcmp(var.value, "hard") == 0)
         RETRORESET=2;
   }

   var.key = "vice_mapper_select";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[2] = keyId(var.value);
   }

   var.key = "vice_mapper_start";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[3] = keyId(var.value);
   }

   var.key = "vice_mapper_y";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[1] = keyId(var.value);
   }

   var.key = "vice_mapper_x";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[9] = keyId(var.value);
   }

   var.key = "vice_mapper_b";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[0] = keyId(var.value);
   }

   var.key = "vice_mapper_l";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[10] = keyId(var.value);
   }

   var.key = "vice_mapper_r";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[11] = keyId(var.value);
   }

   var.key = "vice_mapper_l2";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[12] = keyId(var.value);
   }

   var.key = "vice_mapper_r2";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[13] = keyId(var.value);
   }

   var.key = "vice_mapper_l3";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[14] = keyId(var.value);
   }

   var.key = "vice_mapper_r3";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[15] = keyId(var.value);
   }



   var.key = "vice_mapper_lr";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[16] = keyId(var.value);
   }

   var.key = "vice_mapper_ll";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[17] = keyId(var.value);
   }

   var.key = "vice_mapper_ld";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[18] = keyId(var.value);
   }

   var.key = "vice_mapper_lu";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[19] = keyId(var.value);
   }

   var.key = "vice_mapper_rr";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[20] = keyId(var.value);
   }

   var.key = "vice_mapper_rl";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[21] = keyId(var.value);
   }

   var.key = "vice_mapper_rd";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[22] = keyId(var.value);
   }

   var.key = "vice_mapper_ru";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[23] = keyId(var.value);
   }


   var.key = "vice_mapper_vkbd";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[24] = keyId(var.value);
   }

   var.key = "vice_mapper_statusbar";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[25] = keyId(var.value);
   }

   var.key = "vice_mapper_joyport_switch";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[26] = keyId(var.value);
   }

   var.key = "vice_mapper_reset";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[27] = keyId(var.value);
   }

   var.key = "vice_mapper_warp_mode";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      mapper_keys[28] = keyId(var.value);
   }

}


void retro_shutdown_core(void)
{
   environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
}

void emu_init(void)
{
   update_variables();
   pre_main(RPATH);
}

void emu_uninit(void)
{
   vice_main_exit();
}

void emu_reset(void)
{
   extern int RETRORESET;
   
   switch(RETRORESET) {
      case 0:
         autostart_autodetect(RPATH, NULL, 0, AUTOSTART_MODE_RUN);
         break;
      case 1:
         machine_trigger_reset(MACHINE_RESET_MODE_SOFT);
         break;
      case 2:
         machine_trigger_reset(MACHINE_RESET_MODE_HARD);
         break;
   }
}

void retro_reset(void)
{
   microSecCounter = 0;
   emu_reset();
}

struct DiskImage {
    char* fname;
};

#include <attach.h>

static bool retro_set_eject_state(bool ejected) {
    log_cb(RETRO_LOG_INFO, "EJECT %d", (int)ejected);

	if (dc)
	{
		dc->eject_state = ejected;
		
		if(dc->eject_state)
			file_system_detach_disk(8);
		else
			file_system_attach_disk(8, dc->files[dc->index]);		
	}
	
	return true;
}

/* Gets current eject state. The initial state is 'not ejected'. */
static bool retro_get_eject_state(void) {
	if (dc)
		return dc->eject_state;
	
	return true;
}

/* Gets current disk index. First disk is index 0.
 * If return value is >= get_num_images(), no disk is currently inserted.
 */
static unsigned retro_get_image_index(void) {
	if (dc)
		return dc->index;
	
	return 0;
}

/* Sets image index. Can only be called when disk is ejected.
 * The implementation supports setting "no disk" by using an
 * index >= get_num_images().
 */
static bool retro_set_image_index(unsigned index) {
	// Insert disk
	if (dc)
	{
		// Same disk...
		// This can mess things in the emu
		if(index == dc->index)
			return true;
		
		if ((index < dc->count) && (dc->files[index]))
		{
			dc->index = index;
			log_cb(RETRO_LOG_INFO, "Disk (%d) inserted into drive 8: %s\n", dc->index+1, dc->files[dc->index]);
			return true;
		}
	}
	
	return false;
}

/* Gets total number of images which are available to use. */
static unsigned retro_get_num_images(void) {
	if (dc)
		return dc->count;

	return 0;
}


/* Replaces the disk image associated with index.
 * Arguments to pass in info have same requirements as retro_load_game().
 * Virtual disk tray must be ejected when calling this.
 *
 * Replacing a disk image with info = NULL will remove the disk image
 * from the internal list.
 * As a result, calls to get_image_index() can change.
 *
 * E.g. replace_image_index(1, NULL), and previous get_image_index()
 * returned 4 before.
 * Index 1 will be removed, and the new index is 3.
 */
static bool retro_replace_image_index(unsigned index,
      const struct retro_game_info *info) {
	if (dc)
	{
		if(dc->files[index])
		{
			free(dc->files[index]);
			dc->files[index] = NULL;
		}
		
		// TODO : Handling removing of a disk image when info = NULL
		
		if(info != NULL) {
			dc->files[index] = strdup(info->path);
		}
	}
	
    return false;	
}

/* Adds a new valid index (get_num_images()) to the internal disk list.
 * This will increment subsequent return values from get_num_images() by 1.
 * This image index cannot be used until a disk image has been set
 * with replace_image_index. */
static bool retro_add_image_index(void) {
	if (dc)
	{
		if(dc->count <= DC_MAX_SIZE)
		{
			dc->files[dc->count] = NULL;
			dc->count++;
			return true;
		}
	}
	
    return false;
}

static struct retro_disk_control_callback diskControl = {
    retro_set_eject_state,
    retro_get_eject_state,
    retro_get_image_index,
    retro_set_image_index,
    retro_get_num_images,
    retro_replace_image_index,
    retro_add_image_index,
};

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   /* stub */
}

void retro_init(void)
{
   struct retro_log_callback log;

	// Init disk control context
   	dc = dc_create();

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = fallback_log;

   const char *system_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
   {
      // if defined, use the system directory
      retro_system_directory=system_dir;
   }

   const char *content_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir) && content_dir)
   {
      // if defined, use the system directory
      retro_content_directory=content_dir;
   }

   const char *save_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir)
   {
      // If save directory is defined use it, otherwise use system directory
      retro_save_directory = *save_dir ? save_dir : retro_system_directory;
   }
   else
   {
      // make retro_save_directory the same in case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY is not implemented by the frontend
      retro_save_directory=retro_system_directory;
   }

   if(retro_system_directory==NULL)
   {
#if defined(__ANDROID__) || defined(ANDROID)
      strcpy(RETRO_DIR, "/mnt/sdcard");
#elif defined(VITA)
      strcpy(RETRO_DIR, "ux0:/data");
#elif defined(__SWITCH__)
      strcpy(RETRO_DIR, "/");
#else
      strcpy(RETRO_DIR, ".");
#endif
   }
   else
      sprintf(RETRO_DIR, "%s", retro_system_directory);

   /* Use system directory for data files such as C64/*.vpl etc. */
#if defined(__WIN32__)
   sprintf(retro_system_data_directory, "%s\\vice", RETRO_DIR);
#else
   sprintf(retro_system_data_directory, "%s/vice", RETRO_DIR);
#endif

   archdep_mkdir(retro_system_data_directory, 0);

#ifdef FRONTEND_SUPPORTS_RGB565
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
#else
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
#endif

   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      log_cb(RETRO_LOG_ERROR, "PIXEL FORMAT is not supported.\n");
      exit(0);
   }

#define RETRO_DESCRIPTOR_BLOCK( _user )                                            \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },               \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },               \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },               \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },               \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },     \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },       \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },       \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },         \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },             \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },         \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },               \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },               \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2" },             \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2" },             \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3" },             \
   { _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3" },			   \
   { _user, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Left Stick X" },			   \
   { _user, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Left Stick Y" },			   \
   { _user, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "Right Stick X" },			   \
   { _user, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "Right Stick Y" }
   
   struct retro_input_descriptor inputDescriptors[] =
   {
      RETRO_DESCRIPTOR_BLOCK( 0 ),
      RETRO_DESCRIPTOR_BLOCK( 1 ),
      RETRO_DESCRIPTOR_BLOCK( 2 ),
      RETRO_DESCRIPTOR_BLOCK( 3 ),
      RETRO_DESCRIPTOR_BLOCK( 4 ),

      { 0 },
   };

#undef RETRO_DESCRIPTOR_BLOCK

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &inputDescriptors);
   environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &diskControl);
   microSecCounter = 0;
}

void retro_deinit(void)
{
   app_free();
   emu_uninit();
   
	// Clean the disk control context
	if(dc)
		dc_free(dc);
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device( unsigned port, unsigned device )
{
   if ( port < 5 )
      vice_devices[port] = device;
}

void retro_get_system_info(struct retro_system_info *info)
{
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
   memset(info, 0, sizeof(*info));
   info->library_name     = "VICE " CORE_NAME;
   info->library_version  = "3.3" GIT_VERSION;
#if defined(__VIC20__)
   info->valid_extensions = "20|40|60|a0|b0|d64|d71|d80|d81|d82|g64|g41|x64|t64|tap|prg|p00|crt|bin|zip|gz|d6z|d7z|d8z|g6z|g4z|x6z|cmd|m3u";
#else
   info->valid_extensions = "d64|d71|d80|d81|d82|g64|g41|x64|t64|tap|prg|p00|crt|bin|zip|gz|d6z|d7z|d8z|g6z|g4z|x6z|cmd|m3u";
#endif
   info->need_fullpath    = true;
   info->block_extract    = false;

}

void update_geometry()
{
   int border_disabled = 0;
   float no_border_aspect_ratio;
   struct retro_system_av_info system_av_info;
   system_av_info.geometry.base_width = retroW;
   system_av_info.geometry.base_height = retroH;
   /* this code is needed because changing borders on/off causes a reset */
   if(retro_ui_finalized)
#if defined(__VIC20__)
        resources_get_int("VICBorderMode", &border_disabled);
#elif defined(__PLUS4__)
        resources_get_int("TEDBorderMode", &border_disabled);
#else
        resources_get_int("VICIIBorderMode", &border_disabled);
#endif
      else border_disabled = RETROBORDERS;
   if (border_disabled)
     // When borders are disabled, each system has a different aspect ratio.
     // For example, C64 & C128 have 320 / 200 pixel resolution with a 15 / 16
     // pixel aspect ratio leading to a total aspect of 320 / 200 * 15 / 16 = 1.5
#if defined(__VIC20__)
     system_av_info.geometry.aspect_ratio = (float)1.6;
#elif defined(__PLUS4__)
     system_av_info.geometry.aspect_ratio = (float)1.65;
#elif defined(__PET__)
     system_av_info.geometry.aspect_ratio = (float)4.0/3.0;
#else
     system_av_info.geometry.aspect_ratio = (float)1.5;
#endif
   else
     system_av_info.geometry.aspect_ratio = (float)4.0/3.0;
   environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &system_av_info);
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   /* FIXME handle PAL/NTSC */
   struct retro_game_geometry geom = { 320, 240, retrow, retroh, 4.0 / 3.0 };
   struct retro_system_timing timing = { 50.0, 44100.0 };

   info->geometry = geom;
   info->timing   = timing;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

void retro_audio_cb( short l, short r)
{
   audio_cb(l,r);
}

void retro_audiocb(signed short int *sound_buffer,int sndbufsize){
   int x;
   for(x=0;x<sndbufsize;x++)audio_cb(sound_buffer[x],sound_buffer[x]);
}

void retro_blit(void)
{
   memcpy(Retro_Screen,bmp,PITCH*WINDOW_SIZE);
}

void retro_run(void)
{
   static int mfirst=1;
   bool updated = false;

   if(lastW!=retroW || lastH!=retroH){
      update_geometry();
      log_cb(RETRO_LOG_INFO, "Update Geometry Old(%d,%d) New(%d,%d)\n",lastW,lastH,retroW,retroH);
      lastW=retroW;
      lastH=retroH;
      reset_mouse_pos();
   }

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables();

   if(mfirst==1)
   {
      mfirst++;
      log_cb(RETRO_LOG_INFO, "First time we return from retro_run()!\n");
      retro_load_ok=true;
      app_init();
      memset(SNDBUF,0,1024*2*2);

      emu_init();
      return;
   }

   while(cpuloop==1)
      maincpu_mainloop_retro();
   cpuloop=1;

   retro_blit();
   if(SHOWKEY==1)app_render(0);

   video_cb(Retro_Screen,retroW,retroH,retrow<<PIXEL_BYTES);

   microSecCounter += (1000000/50);
}

/*
   unsigned int lastdown,lastup,lastchar;
   static void keyboard_cb(bool down, unsigned keycode,
   {
   <{

   log_cb(RETRO_LOG_INFO, "Down: %s, Code: %d, Char: %u, Mod: %u.\n",
   down ? "yes" : "no", keycode, character, mod);


   if(down)lastdown=keycode;
   else lastup=keycode;
   lastchar=character;

   }
   */

#define M3U_FILE_EXT "m3u"

bool retro_load_game(const struct retro_game_info *info)
{
   /*
      struct retro_keyboard_callback cb = { keyboard_cb };
      environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &cb);
      */

   if (info)
   {
	const char *full_path;

	(void)info;

	full_path = info->path;

	// If it's a m3u file
	if(strendswith(full_path, M3U_FILE_EXT))
	{
		// Parse the m3u file
		dc_parse_m3u(dc, full_path);

		// Some debugging
		log_cb(RETRO_LOG_INFO, "m3u file parsed, %d file(s) found\n", dc->count);
		for(unsigned i = 0; i < dc->count; i++)
		{
			log_cb(RETRO_LOG_INFO, "file %d: %s\n", i+1, dc->files[i]);
		}	
	}
	else
	{
		// Add the file to disk control context
		// Maybe, in a later version of retroarch, we could add disk on the fly (didn't find how to do this)
		dc_add_file(dc, full_path);
	}

	// Init first disk
	dc->index = 0;
	dc->eject_state = false;
	log_cb(RETRO_LOG_INFO, "Disk (%d) inserted into drive 8: %s\n", dc->index+1, dc->files[dc->index]);
	strcpy(RPATH,dc->files[0]);
   }
   else
   {
      RPATH[0]=0;
   }

   update_variables();

   return true;
}

void retro_unload_game(void){
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_PAL;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data_, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   if ( id == RETRO_MEMORY_SYSTEM_RAM )
      return mem_ram;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   if ( id == RETRO_MEMORY_SYSTEM_RAM )
      return g_mem_ram_size;
   return 0;
}

void retro_cheat_reset(void) {}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

