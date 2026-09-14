#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ATracer
 */
class SM_UI_SETTINGS : public AionServerPacket {
private:
	std::vector<uint8_t> data{}; // fieldmap: Java byte[] as bytes (hub-headers.md §6, writeB)
	int32_t type{};
public:
	/** Constructs new <tt>SM_CHARACTER_UI </tt> packet */
	SM_UI_SETTINGS(std::span<const uint8_t> data, int32_t type);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
