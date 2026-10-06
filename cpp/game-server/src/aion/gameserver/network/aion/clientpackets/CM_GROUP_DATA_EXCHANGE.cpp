#include "aion/gameserver/network/aion/clientpackets/CM_GROUP_DATA_EXCHANGE.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "aion/commons/utils/NetworkUtils.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceGroup.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_DATA_EXCHANGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_GROUP_DATA_EXCHANGE::CM_GROUP_DATA_EXCHANGE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

using model::gameobjects::player::Player;
using serverpackets::SM_GROUP_DATA_EXCHANGE;
using utils::PacketSendUtility;

// Java CM_GROUP_DATA_EXCHANGE.java:36-44
void CM_GROUP_DATA_EXCHANGE::readImpl() {
	action = readUC();
	if (action != 1) {
		groupType = readUC();
		unk2 = readUC();
	}
	int32_t dataSize = readD();
	data = readB(dataSize);
}

// Java CM_GROUP_DATA_EXCHANGE.java:47-88
void CM_GROUP_DATA_EXCHANGE::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (!player || data.empty())
		return;
	if (static_cast<int32_t>(data.size()) > MAX_EXCHANGE_DATA_SIZE) {
		commons::utils::ByteBuffer buffer = commons::utils::ByteBuffer::wrap(data);
		commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_GROUP_DATA_EXCHANGE")
			.error("Player {} exceeded maximum exchange data size (action: {}, groupType: {}, unk2: {}, bytes send: {}): \n{}", player->toString(), action,
				groupType, unk2, data.size(), commons::utils::NetworkUtils::toHex(buffer));
		return;
	}
	if (action == 1) {
		PacketSendUtility::broadcastPacketAndReceive(*player, SM_GROUP_DATA_EXCHANGE(data));
		return;
	}
	std::vector<runtime::Ptr<Player>> players;
	bool hasPlayers = false; // Java: players != null
	switch (groupType) {
		case 0:
			if (player->isInGroup()) {
				players = player->getPlayerGroup()->getOnlineMembers();
				hasPlayers = true;
			}
			break;
		case 1:
			if (player->isInAlliance()) {
				players = player->getPlayerAllianceGroup()->getOnlineMembers();
				hasPlayers = true;
			}
			break;
		case 2:
			if (player->isInLeague()) {
				players = player->getPlayerAllianceGroup()->getOnlineMembers();
				hasPlayers = true;
			}
			break;
	}
	if (!hasPlayers || players.empty())
		return;
	SM_GROUP_DATA_EXCHANGE packet(data, action, unk2);
	for (const runtime::Ptr<Player>& member : players) {
		if (!member->equals(*player))
			PacketSendUtility::sendPacket(*member, packet);
	}
}

AION_CLIENT_PACKET(CM_GROUP_DATA_EXCHANGE);

} // namespace aion::gameserver::network::aion::clientpackets
