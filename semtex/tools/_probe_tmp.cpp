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
  printf("entity_state %zu\n", sizeof(entity_state_t));
  printf("clientdata %zu\n", sizeof(clientdata_t));
  printf("local_state %zu\n", sizeof(local_state_t));
  printf("client.punch %zu\n", offsetof(local_state_t, client) + offsetof(clientdata_t, punchangle));
  printf("client.viewangles? cmd in cl %zu\n", offsetof(client_state_t, cmd));
  printf("cl.punch %zu\n", offsetof(client_state_t, punchangle));
  printf("usercmd %zu viewangles %zu\n", sizeof(usercmd_t), offsetof(usercmd_t, viewangles));
  return 0;
}
