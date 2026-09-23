#pragma once

#include "aion/gameserver/skillengine/effect/CaseHealEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.CaseHealEffect. @author kecimis */
class CaseHealEffect : public ::aion::gameserver::skillengine::effect::AbstractHealEffect {
#include "aion/gameserver/skillengine/effect/CaseHealEffect.xml.inc"
public:
	using AbstractHealEffect::applyEffect; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void applyEffect(model::Effect& effect) const override;

	int32_t getCurrentStatValue(model::Effect& effect) const override;

	int32_t getMaxStatValue(model::Effect& effect) const override;

	void startEffect(model::Effect& effect) const override;

private:
	bool tryHeal(model::Effect& effect) const;

public:
	bool allowHpHealBoost(model::Effect& effect) const override;

	bool allowHpHealSkillDeboost(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
