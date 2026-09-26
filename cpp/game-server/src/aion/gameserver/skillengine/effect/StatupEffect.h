#pragma once

#include "aion/gameserver/skillengine/effect/StatupEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.StatupEffect. @author ATracer */
class StatupEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/StatupEffect.xml.inc"
public:
	void endEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
