#pragma once

#include <cstdint>

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/**
 * Java com.aionemu.gameserver.skillengine.effect.HealEffectTemplate: the heal value calculation shared by the instant heals (AbstractHealEffect)
 * and the heals over time (HealOverTimeEffect).
 * <p>
 * C++ notes: an interface (hub-headers.md §9.2) implemented by static data shells, so every method is const (§9.1); the default methods are
 * virtual functions defined in HealEffectTemplate.cpp (header request shells-2).
 *
 * @author Neon
 */
class HealEffectTemplate {
public:
	virtual ~HealEffectTemplate() = default;

	virtual bool isPercent() const = 0;

	virtual bool allowHpHealBoost(model::Effect& effect) const = 0;

	virtual bool allowHpHealSkillDeboost(model::Effect& effect) const = 0;

	virtual int32_t getCurrentStatValue(model::Effect& effect) const = 0;

	virtual int32_t getMaxStatValue(model::Effect& effect) const = 0;

	virtual int32_t calculateBaseHealValue(model::Effect& effect) const = 0;

	virtual int32_t calculateSnapshotHealValue(model::Effect& effect, model::HealType type) const;

	virtual int32_t applyHealDeboost(model::Effect& effect, int32_t healValue) const;

	virtual int32_t calculateHealValue(model::Effect& effect, model::HealType type) const;

protected:
	HealEffectTemplate() = default;
	HealEffectTemplate(const HealEffectTemplate&) = default;
	HealEffectTemplate& operator=(const HealEffectTemplate&) = default;
};

} // namespace aion::gameserver::skillengine::effect
