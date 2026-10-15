#include "aion/gameserver/handlers/ai/portals/HouseGateAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIRequest.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_TELEPORT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::handlers::ai::portals {

AION_AI(HouseGateAI, "housegate");

// Java: the loop over player.findZones() of handleDialogStart (lines 61-67) and of acceptRequest (lines 82-88): false as soon as one zone of the
// player forbids the return to battle
bool HouseGateAI::canReturnToBattle(Player& player) {
	for (const runtime::Ptr<ZoneInstance>& zone : player.findZones()) {
		if (!zone->canReturnToBattle())
			return false;
	}
	return true;
}

/**
 * Java: the anonymous AIRequest of handleDialogStart (HouseGateAI.java:72-105, fieldmap ai.portals.HouseGateAI$1), stored through
 * AIActions.addRequest until the player answers. It captures the house of the gate's creator and keeps Java's `decided` flag.
 */
struct HouseGateAI_AIRequest final : AIRequest {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<model::house::House> house; // captured local House house (line 42)
	runtime::Field<bool> decided{false};           // Java: private boolean decided = false (line 74)

	static runtime::Ref<HouseGateAI_AIRequest> create(model::house::House& house) { return runtime::makeRef<HouseGateAI_AIRequest>(house); }

	// Java HouseGateAI.java:76-99
	void acceptRequest(runtime::Ptr<Creature> requester, Player& responder, int32_t requestId) override {
		static_cast<void>(requester);
		static_cast<void>(requestId);
		if (decided.get())
			return;

		runtime::Ptr<WorldMapInstance> instance = InstanceService::getOrCreateHouseInstance(*house);
		if (!HouseGateAI::canReturnToBattle(responder)) {
			responder.setBattleReturnCoords(0, nullptr);
		} else {
			PacketSendUtility::sendPacket(responder,
			                              network::aion::serverpackets::SM_HOUSE_TELEPORT(house->getAddress()->getId(), responder.getObjectId()));
			runtime::Ref<runtime::Array<float>> coords = runtime::Array<float>::make(3);
			(*coords)[0] = responder.getX();
			(*coords)[1] = responder.getY();
			(*coords)[2] = responder.getZ();
			responder.setBattleReturnCoords(responder.getWorldId(), coords);
		}
		TeleportService::teleportTo(responder, *instance, house->getX(), house->getY(), house->getZ(), house->getTeleportHeading(),
		                            TeleportAnimation::JUMP_IN_GATE);
		decided.set(true);
	}

	// Java HouseGateAI.java:101-104
	void denyRequest(runtime::Ptr<Creature> requester, Player& responder) override {
		static_cast<void>(requester);
		static_cast<void>(responder);
		decided.set(true);
	}

protected:
	explicit HouseGateAI_AIRequest(model::house::House& houseValue) : house(houseValue) {}
	~HouseGateAI_AIRequest() override = default;
};

// Java HouseGateAI.java:32-107
void HouseGateAI::handleDialogStart(Player& player) {
	const int32_t creatorId = getCreatorId();
	const runtime::Ptr<model::gameobjects::VisibleObject> creator = getCreator();
	bool isCreatorOrFriend = (creator && player.equals(*creator)) || static_cast<bool>(player.getFriendList().getFriend(creatorId)) ||
	                         (player.isInGroup() && player.getCurrentGroup()->hasMember(creatorId));
	if (!isCreatorOrFriend) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_HOUSING_TELEPORT_CANT_USE());
		return;
	}

	const runtime::Ptr<model::house::House> house = HousingService::getInstance().findActiveHouse(creatorId);
	// Uses skill but doesn't have house
	if (!house) {
		if (player.getObjectId() == creatorId)
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_HOUSING_TELEPORT_NEED_HOUSE());
		else
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_HOUSING_TELEPORT_CANT_USE());
		return;
	}

	if (!player.getCommonData()->isDaeva()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_ENTER_NO_RIGHT(house->getAddress()->getId()));
		return;
	}

	if (!house->canEnter(player)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_ENTER_NO_RIGHT2());
		return;
	}
	const bool returnBattle = canReturnToBattle(player);
	int32_t requestId = SM_QUESTION_WINDOW::STR_ASK_GROUP_GATE_DO_YOU_ACCEPT_MOVE;
	if (!returnBattle)
		requestId = SM_QUESTION_WINDOW::STR_HOUSE_GATE_ACCEPT_MOVE_DONT_RETURN;

	runtime::Ref<HouseGateAI_AIRequest> request = HouseGateAI_AIRequest::create(*house);
	gameserver::ai::AIActions::addRequest(*this, player, requestId, 9, *request);
}

} // namespace aion::gameserver::handlers::ai::portals
