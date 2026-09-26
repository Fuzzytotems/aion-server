#pragma once

#include <cstdint>

#include "aion/gameserver/network/chatserver/CsServerPacket.h"
#include "aion/gameserver/network/chatserver/serverpackets/fwd.h"

namespace aion::gameserver::network::chatserver::serverpackets {

/**
 * @author ATracer, Neon
 */
class SM_CS_AUTH : public CsServerPacket {
public:
	SM_CS_AUTH();

protected:
	void writeImpl(ChatServerConnection* con, commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::chatserver::serverpackets
