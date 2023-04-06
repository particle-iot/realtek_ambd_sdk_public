#ifdef _EXAMPLE_PLAYER_H_
#define _EXAMPLE_PLAYER_H_

void example_player(void);

#define CMD_TEST

#ifdef CMD_TEST
#include "platform_stdlib.h"

u32 example_player_thread(u16 argc, u8 *argv[]);
#endif

#endif
