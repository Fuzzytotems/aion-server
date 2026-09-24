#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/fwd.h"
#include "aion/gameserver/model/templates/npcskill/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::model::skill {

/**
 * Skill entry which inherits properties from template (regular npc skills).
 * <p>
 * The element type NpcSkillList builds from the npc skill templates. RefCounted (fieldmap K4), created with create; the template is static
 * data and outlives every entry, so the member is a raw pointer (Java holds the shared NpcSkillTemplate object).
 * <p>
 * Every body is ported (M5b-2); Java's private helpers hasCarvedSignet and spawnNpc are declared since header request m5b2-p2-7.
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

private:
	/** @param curTarget nullable; @param skillTemp nullable (Java: private, header request m5b2-p2-7) */
	bool hasCarvedSignet(runtime::Ptr<gameobjects::VisibleObject> curTarget, const skillengine::model::SkillTemplate* skillTemp,
		int32_t signetLvl) const;

public:
	const templates::npcskill::NpcSkillConditionTemplate* getConditionTemplate() override;

	bool hasCondition() override;

	int32_t getNextSkillTime() override;

	bool hasChain() override;

	int32_t getNextChainId() override;

	int32_t getChainId() override;

	const templates::npcskill::NpcSkillTemplate* getTemplate() override;

	bool canUseNextChain(gameobjects::Npc& owner) override;

	void fireOnEndCastEvents(gameobjects::Npc& npc) override;

private:
	/** Java: private (header request m5b2-p2-7) */
	void spawnNpc(gameobjects::Npc& npc, const templates::npcskill::NpcSkillSpawn& spawn) const;
};

} // namespace aion::gameserver::model::skill
