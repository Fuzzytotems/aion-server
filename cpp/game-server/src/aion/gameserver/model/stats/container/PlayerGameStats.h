#pragma once

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <unordered_set>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/model/templates/stats/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::model::stats::container {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). The game stats part of Player (Player::postConstruct). The overridden
 * getMainHandPAttack/getMainHandMAttack(Set) hide the varargs base overloads, so using-declarations re-expose them. The constructor creates the
 * stats template from the player class and level (updateStatsTemplate), so it stays unported.
 *
 * @author xavier
 */
class PlayerGameStats : public CreatureGameStats {
private:
	runtime::Field<const templates::stats::StatsTemplate*> statsTemplate{};
	runtime::Field<int32_t> cachedAttackSpeed{};
	runtime::Field<int32_t> maxDamageChance{};
	runtime::Field<float> minDamageRatio{};
	runtime::Field<float> skillEfficiency{};

public:
	explicit PlayerGameStats(gameobjects::player::Player& owner);
	~PlayerGameStats() override;

protected:
	void onStatsChange(runtime::Ptr<skillengine::model::Effect> effect) override;

public:
	void updateStatsAndSpeedVisually();

	void updateStatsVisually();

protected:
	bool checkSpeedStats() override;

public:
	const templates::stats::StatsTemplate* getStatsTemplate() override { return statsTemplate.get(); }

	void updateStatsTemplate();

	std::unique_ptr<calc::Stat2> getMaxDp();

	std::unique_ptr<calc::Stat2> getFlyTime();

	int32_t getBaseAttackSpeed() override;

	std::unique_ptr<calc::Stat2> getMovementSpeed() override;

	std::unique_ptr<calc::Stat2> getAttackRange() override;

	std::unique_ptr<calc::Stat2> getParry() override;

	using CreatureGameStats::getMainHandMAttack;
	using CreatureGameStats::getMainHandPAttack;

	std::unique_ptr<calc::Stat2> getMainHandPAttack(const std::unordered_set<utils::stats::CalculationType>& calculationTypes) override;

	/** Java final, CalculationType... calculationTypes */
	std::unique_ptr<calc::Stat2> getOffHandPAttack(std::initializer_list<utils::stats::CalculationType> calculationTypes = {});

	std::unique_ptr<calc::Stat2> getOffHandPAttack(const std::unordered_set<utils::stats::CalculationType>& calculationTypes);

	std::unique_ptr<calc::Stat2> getMainHandMAttack(const std::unordered_set<utils::stats::CalculationType>& calculationTypes) override;

	/** Java final, CalculationType... calculationTypes */
	std::unique_ptr<calc::Stat2> getOffHandMAttack(std::initializer_list<utils::stats::CalculationType> calculationTypes = {});

	std::unique_ptr<calc::Stat2> getOffHandMAttack(const std::unordered_set<utils::stats::CalculationType>& calculationTypes);

	std::unique_ptr<calc::Stat2> getMainHandPCritical() override;

	std::unique_ptr<calc::Stat2> getOffHandPCritical();

	std::unique_ptr<calc::Stat2> getMainHandPAccuracy() override;

	std::unique_ptr<calc::Stat2> getOffHandPAccuracy();

	std::unique_ptr<calc::Stat2> getMBoost() override;

	std::unique_ptr<calc::Stat2> getMAccuracy() override;

	std::unique_ptr<calc::Stat2> getMCritical() override;

	std::unique_ptr<calc::Stat2> getHpRegenRate() override;

	std::unique_ptr<calc::Stat2> getMpRegenRate() override;

	void updateStatInfo() override;

	void updateSpeedInfo() override;

	int32_t getHealthDependentAdditionalHp();

	int32_t getWillDependentAdditionalMp();

	int32_t getAgilityDependentAdditionalBaseBlock();

	int32_t getAgilityDependentAdditionalBaseParry();

	int32_t getAgilityDependentAdditionalBaseEvasion();

	int32_t getAccuracyDependentAdditionalBasePhysicalAccuracy();

	int32_t getAccuracyDependentAdditionalBasePhysicalCritical();

private:
	int32_t calculateBaseStatDependentAdditionalValue(calc::Stat2& baseStat, int32_t multiplier);

	int32_t getPowerShardDamage(bool mainHand, bool removePowerShards);

public:
	float getSkillEfficiency() const { return skillEfficiency.get(); }

	int32_t getMaxDamageChance() const { return maxDamageChance.get(); }

	float getMinDamageRatio() const { return minDamageRatio.get(); }

	void setSkillEfficiency(float value) { skillEfficiency.set(value); }

	void setMaxDamageChance(int32_t value) { maxDamageChance.set(value); }

	void setMinDamageRatio(float value) { minDamageRatio.set(value); }

	float getOffHandDamageRatio();
};

} // namespace aion::gameserver::model::stats::container
