#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"

namespace aion::gameserver::model::stats::calc {

/**
 * A stat value under calculation: base and bonus with their rates, created per CreatureGameStats::getStat call.
 * <p>
 * Hub header (docs/design/hub-headers.md). K5 CONFINED (fieldmap: never stored in shared state), so a plain value class without a runtime
 * base: members are plain (fieldmap `// confined: inferred`), the owner is a borrowed `Ptr<Creature>` that never outlives the task. Stat2 is abstract (AdditionStat,
 * ReverseStat), so getters of CreatureGameStats return `std::unique_ptr<Stat2>` and callers pass `Stat2&`.
 *
 * @author ATracer
 */
class Stat2 {
protected:
	container::StatEnum stat;

private:
	runtime::Ptr<gameobjects::Creature> owner;

public:
	float base;
	float baseRate{1.0f};
	float bonus{};
	float bonusRate{1.0f};
	float fixedBonusRate{};
	float finalRate{1.0f};

	Stat2(container::StatEnum stat, float base, gameobjects::Creature& owner);
	virtual ~Stat2() = default;

	container::StatEnum getStat() const { return stat; }

	int32_t getBase();

	int32_t getBaseWithoutBaseRate();

	float getExactBaseWithoutBaseRate() const { return base; }

	float getExactBonus() const { return bonus; }

	void setBase(float value) { base = value; }

	float getBaseRate() const { return baseRate; }

	void setBaseRate(float rate) { baseRate = rate; }

	virtual void addToBase(float base) = 0;

	int32_t getBonus();

	int32_t getCurrent();

	float getExactCurrent();

	float getExactCurrentWithoutBonus();

	float getExactCurrentWithoutFixedBonus();

	void setBonus(float value) { bonus = value; }

	float getBonusRate() const { return bonusRate; }

	void setBonusRate(float value) { bonusRate = value; }

	virtual void addToBonus(float bonus) = 0;

	void setFixedBonusRate(float value) { fixedBonusRate = value; }

	float getFixedBonusRate() const { return fixedBonusRate; }

	/**
	 * Rate applied to the final value (base and bonus alike), meant for situational penalties which are not part of the stat itself, like the physical
	 * defense loss while flying. Must be set after all stat functions have been applied, since caps are calculated without it.
	 */
	void setFinalRate(float value) { finalRate = value; }

	float getFinalRate() const { return finalRate; }

	virtual float calculatePercent(int32_t delta) = 0;

	runtime::Ptr<gameobjects::Creature> getOwner() const { return owner; }

	std::string toString() const;
};

} // namespace aion::gameserver::model::stats::calc
