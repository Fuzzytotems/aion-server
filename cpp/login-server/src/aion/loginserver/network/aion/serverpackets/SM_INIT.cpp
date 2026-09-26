#include "aion/loginserver/network/aion/serverpackets/SM_INIT.h"

#include <span>

#include "aion/loginserver/network/aion/LoginConnection.h"

namespace aion::loginserver::network::aion::serverpackets {

namespace {

std::vector<uint8_t> toVector(std::span<const uint8_t> bytes) {
	return {bytes.begin(), bytes.end()};
}

} // namespace

SM_INIT::SM_INIT(const LoginConnection& client, const ncrypt::KeyGen::BlowfishKey& blowfishKey)
	: AionServerPacket(0x00), sessionId(client.getSessionId()), publicRsaKey(toVector(client.getEncryptedModulus())), blowfishKey(blowfishKey) {}

void SM_INIT::writeImpl(LoginConnection& con, commons::utils::ByteBuffer& buf) const {
	writeD(buf, sessionId); // session id
	writeD(buf, 0x0000c621); // protocol revision
	writeB(buf, publicRsaKey); // RSA Public Key, 128 bytes
	writeB(buf, std::array<uint8_t, 16>{}); // 0, spacer?
	writeB(buf, blowfishKey); // BlowFish key, 16 bytes
	writeB(buf, std::array<uint8_t, 7>{}); // 0, spacer?
	writeC(buf, 0); // test server id (100)
	writeD(buf, 0); // test server ip (1632857679 = 79.110.83.97)
	writeH(buf, 0); // test server port (7777)
	writeC(buf, 0); // 0, flag?
	writeD(buf, 0x3FCE09ED); // unk (old 0xD98E9655)
	writeD(buf, 0); // unk
}

} // namespace aion::loginserver::network::aion::serverpackets
