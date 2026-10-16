#include "aion/gameserver/network/aion/clientpackets/CM_DIALOG_SELECT.h"

#include <optional>
#include <string>
#include <string_view>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/ClassChangeService.h"
#include "aion/gameserver/services/DialogService.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using utils::PacketSendUtility;
using utils::audit::AuditLogger;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_DIALOG_SELECT");

CM_DIALOG_SELECT::CM_DIALOG_SELECT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_DIALOG_SELECT.java:47-54
void CM_DIALOG_SELECT::readImpl() {
	targetObjectId = readD();
	dialogActionId = readUH();
	extendedRewardIndex = readUH();
	lastPage = readUH();
	questId = readD();
	unk = readUH(); // unk 4.7
}

// Java CM_DIALOG_SELECT.java:57-124
void CM_DIALOG_SELECT::runImpl() {
	using namespace model::DialogAction;
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (player->isProtectionActive())
		player->getController().stopProtectionActiveTask();

	if (player->isTrading())
		return;

	const std::optional<std::string_view> dialogActionName = nameOf(dialogActionId);
	// Java string concatenation writes a null String as "null"
	const std::string dialogActionNameText = dialogActionName ? std::string(*dialogActionName) : std::string("null");
	if (player->hasAccess(configs::administration::AdminConfig::DIALOG_INFO.load())) {
		PacketSendUtility::sendMessage(*player, "Quest ID: " + std::to_string(questId) + ", Dialog Action: " + dialogActionNameText + " (ID: "
													+ std::to_string(dialogActionId) + ")");
	}
	if (!dialogActionName) {
		log.warn("Received unknown dialog action id " + std::to_string(dialogActionId) + " (quest " + std::to_string(questId) + ") from "
			+ player->toString());
		return;
	}

	if (targetObjectId == 0 || targetObjectId == player->getObjectId()) {
		const model::templates::QuestTemplate* questTemplate = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
		if (questTemplate == nullptr)
			return;

		runtime::Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(nullptr, *player, questId, dialogActionId);
		if (questTemplate->isCanReport()) {
			switch (dialogActionId) {
				case SELECTED_QUEST_AUTO_REWARD:
				case SELECTED_QUEST_AUTO_REWARD1:
				case SELECTED_QUEST_AUTO_REWARD2:
				case SELECTED_QUEST_AUTO_REWARD3:
				case SELECTED_QUEST_AUTO_REWARD4:
				case SELECTED_QUEST_AUTO_REWARD5:
				case SELECTED_QUEST_AUTO_REWARD6:
				case SELECTED_QUEST_AUTO_REWARD7:
				case SELECTED_QUEST_AUTO_REWARD8:
				case SELECTED_QUEST_AUTO_REWARD9:
				case SELECTED_QUEST_AUTO_REWARD10:
				case SELECTED_QUEST_AUTO_REWARD11:
				case SELECTED_QUEST_AUTO_REWARD12:
				case SELECTED_QUEST_AUTO_REWARD13:
				case SELECTED_QUEST_AUTO_REWARD14:
				case SELECTED_QUEST_AUTO_REWARD15:
					services::QuestService::finishQuest(*env);
					return;
			}
		}
		if (questEngine::QuestEngine::getInstance().onDialog(*env))
			return;
		if (configs::main::CustomConfig::ENABLE_SIMPLE_2NDCLASS.load() && (questId == 1006 || questId == 2008))
			services::ClassChangeService::changeClassToSelection(*player, dialogActionId);
		return;
	}

	if (const runtime::Ptr<Creature> target = runtime::as<Creature>(player->getKnownList().getObject(targetObjectId))) {
		if (const runtime::Ptr<Npc> npc = runtime::as<Npc>(target)) {
			bool isFunctionDialog = dataholders::DataManager::NPC_DATA->isFunctionDialog(dialogActionId);
			if (isFunctionDialog && !npc->getObjectTemplate()->supportsAction(dialogActionId)) {
				AuditLogger::log(*player, "tried to use unsupported dialog action " + dialogActionNameText + " on " + npc->toString());
				return;
			}
			if ((isFunctionDialog || dialogActionId < SELECT1) && !services::DialogService::isInteractionAllowed(*player, *npc)) {
				AuditLogger::log(*player, "tried to illegally use dialog action " + dialogActionNameText + " on " + npc->toString());
				return;
			}
		}
		target->getController().onDialogSelect(dialogActionId, lastPage, *player, questId, extendedRewardIndex);
	}
}

AION_CLIENT_PACKET(CM_DIALOG_SELECT);

} // namespace aion::gameserver::network::aion::clientpackets
