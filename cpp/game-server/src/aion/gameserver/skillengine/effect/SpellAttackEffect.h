#pragma once

#include "aion/gameserver/skillengine/effect/SpellAttackEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SpellAttackEffect. @author kecimis */
class SpellAttackEffect : public ::aion::gameserver::skillengine::effect::AbstractOverTimeEffect {
#include "aion/gameserver/skillengine/effect/SpellAttackEffect.xml.inc"
protected:
	void resolveMagicalCritical(model::Effect& effect) const override;

public:
	using AbstractOverTimeEffect::startEffect; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void startEffect(model::Effect& effect) const override;

	void onPeriodicAction(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
