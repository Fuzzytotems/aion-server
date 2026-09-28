#pragma once

#include "aion/gameserver/skillengine/effect/PolymorphEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.PolymorphEffect. @author ATracer, Cheatkiller */
class PolymorphEffect : public ::aion::gameserver::skillengine::effect::TransformEffect {
#include "aion/gameserver/skillengine/effect/PolymorphEffect.xml.inc"
public:
	void startEffect(model::Effect& effect) const override;

	void endEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
