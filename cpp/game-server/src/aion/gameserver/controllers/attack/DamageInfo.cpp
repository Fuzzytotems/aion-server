#include "aion/gameserver/controllers/attack/DamageInfo.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"

namespace aion::gameserver::controllers::attack {

DamageInfo::DamageInfo(model::gameobjects::AionObject& attackerValue) : attacker(attackerValue) {
}

void DamageInfo::addDamage(int32_t value) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::attack
