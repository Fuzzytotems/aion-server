#pragma once

#include <cstdint>

#include "aion/loginserver/network/aion/AionServerPacket.h"
#include "aion/loginserver/network/aion/SessionKey.h"

namespace aion::loginserver::network::aion::serverpackets {

/**
 * This packet is send to client to update sessionKey [for fast reconnection feature]
 * <p>
 * Java: com.aionemu.loginserver.network.aion.serverpackets.SM_UPDATE_SESSION
 *
 * @author -Nemesiss-
 */
class SM_UPDATE_SESSION : public AionServerPacket {
public:
	/** @param key session key */
	explicit SM_UPDATE_SESSION(const SessionKey& key) noexcept : AionServerPacket(0x0c), accountId(key.accountId), loginOk(key.loginOk) {}

protected:
	void writeImpl(LoginConnection& con, commons::utils::ByteBuffer& buf) const override {
		writeD(buf, accountId);
		writeD(buf, loginOk);
		writeC(buf, 0x00); // sysmsg if smth is wrong
	}

private:
	/** accountId is part of session key - its used for security purposes */
	const int32_t accountId;
	/** loginOk is part of session key - its used for security purposes */
	const int32_t loginOk;
};

} // namespace aion::loginserver::network::aion::serverpackets
