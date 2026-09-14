#pragma once

#include "aion/gameserver/skillengine/effect/BuffSilenceEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BuffSilenceEffect. @author kecimis */
class BuffSilenceEffect : public ::aion::gameserver::skillengine::effect::SilenceEffect {
#include "aion/gameserver/skillengine/effect/BuffSilenceEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
