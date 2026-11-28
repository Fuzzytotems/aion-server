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
	/** Shows only "Your account has been blocked." */
	SM_ACCOUNT_BANNED() noexcept : SM_ACCOUNT_BANNED(0, 0) {}

	/**
	 * @param reason
	 *          code of a STR_L2AUTH_BLOCK_* message (1 STR_L2AUTH_BLOCK_SELF, 26 STR_L2AUTH_BLOCK_BOT, 27 STR_L2AUTH_BLOCK_ABUSE, ...), or 0
	 * @param flags
	 *          bit mask; the lowest set bit selects a STR_PLAYNC_BLOCK_* message (bit 0 STR_PLAYNC_BLOCK_SELF, bit 1 STR_PLAYNC_BLOCK_BOT_AUTO, ...).
	 *          Used if the reason is 0.
	 */
	SM_ACCOUNT_BANNED(int32_t reason, int32_t flags) noexcept : AionServerPacket(0x02), reason(reason), flags(flags) {}

protected:
	void writeImpl(LoginConnection& con, commons::utils::ByteBuffer& buf) const override {
		writeD(buf, reason);
		writeD(buf, flags);
	}

private:
	const int32_t reason;
	const int32_t flags;
};

} // namespace aion::loginserver::network::aion::serverpackets
