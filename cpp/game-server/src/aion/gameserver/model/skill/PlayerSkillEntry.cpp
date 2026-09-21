#include "aion/gameserver/model/skill/PlayerSkillEntry.h"

#include <algorithm>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/SkillLearnTemplate.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/model/StigmaType.h"

namespace aion::gameserver::model::skill {

namespace {

/**
 * Java: the skillType computation of PlayerSkillEntry(Player, int, int, PersistentState), which assigns the effectively final field after the
 * delegation; C++ computes it first (the member is const, PlayerSkillEntry.h).
 */
int32_t skillTypeOf(gameobjects::player::Player& player, int32_t skillId) {
	std::vector<const skillengine::model::SkillLearnTemplate*> learnTemplates =
		dataholders::DataManager::SKILL_TREE_DATA->getTemplatesForSkill(skillId, player.getPlayerClass(), player.getRace());
	int32_t skillType = 0;
	if (learnTemplates.empty()) {
		// Java: getSkillTemplate().getStigmaType() (SkillEntry.getSkillTemplate reads SKILL_DATA)
		const skillengine::model::SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId);
		if (skillTemplate == nullptr)
			throw runtime::NullPointerException("skill template " + std::to_string(skillId) + " is null");
		skillType = skillTemplate->getStigmaType() == skillengine::model::StigmaType::NONE ? 0 : 1; // no way to tell if linked stigma
	} else {
		for (const skillengine::model::SkillLearnTemplate* learnTemplate : learnTemplates) {
			if (learnTemplate->isStigma()) {
				skillType = learnTemplate->isLinkedStigma() ? 3 : 1;
				break;
			}
		}
	}
	return skillType;
}

} // namespace

PlayerSkillEntry::PlayerSkillEntry(gameobjects::player::Player& player, int32_t skillIdValue, int32_t skillLvl, PersistentState persistentStateValue)
	: PlayerSkillEntry(skillIdValue, skillLvl, skillTypeOf(player, skillIdValue), persistentStateValue) {
}

PlayerSkillEntry::PlayerSkillEntry(int32_t skillIdValue, int32_t skillLvl, int32_t skillTypeValue, PersistentState persistentStateValue)
	: SkillEntry(skillIdValue, skillLvl), skillType(skillTypeValue), persistentState(persistentStateValue) {
}

PlayerSkillEntry::~PlayerSkillEntry() = default;

runtime::Ref<PlayerSkillEntry> PlayerSkillEntry::create(gameobjects::player::Player& player, int32_t skillIdValue, int32_t skillLvl,
	PersistentState persistentStateValue) {
	return runtime::makeRef<PlayerSkillEntry>(player, skillIdValue, skillLvl, persistentStateValue);
}

runtime::Ref<PlayerSkillEntry> PlayerSkillEntry::create(int32_t skillIdValue, int32_t skillLvl, int32_t skillTypeValue,
	PersistentState persistentStateValue) {
	return runtime::makeRef<PlayerSkillEntry>(skillIdValue, skillLvl, skillTypeValue, persistentStateValue);
}

bool PlayerSkillEntry::isStigmaSkill() {
	return skillType > 0;
}

bool PlayerSkillEntry::isNormalStigmaSkill() {
	return isStigmaSkill() && !isLinkedStigmaSkill();
}

bool PlayerSkillEntry::isLinkedStigmaSkill() {
	return skillType >= 3;
}

bool PlayerSkillEntry::isNormalSkill() {
	return !isStigmaSkill() && skillId < 30000;
}

bool PlayerSkillEntry::isNormalOrStigmaSkill() {
	return skillId < 30000;
}

bool PlayerSkillEntry::isTappingSkill() {
	return skillId >= 30001 && skillId <= 30003;
}

bool PlayerSkillEntry::isCraftingSkill() {
	return skillId >= 40001 && skillId <= 40010 && !isMorphSkill();
}

bool PlayerSkillEntry::isMorphSkill() {
	return skillId == 40009;
}

bool PlayerSkillEntry::isProfessionSkill() {
	return skillId >= 30000 && skillId < 50000; // 50000 or greater are actions etc.
}

int32_t PlayerSkillEntry::getProfessionFlag() {
	if (isTappingSkill() || isMorphSkill())
		return 1; // not sure for morph
	if (isCraftingSkill())
		return getCurrentXp(); // not implemented in DB
	return 0;
}

int32_t PlayerSkillEntry::getFlag() {
	return isNormalSkill() ? getDateLearned() : 0;
}

int32_t PlayerSkillEntry::getDateLearned() {
	return static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000); // not implemented in DB
}

void PlayerSkillEntry::setSkillLvl(int32_t value) {
	SkillEntry::setSkillLvl(value);
	if (getPersistentState() != PersistentState::NOACTION)
		setPersistentState(PersistentState::UPDATE_REQUIRED);
}

int32_t PlayerSkillEntry::getProfessionSkillBarSize() {
	if (!isProfessionSkill())
		return 0;
	int32_t level = skillLevel.get();
	int32_t size = level / 100;
	if (isCraftingSkill() && level >= 450)
		size += (level - 350) / 100; // above 400 points, the crafting max points increase by 50 instead of 100
	return isTappingSkill() ? std::min(size, 4) : size; // limit tapping bar size to 4 (499) to prevent black bar above 500 points
}

void PlayerSkillEntry::setPersistentState(PersistentState value) {
	// java-race: unsynchronized check-then-set of the persistent state, as in Java
	switch (value) {
		case PersistentState::DELETED:
			if (this->persistentState.get() == PersistentState::NEW)
				this->persistentState = PersistentState::NOACTION;
			else
				this->persistentState = PersistentState::DELETED;
			break;
		case PersistentState::UPDATE_REQUIRED:
			if (this->persistentState.get() != PersistentState::NEW)
				this->persistentState = PersistentState::UPDATE_REQUIRED;
			break;
		case PersistentState::NOACTION:
			break;
		default:
			this->persistentState = value;
	}
}

} // namespace aion::gameserver::model::skill
