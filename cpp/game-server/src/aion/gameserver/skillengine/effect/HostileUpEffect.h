#pragma once

#include "aion/gameserver/skillengine/effect/HostileUpEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/**
 * Java com.aionemu.gameserver.skillengine.effect.HostileUpEffect. The Java template field tempHate is per-cast state: xmlgen does not bind it
 * and the port keeps it in Effect::hostileUpTempHate (DEVIATIONS, static-data.md §3.6). @author ATracer, Yeats
 */
class HostileUpEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/HostileUpEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	using EffectTemplate::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
