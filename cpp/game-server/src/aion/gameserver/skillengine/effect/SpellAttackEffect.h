#pragma once

#include "aion/gameserver/skillengine/effect/SpellAttackEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SpellAttackEffect. @author kecimis */
class SpellAttackEffect : public ::aion::gameserver::skillengine::effect::AbstractOverTimeEffect {
#include "aion/gameserver/skillengine/effect/SpellAttackEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
