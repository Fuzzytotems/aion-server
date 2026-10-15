#pragma once

#include "aion/gameserver/skillengine/effect/MPHealEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.MPHealEffect. @author kecimis */
class MPHealEffect : public ::aion::gameserver::skillengine::effect::HealOverTimeEffect {
#include "aion/gameserver/skillengine/effect/MPHealEffect.xml.inc"
public:
	using HealOverTimeEffect::startEffect; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void startEffect(model::Effect& effect) const override;

	using HealOverTimeEffect::onPeriodicAction; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void onPeriodicAction(model::Effect& effect) const override;

	int32_t getCurrentStatValue(model::Effect& effect) const override;

	int32_t getMaxStatValue(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
