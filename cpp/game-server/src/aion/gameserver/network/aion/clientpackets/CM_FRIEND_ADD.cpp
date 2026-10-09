#include "aion/gameserver/network/aion/clientpackets/CM_FRIEND_ADD.h"

#include <memory>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/DeniedStatus.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/SocialService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::Creature;
using model::gameobjects::player::DeniedStatus;
using model::gameobjects::player::Player;
using model::gameobjects::player::RequestResponseHandler;
using serverpackets::SM_FRIEND_RESPONSE;
using serverpackets::SM_QUESTION_WINDOW;
using serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

namespace {

// fieldmap-class: com.aionemu.gameserver.network.aion.clientpackets.CM_FRIEND_ADD$1
/**
 * Java: the anonymous RequestResponseHandler<Player> of runImpl (CM_FRIEND_ADD.java:65-79), the answer to
 * SM_QUESTION_WINDOW.STR_BUDDYLIST_ADD_BUDDY_REQUEST, stored in the asked player's ResponseRequester until he answers or leaves the world. Java's
 * handler captures the packet (`this`) for denyRequest's `sendPacket`, which is `getConnection().sendPacket` (AionClientPacket.sendPacket): the
 * C++ handler holds the asker's connection instead, as a weak_ptr, so a handler waiting in the other player's ResponseRequester keeps neither the
 * packet nor the asker's connection alive (a connection that is gone is Java's send to a closed connection: nothing is sent).
 */
class CM_FRIEND_ADD_RequestResponseHandler final : public RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	const std::weak_ptr<AionConnection> connection; // captured this (the packet), for its sendPacket [captured this: packet reference]

	static runtime::Ref<CM_FRIEND_ADD_RequestResponseHandler> create(Player& activePlayer, const std::shared_ptr<AionConnection>& connection) {
		return runtime::makeRef<CM_FRIEND_ADD_RequestResponseHandler>(activePlayer, connection);
	}

	// Java CM_FRIEND_ADD.java:67-73
	void acceptRequest(runtime::Ptr<Creature> requesterValue, Player& responder) override {
		Player& requester = *runtime::cast<Player>(requesterValue); // Java's bridge cast of the generic Player requester
		if (requester.getFriendList().isFull())
			PacketSendUtility::sendPacket(responder, SM_FRIEND_RESPONSE::REQUESTER_LIST_FULL_CANT_ACCEPT(requester.getName()));
		else if (!responder.getFriendList().isFull())
			services::SocialService::makeFriends(requester, responder);
	}

	// Java CM_FRIEND_ADD.java:75-78
	// lint: L7 weak_ptr::lock() takes no monitor
	void denyRequest(runtime::Ptr<Creature> requesterValue, Player& responder) override {
		static_cast<void>(runtime::cast<Player>(requesterValue)); // Java's bridge cast
		if (const std::shared_ptr<AionConnection> con = connection.lock())
			con->sendPacket(SM_FRIEND_RESPONSE::TARGET_DENIED(responder.getName()));
	}

protected:
	CM_FRIEND_ADD_RequestResponseHandler(Player& activePlayer, const std::shared_ptr<AionConnection>& con)
		: RequestResponseHandler(runtime::Ptr<Creature>(activePlayer)), connection(con) {}
	~CM_FRIEND_ADD_RequestResponseHandler() override = default;
};

} // namespace

CM_FRIEND_ADD::CM_FRIEND_ADD(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_FRIEND_ADD.java:34-37
void CM_FRIEND_ADD::readImpl() {
	targetName = readS();
	message = readS();
}

// Java CM_FRIEND_ADD.java:40-92
void CM_FRIEND_ADD::runImpl() {
	const runtime::Ptr<Player> activePlayer = getConnection()->getActivePlayer();
	const runtime::Ptr<Player> targetPlayer = world::World::getInstance().getPlayer(utils::Util::convertName(targetName));
	if (targetPlayer == nullptr || !targetPlayer->isOnline()) {
		sendPacket(*SM_FRIEND_RESPONSE::TARGET_OFFLINE);
	} else if (activePlayer->equals(*targetPlayer)) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_BUDDYLIST_BUSY());
	} else if (configs::main::CustomConfig::FRIENDLIST_GM_RESTRICT.load() &&
		((targetPlayer->isStaff() && !activePlayer->isStaff()) || (activePlayer->isStaff() && !targetPlayer->isStaff()))) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_BUDDY_CANT_ADD_WHEN_HE_IS_ASKED_QUESTION(targetPlayer->getName(true)));
	} else if (activePlayer->getFriendList().getFriend(targetPlayer->getObjectId()) != nullptr) {
		sendPacket(*SM_FRIEND_RESPONSE::TARGET_ALREADY_FRIEND);
	} else if (activePlayer->getRace() != targetPlayer->getRace()) {
		sendPacket(*SM_FRIEND_RESPONSE::TARGET_NOT_FOUND);
	} else if (activePlayer->getBlockList()->contains(targetPlayer->getObjectId())) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_BUDDYLIST_NO_BLOCKED_CHARACTER());
	} else if (targetPlayer->getBlockList()->contains(activePlayer->getObjectId())) {
		sendPacket(*SM_FRIEND_RESPONSE::TARGET_BLOCKED_YOU);
	} else if (activePlayer->getFriendList().isFull()) {
		sendPacket(*SM_FRIEND_RESPONSE::LIST_FULL);
	} else if (targetPlayer->getFriendList().isFull()) {
		sendPacket(SM_FRIEND_RESPONSE::TARGET_LIST_FULL(targetPlayer->getName()));
	} else if (targetPlayer->getPlayerSettings()->isInDeniedStatus(DeniedStatus::FRIEND)) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_REJECTED_FRIEND(targetPlayer->getName()));
	} else {
		runtime::Ref<CM_FRIEND_ADD_RequestResponseHandler> responseHandler = CM_FRIEND_ADD_RequestResponseHandler::create(*activePlayer, getConnection());

		bool requested = targetPlayer->getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_BUDDYLIST_ADD_BUDDY_REQUEST, responseHandler);
		// If the player is busy and could not be asked
		if (!requested) {
			sendPacket(SM_SYSTEM_MESSAGE::STR_BUDDYLIST_BUSY());
			return;
		}

		// Send question packet to buddy
		PacketSendUtility::sendPacket(*targetPlayer, SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_BUDDYLIST_ADD_BUDDY_REQUEST, activePlayer->getObjectId(),
			0, activePlayer->getName(), message));
	}
}

AION_CLIENT_PACKET(CM_FRIEND_ADD);

} // namespace aion::gameserver::network::aion::clientpackets
