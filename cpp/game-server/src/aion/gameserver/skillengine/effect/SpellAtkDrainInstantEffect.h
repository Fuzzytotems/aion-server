#pragma once

#include "aion/gameserver/skillengine/effect/SpellAtkDrainInstantEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SpellAtkDrainInstantEffect. @author ATracer */
class SpellAtkDrainInstantEffect : public ::aion::gameserver::skillengine::effect::DamageEffect {
#include "aion/gameserver/skillengine/effect/SpellAtkDrainInstantEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
