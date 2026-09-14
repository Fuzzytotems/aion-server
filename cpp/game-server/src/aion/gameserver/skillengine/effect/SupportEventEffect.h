#pragma once

#include "aion/gameserver/skillengine/effect/SupportEventEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SupportEventEffect. @author Bobobear */
class SupportEventEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/SupportEventEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
