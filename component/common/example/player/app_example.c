/******************************************************************************
*
* Copyright(c) 2007 - 2018 Realtek Corporation. All rights reserved.
*
******************************************************************************/
#include "ameba_soc.h"
#include "FreeRTOS.h"

#include "example_player.h"

#define CMD_TEST

#ifdef CMD_TEST
extern example_player_thread(u16 argc, u8 *argv[]);

CMD_TABLE_DATA_SECTION
const COMMAND_TABLE cutils_test_cmd_table[] = {
	{
		(const u8 *)"player",  1, example_player_thread, (const u8 *)"\t player\n"
	},
};
#else
void app_example(void)
{
	example_player();
}
#endif
