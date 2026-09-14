#pragma once

#include "aion/gameserver/skillengine/effect/HostileUpEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.HostileUpEffect. @author ATracer, Yeats */
class HostileUpEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/HostileUpEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
