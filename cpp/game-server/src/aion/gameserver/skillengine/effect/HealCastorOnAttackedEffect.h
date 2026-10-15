#pragma once

#include "aion/gameserver/skillengine/effect/HealCastorOnAttackedEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.HealCastorOnAttackedEffect. */
class HealCastorOnAttackedEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/HealCastorOnAttackedEffect.xml.inc"
public:
	friend struct HealCastorOnAttackedEffect_ActionObserver; // C++ only: HealCastorOnAttackedEffect$1 (HealCastorOnAttackedEffect.cpp) reads the
	                                                       // protected range like Java's inner class

	void applyEffect(model::Effect& effect) const override;

	void startEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
