#include "aion/gameserver/controllers/attack/AggroInfo.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::controllers::attack {

AggroInfo::AggroInfo(model::gameobjects::Creature& attackerValue) : attacker(attackerValue) {
}

AggroInfo::~AggroInfo() = default;

runtime::Ref<AggroInfo> AggroInfo::create(model::gameobjects::Creature& attackerValue) {
	return runtime::makeRef<AggroInfo>(attackerValue);
}

void AggroInfo::addDamage(int32_t value) {
	AION_UNPORTED();
}

void AggroInfo::addHate(int32_t value) {
	AION_UNPORTED();
}

void AggroInfo::reduceHate() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::attack
