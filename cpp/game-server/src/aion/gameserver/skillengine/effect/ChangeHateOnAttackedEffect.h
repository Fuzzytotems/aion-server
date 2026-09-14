#pragma once

#include "aion/gameserver/skillengine/effect/ChangeHateOnAttackedEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ChangeHateOnAttackedEffect. @author Sippolo */
class ChangeHateOnAttackedEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/ChangeHateOnAttackedEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
