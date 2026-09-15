#include "aion/gameserver/network/loginserver/serverpackets/SM_LS_PONG.h"

namespace aion::gameserver::network::loginserver::serverpackets {

SM_LS_PONG::SM_LS_PONG() : LsServerPacket(12) {
}

void SM_LS_PONG::writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) {
}

} // namespace aion::gameserver::network::loginserver::serverpackets
