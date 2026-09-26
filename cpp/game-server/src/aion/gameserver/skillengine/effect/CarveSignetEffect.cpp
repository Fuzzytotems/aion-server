#include "aion/gameserver/skillengine/effect/CarveSignetEffect.h"

#include <algorithm>
#include <cstdint>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

namespace {

/** Java int a + b (wraps on overflow) */
constexpr int32_t addInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

} // namespace

void CarveSignetEffect::applyEffect(model::Effect& effect) const {
	DamageEffect::applyEffect(effect);

	if (commons::utils::Rnd::chance() >= static_cast<float>(prob))
		return;

	int32_t nextSignetLevel = signetIncrement;
	runtime::Ptr<model::Effect> activeSignet = effect.getEffected()->getEffectController()->getAbnormalEffect(signet);
	if (activeSignet) {
		activeSignet->endEffect();
		nextSignetLevel =
			std::min(addInt(activeSignet->getCarvedSignet(), signetIncrement), std::max(signetCap, activeSignet->getCarvedSignet()));
	}
	runtime::Ref<model::Effect> signetEffect =
		SkillEngine::getInstance().applyEffect(addInt(addInt(signetId, nextSignetLevel), -1), *effect.getEffector(), *effect.getEffected());
	signetEffect->setCarvedSignet(nextSignetLevel); // Java: a skill id without a template gives a null effect, a NullPointerException here
}

} // namespace aion::gameserver::skillengine::effect
