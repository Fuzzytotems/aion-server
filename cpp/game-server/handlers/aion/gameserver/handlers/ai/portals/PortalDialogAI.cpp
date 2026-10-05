#include "aion/gameserver/handlers/ai/portals/PortalDialogAI.h"

#include <vector>

#include "aion/gameserver/dataholders/AutoGroupData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/Portal2Data.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/services/DialogService.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/findgroup/FindGroupService.h"
#include "aion/gameserver/services/teleport/PortalService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::handlers::ai::portals {

AION_AI(PortalDialogAI, "portal_dialog");

// Java PortalDialogAI.java:108-115
void PortalDialogAI::handleDialogStart(Player& player) {
	if (getTalkDelayInMs() == 0) {
		checkDialog(player);
	} else {
		PortalAI::handleDialogStart(player);
	}
}

// Java PortalDialogAI.java:117-147
bool PortalDialogAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	const runtime::Ref<QuestEnv> env = QuestEnv::create(runtime::Ptr<VisibleObject>(getOwner()), player, questId, dialogActionId);
	env->setExtendedRewardIndex(extendedRewardIndex);
	if (questId > 0 && QuestEngine::getInstance().onDialog(*env)) {
		return true;
	}
	switch (dialogActionId) {
		case model::DialogAction::INSTANCE_PARTY_MATCH: // auto groups
			// Java: AutoGroupType agt = AutoGroupType.getAutoGroup(player.getLevel(), getNpcId()); if (agt != null) sendPacket(new
			// SM_AUTO_GROUP(agt.getTemplate().getMaskId())); sendPacket(new SM_DIALOG_WINDOW(getObjectId(), 0)); return true. AutoGroupType's
			// constructor data and getAutoGroup/getTemplate have no C++ companion yet (only the generated constants, model/autogroup/
			// AutoGroupType.h; DialogService's MATCH_MAKER arm waits for the same), so this arm stays loud until the autogroup milestone
			// (m5c-plan.md W-31; docs/deviations/A1.md).
			AION_UNPORTED();
		case model::DialogAction::OPEN_INSTANCE_RECRUIT:
			FindGroupService::getInstance().showInstanceGroups(player, getOwner());
			return true;
		case model::DialogAction::SELECT1_1:
			if (!player.isInTeam() && DataManager::AUTO_GROUP->getRecruitableInstanceMaskIds(getNpcId()) != nullptr) {
				PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1182)); // show OPEN_INSTANCE_RECRUIT option
				return true;
			}
	}
	if (questId == 0) {
		const PortalPath* portalPath = DataManager::PORTAL2_DATA->getPortalDialogPath(getNpcId(), dialogActionId, player);
		if (portalPath != nullptr)
			PortalService::port(portalPath, player, getOwner());
		return true;
	}
	return false;
}

// Java PortalDialogAI.java:149-152
void PortalDialogAI::handleUseItemFinish(Player& player) {
	checkDialog(player);
}

// Java PortalDialogAI.java:154-229
void PortalDialogAI::checkDialog(Player& player) {
	if (!DialogService::isInteractionAllowed(player, getOwner())) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
		return;
	}

	int32_t npcId = getNpcId();
	int32_t teleportationDialogId = DataManager::PORTAL2_DATA->getTeleportDialogId(npcId);
	const runtime::Ref<model::templates::quest::QuestNpc> questNpc = QuestEngine::getInstance().getQuestNpc(npcId);
	runtime::ArrayList<int32_t>& relatedQuests = questNpc->getOnTalkEvent();
	bool playerHasQuest = false;
	bool playerCanStartQuest = false;
	if (!relatedQuests.isEmpty()) {
		for (int32_t questId : relatedQuests) {
			const runtime::Ptr<QuestState> qs = player.getQuestStateList()->getQuestState(questId);
			if (qs && (qs->getStatus() == QuestStatus::START || qs->getStatus() == QuestStatus::REWARD)) {
				playerHasQuest = true;
				break;
			} else if (!qs || qs->isStartable()) {
				if (QuestService::checkStartConditions(player, questId, false)) {
					playerCanStartQuest = true;
				}
			}
		}
	}

	if (playerHasQuest) { // show quest selection dialog and handle teleportation in script, if needed
		bool isRewardStep = false;
		for (int32_t questId : relatedQuests) {
			const runtime::Ptr<QuestState> qs = player.getQuestStateList()->getQuestState(questId);
			if (qs && qs->getStatus() == QuestStatus::REWARD) { // reward dialog
				const runtime::Ref<QuestEnv> env = QuestEnv::create(runtime::Ptr<VisibleObject>(getOwner()), player, questId, model::DialogAction::USE_OBJECT);
				isRewardStep = QuestEngine::getInstance().onDialog(*env);
				if (isRewardStep)
					break;
			}
		}
		if (!isRewardStep) // normal dialog
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), questDialogId.get()));
	} else if (playerCanStartQuest) { // start quest dialog
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), startingDialogId.get()));
	} else { // show teleportation dialog
		switch (npcId) {
			case 831117:
			case 831131:
				PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1012));
				break;
			case 730841:
			case 730883:
			case 804621:
			case 804624:
			case 804625:
				PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 4762));
				break;
			case 731583:
				PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 10));
				break;
			case 731570:
				if (player.getRace() == Race::ASMODIANS) {
					PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1352)); // seized danuar sanctuary
				} else {
					PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011)); // danuar sanctuary
				}
				break;
			case 731549:
				if (player.getRace() == Race::ELYOS) {
					PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011)); // seized danuar sanctuary
				} else {
					PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1352)); // danuar sanctuary
				}
				break;
			default:
				PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), teleportationDialogId));
				break;
		}
	}
}

} // namespace aion::gameserver::handlers::ai::portals
