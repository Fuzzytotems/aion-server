#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemDeleteType.h"

namespace aion::gameserver::network::aion::serverpackets {

class SM_DELETE_ITEM : public AionServerPacket {
private:
	int32_t itemObjectId{};
	services::item::ItemPacketService_ItemDeleteType deleteType{};

public:
	explicit SM_DELETE_ITEM(int32_t itemObjectId);
	SM_DELETE_ITEM(int32_t itemObjectId, services::item::ItemPacketService_ItemDeleteType deleteType);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
