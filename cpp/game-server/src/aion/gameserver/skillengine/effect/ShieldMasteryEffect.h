#pragma once

#include "aion/gameserver/skillengine/effect/ShieldMasteryEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ShieldMasteryEffect. @author VladimirZ */
class ShieldMasteryEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/ShieldMasteryEffect.xml.inc"
public:
	void startEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
