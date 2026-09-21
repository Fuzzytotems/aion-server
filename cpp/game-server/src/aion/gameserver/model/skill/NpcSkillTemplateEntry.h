#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/fwd.h"
#include "aion/gameserver/model/templates/npcskill/fwd.h"

namespace aion::gameserver::model::skill {

/**
 * Skill entry which inherits properties from template (regular npc skills).
 * <p>
 * The element type NpcSkillList builds from the npc skill templates. RefCounted (fieldmap K4), created with create; the template is static
 * data and outlives every entry, so the member is a raw pointer (Java holds the shared NpcSkillTemplate object).
 * <p>
 * M5a subset (plan D2: every npc runs DummyAI and never casts): the constructor and the template getters are ported, so NpcSkillList can be
 * built for the npcs of npc_skills.xml. conditionReady and fireOnEndCastEvents (and their private Java helpers hasCarvedSignet/spawnNpc, which
 * this header does not declare yet) wait for the skill engine, the effect controller and SpawnEngine.
 *
 * @author ATracer, nrg, Yeats
 */
class NpcSkillTemplateEntry : public NpcSkillEntry {
	AION_MAKE_REF_FRIEND
private:
	const templates::npcskill::NpcSkillTemplate* template_;

protected:
	explicit NpcSkillTemplateEntry(const templates::npcskill::NpcSkillTemplate& templateValue);
	~NpcSkillTemplateEntry() override;

public:
	/** Java: new NpcSkillTemplateEntry(template) */
	static runtime::Ref<NpcSkillTemplateEntry> create(const templates::npcskill::NpcSkillTemplate& templateValue);

	bool isReady(int32_t hpPercentage, int64_t fightingTimeInMSec) override;

	bool chanceReady() override;

	bool hpReady(int32_t hpPercentage) override;

	bool timeReady(int64_t elapsedFightTime) override;

	bool hasCooldown() override;

	bool hasPostSpawnCondition() override;

	int32_t getPriority() override;

	bool conditionReady(gameobjects::Creature& creature) override;

	const templates::npcskill::NpcSkillConditionTemplate* getConditionTemplate() override;

	bool hasCondition() override;

	int32_t getNextSkillTime() override;

	bool hasChain() override;

	int32_t getNextChainId() override;

	int32_t getChainId() override;

	const templates::npcskill::NpcSkillTemplate* getTemplate() override;

	bool canUseNextChain(gameobjects::Npc& owner) override;

	void fireOnEndCastEvents(gameobjects::Npc& npc) override;
};

} // namespace aion::gameserver::model::skill
