#pragma once

#include <cstdint>

#include "aion/loginserver/network/gameserver/GsServerPacket.h"

namespace aion::loginserver::network::gameserver::serverpackets {

/**
 * Requests the number of characters of an account on the game server (answered with CM_GS_CHARACTER).
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.serverpackets.SM_GS_CHARACTER_RESPONSE
 *
 * @author cura
 */
class SM_GS_CHARACTER_RESPONSE : public GsServerPacket {
public:
	explicit SM_GS_CHARACTER_RESPONSE(int32_t accountId) noexcept : accountId(accountId) {}

protected:
	void writeImpl(GsConnection& con, commons::utils::ByteBuffer& buf) const override {
		writeC(buf, 8);
		writeD(buf, accountId);
	}

private:
	const int32_t accountId;
};

} // namespace aion::loginserver::network::gameserver::serverpackets
