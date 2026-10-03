#include "aion/gameserver/skillengine/effect/DelayedFpAtkInstantEffect.h"

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::player::Player;

// Java DelayedFpAtkInstantEffect.java:22-26
void DelayedFpAtkInstantEffect::calculate(model::Effect& effect) const {
	// Only players have FP
	if (runtime::as<Player>(effect.getEffected()))
		EffectTemplate::calculate(effect, std::nullopt, std::nullopt);
}

// Java DelayedFpAtkInstantEffect.java:29-37
// Stored lambda com.aionemu.gameserver.skillengine.effect.DelayedFpAtkInstantEffect@L30:36 (the delayed FP hit, pin {this, &effect}): a task that
// runs once after `delay` and is then released with its pin; nothing keeps its Future
void DelayedFpAtkInstantEffect::applyEffect(model::Effect& effect) const {
	utils::ThreadPoolManager::getInstance().schedule(runtime::Pin{this, &effect}, [this, &effect] { calculateAndApplyDamage(effect); }, delay);
}

// Java DelayedFpAtkInstantEffect.java:39-50
void DelayedFpAtkInstantEffect::calculateAndApplyDamage(model::Effect& effect) const {
	if (!effect.getEffector()->isEnemy(*effect.getEffected()))
		return;

	int32_t valueWithDelta = calculateBaseValue(effect);
	runtime::Ptr<Player> player = runtime::cast<Player>(effect.getEffected());
	int32_t maxFP = player->getLifeStats()->getMaxFp();
	int32_t newValue = valueWithDelta;
	// Support for values in percentage
	if (percent) // Java int (maxFP * valueWithDelta) / 100: the product wraps, the division truncates toward zero
		newValue = static_cast<int32_t>(static_cast<uint32_t>(maxFP) * static_cast<uint32_t>(valueWithDelta)) / 100;
	player->getLifeStats()->reduceFp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE::FP_DAMAGE, newValue, effect.getSkillId(),
		network::aion::serverpackets::SM_ATTACK_STATUS_LOG::FPATTACK);
}

} // namespace aion::gameserver::skillengine::effect
