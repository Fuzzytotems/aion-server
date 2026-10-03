#include "aion/gameserver/skillengine/effect/MpAttackEffect.h"

#include <cstdint>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

namespace {

/** Java int a * b (wraps on overflow) */
constexpr int32_t mulInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

} // namespace

// TODO bosses are resistent to this?
void MpAttackEffect::onPeriodicAction(model::Effect& effect) const {
	int32_t maxMP = effect.getEffected()->getLifeStats()->getMaxMp();
	int32_t newValue = value;
	// Support for values in percentage
	if (percent)
		newValue = mulInt(maxMP, value) / 100;
	// sm_attack_status for type and log - 4.5 checked
	effect.getEffected()->getLifeStats()->reduceMp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE::DAMAGE_MP, newValue, effect.getSkillId(),
		network::aion::serverpackets::SM_ATTACK_STATUS_LOG::MPATTACK);
}

} // namespace aion::gameserver::skillengine::effect
