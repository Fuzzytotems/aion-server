#pragma once

#include "aion/gameserver/skillengine/effect/ResurrectBaseEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ResurrectBaseEffect. */
class ResurrectBaseEffect : public ::aion::gameserver::skillengine::effect::ResurrectEffect {
#include "aion/gameserver/skillengine/effect/ResurrectBaseEffect.xml.inc"
public:
	using ResurrectEffect::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;

	void applyEffect(model::Effect& effect) const override;

	void endEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
