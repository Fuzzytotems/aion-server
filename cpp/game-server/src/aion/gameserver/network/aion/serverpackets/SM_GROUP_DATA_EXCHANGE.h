#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author xTz
 */
class SM_GROUP_DATA_EXCHANGE : public AionServerPacket {
private:
	std::vector<int8_t> byteData{};
	int32_t action{};
	int32_t unk2{};

public:
	SM_GROUP_DATA_EXCHANGE(std::span<const uint8_t> byteData, int32_t action, int32_t unk2);
	explicit SM_GROUP_DATA_EXCHANGE(std::span<const uint8_t> byteData);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
