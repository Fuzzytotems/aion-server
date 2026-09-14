#pragma once

#include "aion/gameserver/skillengine/effect/FearEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.FearEffect. @author Sarynth, SVDNESS */
class FearEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/FearEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
