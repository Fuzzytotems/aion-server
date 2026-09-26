#include "aion/gameserver/skillengine/effect/SkillAtkDrainInstantEffect.h"

#include <cstdint>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::skillengine::effect {

using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;

namespace {

/** Java int a * b (wraps on overflow) */
constexpr int32_t mulInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

} // namespace

void SkillAtkDrainInstantEffect::applyEffect(model::Effect& effect) const {
	DamageEffect::applyEffect(effect);
	// Java: lambda capturing this and effect (fieldmap callback SkillAtkDrainInstantEffect@L28:44), pinned to the template (static data) and the
	// effect until it has run; nothing stores the task or its Future
	utils::ThreadPoolManager::getInstance().schedule({this, &effect}, [this, &effect] {
		if (hpPercent != 0) {
			effect.getEffector()->getLifeStats()->increaseHp(SM_ATTACK_STATUS_TYPE::ABSORBED_HP,
				mulInt(effect.getReserveds(position)->getValue(), hpPercent) / 100, effect, SM_ATTACK_STATUS_LOG::SKILLLATKDRAININSTANT);
		}
		if (mpPercent != 0) {
			effect.getEffector()->getLifeStats()->increaseMp(SM_ATTACK_STATUS_TYPE::MP, mulInt(effect.getReserveds(position)->getValue(), mpPercent) / 100,
				effect.getSkillId(), SM_ATTACK_STATUS_LOG::SKILLLATKDRAININSTANT);
		}
	}, 1000); // on retail the effect is applied about 1sec later (maybe based on animationTime/hitTime?)
}

} // namespace aion::gameserver::skillengine::effect
