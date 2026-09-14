#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ginho1
 */
class SM_SECURITY_TOKEN : public AionServerPacket {
private:
	std::vector<uint8_t> token{}; // fieldmap: Java byte[] as bytes (hub-headers.md §6, writeB)
public:
	explicit SM_SECURITY_TOKEN(std::span<const uint8_t> token);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
