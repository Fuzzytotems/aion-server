#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Response for CM_RECONNECT_AUTH with key that will be use for authentication at LoginServer.
 *
 * @author -Nemesiss-
 */
class SM_RECONNECT_KEY : public AionServerPacket {
private:
	int32_t key{};
public:
	/** Constructs new <tt>SM_RECONNECT_KEY</tt> packet */
	explicit SM_RECONNECT_KEY(int32_t key);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
