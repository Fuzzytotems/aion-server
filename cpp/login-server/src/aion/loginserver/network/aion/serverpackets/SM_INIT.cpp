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
	writeD(buf, sessionId);
	writeD(buf, PROTOCOL_REVISION);
	writeB(buf, publicRsaKey); // scrambled RSA modulus, 128 bytes
	writeB(buf, std::array<uint8_t, 16>{}); // skipped by the client
	writeB(buf, blowfishKey); // 16 bytes
	writeC(buf, 0); // login mode: 0 password; with 3 or 4 the client reads 3 more bytes and logs in with an external token
}

} // namespace aion::loginserver::network::aion::serverpackets
