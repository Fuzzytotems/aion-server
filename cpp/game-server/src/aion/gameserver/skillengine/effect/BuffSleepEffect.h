#pragma once

#include "aion/gameserver/skillengine/effect/BuffSleepEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BuffSleepEffect. @author kecimis */
class BuffSleepEffect : public ::aion::gameserver::skillengine::effect::SleepEffect {
#include "aion/gameserver/skillengine/effect/BuffSleepEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
