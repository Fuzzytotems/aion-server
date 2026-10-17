#include "aion/gameserver/handlers/ai/portals/GroupGateAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIRequest.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::handlers::ai::portals {

AION_AI(GroupGateAI, "groupgate");

/**
 * Java: the anonymous AIRequest of handleDialogStart (GroupGateAI.java:42-72, fieldmap ai.portals.GroupGateAI$1), stored through
 * AIActions.addRequest until the player answers. Java's getNpcId() of the enclosing AI is the gate's npc id, captured at creation (it never
 * changes).
 */
struct GroupGateAI_AIRequest final : AIRequest {
	AION_MAKE_REF_FRIEND
	const int32_t npcId;                     // the enclosing AI's getNpcId()
	runtime::Field<bool> decisionTaken{false}; // Java: private boolean decisionTaken = false (line 43)

	static runtime::Ref<GroupGateAI_AIRequest> create(int32_t npcId) { return runtime::makeRef<GroupGateAI_AIRequest>(npcId); }

	// Java GroupGateAI.java:45-66
	void acceptRequest(runtime::Ptr<Creature> requester, Player& responder, int32_t requestId) override {
		static_cast<void>(requester);
		static_cast<void>(requestId);
		if (!decisionTaken.get()) {
			switch (npcId) {
				// Group Gates
				case 833208:
				case 749017:
					TeleportService::teleportTo(responder, 110010000, 1444.9f, 1577.2f, 572.9f, int8_t{0}, TeleportAnimation::JUMP_IN);
					break;
				case 833207:
				case 749083:
					TeleportService::teleportTo(responder, 120010000, 1657.5f, 1398.7f, 194.7f, int8_t{0}, TeleportAnimation::JUMP_IN);
					break;
				// Binding Group Gates
				case 749131:
				case 749132:
					TeleportService::moveToBindLocation(responder);
					break;
			}
			decisionTaken.set(true);
		}
	}

	// Java GroupGateAI.java:68-71
	void denyRequest(runtime::Ptr<Creature> requester, Player& responder) override {
		static_cast<void>(requester);
		static_cast<void>(responder);
		decisionTaken.set(true);
	}

protected:
	explicit GroupGateAI_AIRequest(int32_t npcIdValue) : npcId(npcIdValue) {}
	~GroupGateAI_AIRequest() override = default;
};

// Java GroupGateAI.java:30-75
void GroupGateAI::handleDialogStart(Player& player) {
	if (!player.getCommonData()->isDaeva()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_USE_GROUPGATE_BEFORE_CHANGE_CLASS());
		return;
	}
	const runtime::Ptr<model::gameobjects::VisibleObject> creator = getCreator();
	if (!(creator && player.equals(*creator)) && (!player.isInGroup() || !player.getPlayerGroup()->hasMember(getCreatorId()))) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_USE_GROUPGATE_NO_RIGHT());
		return;
	}
	runtime::Ref<GroupGateAI_AIRequest> request = GroupGateAI_AIRequest::create(getNpcId());
	gameserver::ai::AIActions::addRequest(*this, player, SM_QUESTION_WINDOW::STR_ASK_GROUP_GATE_DO_YOU_ACCEPT_MOVE, CANCEL_DIALOG_METERS,
	                                      *request);
}

} // namespace aion::gameserver::handlers::ai::portals
