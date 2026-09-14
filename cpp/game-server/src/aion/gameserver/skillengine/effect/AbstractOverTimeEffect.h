#pragma once

#include "aion/gameserver/skillengine/effect/AbstractOverTimeEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.AbstractOverTimeEffect. @author kecimis */
class AbstractOverTimeEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/AbstractOverTimeEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
