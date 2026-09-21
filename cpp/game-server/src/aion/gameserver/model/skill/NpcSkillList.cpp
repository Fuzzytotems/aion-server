#include "aion/gameserver/model/skill/NpcSkillList.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
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
	AION_UNPORTED();
}

runtime::Ptr<NpcSkillEntry> NpcSkillList::getRandomSkill() {
	AION_UNPORTED();
}

runtime::Ptr<NpcSkillEntry> NpcSkillList::getSkillOnPosition(int32_t position) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<NpcSkillEntry>> NpcSkillList::getPostSpawnSkills() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<NpcSkillEntry>> NpcSkillList::getSkillsByPriority(int32_t priority) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<NpcSkillEntry>> NpcSkillList::getChainSkills(NpcSkillEntry& curSkill) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::skill
