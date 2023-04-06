This opus encode example is used to encode PCM data to opus data with ogg container. In order to run the example the following steps must be followed.

	1. Set the parameter CONFIG_EXAMPLE_AUDIO_OPUS_ENCODE to 1 in platform_opts.h file
	
    2. In Cygwin terminal, change to the directory ”project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp”, 
       type ”make menuconfig”, and enable audio related configurations (MENUCONFIG FOR CHIP CONFIG -> Audio Config -> Enable Audio).
	
	3. In function opus_audio_opus_encode_thread(...), you can change source_file and dest_file to play your audio files.
	
    4. Build and flash the binary to test

[Supported List]
	Supported :
	    Ameba-D
	Source code not in project:

[Compilation Tips]
	1. You need to turn WI-FI and Network on to make sure the entry of this example will be called automatically. If not, you need to 
	call it on your own.
	
	2. You may need to change the value of configTOTAL_HEAP_SIZE defined in project\realtek_amebaD_va0_example\inc\inc_hp\FreeRTOSConfig.h 
	to make sure that ram allocated is enough to run this example.
	
	3. Enable psram use in rtl8721dhp_intfcfg.c.
	
	4. In IAR, you need to include lib_opus.a lib_libogg.a lib_opusfile.a lib_opusenc.a under "km4_application/lib" and audio_opus_encode.c file in this folder under "km4_application/utilities/example".