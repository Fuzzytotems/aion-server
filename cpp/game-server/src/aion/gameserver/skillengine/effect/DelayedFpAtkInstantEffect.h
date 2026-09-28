#pragma once

#include "aion/gameserver/skillengine/effect/DelayedFpAtkInstantEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DelayedFpAtkInstantEffect. @author kecimis */
class DelayedFpAtkInstantEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/DelayedFpAtkInstantEffect.xml.inc"
public:
	using EffectTemplate::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;

	void applyEffect(model::Effect& effect) const override;

private:
	void calculateAndApplyDamage(model::Effect& effect) const;
};

} // namespace aion::gameserver::skillengine::effect
