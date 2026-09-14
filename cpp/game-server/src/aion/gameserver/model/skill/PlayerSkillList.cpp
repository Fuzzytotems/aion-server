#include "aion/gameserver/model/skill/PlayerSkillList.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"

namespace aion::gameserver::model::skill {

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
	AION_UNPORTED();
}

std::vector<runtime::Ptr<PlayerSkillEntry>> PlayerSkillList::getDeletedSkills() {
	AION_UNPORTED();
}

runtime::Ptr<PlayerSkillEntry> PlayerSkillList::getSkillEntry(int32_t skillId) {
	AION_UNPORTED();
}

bool PlayerSkillList::addSkill(gameobjects::Creature& player, int32_t skillId, int32_t skillLevel) {
	AION_UNPORTED();
}

bool PlayerSkillList::addTemporarySkill(gameobjects::player::Player& player, int32_t skillId, int32_t skillLevel) {
	AION_UNPORTED();
}

bool PlayerSkillList::addSkill(gameobjects::player::Player& player, int32_t skillId, int32_t skillLevel, bool isTemporary) {
	AION_UNPORTED();
}

bool PlayerSkillList::addSkillXp(gameobjects::player::Player& player, int32_t skillId, int32_t xpReward, int32_t objSkillLvl) {
	AION_UNPORTED();
}

bool PlayerSkillList::isSkillPresent(int32_t skillId) {
	AION_UNPORTED();
}

int32_t PlayerSkillList::getSkillLevel(int32_t skillId) {
	AION_UNPORTED();
}

bool PlayerSkillList::removeSkill(int32_t skillId) {
	AION_UNPORTED();
}

int32_t PlayerSkillList::size() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::skill
