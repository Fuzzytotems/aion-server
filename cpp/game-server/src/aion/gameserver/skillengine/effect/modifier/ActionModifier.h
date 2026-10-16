#pragma once

#include "aion/gameserver/skillengine/effect/modifier/ActionModifier.xml.h"

#include <cstdint>

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect::modifier {

/** Java com.aionemu.gameserver.skillengine.effect.modifier.ActionModifier. @author ATracer */
class ActionModifier : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/effect/modifier/ActionModifier.xml.inc"
public:
	/** Applies modifier to original value */
	virtual int32_t analyze(model::Effect& effect) const = 0;

	/** Performs check of condition */
	virtual bool check(model::Effect& effect) const = 0;
};

} // namespace aion::gameserver::skillengine::effect::modifier
