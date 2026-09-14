#pragma once

#include "aion/gameserver/skillengine/effect/RecallInstantEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.RecallInstantEffect. @author Bio, Sippolo, SVDNESS */
class RecallInstantEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/RecallInstantEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
