#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/loginserver/LsServerPacket.h"
#include "aion/gameserver/network/loginserver/serverpackets/fwd.h"

namespace aion::gameserver::network::loginserver::serverpackets {

/**
 * @author Aionchs-Wylovech
 */
class SM_LS_CONTROL : public LsServerPacket {
private:
	const int32_t type, param, accountId, adminId;

public:
	SM_LS_CONTROL(int32_t type, int32_t param, model::gameobjects::player::Player& player, model::gameobjects::player::Player& admin);

protected:
	void writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::loginserver::serverpackets
