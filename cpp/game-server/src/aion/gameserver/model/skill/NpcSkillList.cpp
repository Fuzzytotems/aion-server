#include "aion/gameserver/model/skill/NpcSkillList.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTemplates.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"

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
		// Java: one NpcSkillTemplateEntry per template whose skill exists in DataManager.SKILL_DATA (a warning otherwise), then the distinct
		// priorities in descending order. NpcSkillTemplateEntry (P5-02) has no C++ class yet.
		AION_UNPORTED();
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
