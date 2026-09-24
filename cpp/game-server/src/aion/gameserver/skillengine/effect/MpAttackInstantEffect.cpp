#include "aion/gameserver/skillengine/effect/MpAttackInstantEffect.h"

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"

namespace aion::gameserver::skillengine::effect {

void MpAttackInstantEffect::calculate(model::Effect& effect) const {
	int32_t maxMP = effect.getEffected()->getLifeStats()->getMaxMp();
	int32_t newValue = value;
	// Support for values in percentage
	if (percent) // Java int (maxMP * value) / 100: the product wraps, the division truncates toward zero
		newValue = static_cast<int32_t>(static_cast<uint32_t>(maxMP) * static_cast<uint32_t>(value)) / 100;

	effect.setReserveds(*model::EffectReserved::create(position, newValue, model::EffectReserved::ResourceType::MP, true), false);

	// Java `this.calculate(effect, null, null, element)`: the result is not read (the reserve above is set whether the effect lands or not)
	EffectTemplate::calculate(effect, std::nullopt, std::nullopt, element);
}

void MpAttackInstantEffect::applyEffect(model::Effect& effect) const {
	effect.getEffected()->getLifeStats()->reduceMp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE::DAMAGE_MP,
		effect.getReserveds(position)->getValue(), effect.getSkillId(), network::aion::serverpackets::SM_ATTACK_STATUS_LOG::MPATTACK);
}

} // namespace aion::gameserver::skillengine::effect
