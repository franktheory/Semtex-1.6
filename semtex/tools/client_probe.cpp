#include <cstdio>
#include <cstddef>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define OXWARE_HLSK "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk"
#define OXWARE_PUB  "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/public"

#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/public/vector.h"
using Vector = vector_3d<float>;

namespace hl {
#include OXWARE_HLSK "/tier0/basetypes.h"
#include OXWARE_HLSK "/common/goldsrclimits.h"
#include OXWARE_HLSK "/common/protocol.h"
#include OXWARE_HLSK "/common/Color.h"
#include OXWARE_HLSK "/engine/keydefs.h"
#include OXWARE_HLSK "/engine/engine_math.h"
#include OXWARE_HLSK "/engine/wrect.h"
#include OXWARE_HLSK "/engine/const.h"
#include OXWARE_HLSK "/engine/usercmd.h"
#include OXWARE_HLSK "/engine/cvardef.h"
#include OXWARE_HLSK "/engine/ref_params.h"
#include OXWARE_HLSK "/engine/custom.h"
#include OXWARE_HLSK "/engine/customentity.h"
#include OXWARE_HLSK "/engine/entity_types.h"
#include OXWARE_HLSK "/engine/entity_state.h"
#include OXWARE_HLSK "/engine/cl_entity.h"
#include OXWARE_HLSK "/engine/dlight.h"
#include OXWARE_HLSK "/engine/event_state.h"
#include OXWARE_HLSK "/engine/player_info.h"
#include OXWARE_HLSK "/engine/client.h"
}

int main()
{
    using namespace hl;
    std::printf("client_state sizeof %zu\n", sizeof(client_state_t));
    std::printf("time %zu\n", offsetof(client_state_t, time));
    std::printf("playernum %zu\n", offsetof(client_state_t, playernum));
    std::printf("maxclients %zu\n", offsetof(client_state_t, maxclients));
    std::printf("players %zu\n", offsetof(client_state_t, players));
    std::printf("player_info_t sizeof %zu name %zu topcolor %zu\n",
        sizeof(player_info_t), offsetof(player_info_t, name), offsetof(player_info_t, topcolor));
    std::printf("cl_entity_t sizeof %zu origin %zu curstate %zu\n",
        sizeof(cl_entity_t), offsetof(cl_entity_t, origin), offsetof(cl_entity_t, curstate));
    return 0;
}
