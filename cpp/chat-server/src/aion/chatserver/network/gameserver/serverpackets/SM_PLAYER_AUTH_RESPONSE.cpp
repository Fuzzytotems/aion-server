#include "aion/chatserver/network/gameserver/serverpackets/SM_PLAYER_AUTH_RESPONSE.h"

#include "aion/chatserver/model/ChatClient.h"

namespace aion::chatserver::network::gameserver::serverpackets {

SM_PLAYER_AUTH_RESPONSE::SM_PLAYER_AUTH_RESPONSE(const model::ChatClient& chatClient) : playerId(chatClient.getClientId()), token(chatClient.getToken()) {
}

void SM_PLAYER_AUTH_RESPONSE::writeImpl(GsConnection* con, commons::utils::ByteBuffer& buf) const {
	writeC(buf, 1);
	writeD(buf, playerId);
	writeC(buf, static_cast<int32_t>(token.size()));
	writeB(buf, token);
}

} // namespace aion::chatserver::network::gameserver::serverpackets
