#pragma once

#include "aion/gameserver/skillengine/effect/RebirthEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.RebirthEffect. @author Sarynth */
class RebirthEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/RebirthEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
