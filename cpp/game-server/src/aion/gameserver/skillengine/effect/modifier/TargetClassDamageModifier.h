#pragma once

#include "aion/gameserver/skillengine/effect/modifier/TargetClassDamageModifier.xml.h"

#include <cstdint>

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect::modifier {

/** Java com.aionemu.gameserver.skillengine.effect.modifier.TargetClassDamageModifier. @author Rolandas */
class TargetClassDamageModifier : public ::aion::gameserver::skillengine::effect::modifier::ActionModifier {
#include "aion/gameserver/skillengine/effect/modifier/TargetClassDamageModifier.xml.inc"
public:
	int32_t analyze(model::Effect& effect) const override;

	bool check(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect::modifier
