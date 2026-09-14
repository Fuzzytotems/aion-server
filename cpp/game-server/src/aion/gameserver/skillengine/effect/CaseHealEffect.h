#pragma once

#include "aion/gameserver/skillengine/effect/CaseHealEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.CaseHealEffect. @author kecimis */
class CaseHealEffect : public ::aion::gameserver::skillengine::effect::AbstractHealEffect {
#include "aion/gameserver/skillengine/effect/CaseHealEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
