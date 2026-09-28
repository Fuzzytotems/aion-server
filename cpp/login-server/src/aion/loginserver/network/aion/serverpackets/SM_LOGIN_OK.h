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
		writeD(buf, 0x00);
		writeD(buf, 0x00);
		writeD(buf, 0x000003ea);
		writeD(buf, 0x00);
		writeD(buf, 0x00);
		writeD(buf, 0x00);
		writeD(buf, 0x00);
		writeD(buf, 0x00);
		writeD(buf, 0x00);
		writeD(buf, 0x00);
		writeB(buf, std::array<uint8_t, 0x13>{});
	}

private:
	/** accountId is part of session key - its used for security purposes */
	const int32_t accountId;
	/** loginOk is part of session key - its used for security purposes */
	const int32_t loginOk;
};

} // namespace aion::loginserver::network::aion::serverpackets
