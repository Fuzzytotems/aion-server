#include "aion/gameserver/controllers/attack/DamageInfo.h"

#include <cstdint>

#include "aion/gameserver/model/gameobjects/AionObject.h"

namespace aion::gameserver::controllers::attack {

DamageInfo::DamageInfo(model::gameobjects::AionObject& attackerValue) : attacker(attackerValue) {
}

void DamageInfo::addDamage(int32_t value) {
	// Java: this.damage += damage (an int addition, which wraps on overflow)
	damage = static_cast<int32_t>(static_cast<uint32_t>(damage) + static_cast<uint32_t>(value));
}

} // namespace aion::gameserver::controllers::attack
