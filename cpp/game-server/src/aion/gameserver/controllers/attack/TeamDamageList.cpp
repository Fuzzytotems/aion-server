#include "aion/gameserver/controllers/attack/TeamDamageList.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/attack/DamageList.h"

namespace aion::gameserver::controllers::attack {

// callbacks: the compute/computeIfAbsent lambdas of the Java constructor run during the call
TeamDamageList::TeamDamageList(DamageList& damageList) {
	AION_UNPORTED();
}

std::vector<DamageInfo> TeamDamageList::getCreatureOrTeamDamages() {
	AION_UNPORTED();
}

std::optional<DamageInfo> TeamDamageList::getMostDamage() {
	AION_UNPORTED();
}

std::optional<DamageInfo> TeamDamageList::getMostDamageByTeam(model::team::TemporaryPlayerTeam& team) {
	AION_UNPORTED();
}

int32_t TeamDamageList::getTotalDamage() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::attack
