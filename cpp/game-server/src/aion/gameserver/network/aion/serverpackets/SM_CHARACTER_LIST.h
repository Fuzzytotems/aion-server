#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/AbstractPlayerInfoPacket.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * In this packet Server is sending Character List to client.
 *
 * @author Nemesiss, AEJTester, Neon
 */
class SM_CHARACTER_LIST : public AbstractPlayerInfoPacket {
private:
	int32_t playOk2{};

public:
	/** Constructs new <tt>SM_CHARACTER_LIST </tt> packet */
	explicit SM_CHARACTER_LIST(int32_t playOk2);
	/** writeImpl reads the connection: serialized per recipient (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
