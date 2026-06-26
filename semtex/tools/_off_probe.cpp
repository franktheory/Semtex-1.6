#include <cstdio>
#include <cstddef>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define OXWARE_HLSK "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk"
#define OXWARE_PUB  "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/public"

#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/public/vector.h"
using Vector = vector_3d<float>;

#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/tier0/basetypes.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/common/goldsrclimits.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/common/protocol.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/common/Color.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/keydefs.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/engine_math.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/wrect.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/const.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/usercmd.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/cvardef.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/ref_params.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/custom.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/customentity.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/entity_types.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/entity_state.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/cl_entity.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/dlight.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/event_state.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/player_info.h"
#include "C:/Users/caden/Downloads/cs1.6reference/oxware-main/oxware-main/src/external/hlsdk/engine/client.h"

int main()
{
    std::printf("time %zu\n", offsetof(hl::client_state_t, time));
    std::printf("onground %zu\n", offsetof(hl::client_state_t, onground));
    std::printf("parsecountmod %zu\n", offsetof(hl::client_state_t, parsecountmod));
    std::printf("frames %zu\n", offsetof(hl::client_state_t, frames));
    std::printf("playernum %zu\n", offsetof(hl::client_state_t, playernum));
    std::printf("frame clientdata %zu flags %zu frame_size %zu\n",
        offsetof(hl::frame_t, clientdata), offsetof(hl::clientdata_t, flags), sizeof(hl::frame_t));
    return 0;
}
