#pragma once

#include "aion/gameserver/skillengine/effect/StumbleEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.StumbleEffect. @author ATracer */
class StumbleEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/StumbleEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	void startEffect(model::Effect& effect) const override;

	using EffectTemplate::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;

	void endEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
