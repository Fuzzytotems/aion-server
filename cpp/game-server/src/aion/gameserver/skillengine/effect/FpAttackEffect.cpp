#include "aion/gameserver/skillengine/effect/FpAttackEffect.h"

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::player::Player;

void FpAttackEffect::calculate(model::Effect& effect) const {
	// Only players have FP
	if (runtime::as<Player>(effect.getEffected()))
		AbstractOverTimeEffect::calculate(effect, std::nullopt, std::nullopt);
}

void FpAttackEffect::onPeriodicAction(model::Effect& effect) const {
	runtime::Ptr<Player> effected = runtime::cast<Player>(effect.getEffected());
	int32_t maxFP = effected->getLifeStats()->getMaxFp();
	int32_t newValue = value;
	// Support for values in percentage
	if (percent) // Java int (maxFP * value) / 100: the product wraps, the division truncates toward zero
		newValue = static_cast<int32_t>(static_cast<uint32_t>(maxFP) * static_cast<uint32_t>(value)) / 100;
	effected->getLifeStats()->reduceFp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE::FP_DAMAGE, newValue, effect.getSkillId(),
		network::aion::serverpackets::SM_ATTACK_STATUS_LOG::FPATTACK);
}

} // namespace aion::gameserver::skillengine::effect
