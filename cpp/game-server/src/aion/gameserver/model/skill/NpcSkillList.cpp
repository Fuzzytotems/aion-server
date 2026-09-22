#include "aion/gameserver/model/skill/NpcSkillList.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTemplates.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/NpcSkillTemplateEntry.h"

namespace aion::gameserver::model::skill {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.skill.NpcSkillList");

NpcSkillList::NpcSkillList(gameobjects::Npc& owner) : OwnedPart(owner) {
	initSkillList(owner.getNpcId());
}

NpcSkillList::~NpcSkillList() = default;

void NpcSkillList::initSkillList(int32_t npcId) {
	const templates::npcskill::NpcSkillTemplates* npcSkillTemplates = dataholders::DataManager::NPC_SKILL_DATA->getNpcSkillList(npcId);
	if (npcSkillTemplates == nullptr || npcSkillTemplates->getNpcSkills().empty()) {
		// Java: skills = Collections.emptyList() (immutable); C++: an empty list that nothing adds to, priorities stay null
		skills.set(runtime::RcArrayList<runtime::Ref<NpcSkillEntry>>::create(AION_LOCK_CLASS(NpcSkillList::skills)));
	} else {
		const std::vector<templates::npcskill::NpcSkillTemplate>& npcSkills = npcSkillTemplates->getNpcSkills();
		runtime::Ref<runtime::RcArrayList<runtime::Ref<NpcSkillEntry>>> entries =
			runtime::RcArrayList<runtime::Ref<NpcSkillEntry>>::create(AION_LOCK_CLASS(NpcSkillList::skills));
		std::vector<int32_t> prios;
		for (const templates::npcskill::NpcSkillTemplate& npcSkill : npcSkills) {
			if (dataholders::DataManager::SKILL_DATA->getSkillTemplate(npcSkill.getSkillId()) == nullptr) {
				log.warn("Missing skill " + std::to_string(npcSkill.getSkillId()) + " for npc " + std::to_string(npcId));
				// Java: iter.remove() on the list of the shared NpcSkillTemplates (static data), so the next npc of this id does not warn again.
				// The C++ static data is immutable, so the template is only skipped here (docs/deviations/P5-02.md).
				continue;
			}
			entries->add(NpcSkillTemplateEntry::create(npcSkill));
			if (std::ranges::find(prios, npcSkill.getPriority()) == prios.end())
				prios.push_back(npcSkill.getPriority());
		}
		std::ranges::sort(prios, std::ranges::greater{});
		runtime::Ref<runtime::Array<int32_t>> priorityValues = runtime::Array<int32_t>::make(static_cast<int32_t>(prios.size()));
		for (size_t i = 0; i < prios.size(); ++i)
			(*priorityValues)[static_cast<int32_t>(i)] = prios[i];
		skills.set(std::move(entries));
		priorities.set(std::move(priorityValues));
	}
}

bool NpcSkillList::isEmpty() {
	return skills.get()->isEmpty();
}

runtime::Ptr<NpcSkillEntry> NpcSkillList::getRandomSkill() {
	// Java: return Rnd.get(skills) - Rnd.java:50-52 is `list.isEmpty() ? null : list.size() == 1 ? list.getFirst() : list.get(nextInt(list.size()))`.
	// Written out instead of calling commons::utils::Rnd::get, which takes a sized forward range and does not accept the ArrayList shim (and
	// whose range overload draws a random index even for a single element, where Java draws none).
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<NpcSkillEntry>>> currentSkills = skills.get();
	int32_t size = currentSkills->size();
	if (size == 0)
		return nullptr;
	return currentSkills->get(size == 1 ? 0 : commons::utils::Rnd::nextInt(size));
}

runtime::Ptr<NpcSkillEntry> NpcSkillList::getSkillOnPosition(int32_t position) {
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<NpcSkillEntry>>> currentSkills = skills.get();
	if (currentSkills->isEmpty())
		return nullptr;
	if (position >= currentSkills->size())
		position = currentSkills->size() - 1;

	return currentSkills->get(position);
}

std::vector<runtime::Ptr<NpcSkillEntry>> NpcSkillList::getPostSpawnSkills() {
	// Java filters the entries whose hasPostSpawnCondition() is true (NpcSkillList.java:70-76). The only caller, SpawnEventHandler.onSpawn
	// (SpawnEventHandler.java:20-22), then casts each one through SkillEngine.getSkill(...).useWithoutPropSkill(), and SkillEngine::getSkill is
	// AION_UNPORTED until M5b-2, so a non-empty answer would throw out of the spawn path (m5b-plan.md D3, docs/deviations/P5-02.md).
	// 185 npc ids carry is_post_spawn="true"; none of them is on Poeta or Ishalgen, so no M5b-1 gate run reaches this site.
	AION_PARTIAL("post-spawn npc skills are not cast yet (M5b-2)");
	return {};
}

std::vector<runtime::Ptr<NpcSkillEntry>> NpcSkillList::getSkillsByPriority(int32_t priority) {
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<NpcSkillEntry>>> currentSkills = skills.get();
	if (currentSkills->isEmpty())
		return {}; // Java: Collections.emptyList()

	std::vector<runtime::Ptr<NpcSkillEntry>> skillsByPriority;
	for (const runtime::Ptr<NpcSkillEntry>& skill : *currentSkills) {
		if (skill->getPriority() == priority) {
			skillsByPriority.push_back(skill);
		}
	}
	return skillsByPriority;
}

std::vector<runtime::Ptr<NpcSkillEntry>> NpcSkillList::getChainSkills(NpcSkillEntry& curSkill) {
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<NpcSkillEntry>>> currentSkills = skills.get();
	if (currentSkills->isEmpty())
		return {}; // Java: Collections.emptyList()

	std::vector<runtime::Ptr<NpcSkillEntry>> chainSkills;
	int32_t id = curSkill.getNextChainId();
	if (id > 0) {
		for (const runtime::Ptr<NpcSkillEntry>& skill : *currentSkills) {
			if (skill->getChainId() == id) {
				chainSkills.push_back(skill);
			}
		}
	}
	return chainSkills;
}

} // namespace aion::gameserver::model::skill
