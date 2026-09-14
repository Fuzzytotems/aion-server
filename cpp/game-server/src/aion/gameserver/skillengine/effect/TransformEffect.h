#pragma once

#include "aion/gameserver/skillengine/effect/TransformEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.TransformEffect. @author Sweetkr, kecimis */
class TransformEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/TransformEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
