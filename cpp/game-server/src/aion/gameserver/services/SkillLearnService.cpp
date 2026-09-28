#include "aion/gameserver/services/SkillLearnService.h"

#include <optional>
#include <string_view>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/animations/ActionAnimation.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ACTION_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_REMOVE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/RecipeService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillLearnTemplate.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services {

using network::aion::serverpackets::SM_SKILL_LIST;
using utils::PacketSendUtility;

void SkillLearnService::onLearnSkill(model::gameobjects::player::Player& player, int32_t skillId, int32_t skillLevel, bool isNew) {
	runtime::Ptr<model::skill::PlayerSkillEntry> skill = player.getSkillList()->getSkillEntry(skillId);
	if (skill->isProfessionSkill()) {
		switch (skillLevel) {
			case 1:
			case 100:
			case 200:
			case 300:
			case 400:
			case 450:
			case 500:
				if (skillLevel != 1 || skill->isCraftingSkill()) // exclude lvl 1 tapping skills
					PacketSendUtility::broadcastPacket(player,
						network::aion::serverpackets::SM_ACTION_ANIMATION(player.getObjectId(), model::animations::ActionAnimation::CRAFT_LEVEL_UP), true);
				break;
			default:
				break;
		}
	}
	if (player.getEffectController()) { // null on character creation
		if (player.isSpawned())
			sendPacket(player, *skill, isNew);
		const skillengine::model::SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId);
		if (skillTemplate == nullptr)
			throw runtime::NullPointerException("skillTemplate"); // Java: NullPointerException at skillTemplate.isPassive()
		if (skillTemplate->isPassive())
			skillengine::SkillEngine::getInstance().applyEffectDirectly(skillTemplate, skillLevel, player, player);
		if (skill->isProfessionSkill() && (skill->getSkillLevel() == 399 || skill->getSkillLevel() == 499))
			player.getController().updateNearbyQuests();
	}
	if (skill->isCraftingSkill() || skill->isMorphSkill())
		RecipeService::autoLearnRecipes(player, skillId, skillLevel);
}

void SkillLearnService::sendPacket(model::gameobjects::player::Player& player, model::skill::PlayerSkillEntry& skill, bool isNew) {
	if (skill.isProfessionSkill()) {
		if (skill.isTappingSkill())
			PacketSendUtility::sendPacket(player, SM_SKILL_LIST(skill, isNew ? 1330004 : 1330005));
		else
			PacketSendUtility::sendPacket(player, SM_SKILL_LIST(skill, isNew ? 1330061 : 1330064));
	} else if (isNew)
		PacketSendUtility::sendPacket(player, SM_SKILL_LIST(skill, skill.isStigmaSkill() ? skill.isLinkedStigmaSkill() ? 1402891 : 1300401 : 1300050));
	else
		PacketSendUtility::sendPacket(player, SM_SKILL_LIST(skill, 0));
}

void SkillLearnService::learnNewSkills(model::gameobjects::player::Player& player, int32_t fromLevel, int32_t toLevel) {
	model::PlayerClass playerClass = player.getCommonData()->getPlayerClass();
	std::optional<model::PlayerClass> playerStartClass =
		model::isStartingClass(playerClass) ? std::nullopt : std::optional<model::PlayerClass>(model::getStartingClass(playerClass));
	for (int32_t level = toLevel; level >= fromLevel; level--) { // reversed order to add only the highest of each skill (more efficient)
		if (level < 10 && playerStartClass) // add missing start class skills if already switched class
			autoLearnSkills(player, level, *playerStartClass, player.getRace());
		autoLearnSkills(player, level, playerClass, player.getRace());
	}

	// upgrade human gathering to daeva essence tapping
	if (toLevel >= 10 && player.getCommonData()->isDaeva() && player.getSkillList()->isSkillPresent(30001)) {
		if (!player.getSkillList()->isSkillPresent(30002))
			player.getSkillList()->addSkill(player, 30002, player.getSkillList()->getSkillLevel(30001));
		removeSkill(player, 30001);
	}
}

void SkillLearnService::learnTemporarySkill(model::gameobjects::player::Player& player, int32_t skillId, int32_t skillLevel) {
	player.getSkillList()->addTemporarySkill(player, skillId, skillLevel);
}

void SkillLearnService::autoLearnSkills(model::gameobjects::player::Player& player, int32_t level, model::PlayerClass playerClass,
	model::Race playerRace) {
	for (const skillengine::model::SkillLearnTemplate* skillTemplate :
		dataholders::DataManager::SKILL_TREE_DATA->getTemplatesFor(playerClass, level, playerRace)) {
		if (!skillTemplate->isAutolearn())
			continue;
		if (skillTemplate->getSkillId() == 30001 && !model::isStartingClass(playerClass)) // no human gathering for main classes
			continue;

		player.getSkillList()->addSkill(player, skillTemplate->getSkillId(), skillTemplate->getSkillLevel());
	}
}

void SkillLearnService::learnSkillBook(model::gameobjects::player::Player& player, int32_t skillId) {
	for (const skillengine::model::SkillLearnTemplate* skill :
		dataholders::DataManager::SKILL_TREE_DATA->getSkillsForSkill(skillId, player.getPlayerClass(), player.getRace(), player.getLevel()))
		player.getSkillList()->addSkill(player, skillId, skill->getSkillLevel());
}

bool SkillLearnService::removeSkill(model::gameobjects::player::Player& player, int32_t skillId) {
	runtime::Ptr<model::skill::PlayerSkillEntry> skill = player.getSkillList()->getSkillEntry(skillId);
	if (skill) {
		const skillengine::model::SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId);
		if (skillTemplate == nullptr)
			throw runtime::NullPointerException("skillTemplate"); // Java: NullPointerException at skillTemplate.isPassive()
		if (skillTemplate->isPassive() || skillTemplate->isToggle() || skillTemplate->isDeityAvatar()
			|| std::string_view(skillTemplate->getStack()).starts_with("WS_"))
			player.getEffectController()->removeEffect(skillId);
		player.getSkillList()->removeSkill(skillId);
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SKILL_REMOVE(*skill));
		return true;
	}
	return false;
}

} // namespace aion::gameserver::services
