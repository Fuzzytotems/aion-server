#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player names his private store (C_SHOP_MSG).
 * <p>
 * C++ only: `CM_PRIVATE_STORE_NAMETestAccess` (tests/cm_lz/PrivateStorePacketsTest.cpp) reads the name readImpl decoded, which Java keeps
 * private without a getter.
 *
 * @author Simple
 */
class CM_PRIVATE_STORE_NAME : public AionClientPacket {
	friend struct CM_PRIVATE_STORE_NAMETestAccess;

private:
	std::string name;

public:
	/**
	 * Constructs new instance of <tt>CM_PRIVATE_STORE</tt> packet
	 *
	 * @param opcode
	 */
	CM_PRIVATE_STORE_NAME(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
