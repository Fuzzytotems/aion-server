#pragma once

#include "aion/gameserver/skillengine/effect/SnareEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SnareEffect. @author ATracer */
class SnareEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/SnareEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	using BufEffect::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;

	void endEffect(model::Effect& effect) const override;

	void startEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
