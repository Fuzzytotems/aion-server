#pragma once

#include "aion/gameserver/skillengine/effect/Effects.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.Effects. @author ATracer */
class Effects : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/effect/Effects.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
