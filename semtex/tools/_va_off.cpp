#include <cstdio>
#include <cstddef>
#define Vector vec3_t
namespace hl {
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/keydefs.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/common/goldsrclimits.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/entity_state.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/client.h"
}
int main() {
  using namespace hl;
  printf("viewangles %zu cmd %zu time %zu playernum %zu\n",
    offsetof(client_state_t, viewangles), offsetof(client_state_t, cmd),
    offsetof(client_state_t, time), offsetof(client_state_t, playernum));
  printf("sizeof usercmd %zu\n", sizeof(usercmd_t));
  return 0;
}
