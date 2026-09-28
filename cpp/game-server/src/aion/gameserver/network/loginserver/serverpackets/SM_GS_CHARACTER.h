#pragma once

#include <cstdint>

#include "aion/gameserver/network/loginserver/LsServerPacket.h"
#include "aion/gameserver/network/loginserver/serverpackets/fwd.h"

namespace aion::gameserver::network::loginserver::serverpackets {

/**
 * @author cura
 */
class SM_GS_CHARACTER : public LsServerPacket {
private:
	int32_t accountId;
	int32_t characterCount;

public:
	SM_GS_CHARACTER(int32_t accountId, int32_t characterCount);

protected:
	void writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::loginserver::serverpackets
