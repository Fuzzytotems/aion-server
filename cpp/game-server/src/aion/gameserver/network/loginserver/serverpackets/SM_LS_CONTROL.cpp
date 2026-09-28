#include "aion/gameserver/network/loginserver/serverpackets/SM_LS_CONTROL.h"

#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::network::loginserver::serverpackets {

SM_LS_CONTROL::SM_LS_CONTROL(int32_t typeValue, int32_t paramValue, model::gameobjects::player::Player& player,
	model::gameobjects::player::Player& admin)
	: LsServerPacket(0x05), type(typeValue), param(paramValue), accountId(player.getAccount()->getId()), adminId(admin.getObjectId()) {
}

void SM_LS_CONTROL::writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) {
	writeC(buf, type);
	writeC(buf, param);
	writeD(buf, accountId);
	writeD(buf, adminId);
}

} // namespace aion::gameserver::network::loginserver::serverpackets
