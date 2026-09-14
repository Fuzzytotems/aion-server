#pragma once

#include "aion/gameserver/skillengine/effect/SearchEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SearchEffect. @author Sweetkr */
class SearchEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/SearchEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
