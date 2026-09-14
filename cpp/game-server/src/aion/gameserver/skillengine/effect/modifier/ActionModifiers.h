#pragma once

#include "aion/gameserver/skillengine/effect/modifier/ActionModifiers.xml.h"

namespace aion::gameserver::skillengine::effect::modifier {

/** Java com.aionemu.gameserver.skillengine.effect.modifier.ActionModifiers. @author ATracer */
class ActionModifiers : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/effect/modifier/ActionModifiers.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect::modifier
