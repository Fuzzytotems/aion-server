#pragma once

#include "aion/gameserver/skillengine/effect/SpellAtkDrainEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SpellAtkDrainEffect. @author Sippolo, kecimis */
class SpellAtkDrainEffect : public ::aion::gameserver::skillengine::effect::AbstractOverTimeEffect {
#include "aion/gameserver/skillengine/effect/SpellAtkDrainEffect.xml.inc"
protected:
	void resolveMagicalCritical(model::Effect& effect) const override;

public:
	void onPeriodicAction(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
