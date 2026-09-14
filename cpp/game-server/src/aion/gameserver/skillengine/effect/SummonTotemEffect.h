#pragma once

#include "aion/gameserver/skillengine/effect/SummonTotemEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SummonTotemEffect. @author kecimis */
class SummonTotemEffect : public ::aion::gameserver::skillengine::effect::SummonServantEffect {
#include "aion/gameserver/skillengine/effect/SummonTotemEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
