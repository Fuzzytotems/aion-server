#include "aion/gameserver/skillengine/effect/FpAttackInstantEffect.h"

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::player::Player;

void FpAttackInstantEffect::calculate(model::Effect& effect) const {
	// Only players have FP
	if (runtime::Ptr<Player> player = runtime::as<Player>(effect.getEffected())) {
		int32_t maxFP = player->getLifeStats()->getMaxFp();
		int32_t newValue = value;
		// Support for values in percentage
		if (percent) // Java int (maxFP * value) / 100: the product wraps, the division truncates toward zero
			newValue = static_cast<int32_t>(static_cast<uint32_t>(maxFP) * static_cast<uint32_t>(value)) / 100;

		effect.setReserveds(*model::EffectReserved::create(position, newValue, model::EffectReserved::ResourceType::FP, true), false);

		EffectTemplate::calculate(effect, std::nullopt, std::nullopt);
	}
}

void FpAttackInstantEffect::applyEffect(model::Effect& effect) const {
	// Restriction to players because lack of FP on other Creatures
	runtime::Ptr<Player> player = runtime::as<Player>(effect.getEffected());
	if (!player)
		return;
	player->getLifeStats()->reduceFp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE::FP_DAMAGE, effect.getReserveds(position)->getValue(),
		effect.getSkillId(), network::aion::serverpackets::SM_ATTACK_STATUS_LOG::FPATTACK);
}

} // namespace aion::gameserver::skillengine::effect
