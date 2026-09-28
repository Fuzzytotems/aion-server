#pragma once

#include "aion/gameserver/skillengine/effect/StatdownEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.StatdownEffect. @author ATracer */
class StatdownEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/StatdownEffect.xml.inc"
public:
	void startEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
