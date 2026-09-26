#pragma once

#include "aion/gameserver/skillengine/effect/ReturnPointEffect.xml.h"

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ReturnPointEffect. @author ATracer */
class ReturnPointEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/ReturnPointEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	using EffectTemplate::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;

private:
	/** Java private final */
	int32_t getTargetObjectId(gameserver::model::gameobjects::player::Player& player) const;
};

} // namespace aion::gameserver::skillengine::effect
