#pragma once

#include "aion/loginserver/network/aion/AionServerPacket.h"

namespace aion::loginserver::network::aion::serverpackets {

/**
 * Sent to notify that the account was banned<br>
 * Player will see a dialog box saying
 * <pre>
 * Your account has been blocked.
 * </pre>
 * Java: com.aionemu.loginserver.network.aion.serverpackets.SM_ACCOUNT_BANNED_2
 *
 * @author Neon
 */
class SM_ACCOUNT_BANNED_2 final : public AionServerPacket {
public:
	SM_ACCOUNT_BANNED_2() noexcept : AionServerPacket(0x09) {}

protected:
	void writeImpl(LoginConnection& con, commons::utils::ByteBuffer& buf) const override {
		// maybe some option to specify a custom message (STR_L2AUTH_BLOCK_*)? need retail sniff
	}
};

} // namespace aion::loginserver::network::aion::serverpackets
