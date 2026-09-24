#pragma once

#include "aion/gameserver/skillengine/effect/HideEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.HideEffect. @author Sweetkr, Cura */
class HideEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/HideEffect.xml.inc"
public:
	friend struct HideEffect_ActionObserver; // C++ only: HideEffect$1 (HideEffect.cpp) reads the protected buffCount like Java's inner class

	void applyEffect(model::Effect& effect) const override;

	void endEffect(model::Effect& effect) const override;

	void startEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
