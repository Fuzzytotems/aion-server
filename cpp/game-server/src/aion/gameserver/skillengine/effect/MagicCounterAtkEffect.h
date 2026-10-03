#pragma once

#include "aion/gameserver/skillengine/effect/MagicCounterAtkEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.MagicCounterAtkEffect. @author ViAl */
class MagicCounterAtkEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/MagicCounterAtkEffect.xml.inc"
public:
	friend struct MagicCounterAtkEffect_ActionObserver; // C++ only: MagicCounterAtkEffect$1 (MagicCounterAtkEffect.cpp) reads the protected maxdmg, element, hopType and calculateBaseValue like Java's inner class

	void applyEffect(model::Effect& effect) const override;

	void startEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
