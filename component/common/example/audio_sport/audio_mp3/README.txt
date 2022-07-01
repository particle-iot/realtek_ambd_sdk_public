This mp3 example is used to play mp3 files from the SDCARD. In order to run the example the following steps must be followed.

	1. Set the parameter CONFIG_EXAMPLE_AUDIO_MP3 to 1 in platform_opts.h file
	
	2. In Cygwin terminal, change to the directory ”project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp”, 
	type ”make menuconfig”, and enable audio related configurations (MENUCONFIG FOR CHIP CONFIG -> Audio Config -> Enable Audio).
	
	3. In the example file "example_audio_mp3.c" and "example_audio_mp3.h", set the config parameters in the start of the file

		-->MP3_DECODE_SIZE :- Should be set to the value of decoded bytes depending upon the frequency of the mp3.
		-->NUM_CHANNELS :- Should be set to CH_MONO if number of channels in mp3 file is 1 and CH_STEREO if its 2.
		-->SAMPLING_FREQ:- Check the properties of the mp3 file to determine the sampling frequency and choose the appropriate macro.
		-->FILE_NAME:- The mp3 file to be played. Here in this example is "AudioSDTest.mp3"
		
		The default values of the parameters are pre-set to the values of the sample audio file provided in the folder, in case you are using your own audio file please change the above parameters to the parameters for your audio file else the audio will not be played properly.

	4. Build and flash the binary to test

[Supported List]
	Supported :
	    Ameba-D
	Source code not in project:

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
	
	4. In IAR, you need to include lib_mp3.a under "km4_application/lib" and example_audio_mp3.c file in this folder under "km4_application/utilities/example".
	
	5. Since SD card is needed, in component\soc\realtek\amebad\fwlib\usrcfg\rtl8721dhp_intfcfg.c, set both sdioh_cd_pin and sdioh_wp_pin to _PNC