#include "aion/gameserver/services/ClassChangeService.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/animations/ActionAnimation.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ACTION_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/SkillLearnService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services {

namespace {

using model::PlayerClass;
using model::Race;
using network::aion::serverpackets::SM_ACTION_ANIMATION;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_PLAYER_INFO;
using network::aion::serverpackets::SM_QUEST_ACTION;
using questEngine::model::QuestState;
using questEngine::model::QuestStatus;
using utils::PacketSendUtility;

} // namespace

void ClassChangeService::showClassChangeDialog(model::gameobjects::player::Player& player) {
	PlayerClass playerClass = player.getPlayerClass();
	Race playerRace = player.getRace();
	if (player.getLevel() >= 9 && model::isStartingClass(playerClass))
		PacketSendUtility::sendPacket(player,
			SM_DIALOG_WINDOW(0, getClassSelectionDialogPageId(playerRace, playerClass), playerRace == Race::ELYOS ? 1006 : 2008));
}

void ClassChangeService::changeClassToSelection(model::gameobjects::player::Player& player, int32_t dialogActionId) {
	// Java checks no level here (only showClassChangeDialog does): kept, m5e-plan.md D11 (docs/deviations/P5-08.md)
	setClass(player, getSelectedPlayerClass(player.getRace(), dialogActionId), true, true);
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(0, 0)); // close dialog window
}

void ClassChangeService::completeAscensionQuest(model::gameobjects::player::Player& player) {
	int32_t questId = player.getRace() == Race::ELYOS ? 1006 : 2008;
	runtime::Ptr<QuestState> existing = player.getQuestStateList()->getQuestState(questId);
	runtime::Ref<QuestState> qs; // Java's local qs: the list's state or the new one
	if (!existing) {
		qs = QuestState::create(questId, QuestStatus::COMPLETE);
		player.getQuestStateList()->addQuest(questId, *qs);
		PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::ADD, *qs));
	} else {
		qs = runtime::Ref<QuestState>(*existing);
		qs->setStatus(QuestStatus::COMPLETE);
	}
	qs->setQuestVar(0);
	qs->setRewardGroup(0);
	PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::UPDATE, *qs));
}

bool ClassChangeService::setClass(model::gameobjects::player::Player& player, model::PlayerClass newClass) {
	return setClass(player, newClass, true, false);
}

bool ClassChangeService::setClass(model::gameobjects::player::Player& player, std::optional<model::PlayerClass> newClass, bool validate, bool updateDaevaStatus) {
	if (!newClass)
		return false;

	if (validate) {
		PlayerClass oldClass = player.getPlayerClass();
		if (!model::isStartingClass(oldClass)) {
			PacketSendUtility::sendMessage(player, "You already switched class");
			return false;
		}
		int8_t id = model::getClassId(oldClass); // starting class ID +1/+2 equals valid subclass ID
		if (oldClass == *newClass || model::getClassId(*newClass) <= id || model::getClassId(*newClass) > id + 2) {
			PacketSendUtility::sendMessage(player, "Invalid class chosen");
			return false;
		}
	}

	player.getCommonData()->setPlayerClass(*newClass);
	player.getGameStats()->updateStatsTemplate();
	player.getController().upgradePlayer();
	PacketSendUtility::broadcastPacket(player, SM_ACTION_ANIMATION(player.getObjectId(), model::animations::ActionAnimation::CLASS_CHANGE, player.getLevel()),
		true);
	// Java's two-argument broadcastPacket(VisibleObject, packet): the known players only, not the player himself (m5e-plan.md §2.3 S7)
	PacketSendUtility::broadcastPacket(static_cast<model::gameobjects::VisibleObject&>(player), SM_PLAYER_INFO(player));
	SkillLearnService::learnNewSkills(player, 9, player.getLevel());

	if (updateDaevaStatus) {
		if (!model::isStartingClass(*newClass)) {
			completeAscensionQuest(player);
			player.getCommonData()->updateDaeva();
		} else {
			player.getCommonData()->setDaeva(false);
		}
	}
	return true;
}

int32_t ClassChangeService::getClassSelectionDialogPageId(model::Race playerRace, model::PlayerClass playerClass) {
	switch (playerClass) {
		case PlayerClass::WARRIOR:
			return playerRace == Race::ELYOS ? 2375 : 3057;
		case PlayerClass::SCOUT:
			return playerRace == Race::ELYOS ? 2716 : 3398;
		case PlayerClass::MAGE:
			return playerRace == Race::ELYOS ? 3057 : 3739;
		case PlayerClass::PRIEST:
			return playerRace == Race::ELYOS ? 3398 : 4080;
		case PlayerClass::ENGINEER:
			return playerRace == Race::ELYOS ? 3739 : 3569;
		case PlayerClass::ARTIST:
			return playerRace == Race::ELYOS ? 4080 : 3910;
		default:
			return 0;
	}
}

std::optional<model::PlayerClass> ClassChangeService::getSelectedPlayerClass(model::Race race, int32_t dialogActionId) {
	namespace DialogAction = model::DialogAction;
	switch (race) {
		case Race::ELYOS:
			switch (dialogActionId) {
				case DialogAction::SELECT5_1:
					return PlayerClass::GLADIATOR;
				case DialogAction::SELECT5_2:
					return PlayerClass::TEMPLAR;
				case DialogAction::SELECT6_1:
					return PlayerClass::ASSASSIN;
				case DialogAction::SELECT6_2:
					return PlayerClass::RANGER;
				case DialogAction::SELECT7_1:
					return PlayerClass::SORCERER;
				case DialogAction::SELECT7_2:
					return PlayerClass::SPIRIT_MASTER;
				case DialogAction::SELECT8_1:
					return PlayerClass::CLERIC;
				case DialogAction::SELECT8_2:
					return PlayerClass::CHANTER;
				case DialogAction::SELECT9_1:
					return PlayerClass::GUNNER;
				case DialogAction::SELECT9_2:
					return PlayerClass::RIDER;
				case DialogAction::SELECT10_1:
					return PlayerClass::BARD;
				default:
					break;
			}
			break;
		case Race::ASMODIANS:
			switch (dialogActionId) {
				case DialogAction::SELECT7_1:
					return PlayerClass::GLADIATOR;
				case DialogAction::SELECT7_2:
					return PlayerClass::TEMPLAR;
				case DialogAction::SELECT8_1:
					return PlayerClass::ASSASSIN;
				case DialogAction::SELECT8_2:
					return PlayerClass::RANGER;
				case DialogAction::SELECT9_1:
					return PlayerClass::SORCERER;
				case DialogAction::SELECT9_2:
					return PlayerClass::SPIRIT_MASTER;
				case DialogAction::SELECT10_1:
					return PlayerClass::CLERIC;
				case DialogAction::SELECT10_2:
					return PlayerClass::CHANTER;
				case DialogAction::SELECT8_3_1:
					return PlayerClass::GUNNER;
				case DialogAction::SELECT8_3_2:
					return PlayerClass::RIDER;
				case DialogAction::SELECT9_3_1:
					return PlayerClass::BARD;
				default:
					break;
			}
			break;
		default:
			break;
	}
	return std::nullopt;
}

} // namespace aion::gameserver::services
