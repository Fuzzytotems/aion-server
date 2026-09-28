#pragma once

#include <cstdint>
#include <memory>
#include <unordered_set>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/skill/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/model/templates/stats/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::model::stats::container {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). The game stats part of Npc (`setGameStats(std::make_unique<NpcGameStats>(*this))`
 * in Npc::setupStatContainers), an OwnedPart bound in the CreatureGameStats constructor. Binds the erased type variable to Npc (bodies cast
 * `owner`). Field reads and one-statement setters are ported inline.
 *
 * @author xavier, Estrayl
 */
class NpcGameStats : public CreatureGameStats {
private:
	runtime::Field<int64_t> lastAttackTime{0};
	runtime::Field<int64_t> lastAttackedTime{0};
	runtime::Field<int64_t> nextAttackTime{0};
	runtime::Field<int64_t> lastSkillTime{0};
	runtime::Field<int32_t> nextSkillDelay{0};
	runtime::Field<runtime::Ref<skill::NpcSkillEntry>> lastSkill{};
	runtime::Field<int64_t> fightStartingTime{0};
	runtime::Field<int64_t> nextGeoZUpdate{};
	runtime::Field<int64_t> lastChangeTarget{0};

public:
	explicit NpcGameStats(gameobjects::Npc& owner);
	~NpcGameStats() override;

protected:
	void onStatsChange(runtime::Ptr<skillengine::model::Effect> effect) override;

public:
	const templates::stats::StatsTemplate* getStatsTemplate() override;

	calc::Stat2& applyStatFunctions(StatEnum statEnum, calc::Stat2& stat,
		const std::unordered_set<utils::stats::CalculationType>& calculationTypes) override;

	int32_t getBaseAttackSpeed() override;

	std::unique_ptr<calc::Stat2> getMovementSpeed() override;

	std::unique_ptr<calc::Stat2> getAttackRange() override;

	std::unique_ptr<calc::Stat2> getHpRegenRate() override;

	/** @throws IllegalStateException ("No mp regen for NPC") */
	std::unique_ptr<calc::Stat2> getMpRegenRate() override;

	int32_t getCastSpeed();

	int32_t getLastAttackTimeDelta();

	int32_t getLastAttackedTimeDelta();

	void renewLastAttackTime();

	void renewLastAttackedTime();

	bool isNextAttackScheduled();

	void setFightStartingTime();

	int64_t getFightStartingTime() const { return fightStartingTime.get(); }

	void setNextAttackTime(int64_t value) { nextAttackTime.set(value); }

	/**
	 * @return next possible attack time depending on stats
	 */
	int32_t getNextAttackInterval();

	void renewLastSkillTime();

	void renewLastChangeTargetTime();

	int32_t getLastSkillTimeDelta();

	int32_t getLastChangeTargetTimeDelta();

	int64_t getLastSkillTime() const { return lastSkillTime.get(); }

	bool canUseNextSkill();

	void setNextSkillDelay(int32_t nextSkillDelay);

	void setLastSkill(runtime::Ptr<skill::NpcSkillEntry> lastSkill);

	runtime::Ptr<skill::NpcSkillEntry> getLastSkill() const { return lastSkill.get(); }

	int64_t getNextGeoZUpdate() const { return nextGeoZUpdate.get(); }

	/**
	 * @param nextGeoZUpdate the nextGeoZUpdate to set
	 */
	void setNextGeoZUpdate(int64_t value) { nextGeoZUpdate.set(value); }

	void resetFightStats();

	/**
	 * @return time until the npc can use a skill for the first time in this fight
	 */
	int32_t getInitialSkillDelay();
};

} // namespace aion::gameserver::model::stats::container
