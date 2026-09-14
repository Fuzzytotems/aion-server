#pragma once

#include "aion/gameserver/skillengine/effect/EscapeEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.EscapeEffect. @author Rolandas */
class EscapeEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/EscapeEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
