#include "aion/loginserver/network/aion/AionServerPacket.h"

#include "aion/loginserver/network/aion/LoginConnection.h"

namespace aion::loginserver::network::aion {

void AionServerPacket::write(LoginConnection& con, commons::utils::ByteBuffer& buf) const {
	writeH(buf, 0);
	writeC(buf, getOpCode());
	writeImpl(con, buf);
	const int32_t size = static_cast<int16_t>(con.encrypt(buf.span().subspan(2), buf.position() - 2) + 2);
	buf.putShort(0, static_cast<int16_t>(size));
	buf.position(0).limit(size);
}

} // namespace aion::loginserver::network::aion
