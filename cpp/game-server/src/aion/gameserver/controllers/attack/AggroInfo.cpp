#include "aion/gameserver/controllers/attack/AggroInfo.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::controllers::attack {

AggroInfo::AggroInfo(model::gameobjects::Creature& attackerValue) : attacker(attackerValue) {
}

AggroInfo::~AggroInfo() = default;

runtime::Ref<AggroInfo> AggroInfo::create(model::gameobjects::Creature& attackerValue) {
	return runtime::makeRef<AggroInfo>(attackerValue);
}

void AggroInfo::addDamage(int32_t value) {
	// java-race: unsynchronized read-modify-write of the damage, as in Java (callers hold no common lock)
	if (value > 0)
		this->damage = damage.get() + value;
}

void AggroInfo::addHate(int32_t value) {
	// java-race: unsynchronized read-modify-write of the hate, as in Java
	this->hate = hate.get() + value;
	if (this->hate.get() < 1)
		this->hate = 1;
	lastInteractionTime = commons::utils::currentTimeMillis();
	hateReduceCount = 1;
}

void AggroInfo::reduceHate() {
	// java-race: unsynchronized read-modify-write of the hate (the hate reduction task runs beside addHate), as in Java
	if (hate.get() > 1) {
		hate = hate.get() - HATE_REDUCE_VALUE * hateReduceCount.get();
		hateReduceCount = hateReduceCount.get() + 1;
		if (hate.get() < 1)
			hate = 1;
	}
}

} // namespace aion::gameserver::controllers::attack
