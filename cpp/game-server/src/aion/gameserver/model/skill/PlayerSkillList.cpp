#include "aion/gameserver/model/skill/PlayerSkillList.h"

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/SkillLearnService.h"
#include "aion/gameserver/skillengine/model/SkillLearnTemplate.h"

namespace aion::gameserver::model::skill {

using gameobjects::Persistable_PersistentState;
using runtime::Ptr;
using runtime::Ref;

PlayerSkillList::PlayerSkillList() = default;

PlayerSkillList::PlayerSkillList(const std::vector<runtime::Ptr<PlayerSkillEntry>>& playerSkills) {
	for (const runtime::Ptr<PlayerSkillEntry>& entry : playerSkills)
		skills.put(entry->getSkillId(), runtime::Ref<PlayerSkillEntry>(entry));
}

PlayerSkillList::~PlayerSkillList() = default;

runtime::Ref<PlayerSkillList> PlayerSkillList::create() {
	return runtime::makeRef<PlayerSkillList>();
}

runtime::Ref<PlayerSkillList> PlayerSkillList::create(const std::vector<runtime::Ptr<PlayerSkillEntry>>& playerSkills) {
	return runtime::makeRef<PlayerSkillList>(playerSkills);
}

std::vector<runtime::Ptr<PlayerSkillEntry>> PlayerSkillList::getAllSkills() {
	std::vector<Ptr<PlayerSkillEntry>> allSkills;
	for (Ptr<PlayerSkillEntry> entry : skills.values())
		allSkills.push_back(entry);
	return allSkills;
}

std::vector<runtime::Ptr<PlayerSkillEntry>> PlayerSkillList::getDeletedSkills() {
	SYNCHRONIZED(deletedSkills) {
		return deletedSkills.snapshot();
	}
}

runtime::Ptr<PlayerSkillEntry> PlayerSkillList::getSkillEntry(int32_t skillId) {
	return skills.get(skillId);
}

bool PlayerSkillList::addSkill(gameobjects::Creature& player, int32_t skillId, int32_t skillLevel) {
	return addSkill(*runtime::cast<gameobjects::player::Player>(player), skillId, skillLevel, false);
}

bool PlayerSkillList::addTemporarySkill(gameobjects::player::Player& player, int32_t skillId, int32_t skillLevel) {
	return addSkill(player, skillId, skillLevel, true);
}

bool PlayerSkillList::addSkill(gameobjects::player::Player& player, int32_t skillId, int32_t skillLevel, bool isTemporary) {
	bool isNew = true;
	SYNCHRONIZED(*this) {
		Ptr<PlayerSkillEntry> existingSkill = skills.get(skillId);
		if (existingSkill) {
			if (skillLevel <= existingSkill->getSkillLevel())
				return false;
			existingSkill->setSkillLvl(skillLevel);
			isNew = false;
		} else {
			skills.put(skillId, PlayerSkillEntry::create(player, skillId, skillLevel,
									isTemporary ? Persistable_PersistentState::NOACTION : Persistable_PersistentState::NEW));
			std::vector<const skillengine::model::SkillLearnTemplate*> learnTemplates =
				dataholders::DataManager::SKILL_TREE_DATA->getSkillsForSkill(skillId, player.getPlayerClass(), player.getRace(), player.getLevel());
			for (const skillengine::model::SkillLearnTemplate* learnTemplate : learnTemplates) {
				if (learnTemplate->getLearnSkill() && skills.get(*learnTemplate->getLearnSkill())) {
					isNew = false;
					break;
				}
			}
		}
		services::SkillLearnService::onLearnSkill(player, skillId, skillLevel, isNew);
		return true;
	}
}

bool PlayerSkillList::addSkillXp(gameobjects::player::Player& player, int32_t skillId, int32_t xpReward, int32_t objSkillLvl) {
	AION_UNPORTED();
}

bool PlayerSkillList::isSkillPresent(int32_t skillId) {
	return skills.containsKey(skillId);
}

int32_t PlayerSkillList::getSkillLevel(int32_t skillId) {
	Ptr<PlayerSkillEntry> entry = skills.get(skillId);
	if (!entry) // Java: skills.get(skillId).getSkillLevel() on a missing skill
		throw runtime::NullPointerException("skill " + std::to_string(skillId) + " is not in the skill list");
	return entry->getSkillLevel();
}

bool PlayerSkillList::removeSkill(int32_t skillId) {
	SYNCHRONIZED(*this) {
		Ptr<PlayerSkillEntry> entry = skills.remove(skillId);
		if (!entry)
			return false;
		entry->setPersistentState(Persistable_PersistentState::DELETED);
		SYNCHRONIZED(deletedSkills) {
			deletedSkills.add(Ref<PlayerSkillEntry>(entry));
		}
		return true;
	}
}

int32_t PlayerSkillList::size() {
	return skills.size();
}

} // namespace aion::gameserver::model::skill
