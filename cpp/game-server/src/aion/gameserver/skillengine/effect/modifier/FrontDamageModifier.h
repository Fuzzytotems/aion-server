#pragma once

#include "aion/gameserver/skillengine/effect/modifier/FrontDamageModifier.xml.h"

#include <cstdint>

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect::modifier {

/** Java com.aionemu.gameserver.skillengine.effect.modifier.FrontDamageModifier. @author ATracer */
class FrontDamageModifier : public ::aion::gameserver::skillengine::effect::modifier::ActionModifier {
#include "aion/gameserver/skillengine/effect/modifier/FrontDamageModifier.xml.inc"
public:
	int32_t analyze(model::Effect& effect) const override;

	bool check(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect::modifier
