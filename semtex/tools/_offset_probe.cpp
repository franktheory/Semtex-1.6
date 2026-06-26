#include <cstdio>
#include <cstddef>
#define Vector vec3_t
namespace hl {
#include \"C:\Users\caden\Downloads\cs1.6reference\oxware-main\oxware-main\src\external\hlsdk\engine\keydefs.h\"
#include \"C:\Users\caden\Downloads\cs1.6reference\oxware-main\oxware-main\src\external\hlsdk\common\goldsrclimits.h\"
#include \"C:\Users\caden\Downloads\cs1.6reference\oxware-main\oxware-main\src\external\hlsdk\engine\entity_state.h\"
#include \"C:\Users\caden\Downloads\cs1.6reference\oxware-main\oxware-main\src\external\hlsdk\engine\client.h\"
}
int main() {
  using namespace hl;
  printf(\"playernum %zu maxclients %zu levelname %zu signon %zu state %zu connect_time %zu\n\",
    offsetof(client_state_t, playernum), offsetof(client_state_t, maxclients),
    offsetof(client_state_t, levelname), offsetof(client_static_t, signon),
    offsetof(client_static_t, state), offsetof(client_static_t, connect_time));
  return 0;
}
