#include "aion/gameserver/network/chatserver/CsServerPacket.h"

namespace aion::gameserver::network::chatserver {

CsServerPacket::CsServerPacket(int32_t opcode) : BaseServerPacket(opcode) {
}

CsServerPacket::~CsServerPacket() = default;

void CsServerPacket::write(ChatServerConnection* con, commons::utils::ByteBuffer& buffer) {
	writeH(buffer, 0);
	writeC(buffer, getOpCode());
	writeImpl(con, buffer);
	buffer.flip();
	buffer.putShort(static_cast<int16_t>(buffer.limit()));
	buffer.position(0);
}

} // namespace aion::gameserver::network::chatserver
