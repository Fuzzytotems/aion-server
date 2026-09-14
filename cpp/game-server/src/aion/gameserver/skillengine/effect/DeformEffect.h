#pragma once

#include "aion/gameserver/skillengine/effect/DeformEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DeformEffect. @author ATracer */
class DeformEffect : public ::aion::gameserver::skillengine::effect::TransformEffect {
#include "aion/gameserver/skillengine/effect/DeformEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
