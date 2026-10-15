#pragma once

#include <cstdint>

#include "aion/loginserver/network/aion/AionAuthResponse.h"
#include "aion/loginserver/network/aion/AionServerPacket.h"

namespace aion::loginserver::network::aion::serverpackets {

/**
 * Sent to notify the client that he was kicked from loginserver
 * <p>
 * Java: com.aionemu.loginserver.network.aion.serverpackets.SM_ACCOUNT_KICK
 *
 * @author Neon
 */
class SM_ACCOUNT_KICK final : public AionServerPacket {
public:
	explicit SM_ACCOUNT_KICK(AionAuthResponse msg) noexcept : AionServerPacket(0x08), msgId(getId(msg)) {}

protected:
	void writeImpl(LoginConnection& con, commons::utils::ByteBuffer& buf) const override {
		writeD(buf, msgId); // reason
	}

private:
	const int32_t msgId;
};

} // namespace aion::loginserver::network::aion::serverpackets
