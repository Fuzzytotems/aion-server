#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "aion/loginserver/network/gameserver/GsServerPacket.h"

namespace aion::loginserver::network::gameserver::serverpackets {

/**
 * In this packet LoginServer is answering on GameServer ban request
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.serverpackets.SM_BAN_RESPONSE
 *
 * @author Watson
 */
class SM_BAN_RESPONSE : public GsServerPacket {
public:
	SM_BAN_RESPONSE(int8_t type, int32_t accountId, std::string ip, int32_t time, int32_t adminObjId, bool result)
		: type(type), accountId(accountId), ip(std::move(ip)), time(time), adminObjId(adminObjId), result(result) {}

protected:
	void writeImpl(GsConnection& con, commons::utils::ByteBuffer& buf) const override {
		writeC(buf, 5);
		writeC(buf, type);
		writeD(buf, accountId);
		writeS(buf, ip);
		writeD(buf, time);
		writeD(buf, adminObjId);
		writeC(buf, result ? 1 : 0);
	}

private:
	const int8_t type;
	const int32_t accountId;
	const std::string ip;
	const int32_t time;
	const int32_t adminObjId;
	const bool result;
};

} // namespace aion::loginserver::network::gameserver::serverpackets
