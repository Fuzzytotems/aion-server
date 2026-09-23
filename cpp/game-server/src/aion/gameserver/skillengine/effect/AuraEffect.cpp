#include "aion/gameserver/skillengine/effect/AuraEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void AuraEffect::applyEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void AuraEffect::onPeriodicAction(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void AuraEffect::applyAuraTo(gameserver::model::gameobjects::Creature& /*effected*/) const {
	AION_UNPORTED();
}

void AuraEffect::startEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void AuraEffect::endEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
