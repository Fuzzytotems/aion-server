#pragma once

#include "aion/gameserver/skillengine/effect/SummonFunctionalNpcEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SummonFunctionalNpcEffect. @author ginho1 */
class SummonFunctionalNpcEffect : public ::aion::gameserver::skillengine::effect::SummonEffect {
#include "aion/gameserver/skillengine/effect/SummonFunctionalNpcEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
