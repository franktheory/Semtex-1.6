#pragma once

#include <cstddef>
#include <cstdint>

struct Vector
{
    float x, y, z;

    Vector() : x(0), y(0), z(0) {}
    Vector(float X, float Y, float Z) : x(X), y(Y), z(Z) {}

    bool IsZero() const { return x == 0.f && y == 0.f && z == 0.f; }

    Vector operator+(const Vector& o) const { return { x + o.x, y + o.y, z + o.z }; }
};

struct Vector2D
{
    float x, y;
};

typedef int qboolean;
typedef unsigned char byte;

struct color24
{
    byte r, g, b;
};

#define MAX_SCOREBOARDNAME 32
#define MAX_INFO_STRING 256
#define MAX_CLIENTS 32
#define MAXSTUDIOBONES 128
#define HISTORY_MAX 64
#define HISTORY_MASK (HISTORY_MAX - 1)

struct entity_state_t
{
    int       entityType;
    int       number;
    float     msg_time;
    int       messagenum;
    Vector    origin;
    Vector    angles;
    int       modelindex;
    int       sequence;
    float     frame;
    int       colormap;
    short     skin;
    short     solid;
    int       effects;
    float     scale;
    byte      eflags;
    byte      pad_eflags[3];
    int       rendermode;
    int       renderamt;
    color24   rendercolor;
    byte      pad_rendercolor;
    int       renderfx;
    int       movetype;
    float     animtime;
    float     framerate;
    int       body;
    byte      controller[4];
    byte      blending[4];
    Vector    velocity;
    Vector    mins;
    Vector    maxs;
    int       aiment;
    int       owner;
    float     friction;
    float     gravity;
    int       team;
    int       playerclass;
    int       health;
    qboolean  spectator;
    int       weaponmodel;
    int       gaitsequence;
    Vector    basevelocity;
    int       usehull;
    int       oldbuttons;
    int       onground;
    int       iStepLeft;
    float     flFallVelocity;
    float     fov;
    int       weaponanim;
    Vector    startpos;
    Vector    endpos;
    float     impacttime;
    float     starttime;
    int       iuser1;
    int       iuser2;
    int       iuser3;
    int       iuser4;
    float     fuser1;
    float     fuser2;
    float     fuser3;
    float     fuser4;
    Vector    vuser1;
    Vector    vuser2;
    Vector    vuser3;
    Vector    vuser4;
};

struct mouth_t
{
    byte mouthopen;
    byte sndcount;
    byte pad[2];
    int  sndavg;
};

struct latchedvars_t
{
    float prevanimtime;
    float sequencetime;
    byte  prevseqblending[2];
    byte  pad0[2];
    Vector prevorigin;
    Vector prevangles;
    int   prevsequence;
    float prevframe;
    byte  prevcontroller[4];
    byte  prevblending[2];
    byte  pad1[2];
};

struct position_history_t
{
    float  animtime;
    Vector origin;
    Vector angles;
};

struct model_t;
struct mnode_t;
struct colorVec
{
    int r, g, b, a;
};

struct cl_entity_t
{
    int                  index;
    qboolean             player;
    entity_state_t       baseline;
    entity_state_t       prevstate;
    entity_state_t       curstate;
    int                  current_position;
    position_history_t   ph[HISTORY_MAX];
    mouth_t              mouth;
    latchedvars_t        latched;
    float                lastmove;
    Vector               origin;
    Vector               angles;
    Vector               attachment[4];
    int                  trivial_accept;
    model_t*             model;
    void*                efrag;
    mnode_t*             topnode;
    float                syncbase;
    int                  visframe;
    colorVec             cvFloorColor;
};

struct player_info_t
{
    int  userid;
    char userinfo[MAX_INFO_STRING];
    char name[MAX_SCOREBOARDNAME];
    int  spectator;
    int  ping;
    int  packet_loss;
    char model[64];
    int  topcolor;
    int  bottomcolor;
};


struct extra_player_info_t
{
    int16_t frags;
    int16_t deaths;
    int16_t team_id;
    int32_t has_c4;
    int32_t vip;
    Vector  origin;
    float   radarflash;
    int32_t radarflashon;
    int32_t radarflashes;
    int16_t playerclass;
    int16_t teamnumber;
    char    teamname[16];
    uint8_t dead;
    uint8_t pad3D[3];
    float   showhealth;
    int32_t health;
    char    location[32];
    uint8_t pad68[0x74 - 0x68];
};

struct studiohdr_t
{
    int    id;
    int    version;
    char   name[64];
    int    length;
    Vector eyeposition;
    Vector min;
    Vector max;
    Vector bbmin;
    Vector bbmax;
    int    flags;
    int    numbones;
    int    boneindex;
};

struct mstudiobone_t
{
    char  name[32];
    int   parent;
    int   flags;
    int   bonecontroller[6];
    float value[6];
    float scale[6];
};

struct alight_t
{
    int    ambientlight;
    int    shadelight;
    float  color[3];
    float* plightvec;
};


struct engine_studio_api_t
{
    void*            (*Mem_Calloc)(int number, size_t size);
    void*            (*Cache_Check)(void* c);
    void             (*LoadCacheFile)(char* path, void* cu);
    model_t*         (*Mod_ForName)(const char* name, int crash_if_missing);
    void*            (*Mod_Extradata)(model_t* mod);
    model_t*         (*GetModelByIndex)(int index);
    cl_entity_t*     (*GetCurrentEntity)(void);
    player_info_t*   (*PlayerInfo)(int index);
    entity_state_t*  (*GetPlayerState)(int index);
    cl_entity_t*     (*GetViewEntity)(void);
    void             (*GetTimes)(int* framecount, double* current, double* old);
    void*            (*GetCvar)(char* name);
    void             (*GetViewInfo)(float* origin, float* upv, float* rightv, float* vpnv);
    model_t*         (*GetChromeSprite)(void);
    void             (*GetModelCounters)(int** s, int** a);
    void             (*GetAliasScale)(float* x, float* y);
    float****        (*StudioGetBoneTransform)(void);
    float****        (*StudioGetLightTransform)(void);
    float***         (*StudioGetAliasTransform)(void);
    float***         (*StudioGetRotationMatrix)(void);
    void             (*StudioSetupModel)(int bodypart, void** ppbodypart, void** ppsubmodel);
    int              (*StudioCheckBBox)(void);
    void             (*StudioDynamicLight)(cl_entity_t* ent, alight_t* plight);
    void             (*StudioEntityLight)(alight_t* plight);
    void             (*StudioSetupLighting)(alight_t* plighting);
    void             (*StudioDrawPoints)(void);
    void             (*StudioDrawHulls)(void);
    void             (*StudioDrawAbsBBox)(void);
    void             (*StudioDrawBones)(void);
    void             (*StudioSetupSkin)(studiohdr_t* ptexturehdr, int index);
    void             (*StudioSetRemapColors)(int top, int bottom);
    model_t*         (*SetupPlayerModel)(int index);
    void             (*StudioClientEvents)(void);
    int              (*GetForceFaceFlags)(void);
    void             (*SetForceFaceFlags)(int flags);
    void             (*StudioSetHeader)(void* header);
    void             (*SetRenderModel)(model_t* model);
    void             (*SetupRenderer)(int rendermode);
    void             (*RestoreRenderer)(void);
    void             (*SetChromeOrigin)(void);
};

struct r_studio_interface_t
{
    int version;
    int (*StudioDrawModel)(int flags);
    int (*StudioDrawPlayer)(int flags, entity_state_t* pplayer);
};


using StudioRenderModelFn = void(__fastcall*)(void* self, void* edx);
using StudioRenderFinalFn   = void(__fastcall*)(void* self, void* edx);

struct studio_model_renderer_t
{
    void*                 CStudioModelRenderer;
    void*                 Init;
    int(__fastcall*       StudioDrawModel)(void* self, void* edx, int flags);
    int(__fastcall*       StudioDrawPlayer)(void* self, void* edx, int flags, entity_state_t* pplayer);
    void*                 StudioGetAnim;
    void*                 StudioSetUpTransform;
    void*                 StudioSetupBones;
    void*                 StudioCalcAttachments;
    void*                 StudioSaveBones;
    void*                 StudioMergeBones;
    void*                 StudioEstimateInterpolant;
    void*                 StudioEstimateFrame;
    void*                 StudioFxTransform;
    void*                 StudioSlerpBones;
    void*                 StudioCalcBoneAdj;
    void*                 StudioCalcBoneQuaterion;
    void*                 StudioCalcBonePosition;
    void*                 StudioCalcRotations;
    StudioRenderModelFn   StudioRenderModel;
    StudioRenderFinalFn   StudioRenderFinal;
};


namespace studio_api_off
{
    constexpr std::ptrdiff_t StudioSetRemapColors = 120;
    constexpr std::ptrdiff_t SetupPlayerModel       = 124;
    constexpr std::ptrdiff_t SetForceFaceFlags      = 136;
    constexpr std::ptrdiff_t SetChromeOrigin        = 156;
    constexpr int            StudioRenderModel_idx  = 18;
    constexpr int            StudioRenderFinal_idx  = 19;
}

constexpr int STUDIO_NF_CHROME = 0x0002;

struct client_state_t;

namespace cl_off
{
    constexpr std::ptrdiff_t time          = 176024;

    constexpr std::ptrdiff_t viewangles    = time - 928;

    constexpr std::ptrdiff_t cmd           = viewangles - 52;
    constexpr std::ptrdiff_t punchangle    = viewangles + 12;

    constexpr std::ptrdiff_t onground      = time - 44;
    constexpr std::ptrdiff_t oldtime       = time + 8;
    constexpr std::ptrdiff_t frames        = oldtime + 8;
    constexpr std::ptrdiff_t parsecountmod = 1686628;
    constexpr std::ptrdiff_t playernum     = 1686700;
    constexpr std::ptrdiff_t maxclients    = 1717476;
    constexpr std::ptrdiff_t levelname     = maxclients - 40;
    constexpr std::ptrdiff_t players       = 1720552;
}

namespace frame_off
{
    constexpr std::ptrdiff_t clientdata = 10952;
    constexpr std::ptrdiff_t frame_size = 13672;
}

namespace clientdata_off
{
    constexpr std::ptrdiff_t flags = 24;
}

enum CsTeam
{
    TEAM_UNASSIGNED = 0,
    TEAM_TERRORIST  = 1,
    TEAM_CT         = 2,
    TEAM_SPECTATOR  = 3,
};

enum keydest_t
{
    KEY_GAME    = 0,
    KEY_MESSAGE = 1,
    KEY_MENU    = 2,
};

enum cactive_t
{
    CA_DEDICATED     = 0,
    CA_DISCONNECTED  = 1,
    CA_CONNECTING    = 2,
    CA_CONNECTED     = 3,
    CA_UNINITIALIZED = 4,
    CA_ACTIVE        = 5,
};


struct client_static_t
{
    int state;
};

static_assert(sizeof(entity_state_t) == 340, "entity_state_t layout mismatch");
static_assert(sizeof(cl_entity_t) == 3000, "cl_entity_t layout mismatch");
static_assert(offsetof(cl_entity_t, curstate) == 688, "cl_entity_t::curstate offset mismatch");
static_assert(offsetof(cl_entity_t, origin) == 2888, "cl_entity_t::origin offset mismatch");
static_assert(offsetof(engine_studio_api_t, GetCurrentEntity) == 24, "engine_studio_api_t offset mismatch");
static_assert(offsetof(engine_studio_api_t, StudioGetBoneTransform) == 64, "engine_studio_api_t offset mismatch");
static_assert(offsetof(engine_studio_api_t, StudioSetRemapColors) == 120, "engine_studio_api_t::StudioSetRemapColors offset mismatch");
static_assert(offsetof(engine_studio_api_t, SetupPlayerModel) == 124, "engine_studio_api_t::SetupPlayerModel offset mismatch");
static_assert(offsetof(engine_studio_api_t, SetForceFaceFlags) == 136, "engine_studio_api_t::SetForceFaceFlags offset mismatch");
static_assert(offsetof(engine_studio_api_t, SetChromeOrigin) == 156, "engine_studio_api_t::SetChromeOrigin offset mismatch");
static_assert(offsetof(player_info_t, name) == 260, "player_info_t::name offset mismatch");
static_assert(offsetof(player_info_t, topcolor) == 368, "player_info_t::topcolor offset mismatch");
static_assert(sizeof(extra_player_info_t) == 0x74, "extra_player_info_t size mismatch");
static_assert(offsetof(extra_player_info_t, teamnumber) == 0x2A, "extra_player_info_t::teamnumber offset mismatch");
