#pragma once

#include "aion/gameserver/skillengine/effect/SpellAtkDrainEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SpellAtkDrainEffect. @author Sippolo, kecimis */
class SpellAtkDrainEffect : public ::aion::gameserver::skillengine::effect::AbstractOverTimeEffect {
#include "aion/gameserver/skillengine/effect/SpellAtkDrainEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
