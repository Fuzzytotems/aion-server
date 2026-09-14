#pragma once

#include "aion/gameserver/skillengine/effect/ResurrectBaseEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ResurrectBaseEffect. */
class ResurrectBaseEffect : public ::aion::gameserver::skillengine::effect::ResurrectEffect {
#include "aion/gameserver/skillengine/effect/ResurrectBaseEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
