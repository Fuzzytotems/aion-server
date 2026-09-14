#include "aion/gameserver/model/skill/PlayerSkillEntry.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::model::skill {

PlayerSkillEntry::PlayerSkillEntry(gameobjects::player::Player& player, int32_t skillIdValue, int32_t skillLvl, PersistentState persistentStateValue)
	: PlayerSkillEntry(skillIdValue, skillLvl, 0, persistentStateValue) {
	// Java: skillType from DataManager.SKILL_TREE_DATA.getTemplatesForSkill(...) or the skill template's stigma type
	AION_UNPORTED();
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
	AION_UNPORTED();
}

bool PlayerSkillEntry::isNormalStigmaSkill() {
	AION_UNPORTED();
}

bool PlayerSkillEntry::isLinkedStigmaSkill() {
	AION_UNPORTED();
}

bool PlayerSkillEntry::isNormalSkill() {
	AION_UNPORTED();
}

bool PlayerSkillEntry::isNormalOrStigmaSkill() {
	AION_UNPORTED();
}

bool PlayerSkillEntry::isTappingSkill() {
	AION_UNPORTED();
}

bool PlayerSkillEntry::isCraftingSkill() {
	AION_UNPORTED();
}

bool PlayerSkillEntry::isMorphSkill() {
	AION_UNPORTED();
}

bool PlayerSkillEntry::isProfessionSkill() {
	AION_UNPORTED();
}

int32_t PlayerSkillEntry::getProfessionFlag() {
	AION_UNPORTED();
}

int32_t PlayerSkillEntry::getFlag() {
	AION_UNPORTED();
}

int32_t PlayerSkillEntry::getDateLearned() {
	AION_UNPORTED();
}

void PlayerSkillEntry::setSkillLvl(int32_t value) {
	AION_UNPORTED();
}

int32_t PlayerSkillEntry::getProfessionSkillBarSize() {
	AION_UNPORTED();
}

void PlayerSkillEntry::setPersistentState(PersistentState value) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::skill
