#pragma once

#include "aion/gameserver/skillengine/effect/ReflectorEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ReflectorEffect. @author ginho1, Wakizashi, kecimis, Neon */
class ReflectorEffect : public ::aion::gameserver::skillengine::effect::ShieldEffect {
#include "aion/gameserver/skillengine/effect/ReflectorEffect.xml.inc"
public:
	model::ShieldType getType() const override;
};

} // namespace aion::gameserver::skillengine::effect
