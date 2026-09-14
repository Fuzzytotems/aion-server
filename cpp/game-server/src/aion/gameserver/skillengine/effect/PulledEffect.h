#pragma once

#include "aion/gameserver/skillengine/effect/PulledEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.PulledEffect. @author Sarynth, Wakizashi, Sippolo */
class PulledEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/PulledEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
