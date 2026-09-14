#pragma once

#include "aion/gameserver/skillengine/effect/AbstractAbsoluteStatEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.AbstractAbsoluteStatEffect. @author Rolandas */
class AbstractAbsoluteStatEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/AbstractAbsoluteStatEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
