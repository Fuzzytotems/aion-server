#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/attack/DamageInfo.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/team/fwd.h"

namespace aion::gameserver::controllers::attack {

/**
 * List of combined creature damages, grouped by the team they belong to (if present).
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5): the return type of DamageList::toTeamDamages. K5 confined value class
 * (fieldmap); DamageInfo values, std::optional for Java's null results.
 */
class TeamDamageList {
private:
	std::unordered_map<runtime::Ptr<model::gameobjects::AionObject>, DamageInfo> damageByCreatureOrTeam{};
	std::unordered_map<runtime::Ptr<model::team::TemporaryPlayerTeam>, DamageInfo> mostDamageByTeam{};

public:
	/** Java package-private (DamageList) */
	explicit TeamDamageList(DamageList& damageList);

	/** Java: Collection<DamageInfo<AionObject>> (the live values; a copy) */
	std::vector<DamageInfo> getCreatureOrTeamDamages();

	std::optional<DamageInfo> getMostDamage();

	std::optional<DamageInfo> getMostDamageByTeam(model::team::TemporaryPlayerTeam& team);

	int32_t getTotalDamage();
};

} // namespace aion::gameserver::controllers::attack
