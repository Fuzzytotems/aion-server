#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * This class is responsible for armsfusion related tasks (fusion and breaking, called COMPOUND and DECOMPOUND by the client)
 * <p>
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author Wakizashi, Source, xTz, Neon
 */
class ArmsfusionService {
public:
	static void fusionWeapons(model::gameobjects::player::Player& player, int32_t mainWeaponObjId, int32_t fuseWeaponObjId);
private:
	static int64_t getBasePricePerLevelSquared(model::templates::item::ItemQuality rarity);
public:
	static void breakWeapons(model::gameobjects::player::Player& player, int32_t weaponToBreakUniqueId);
};

} // namespace aion::gameserver::services
