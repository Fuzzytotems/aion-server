#pragma once

#include "aion/gameserver/skillengine/effect/ConfuseEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ConfuseEffect. @author Yeats, SVDNESS */
class ConfuseEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/ConfuseEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
