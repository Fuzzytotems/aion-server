#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "aion/loginserver/network/aion/AionServerPacket.h"
#include "aion/loginserver/network/ncrypt/KeyGen.h"

namespace aion::loginserver::network::aion::serverpackets {

/**
 * The first packet sent to a client: session id, protocol revision, the scrambled RSA public key for the login data and the Blowfish key for all
 * further packets. It is encrypted with the static initial key (see CryptEngine).
 * <p>
 * Java: com.aionemu.loginserver.network.aion.serverpackets.SM_INIT
 */
class SM_INIT final : public AionServerPacket {
public:
	/** @param client the connection (its session id and scrambled modulus are copied) */
	SM_INIT(const LoginConnection& client, const ncrypt::KeyGen::BlowfishKey& blowfishKey);

protected:
	void writeImpl(LoginConnection& con, commons::utils::ByteBuffer& buf) const override;

private:
	/** Session Id of this connection */
	const int32_t sessionId;
	/** public Rsa key that client will use to encrypt login and password that will be send in RequestAuthLogin client packet. */
	const std::vector<uint8_t> publicRsaKey;
	/** blowfish key for packet encryption/decryption. */
	const ncrypt::KeyGen::BlowfishKey blowfishKey;
};

} // namespace aion::loginserver::network::aion::serverpackets
