#pragma once

#include "aion/gameserver/skillengine/effect/BuffBindEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BuffBindEffect. @author kecimis */
class BuffBindEffect : public ::aion::gameserver::skillengine::effect::BindEffect {
#include "aion/gameserver/skillengine/effect/BuffBindEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
