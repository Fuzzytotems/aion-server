#pragma once

#include "aion/gameserver/skillengine/effect/ProtectEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ProtectEffect. @author Sippolo, kecimis */
class ProtectEffect : public ::aion::gameserver::skillengine::effect::ShieldEffect {
#include "aion/gameserver/skillengine/effect/ProtectEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
