#include "aion/gameserver/network/loginserver/LsServerPacket.h"

namespace aion::gameserver::network::loginserver {

LsServerPacket::LsServerPacket(int32_t opcode) : BaseServerPacket(opcode) {
}

LsServerPacket::~LsServerPacket() = default;

void LsServerPacket::write(LoginServerConnection* con, commons::utils::ByteBuffer& buffer) {
	writeH(buffer, 0);
	writeC(buffer, getOpCode());
	writeImpl(con, buffer);
	buffer.flip();
	buffer.putShort(static_cast<int16_t>(buffer.limit()));
	buffer.position(0);
}

} // namespace aion::gameserver::network::loginserver
