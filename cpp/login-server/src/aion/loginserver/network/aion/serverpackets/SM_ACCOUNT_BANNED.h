#pragma once

#include "aion/loginserver/network/aion/AionServerPacket.h"

namespace aion::loginserver::network::aion::serverpackets {

/**
 * Sent to notify that the account was banned.
 * Player will see a dialog box saying
 * <pre>
 * Your account has been blocked.
 * Your account rights are limited.
 * You can find further information on the official support website (http://support.aionfreetoplay.com).
 * </pre>
 * Java: com.aionemu.loginserver.network.aion.serverpackets.SM_ACCOUNT_BANNED
 *
 * @author Neon
 */
class SM_ACCOUNT_BANNED final : public AionServerPacket {
public:
	SM_ACCOUNT_BANNED() noexcept : AionServerPacket(0x02) {}

protected:
	void writeImpl(LoginConnection& con, commons::utils::ByteBuffer& buf) const override {
		// maybe some option to specify a custom message (STR_L2AUTH_BLOCK_*)? need retail sniff
	}
};

} // namespace aion::loginserver::network::aion::serverpackets
