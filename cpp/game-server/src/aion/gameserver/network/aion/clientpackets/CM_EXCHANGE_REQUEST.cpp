#include "aion/gameserver/network/aion/clientpackets/CM_EXCHANGE_REQUEST.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/DeniedStatus.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/ExchangeService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::Creature;
using model::gameobjects::player::DeniedStatus;
using model::gameobjects::player::Player;
using model::gameobjects::player::RequestResponseHandler;
using serverpackets::SM_QUESTION_WINDOW;
using serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_EXCHANGE_REQUEST");

namespace {

// fieldmap-class: com.aionemu.gameserver.network.aion.clientpackets.CM_EXCHANGE_REQUEST$1
/**
 * Java: the anonymous RequestResponseHandler<Player> of runImpl (CM_EXCHANGE_REQUEST.java:78-89), the answer to
 * SM_QUESTION_WINDOW.STR_EXCHANGE_DO_YOU_ACCEPT_EXCHANGE, stored in the asked player's ResponseRequester until he answers or leaves the world
 * (denyAll). It captures nothing; the requester (the asking player) is the base's. Java's generic parameter is Player, so the bridge method
 * casts the requester: runtime::cast (a requester that is not a Player is Java's ClassCastException; the packet only ever passes the asker).
 */
class CM_EXCHANGE_REQUEST_RequestResponseHandler final : public RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<CM_EXCHANGE_REQUEST_RequestResponseHandler> create(Player& activePlayer) {
		return runtime::makeRef<CM_EXCHANGE_REQUEST_RequestResponseHandler>(activePlayer);
	}

	// Java CM_EXCHANGE_REQUEST.java:80-83
	void acceptRequest(runtime::Ptr<Creature> requesterValue, Player& responder) override {
		services::ExchangeService::getInstance().registerExchange(*runtime::cast<Player>(requesterValue), responder);
	}

	// Java CM_EXCHANGE_REQUEST.java:85-88
	void denyRequest(runtime::Ptr<Creature> requesterValue, Player& responder) override {
		PacketSendUtility::sendPacket(*runtime::cast<Player>(requesterValue), SM_SYSTEM_MESSAGE::STR_EXCHANGE_HE_REJECTED_EXCHANGE(responder.getName()));
	}

protected:
	explicit CM_EXCHANGE_REQUEST_RequestResponseHandler(Player& activePlayer) : RequestResponseHandler(runtime::Ptr<Creature>(activePlayer)) {}
	~CM_EXCHANGE_REQUEST_RequestResponseHandler() override = default;
};

} // namespace

CM_EXCHANGE_REQUEST::CM_EXCHANGE_REQUEST(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_EXCHANGE_REQUEST.java:34-36
void CM_EXCHANGE_REQUEST::readImpl() {
	targetObjectId = readD();
}

// Java CM_EXCHANGE_REQUEST.java:39-99
void CM_EXCHANGE_REQUEST::runImpl() {
	const runtime::Ptr<Player> activePlayer = getConnection()->getActivePlayer();
	const runtime::Ptr<Player> targetPlayer = world::World::getInstance().getPlayer(targetObjectId.value());

	if (!targetPlayer || activePlayer->equals(*targetPlayer)) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_EXCHANGE_NO_ONE_TO_EXCHANGE());
		return;
	}

	if (activePlayer->isDead() || targetPlayer->isDead()) {
		log.warn("CM_EXCHANGE_REQUEST dead players target from {} to {}", activePlayer->getObjectId(), *targetObjectId);
		return;
	}

	if (!utils::PositionUtil::isInRange(*activePlayer, *targetPlayer, 5)) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_EXCHANGE_TOO_FAR_TO_EXCHANGE());
		return;
	}

	if (activePlayer->isInAnyHide()) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_EXCHANGE_CANT_EXCHANGE_WHILE_INVISIBLE());
		return;
	}

	if (targetPlayer->isInAnyHide()) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_EXCHANGE_CANT_EXCHANGE_WITH_INVISIBLE_USER());
		return;
	}

	if (activePlayer->getRace() != targetPlayer->getRace()) {
		log.info("[AUDIT] Player " + activePlayer->getName() + " tried trade with player (" + targetPlayer->getName() + ") another race.");
		return;
	}

	if (targetPlayer->getPlayerSettings()->isInDeniedStatus(DeniedStatus::TRADE)) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_REJECTED_TRADE(targetPlayer->getName()));
		return;
	}

	runtime::Ref<CM_EXCHANGE_REQUEST_RequestResponseHandler> responseHandler = CM_EXCHANGE_REQUEST_RequestResponseHandler::create(*activePlayer);

	bool requested = targetPlayer->getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_EXCHANGE_DO_YOU_ACCEPT_EXCHANGE, responseHandler);
	if (requested) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_EXCHANGE_ASKED_EXCHANGE_TO_HIM(targetPlayer->getName()));
		PacketSendUtility::sendPacket(*targetPlayer,
			SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_EXCHANGE_DO_YOU_ACCEPT_EXCHANGE, 0, 0, activePlayer->getName()));
	} else {
		sendPacket(SM_SYSTEM_MESSAGE::STR_EXCHANGE_CANT_ASK_WHEN_HE_IS_ASKED_QUESTION(targetPlayer->getName()));
	}
}

AION_CLIENT_PACKET(CM_EXCHANGE_REQUEST);

} // namespace aion::gameserver::network::aion::clientpackets
