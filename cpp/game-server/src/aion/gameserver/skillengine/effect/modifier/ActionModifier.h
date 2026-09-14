#pragma once

#include "aion/gameserver/skillengine/effect/modifier/ActionModifier.xml.h"

namespace aion::gameserver::skillengine::effect::modifier {

/** Java com.aionemu.gameserver.skillengine.effect.modifier.ActionModifier. @author ATracer */
class ActionModifier : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/effect/modifier/ActionModifier.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect::modifier
