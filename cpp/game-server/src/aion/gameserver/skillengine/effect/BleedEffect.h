#pragma once

#include "aion/gameserver/skillengine/effect/BleedEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BleedEffect. @author ATracer, kecimis */
class BleedEffect : public ::aion::gameserver::skillengine::effect::AbstractOverTimeEffect {
#include "aion/gameserver/skillengine/effect/BleedEffect.xml.inc"
protected:
	void resolveMagicalCritical(model::Effect& effect) const override;

public:
	using AbstractOverTimeEffect::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;

	using AbstractOverTimeEffect::startEffect; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void startEffect(model::Effect& effect) const override;

	using AbstractOverTimeEffect::endEffect; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void endEffect(model::Effect& effect) const override;

	void onPeriodicAction(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
