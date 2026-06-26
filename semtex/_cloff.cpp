#include <cstdio>
#include <cstddef>
#define Vector vec3_t
namespace hl {
#include "C:\Users\caden\Downloads\cs1.6reference\oxware-main\oxware-main\src\external\hlsdk/engine/keydefs.h"
#include "C:\Users\caden\Downloads\cs1.6reference\oxware-main\oxware-main\src\external\hlsdk/common/goldsrclimits.h"
#include "C:\Users\caden\Downloads\cs1.6reference\oxware-main\oxware-main\src\external\hlsdk/engine/entity_state.h"
#include "C:\Users\caden\Downloads\cs1.6reference\oxware-main\oxware-main\src\external\hlsdk/engine/client.h"
}
int main() {
  using namespace hl;
  printf("time %zu onground %zu parsecountmod %zu frames %zu\n",
    offsetof(client_state_t, time), offsetof(client_state_t, onground),
    offsetof(client_state_t, parsecountmod), offsetof(client_state_t, frames));
  printf("frame clientdata %zu flags %zu frame_size %zu\n",
    offsetof(frame_t, clientdata), offsetof(clientdata_t, flags), sizeof(frame_t));
  return 0;
}
