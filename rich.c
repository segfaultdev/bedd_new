#include <discord_rpc.h>
#include <string.h>
#include <stdio.h>
#include <bedd.h>
#include <time.h>

static time_t rich_start;

void rich_init(void) {
  DiscordEventHandlers rich;
  memset(&rich, 0, sizeof(DiscordEventHandlers));
  
  rich.ready = NULL;
  rich.errored = NULL;
  rich.disconnected = NULL;
  rich.joinGame = NULL;
  rich.spectateGame = NULL;
  rich.joinRequest = NULL;
  
  Discord_Initialize("938134000030056449", &rich, 1, "");
  rich_start = time(0);
}

void rich_exit(void) {
  Discord_Shutdown();
}

void rich_tick(void) {
  DiscordRichPresence rich;
  memset(&rich, 0, sizeof(DiscordRichPresence));
  
  char buffer_1[128];
  
  if (bedd_tabs[bedd_active].prog >= 0) {
    sprintf(buffer_1, "running \"%s\"", bedd_tabs[bedd_active].path);
  } else {
    sprintf(buffer_1, "editing file \"%s\"", bedd_tabs[bedd_active].path ? bedd_tabs[bedd_active].path : "no name");
  }
  
  char buffer_2[128];
  sprintf(buffer_2, "%d line%s", bedd_tabs[bedd_active].line_cnt, (bedd_tabs[bedd_active].line_cnt == 1) ? "" : "s");
  
  rich.details = buffer_1;
  rich.state = buffer_2;
  
  rich.startTimestamp = rich_start;
  rich.largeImageKey = "bedd";
  
  Discord_UpdatePresence(&rich);
}
