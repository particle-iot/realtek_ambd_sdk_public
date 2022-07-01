This ac3 example is used to play ac3 files from the SD card. In order to run the example the following steps must be followed.

	1. Set the parameter CONFIG_EXAMPLE_AUDIO_AC3 to 1 in platform_opts.h file
	
	2. In Cygwin terminal, change to the directory ”project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp”, 
	type ”make menuconfig”, and enable audio related configurations (MENUCONFIG FOR CHIP CONFIG -> Audio Config -> Enable Audio).

	3. Parameters should be configured in component\common\example\audio_sport\audio_ac3\a52dec.c
	You can change the content of the array named "audio_files" for you own audio data.
	The default values of the parameters are pre-set to the values of the sample audio file provided in the folder, in case you are using your own audio file please change the above parameters to the parameters for your audio file else the audio will not be played properly.

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
	
	3. In IAR, you need to include lib_ac3.a under "km4_application/lib", example_audio_ac3.c and a52dec.c in this folder under "km4_application/utilities/example"