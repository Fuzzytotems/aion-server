#pragma once

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <span>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/model/templates/stats/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::model::stats::container {

/**
 * The stat functions of a creature and the calculation of its stats.
 * <p>
 * Hub header (docs/design/hub-headers.md). Java `CreatureGameStats<T extends Creature>` is one non-template class (§8.1 erasure rule): `owner`
 * is `Creature&` and subclasses narrow getOwner-style accessors by casting. A part of Creature (OwnedPart, fieldmap `Creature.gameStats`) with
 * a non-retaining `OwnerRef` to the creature, bound in the constructor.
 * - Stat2 is K5 and abstract: stat getters return `std::unique_ptr<calc::Stat2>`; applyStatFunctions and getItemStatBoost return their `stat`
 *   argument as `calc::Stat2&` (Java returns the same object).
 * - `Set<CalculationType>` parameters are `const std::unordered_set<CalculationType>&` (Java passes EnumSets or Collections.emptySet());
 *   the varargs overloads take `std::initializer_list<CalculationType>`. Subclasses that override one overload of getStat,
 *   getMainHandPAttack or getMainHandMAttack need `using CreatureGameStats::<name>;` to keep the others visible (C++ name hiding).
 * - `stats` is a per-creature ConcurrentHashMap as in Java (spine-status.md: per-creature CHM memory stays an open kernel item); its lists are
 *   `RcArrayList` values locked with SYNCHRONIZED in the bodies, like Java's `synchronized (statFunctions)`.
 *
 * @author xavier, Neon
 */
class CreatureGameStats : public runtime::OwnedPart {
private:
	static constexpr int32_t ATTACK_MAX_COUNTER = std::numeric_limits<int32_t>::max();

protected:
	runtime::OwnerRef<gameobjects::Creature> owner;

private:
	runtime::ConcurrentHashMap<StatEnum, runtime::Ref<runtime::RcArrayList<runtime::Ref<calc::functions::IStatFunction>>>> stats{
		AION_LOCK_CLASS(CreatureGameStats::stats#stripe)};
	runtime::Field<int32_t> attackCounter{0};
	runtime::Field<int32_t> cachedMaxHp{};
	runtime::Field<int32_t> cachedMaxMp{};
	runtime::Field<int32_t> cachedSpeed{};

protected:
	explicit CreatureGameStats(gameobjects::Creature& owner);

public:
	~CreatureGameStats() override;

	/** @return the atcount */
	int32_t getAttackCounter() const { return attackCounter.get(); }

protected:
	/** @param attackCounter the atcount to set */
	void setAttackCounter(int32_t attackCounter);

public:
	void increaseAttackCounter();

	/** @param statOwner nullable (RoahCustomInstanceHandler.java:291 passes null) */
	void addEffectOnly(runtime::Ptr<calc::StatOwner> statOwner, const std::vector<runtime::Ptr<calc::functions::IStatFunction>>& functions);

	/** @param statOwner nullable (RoahCustomInstanceHandler.java:291 passes null) */
	void addEffect(runtime::Ptr<calc::StatOwner> statOwner, const std::vector<runtime::Ptr<calc::functions::IStatFunction>>& functions);

	void endEffect(calc::StatOwner& statOwner);

	/**
	 * C++ only (LogoutBreakers D4, cycles.toml CreatureGameStats.stats): removes every stat function whose owner is a skillengine::model::Effect,
	 * under the same list locks as endEffect but without onStatsChange, packets or updates. Idempotent. Not noexcept: taking a lock may throw
	 * (lock order); LogoutBreakers logs a throwing step.
	 */
	void clearEffectFunctionsWithoutNotify();

	float getPositiveStat(StatEnum statEnum, float base);

	int32_t getPositiveReverseStat(StatEnum statEnum, int32_t base);

	std::unique_ptr<calc::Stat2> getStat(StatEnum statEnum, float base);

protected:
	virtual std::unique_ptr<calc::Stat2> getStat(StatEnum statEnum, float base, const std::unordered_set<utils::stats::CalculationType>& calculationTypes);

public:
	std::unique_ptr<calc::Stat2> getReverseStat(StatEnum statEnum, float base);

	/** @return `stat` itself (Java returns the argument) */
	virtual calc::Stat2& applyStatFunctions(StatEnum statEnum, calc::Stat2& stat,
		const std::unordered_set<utils::stats::CalculationType>& calculationTypes);

	/** @return `stat` itself (Java returns the argument) */
	calc::Stat2& getItemStatBoost(StatEnum statEnum, calc::Stat2& stat);

	virtual const templates::stats::StatsTemplate* getStatsTemplate() = 0;

	std::unique_ptr<calc::Stat2> getPower();

	std::unique_ptr<calc::Stat2> getHealth();

	std::unique_ptr<calc::Stat2> getAccuracy();

	std::unique_ptr<calc::Stat2> getAgility();

	std::unique_ptr<calc::Stat2> getKnowledge();

	std::unique_ptr<calc::Stat2> getWill();

	std::unique_ptr<calc::Stat2> getMaxHp();

	std::unique_ptr<calc::Stat2> getMaxMp();

	std::unique_ptr<calc::Stat2> getPDef();

	std::unique_ptr<calc::Stat2> getMDef();

	std::unique_ptr<calc::Stat2> getEvasion();

	virtual std::unique_ptr<calc::Stat2> getParry();

	std::unique_ptr<calc::Stat2> getBlock();

	std::unique_ptr<calc::Stat2> getMResist();

	std::unique_ptr<calc::Stat2> getPCR();

	std::unique_ptr<calc::Stat2> getMCR();

	std::unique_ptr<calc::Stat2> getMainHandPAttack(std::initializer_list<utils::stats::CalculationType> calculationTypes);

	virtual std::unique_ptr<calc::Stat2> getMainHandPAttack(const std::unordered_set<utils::stats::CalculationType>& calculationTypes);

	virtual std::unique_ptr<calc::Stat2> getMainHandPCritical();

	virtual std::unique_ptr<calc::Stat2> getMainHandPAccuracy();

	std::unique_ptr<calc::Stat2> getMainHandMAttack(std::initializer_list<utils::stats::CalculationType> calculationTypes);

	virtual std::unique_ptr<calc::Stat2> getMainHandMAttack(const std::unordered_set<utils::stats::CalculationType>& calculationTypes);

	virtual std::unique_ptr<calc::Stat2> getMCritical();

	virtual std::unique_ptr<calc::Stat2> getMAccuracy();

	virtual std::unique_ptr<calc::Stat2> getMBoost();

	std::unique_ptr<calc::Stat2> getMBResist();

	std::unique_ptr<calc::Stat2> getAbnormalResistance();

	std::unique_ptr<calc::Stat2> getResistance(StatEnum statEnum);

	std::unique_ptr<calc::Stat2> getAttackSpeed();

	virtual int32_t getBaseAttackSpeed() = 0;

	float getAttackSpeedRate();

	virtual std::unique_ptr<calc::Stat2> getMovementSpeed() = 0;

	virtual std::unique_ptr<calc::Stat2> getAttackRange() = 0;

	virtual std::unique_ptr<calc::Stat2> getHpRegenRate() = 0;

	virtual std::unique_ptr<calc::Stat2> getMpRegenRate() = 0;

	int32_t getElementalDefenseFor(SkillElement element);

	float getMovementSpeedFloat();

	void updateArmorMasteryStats(const std::vector<runtime::Ptr<gameobjects::Item>>& equipment);

	/** Send packet about stats info */
	virtual void updateStatInfo();

	/** Send packet about speed info */
	virtual void updateSpeedInfo();

protected:
	virtual bool checkSpeedStats();

public:
	/** @return All stat functions for the given stat sorted by priority */
	std::vector<runtime::Ptr<calc::functions::IStatFunction>> getStatsSorted(StatEnum stat);

protected:
	/**
	 * Perform additional calculations after effects added/removed<br>
	 * This method will be called outside of stats lock.
	 *
	 * @param effect nullable (addEffect passes null for owners that are no Effect, endEffect always)
	 */
	virtual void onStatsChange(runtime::Ptr<skillengine::model::Effect> effect);

private:
	void checkMaxHPChanged(runtime::Ptr<skillengine::model::Effect> effect);

	/** @param effect nullable (forwarded from onStatsChange; unused by the Java body) */
	void checkMaxMPChanged(runtime::Ptr<skillengine::model::Effect> effect);

protected:
	static std::unordered_set<utils::stats::CalculationType> toSet(std::span<const utils::stats::CalculationType> calculationTypes);

	/** Java returns an EnumSet; the result is passed on as a `Set<CalculationType>` parameter (PlayerGameStats.java:168) */
	static std::unordered_set<utils::stats::CalculationType> copyWith(const std::unordered_set<utils::stats::CalculationType>& types,
		utils::stats::CalculationType type);
};

} // namespace aion::gameserver::model::stats::container
