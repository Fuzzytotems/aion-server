#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/skill/fwd.h"

namespace aion::gameserver::model::skill {

/**
 * The skills of an npc (from the npc skill templates) and their priorities.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part of Npc (fieldmap: `Npc.skillList`, `const std::unique_ptr<NpcSkillList>`
 * created in Npc's constructor), bound to the npc in the constructor; Java keeps no owner field. The constructor reads the npc skill templates
 * (DataManager.NPC_SKILL_DATA); an npc without skills gets an empty list (the entries of an npc with skills wait for NpcSkillTemplateEntry,
 * P5-02). getNpcSkills and getPriorities return the live list and array (Java returns the fields; the skill list
 * is Java's immutable `Collections.emptyList()` when the npc has no skills).
 *
 * @author ATracer, Yeats, Neon
 */
class NpcSkillList : public runtime::OwnedPart {
private:
	runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::Ref<NpcSkillEntry>>>> skills{};
	runtime::Field<runtime::Ref<runtime::Array<int32_t>>> priorities{};

public:
	explicit NpcSkillList(gameobjects::Npc& owner);
	~NpcSkillList() override;

private:
	void initSkillList(int32_t npcId);

public:
	bool isEmpty();

	runtime::Ptr<NpcSkillEntry> getRandomSkill();

	runtime::Ptr<NpcSkillEntry> getSkillOnPosition(int32_t position);

	/** Java: a new list */
	std::vector<runtime::Ptr<NpcSkillEntry>> getPostSpawnSkills();

	/** Java: List<NpcSkillEntry> (the live list) */
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<NpcSkillEntry>>> getNpcSkills() const { return skills.get(); }

	/** Java: a new list */
	std::vector<runtime::Ptr<NpcSkillEntry>> getSkillsByPriority(int32_t priority);

	/** Java: int[] (the live array, null when the npc has no skills) */
	runtime::Ptr<runtime::Array<int32_t>> getPriorities() const { return priorities.get(); }

	/** Java: a new list */
	std::vector<runtime::Ptr<NpcSkillEntry>> getChainSkills(NpcSkillEntry& curSkill);
};

} // namespace aion::gameserver::model::skill
