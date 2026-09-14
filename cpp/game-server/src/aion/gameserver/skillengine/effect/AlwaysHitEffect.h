#pragma once

#include "aion/gameserver/skillengine/effect/AlwaysHitEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.AlwaysHitEffect. @author Bobobear */
class AlwaysHitEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/AlwaysHitEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
