#pragma once

#include "aion/gameserver/skillengine/effect/CurseEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.CurseEffect. @author ATracer */
class CurseEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/CurseEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
