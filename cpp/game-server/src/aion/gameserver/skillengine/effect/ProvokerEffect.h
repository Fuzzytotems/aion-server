#pragma once

#include "aion/gameserver/skillengine/effect/ProvokerEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ProvokerEffect. @author ATracer, kecimis */
class ProvokerEffect : public ::aion::gameserver::skillengine::effect::ShieldEffect {
#include "aion/gameserver/skillengine/effect/ProvokerEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
