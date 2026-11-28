#pragma once

#include <array>
#include <cstdint>

#include "aion/loginserver/network/aion/AionServerPacket.h"
#include "aion/loginserver/network/aion/SessionKey.h"

namespace aion::loginserver::network::aion::serverpackets {

/**
 * The login was successful: sends the account id and loginOk part of the session key.
 * <p>
 * Java: com.aionemu.loginserver.network.aion.serverpackets.SM_LOGIN_OK
 *
 * @author -Nemesiss-
 */
class SM_LOGIN_OK : public AionServerPacket {
public:
	/** @param key session key */
	explicit SM_LOGIN_OK(const SessionKey& key) noexcept : AionServerPacket(0x03), accountId(key.accountId), loginOk(key.loginOk) {}

protected:
	void writeImpl(LoginConnection& con, commons::utils::ByteBuffer& buf) const override {
		writeD(buf, accountId);
		writeD(buf, loginOk);
		writeB(buf, std::array<uint8_t, 20>{}); // not used by the 4.8 client (it stores the last three ints, but never reads them)
		writeD(buf, 0); // penalty flags: 0x10 shows the internet cafe complaint penalty (STR_BLOCK_PC_ROOM_COMPLAIN2)
		writeB(buf, std::array<uint8_t, 16>{}); // not read by the 4.8 client
		writeD(buf, 0); // 1 shows the fatigue warning (STR_AUTH_WARNING_SNDA_FATIGUE) instead of requesting the server list
	}

private:
	/** accountId is part of session key - its used for security purposes */
	const int32_t accountId;
	/** loginOk is part of session key - its used for security purposes */
	const int32_t loginOk;
};

} // namespace aion::loginserver::network::aion::serverpackets
