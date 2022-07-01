This m4a self parse example is used to play m4a files from the SDCARD. In order to run the example the following steps must be followed.

	1. Set the parameter CONFIG_EXAMPLE_AUDIO_M4A_SELFPARSE to 1 in platform_opts.h file
	
	2. In Cygwin terminal, change to the directory ”project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp”, 
	type ”make menuconfig”, and enable audio related configurations (MENUCONFIG FOR CHIP CONFIG -> Audio Config -> Enable Audio).
	
	3. In the example file "example_audio_m4a_selfparse.c" set the config parameters in the start of the file
	Set mono/stereo and sampling frequency in function initialize_audio(...), for example:
		uint8_t smpl_rate_idx = SR_16K;
		uint8_t ch_num_idx = CH_STEREO;
	The default values of the parameters are pre-set to the values of the sample audio file provided in the folder, in case you are using your own audio file please change the above parameters to the parameters for your audio file else the audio will not be played properly.
	The default m4a file to be played in this example is "AudioSDTest.m4a"(#define FILE_NAME "AudioSDTest.m4a").
	
	4. Build and flash the binary to test

[Supported List]
	Supported :
	    Ameba-D
	Source code not in project:

[Compilation Tips]
	1. You need to turn WI-FI and Network on to make sure the entry of this example will be called automatically. If not, you need to 
	call it on your own.
	
	2. You may need to change the value of configTOTAL_HEAP_SIZE  defined in project\realtek_amebaD_va0_example\inc\inc_hp\FreeRTOSConfig.h 
	to make sure that ram allocated is enough to run this example.
	
	3. This example needs a big memory space, or hardfault may happen. Psram is used in this example, please enable psram use in rtl8721dhp_intfcfg.c(set psram_dev_enable psram_dev_cal_enable psram_dev_retention to TRUE).
	
	4. Since SD card is needed, in component\soc\realtek\amebad\fwlib\usrcfg\rtl8721dhp_intfcfg.c, set both sdioh_cd_pin and sdioh_wp_pin to _PNC
	