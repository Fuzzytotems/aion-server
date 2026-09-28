#include "aion/gameserver/network/aion/clientpackets/CM_QUESTION_RESPONSE.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/ExchangeService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;

CM_QUESTION_RESPONSE::CM_QUESTION_RESPONSE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_QUESTION_RESPONSE.java:27-36
void CM_QUESTION_RESPONSE::readImpl() {
	questionid = readD();

	response = readUC(); // y/n
	readC(); // unk 0x00 - 0x01 ?
	readH();
	senderid = readD();
	readD();
	readH();
}

// Java CM_QUESTION_RESPONSE.java:39-44
void CM_QUESTION_RESPONSE::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (player->isTrading() && response != 0) // answered request with yes during exchange
		services::ExchangeService::getInstance().cancelExchange(*player);
	player->getResponseRequester().respond(questionid, response);
}

AION_CLIENT_PACKET(CM_QUESTION_RESPONSE);

} // namespace aion::gameserver::network::aion::clientpackets
