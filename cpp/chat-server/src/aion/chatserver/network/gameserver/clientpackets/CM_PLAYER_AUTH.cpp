#include "aion/chatserver/network/gameserver/clientpackets/CM_PLAYER_AUTH.h"

#include "aion/chatserver/model/ChatClient.h"
#include "aion/chatserver/model/Race.h"
#include "aion/chatserver/network/gameserver/serverpackets/SM_PLAYER_AUTH_RESPONSE.h"
#include "aion/chatserver/service/ChatService.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::chatserver::network::gameserver::clientpackets {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.network.gameserver.clientpackets.CM_PLAYER_AUTH"));
	return *logger;
}

} // namespace

void CM_PLAYER_AUTH::readImpl() {
	playerId = readD();
	accName = readS();
	nick = readS();
	raceId = readD();
	accessLevel = readC();
}

void CM_PLAYER_AUTH::runImpl() {
	try {
		std::shared_ptr<model::ChatClient> chatClient =
			service::ChatService::getInstance().registerPlayer(playerId, accName, nick, model::getById(raceId), accessLevel);
		sendPacket(std::make_shared<serverpackets::SM_PLAYER_AUTH_RESPONSE>(*chatClient));
	} catch (const service::NoSuchAlgorithmException& e) {
		log().error("Error registering player {} on ChatServer", playerId, e);
	}
}

} // namespace aion::chatserver::network::gameserver::clientpackets
