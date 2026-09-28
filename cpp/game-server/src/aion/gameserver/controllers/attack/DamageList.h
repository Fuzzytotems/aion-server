#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/attack/DamageInfo.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::controllers::attack {

/**
 * List of combined creature damages, grouped by their master (if present, like with Summons and summoned objects).
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). K5 confined value class (fieldmap), returned by value from
 * AggroList::getFinalDamageList. The DamageInfo elements are values; getCreatureDamages returns a copy, getMostDamage std::optional (Java
 * null when empty).
 */
class DamageList {
private:
	std::unordered_map<runtime::Ptr<model::gameobjects::Creature>, DamageInfo> damageByCreature{};

public:
	/** Java package-private (AggroList) */
	DamageList(const std::vector<runtime::Ptr<AggroInfo>>& aggroInfos, model::gameobjects::Creature& owner);

	TeamDamageList toTeamDamages();

	/** Java: Collection<DamageInfo<Creature>> (the live values; a copy) */
	std::vector<DamageInfo> getCreatureDamages();

	std::optional<DamageInfo> getMostDamage();

	int32_t getTotalDamage();
};

} // namespace aion::gameserver::controllers::attack
