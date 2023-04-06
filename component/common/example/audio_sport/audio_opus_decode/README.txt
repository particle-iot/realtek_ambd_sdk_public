This opus decode example is used to play opus files from the flash. In order to run the example the following steps must be followed:

    1. Set the parameter CONFIG_EXAMPLE_AUDIO_OPUS_DECODE to 1 in project/realtek_amebaD_va0_example/inc/inc_hp/platform_opts.h file
       
    2. In Cygwin terminal, change to the directory ”project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp”, 
       type ”make menuconfig”, and enable audio related configurations (MENUCONFIG FOR CHIP CONFIG -> Audio Config -> Enable Audio).
       
    3. The file to be played is "ready_to_convert0.h", which containes 1000Hz audio data in an ogg container.
       
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
	
	3. Psram is needed to run this example. Enable psram use in rtl8721dhp_intfcfg.c.

	3. In IAR, you need to include lib_opus.a lib_libogg.a lib_opusfile.a under "km4_application/lib", example_audio_opus_decode.c and Psram_realloc.c files in this folder under "km4_application/utilities/example".