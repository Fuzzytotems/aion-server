#pragma once

#include "aion/gameserver/skillengine/effect/DispelBuffEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DispelBuffEffect. @author ATracer */
class DispelBuffEffect : public ::aion::gameserver::skillengine::effect::AbstractDispelEffect {
#include "aion/gameserver/skillengine/effect/DispelBuffEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
