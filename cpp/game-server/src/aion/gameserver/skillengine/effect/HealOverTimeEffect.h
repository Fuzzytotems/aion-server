#pragma once

#include "aion/gameserver/skillengine/effect/HealOverTimeEffect.xml.h"

#include "aion/gameserver/skillengine/effect/HealEffectTemplate.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.HealOverTimeEffect. @author ATracer, kecimis */
class HealOverTimeEffect : public ::aion::gameserver::skillengine::effect::AbstractOverTimeEffect,
						   public ::aion::gameserver::skillengine::effect::HealEffectTemplate {
#include "aion/gameserver/skillengine/effect/HealOverTimeEffect.xml.inc"
public:
	using AbstractOverTimeEffect::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;

	using AbstractOverTimeEffect::startEffect; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void startEffect(model::Effect& effect, model::HealType healType) const;

	using AbstractOverTimeEffect::onPeriodicAction; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void onPeriodicAction(model::Effect& effect, model::HealType healType) const;

	bool isPercent() const override;

	bool allowHpHealBoost(model::Effect& effect) const override;

	bool allowHpHealSkillDeboost(model::Effect& effect) const override;

	int32_t calculateBaseHealValue(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
