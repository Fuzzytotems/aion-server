#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/network/aion/AionServerPacket.h"

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The client's party UI data: action 1 to the known list and the sender, any other action to the other online members of the group or alliance group (C_GROUP_DATA_EXCHANGE).
 *
 * @author xTz
 */
class CM_GROUP_DATA_EXCHANGE : public AionClientPacket {
private:
	/**
	 * Maximum size of the exchange data. The size is determined by subtracting the maximum usable packet body size in bytes by the overhead bytes
	 * required to send it via SM_GROUP_DATA_EXCHANGE
	 */
	static constexpr int32_t MAX_EXCHANGE_DATA_SIZE = AionServerPacket::MAX_USABLE_PACKET_BODY_SIZE - 6;
	int32_t groupType{};
	int32_t action{};
	int32_t unk2{};
	std::vector<uint8_t> data;

public:
	CM_GROUP_DATA_EXCHANGE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
