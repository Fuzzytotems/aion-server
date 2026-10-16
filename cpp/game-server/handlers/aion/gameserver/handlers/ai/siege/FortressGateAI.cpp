#include "aion/gameserver/handlers/ai/siege/FortressGateAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIRequest.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/model/templates/siegelocation/DoorRepairData.h"
#include "aion/gameserver/model/templates/siegelocation/DoorRepairStone.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(FortressGateAI, "fortressgate");

/** Java: the anonymous AIRequest of handleDialogStart (FortressGateAI.java:35-44, fieldmap ai.siege.FortressGateAI$1); it captures nothing */
struct FortressGateAI_AIRequest final : AIRequest {
	AION_MAKE_REF_FRIEND

	static runtime::Ref<FortressGateAI_AIRequest> create() { return runtime::makeRef<FortressGateAI_AIRequest>(); }

	// Java FortressGateAI.java:37-43 (Java's cast (Npc) fortressGate)
	void acceptRequest(runtime::Ptr<Creature> fortressGate, Player& responder, int32_t requestId) override {
		static_cast<void>(requestId);
		if (PositionUtil::isInTalkRange(responder, *runtime::cast<Npc>(*fortressGate))) {
			TeleportService::moveToTargetWithDistance(*fortressGate, responder, PositionUtil::isBehind(responder, *fortressGate) ? 0 : 1, 3);
		} else {
			PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_FAR_FROM_NPC());
		}
	}

protected:
	FortressGateAI_AIRequest() = default;
	~FortressGateAI_AIRequest() override = default;
};

// Java FortressGateAI.java:33-46
void FortressGateAI::handleDialogStart(Player& player) {
	runtime::Ref<FortressGateAI_AIRequest> request = FortressGateAI_AIRequest::create();
	AIActions::addRequest(*this, player, SM_QUESTION_WINDOW::STR_ASK_PASS_BY_GATE, *request);
}

// Java FortressGateAI.java:48-55
bool FortressGateAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
		case AIQuestion::REWARD_AP:
			return true;
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::ALLOW_RESPAWN:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

// Java FortressGateAI.java:57-72
void FortressGateAI::handleDied() {
	if (runtime::Ptr<SiegeSpawnTemplate> spawnTemplate = runtime::as<SiegeSpawnTemplate>(getSpawnTemplate())) {
		const model::templates::siegelocation::DoorRepairData* repairData = SiegeService::getInstance().getDoorRepairData(spawnTemplate->getSiegeId());
		if (repairData != nullptr) {
			for (const model::templates::siegelocation::DoorRepairStone* stone : repairData->getRepairStones()) {
				if (stone->getDoorId() == getSpawnTemplate()->getStaticId()) {
					runtime::Ptr<VisibleObject> obj = getPosition()->getWorldMapInstance()->getObjectByStaticId(stone->getStaticId());
					if (obj != nullptr)
						obj->getController().delete_();
					break;
				}
			}
		}
	}
	NpcAI::handleDied();
}

} // namespace aion::gameserver::handlers::ai::siege
