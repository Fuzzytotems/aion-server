#include "aion/gameserver/network/chatserver/serverpackets/SM_CS_PLAYER_AUTH.h"

#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/detail/EnumIds.h"

namespace aion::gameserver::network::chatserver::serverpackets {

SM_CS_PLAYER_AUTH::SM_CS_PLAYER_AUTH(model::gameobjects::player::Player& player)
	: CsServerPacket(0x01), playerId(player.getObjectId()), accName(player.getAccountName()), nick(player.getName(true)),
		raceId(detail::raceIdOf(player.getRace())), accessLevel(player.getAccount()->getAccessLevel()) {
}

void SM_CS_PLAYER_AUTH::writeImpl(ChatServerConnection* con, commons::utils::ByteBuffer& buf) {
	writeD(buf, playerId);
	writeS(buf, accName);
	writeS(buf, nick);
	writeD(buf, raceId);
	writeC(buf, accessLevel);
}

} // namespace aion::gameserver::network::chatserver::serverpackets
