This flac example is used to play flac files from the SDCARD. In order to run the example the following steps must be followed.

	1. Set the parameter CONFIG_EXAMPLE_AUDIO_FLAC to 1 in platform_opts.h file

	2. In Cygwin terminal, change to the directory ”project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp”, 
	type ”make menuconfig”, and enable audio related configurations (MENUCONFIG FOR CHIP CONFIG -> Audio Config -> Enable Audio).
	
	3. In the example file "example_audio_flac.c", you can change the content of the array named "files" for your own audio data.
	The default values of the parameters are pre-set to the values of the sample audio file provided in the folder, in case you are using your own audio file please change the above parameters to the parameters for your audio file else the audio will not be played properly.

	4. Build and flash the binary to test

[Supported List]
	Supported :
	    Ameba-D
	Source code not in project:
	    Ameba-1, Ameba-z, Ameba-pro

[Compilation Tips]
	1. You need to turn WI-FI and Network on to make sure the entry of this example will be called automatically. If not, you need to 
	call it on your own.
	
	2. When meeting "BD_SRAM overflow" problem in compilation, you may need to change the value of configTOTAL_HEAP_SIZE  defined in project\realtek_amebaD_va0_example\inc\inc_hp\FreeRTOSConfig.h 
	to make sure that ram allocated is enough to run this example.
	
	3. You can chooose to place audio buffer in sram(use malloc) or psram(use Psram_reserve_malloc). If you use sram, use this configuration:
	//#define MEM_ALLOC Psram_reserve_malloc
	#define MEM_ALLOC malloc
	//#define MEM_FREE Psram_reserve_free
	#define MEM_FREE free
	
	if you use psram, use this configuration:
	#define MEM_ALLOC Psram_reserve_malloc
	//#define MEM_ALLOC malloc
	#define MEM_FREE Psram_reserve_free
	//#define MEM_FREE free
	
	And please enable psram use in rtl8721dhp_intfcfg.c(set psram_dev_enable psram_dev_cal_enable psram_dev_retention to TRUE).
	
	4. Since SD card is needed, in component\soc\realtek\amebad\fwlib\usrcfg\rtl8721dhp_intfcfg.c, set both sdioh_cd_pin and sdioh_wp_pin to _PNC