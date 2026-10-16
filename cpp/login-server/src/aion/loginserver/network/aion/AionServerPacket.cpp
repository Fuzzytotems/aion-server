#include "aion/loginserver/network/aion/AionServerPacket.h"

#include "aion/loginserver/network/aion/LoginConnection.h"

namespace aion::loginserver::network::aion {

void AionServerPacket::write(LoginConnection& con, commons::utils::ByteBuffer& buf) const {
	writeH(buf, 0);
	writeC(buf, getOpCode());
	writeImpl(con, buf);
	// Java: buf.flip(); buf.putShort((short) 0); ByteBuffer b = buf.slice(); con.encrypt(b) encrypts b.limit() - 2 bytes starting at offset 2
	const int32_t payloadSize = buf.position() - 2;
	const int32_t size = static_cast<int16_t>(con.encrypt(buf.span().subspan(2), payloadSize - 2) + 2);
	buf.putShort(0, static_cast<int16_t>(size));
	buf.position(0).limit(size);
}

} // namespace aion::loginserver::network::aion
