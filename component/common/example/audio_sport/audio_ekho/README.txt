This amr example is used to run ekho, which is a TTS application. In order to run the example the following steps must be followed.

	1. Set the parameter CONFIG_EXAMPLE_EKHO to 1 in platform_opts.h file
	   
	2. In Cygwin terminal, change to the directory ”project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp”, 
	type ”make menuconfig”, and enable audio related configurations (MENUCONFIG FOR CHIP CONFIG -> Audio Config -> Enable Audio).
	
	3. change the content of variable "p_original_text" in function "void audio_play_ekho(void)" according to your need.
	
	3. Build and flash the binary to test

[Supported List]
	Supported :
	    Ameba-D
	Source code not in project:
	    Ameba-1, Ameba-z, Ameba-pro

[Compilation Tips]
	1. You need to turn WI-FI and Network on to make sure the entry of this example will be called automatically. If not, you need to 
	call it on your own.
	
	2. You may need to change the value of configTOTAL_HEAP_SIZE  defined in project\realtek_amebaD_va0_example\inc\inc_hp\FreeRTOSConfig.h 
	to make sure that ram allocated is enough to run this example.
	
	3. In IAR, you need to include lib_ekho.a and lib_gsm610.a under "km4_application/lib" and example_audio_ekho.c file in this folder under "km4_application/utilities/example".