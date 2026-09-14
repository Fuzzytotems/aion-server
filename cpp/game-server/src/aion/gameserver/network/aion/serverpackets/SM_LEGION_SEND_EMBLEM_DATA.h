#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author cura
 */
class SM_LEGION_SEND_EMBLEM_DATA : public AionServerPacket {
private:
	int32_t size{};
	std::vector<uint8_t> data{}; // fieldmap: Java byte[] as bytes (hub-headers.md §6, writeB)
public:
	SM_LEGION_SEND_EMBLEM_DATA(int32_t size, std::span<const uint8_t> data);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
