#include "aion/gameserver/skillengine/periodicaction/HpUsePeriodicAction.h"

#include <limits>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::periodicaction {

using gameserver::model::gameobjects::Creature;
using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;
using runtime::Ptr;

namespace {

/** Java `(int) f`: NaN 0, saturating (a C++ cast is undefined out of range) - Skill.cpp's and MotionData.cpp's helper */
int32_t javaFloatToInt(float value) {
	if (value != value)
		return 0;
	if (value >= 2147483648.0f)
		return std::numeric_limits<int32_t>::max();
	if (value <= -2147483648.0f)
		return std::numeric_limits<int32_t>::min();
	return static_cast<int32_t>(value);
}

} // namespace

void HpUsePeriodicAction::act(model::Effect& effect) const {
	Ptr<Creature> effected = effect.getEffected();
	int32_t maxHp = effected->getGameStats()->getMaxHp()->getCurrent();
	// Java: (int) (maxHp * (value / 100f)) - float arithmetic, truncated toward zero, saturating beyond the int range
	int32_t requiredHp = ratio ? javaFloatToInt(static_cast<float>(maxHp) * (static_cast<float>(value) / 100.0f)) : value;
	if (effected->getLifeStats()->getCurrentHp() < requiredHp) {
		effect.endEffect();
		return;
	}
	effected->getLifeStats()->reduceHp(SM_ATTACK_STATUS_TYPE::USED_HP, requiredHp, 0, SM_ATTACK_STATUS_LOG::REGULAR, *effected);
}

} // namespace aion::gameserver::skillengine::periodicaction
