#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/BeshmundirsWalkAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIRequest.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/Portal2Data.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/findgroup/FindGroupService.h"
#include "aion/gameserver/services/teleport/PortalService.h"
#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

AION_AI(BeshmundirsWalkAI, "beshmundirswalk");

/**
 * Java: the anonymous AIRequest of onDialogSelect (BeshmundirsWalkAI.java:61-72): the answer to the difficulty question ports the responder,
 * difficult 2 for STR_INSTANCE_DUNGEON_WITH_DIFFICULTY_ENTER_CONFIRM, else 1.
 */
struct BeshmundirsWalkAI_AIRequest final : AIRequest {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<BeshmundirsWalkAI> beshmundirsWalkAI; // captured this BeshmundirsWalkAI (line 66)

	static runtime::Ref<BeshmundirsWalkAI_AIRequest> create(BeshmundirsWalkAI& aiValue) {
		return runtime::makeRef<BeshmundirsWalkAI_AIRequest>(aiValue);
	}

	void acceptRequest(runtime::Ptr<Creature> requester, Player& responder, int32_t requestId) override {
		static_cast<void>(requester);
		if (requestId == SM_QUESTION_WINDOW::STR_INSTANCE_DUNGEON_WITH_DIFFICULTY_ENTER_CONFIRM) {
			beshmundirsWalkAI->moveToInstance(responder, int8_t{2});
		} else {
			beshmundirsWalkAI->moveToInstance(responder, int8_t{1});
		}
	}

protected:
	explicit BeshmundirsWalkAI_AIRequest(BeshmundirsWalkAI& aiValue) : beshmundirsWalkAI(aiValue) {}
	~BeshmundirsWalkAI_AIRequest() override = default;
};

// Java BeshmundirsWalkAI.java:33-36
void BeshmundirsWalkAI::handleUseItemFinish(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 10)); // Initial dialog
}

// Java BeshmundirsWalkAI.java:38-81
bool BeshmundirsWalkAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	switch (dialogActionId) {
		case model::DialogAction::OPEN_INSTANCE_RECRUIT:
			FindGroupService::getInstance().showInstanceGroups(player, getOwner());
			break;
		case model::DialogAction::INSTANCE_ENTRY: // I'm ready to enter
			if (!player.isInGroup()) {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ENTER_ONLY_PARTY_DON());
				return true;
			}
			if (player.getPlayerGroup()->isLeader(player)) {
				PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 4762)); // Path selection
			} else {
				if (!isAGroupMemberInInstance(player)) {
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_DUNGEON_CANT_ENTER_NOT_OPENED());
					return true;
				}
				moveToInstance(player, int8_t{0});
			}
			break;
		case model::DialogAction::SELECT_NONE_1: // I'll take the safer path
		case model::DialogAction::SELECT_NONE_2: { // Give me the dangerous path
			runtime::Ref<BeshmundirsWalkAI_AIRequest> request = BeshmundirsWalkAI_AIRequest::create(*this);
			// 902051 = STR_INSTANCE_DUNGEON_DIFFICULTY_NORMAL (Normal), 902052 = STR_INSTANCE_DUNGEON_DIFFICULTY_HARD (Difficult)
			int32_t pathL10nId = dialogActionId == model::DialogAction::SELECT_NONE_1 ? 902051 : 902052;
			AIActions::addRequest(*this, player, SM_QUESTION_WINDOW::STR_INSTANCE_DUNGEON_WITH_DIFFICULTY_ENTER_CONFIRM, *request,
				{"300170000", ChatUtil::l10n(pathL10nId)});
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 4762)); // Path selection
			break;
		}
	}
	return true;
}

// Java BeshmundirsWalkAI.java:83-94
bool BeshmundirsWalkAI::isAGroupMemberInInstance(Player& player) {
	if (player.isInGroup()) {
		for (const runtime::Ptr<AionObject>& object : player.getPlayerGroup()->getMembers()) { // Java: List<Player>
			runtime::Ptr<Player> member = runtime::cast<Player>(object);
			if (member->getWorldId() == 300170000) {
				return true;
			}
		}
	} else {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ENTER_ONLY_PARTY_DON());
	}
	return false;
}

// Java BeshmundirsWalkAI.java:96-101
void BeshmundirsWalkAI::moveToInstance(Player& player, int8_t difficult) {
	const PortalPath* portalPath = DataManager::PORTAL2_DATA->getPortalUsePath(getNpcId(), player);
	if (portalPath != nullptr) {
		PortalService::port(portalPath, player, getOwner(), difficult);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
