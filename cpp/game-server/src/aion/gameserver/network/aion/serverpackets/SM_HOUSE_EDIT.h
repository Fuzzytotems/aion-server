#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Rolandas
 */
class SM_HOUSE_EDIT : public AionServerPacket {
private:
	int32_t action{};
	int32_t storeId{};
	int32_t itemObjectId{};
	float x{};
	float y{};
	float z{};
	int32_t rotation{};

public:
	explicit SM_HOUSE_EDIT(int32_t action);
	SM_HOUSE_EDIT(int32_t action, int32_t storeId, int32_t itemObjectId);
	SM_HOUSE_EDIT(int32_t action, int32_t itemObjectId, float x, float y, float z, int32_t rotation);
	/** writeImpl reads the connection: serialized per recipient (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
