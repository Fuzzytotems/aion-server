#pragma once

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * C++: writeImpl reads the connection (enableCryptKey), but SM_KEY is not a PER_RECIPIENT broadcast packet: only AionConnection::initialized
 * sends it, serialized for that connection, and the IO strand enables the crypt key after writing it (SerializedBody::enablesCrypt,
 * runtime-architecture.md §8.5).
 *
 * @author -Nemesiss-
 */
class SM_KEY : public AionServerPacket {
public:
	/** Java: implicit default constructor */
	SM_KEY();

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
