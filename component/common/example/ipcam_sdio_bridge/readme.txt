IPCAM SDIO BRIDGE EXAMPLE

Description:
Bridge for ipcam sdio host and amebad sdio device.

Configuration:
1. Modify \project\realtek_amebaD_va0_example\inc\inc_hp\platform_opts.h.
	#define CONFIG_RTS_SDIO_BRIDGE			1//on or off for ipcam app
	
	#if CONFIG_RTS_SDIO_BRIDGE
	#define CONFIG_BRIDGE                   1
	#undef CONFIG_EXAMPLE_WLAN_FAST_CONNECT
	#define CONFIG_EXAMPLE_WLAN_FAST_CONNECT 1
	#endif

2. Modify \component\common\example\example_entry.c
	void example_entry(void)
	{	
	#if defined(CONFIG_RTS_SDIO_BRIDGE) && CONFIG_RTS_SDIO_BRIDGE
		example_ipcam();
	#endif
	}
	
3. Modify \project\realtek_amebaD_va0_example\GCC-RELEASE\project_hp\asdk\make\network\sram\Makefile.
	remove:
		$(LWIPDIR)/port/realtek/freertos/bridgeif.o \
		$(LWIPDIR)/port/realtek/freertos/bridgeif_fdb.o \
	add:
		$(LWIPDIR)/port/realtek/freertos/bridgeif_sdio.o \
		$(LWIPDIR)/port/realtek/freertos/bridgeif_fdb_sdio.o \
4. Modify \project\realtek_amebaD_va0_example\GCC-RELEASE\project_hp\asdk\make\utilities_example\Makefile.
	add:
		CSRC += $(DIR)/ipcam_sdio_bridge/example_ipcam_sdio_bridge.c
		
Note:
1. If define CONFIG_INCLUDE_SIMPLE_CONFIG to 1, please set SC_SOFTAP_EN to 0. Modify \component\common\api\wifi\wifi_simple_config.c.
	#if CONFIG_INIC_EN
	#undef SC_SOFTAP_EN
	#define SC_SOFTAP_EN      0 // disable softAP mode for iNIC applications
	#endif
	
	#if CONFIG_RTS_SDIO_BRIDGE
	#undef SC_SOFTAP_EN
	#define SC_SOFTAP_EN      0 // disable softAP mode for ipcam sdio applications
	#endif
	
2. The Dynamic and/or Private Ports are those from 49152 through 65535
	#define TCP_LOCAL_PORT_RANGE_START        0xc000
	#define TCP_LOCAL_PORT_RANGE_END          0xffff
