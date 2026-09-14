#include "aion/gameserver/controllers/attack/DamageList.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/controllers/attack/TeamDamageList.h"
#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::controllers::attack {

DamageList::DamageList(const std::vector<runtime::Ptr<AggroInfo>>& aggroInfos, model::gameobjects::Creature& owner) {
	AION_UNPORTED();
}

TeamDamageList DamageList::toTeamDamages() {
	AION_UNPORTED();
}

std::vector<DamageInfo> DamageList::getCreatureDamages() {
	AION_UNPORTED();
}

std::optional<DamageInfo> DamageList::getMostDamage() {
	AION_UNPORTED();
}

int32_t DamageList::getTotalDamage() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::attack
