#pragma once

#include "aion/gameserver/skillengine/effect/AbstractHealEffect.xml.h"

#include "aion/gameserver/skillengine/effect/HealEffectTemplate.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.AbstractHealEffect. @author ATracer, Wakizashi, kecimis */
class AbstractHealEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate,
						   public ::aion::gameserver::skillengine::effect::HealEffectTemplate {
#include "aion/gameserver/skillengine/effect/AbstractHealEffect.xml.inc"
public:
	bool isPercent() const override;

	bool allowHpHealBoost(model::Effect& effect) const override;

	bool allowHpHealSkillDeboost(model::Effect& effect) const override;

	int32_t calculateBaseHealValue(model::Effect& effect) const override;

	int32_t calculateHealValue(model::Effect& effect, model::HealType type) const override;
};

} // namespace aion::gameserver::skillengine::effect
