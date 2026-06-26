#pragma once

namespace weapons
{
    enum class kind
    {
        unknown = 0,
        gun,
        knife,
        grenade,
        c4,
    };

    kind local_kind();
    bool can_rage_shoot();
}
