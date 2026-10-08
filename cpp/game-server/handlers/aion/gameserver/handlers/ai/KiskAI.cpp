#include "aion/gameserver/handlers/ai/KiskAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIRequest.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/KiskService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::handlers::ai {

AION_AI(KiskAI, "kisk");

/**
 * Java: the anonymous AIRequest of handleDialogStart (KiskAI.java:57-76, fieldmap ai.KiskAI$1, K4), stored through AIActions.addRequest in the
 * player's ResponseRequester until he answers STR_ASK_REGISTER_BINDSTONE. It keeps its decisionTaken flag and captures the AI, whose getOwner()
 * is the kisk.
 */
struct KiskAI_AIRequest final : AIRequest {
	AION_MAKE_REF_FRIEND

	runtime::Field<bool> decisionTaken{false}; // private boolean decisionTaken (KiskAI.java:59)
	const runtime::Ref<Kisk> kisk;             // captured this KiskAI (line 65): its getOwner()

	static runtime::Ref<KiskAI_AIRequest> create(Kisk& kiskValue) { return runtime::makeRef<KiskAI_AIRequest>(kiskValue); }

	// Java KiskAI.java:61-71
	void acceptRequest(runtime::Ptr<Creature> requester, Player& responder, int32_t requestId) override {
		static_cast<void>(requester);
		static_cast<void>(requestId);
		if (!decisionTaken.get()) {
			// Check again if it's full (If they waited to press OK)
			if (!kisk->canBind(responder)) {
				PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_CANNOT_REGISTER_BINDSTONE_HAVE_NO_AUTHORITY());
				return;
			}
			KiskService::getInstance().onBind(*kisk, responder);
		}
	}

	// Java KiskAI.java:73-76
	void denyRequest(runtime::Ptr<Creature> requester, Player& responder) override {
		static_cast<void>(requester);
		static_cast<void>(responder);
		decisionTaken.set(true);
	}

protected:
	explicit KiskAI_AIRequest(Kisk& kiskValue) : kisk(kiskValue) {}
	~KiskAI_AIRequest() override = default;
};

// Java KiskAI.java:28-31
Kisk& KiskAI::getKiskOwner() const {
	return static_cast<Kisk&>(getOwner());
}

// Java KiskAI.java:33-37
void KiskAI::handleAttack(runtime::Ptr<Creature> creature) {
	static_cast<void>(creature);
	if (getLifeStats()->isFullyRestoredHp()) {
		SM_SYSTEM_MESSAGE message = SM_SYSTEM_MESSAGE::STR_BINDSTONE_IS_ATTACKED();
		getKiskOwner().broadcastPacket(message);
	}
}

// Java KiskAI.java:39-47
void KiskAI::handleDespawned() {
	KiskService::getInstance().removeKisk(getKiskOwner());
	if (isDead()) {
		SM_SYSTEM_MESSAGE message = SM_SYSTEM_MESSAGE::STR_BINDSTONE_IS_DESTROYED();
		getKiskOwner().broadcastPacket(message);
	} else {
		SM_SYSTEM_MESSAGE message = SM_SYSTEM_MESSAGE::STR_BINDSTONE_IS_REMOVED();
		getKiskOwner().broadcastPacket(message);
	}
	NpcAI::handleDespawned();
}

// Java KiskAI.java:49-83
void KiskAI::handleDialogStart(Player& player) {
	runtime::Ptr<Kisk> current = player.getKisk();
	if (current != nullptr && current->equals(getKiskOwner())) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_BINDSTONE_ALREADY_REGISTERED());
		return;
	}

	if (getKiskOwner().canBind(player)) {
		runtime::Ref<KiskAI_AIRequest> request = KiskAI_AIRequest::create(getKiskOwner());
		AIActions::addRequest(*this, player, SM_QUESTION_WINDOW::STR_ASK_REGISTER_BINDSTONE, *request);
	} else if (getKiskOwner().getCurrentMemberCount() >= getKiskOwner().getMaxMembers())
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANNOT_REGISTER_BINDSTONE_FULL());
	else
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANNOT_REGISTER_BINDSTONE_HAVE_NO_AUTHORITY());
}

// Java KiskAI.java:85-92
bool KiskAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
		case AIQuestion::REMOVE_EFFECTS_ON_MAP_REGION_DEACTIVATE:
			return false;
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
			return true;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai
