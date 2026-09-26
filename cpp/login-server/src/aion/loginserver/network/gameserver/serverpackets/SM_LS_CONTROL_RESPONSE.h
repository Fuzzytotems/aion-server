#pragma once

#include <cstdint>

#include "aion/loginserver/network/gameserver/GsServerPacket.h"

namespace aion::loginserver::network::gameserver::serverpackets {

/**
 * Result of a CM_LS_CONTROL request (access level or membership change).
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.serverpackets.SM_LS_CONTROL_RESPONSE
 *
 * @author Aionchs-Wylovech
 */
class SM_LS_CONTROL_RESPONSE : public GsServerPacket {
public:
	SM_LS_CONTROL_RESPONSE(int8_t type, int8_t param, int32_t accountId, int32_t adminId, bool result) noexcept
		: type(type), param(param), accountId(accountId), adminId(adminId), result(result) {}

protected:
	void writeImpl(GsConnection& con, commons::utils::ByteBuffer& buf) const override {
		writeC(buf, 4);
		writeC(buf, type);
		writeC(buf, param);
		writeD(buf, accountId);
		writeD(buf, adminId);
		writeC(buf, result ? 1 : 0);
	}

private:
	const int8_t type, param;
	const int32_t accountId, adminId;
	const bool result;
};

} // namespace aion::loginserver::network::gameserver::serverpackets
