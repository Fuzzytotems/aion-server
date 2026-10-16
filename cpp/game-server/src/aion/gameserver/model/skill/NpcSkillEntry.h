#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/skill/SkillEntry.h"
#include "aion/gameserver/model/skill/fwd.h"
#include "aion/gameserver/model/templates/npcskill/fwd.h"

namespace aion::gameserver::model::skill {

/**
 * A skill an npc can use, with its usage conditions.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). Abstract RefCounted (fieldmap K4), held by `Npc::queuedSkills`,
 * `NpcGameStats::lastSkill` and NpcSkillList; implementations (NpcSkillTemplateEntry, QueuedNpcSkillEntry) provide create.
 *
 * @author ATracer, nrg, Yeats
 */
class NpcSkillEntry : public SkillEntry {
	AION_MAKE_REF_FRIEND
protected:
	runtime::Field<int64_t> lastTimeUsed{0};

	/** Java: public constructor of the abstract class */
	NpcSkillEntry(int32_t skillId, int32_t skillLevel);
	~NpcSkillEntry() override;

public:
	virtual bool isReady(int32_t hpPercentage, int64_t fightingTimeInMSec) = 0;

	virtual bool chanceReady() = 0;

	virtual bool hpReady(int32_t hpPercentage) = 0;

	virtual bool timeReady(int64_t fightingTimeInMSec) = 0;

	virtual bool hasCooldown() = 0;

	virtual bool hasPostSpawnCondition() = 0;

	int64_t getLastTimeUsed() const { return lastTimeUsed.get(); }

	void setLastTimeUsed();

	virtual int32_t getPriority() = 0;

	virtual bool conditionReady(gameobjects::Creature& creature) = 0;

	virtual const templates::npcskill::NpcSkillConditionTemplate* getConditionTemplate() = 0;

	virtual bool hasCondition() = 0;

	virtual int32_t getNextSkillTime() = 0;

	virtual bool hasChain() = 0;

	virtual int32_t getNextChainId() = 0;

	virtual int32_t getChainId() = 0;

	virtual bool canUseNextChain(gameobjects::Npc& owner) = 0;

	virtual const templates::npcskill::NpcSkillTemplate* getTemplate() = 0;

	virtual void fireOnEndCastEvents(gameobjects::Npc& npc) = 0;
};

} // namespace aion::gameserver::model::skill
