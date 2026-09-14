#pragma once

#include "aion/gameserver/skillengine/effect/FlyoffEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.FlyoffEffect. @author Rolandas */
class FlyoffEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/FlyoffEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
