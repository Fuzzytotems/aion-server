#pragma once

#include "aion/gameserver/network/loginserver/LsServerPacket.h"
#include "aion/gameserver/network/loginserver/serverpackets/fwd.h"

namespace aion::gameserver::network::loginserver::serverpackets {

/**
 * This is authentication packet that GS will send to login server for registration.
 *
 * @author -Nemesiss-, Neon
 */
class SM_GS_AUTH : public LsServerPacket {
public:
	SM_GS_AUTH();

protected:
	void writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::loginserver::serverpackets
