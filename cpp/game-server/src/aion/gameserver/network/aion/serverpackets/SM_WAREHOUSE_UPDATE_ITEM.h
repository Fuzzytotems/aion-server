#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author kosyachok, -Nemesiss-
 */
class SM_WAREHOUSE_UPDATE_ITEM : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::player::Player> player{};
	runtime::Ref<model::gameobjects::Item> item{};
	int32_t warehouseType{};
	services::item::ItemPacketService_ItemUpdateType updateType{}; // Java ItemPacketService.ItemUpdateType (generated enum)

public:
	SM_WAREHOUSE_UPDATE_ITEM(model::gameobjects::player::Player& player, model::gameobjects::Item& item, int32_t warehouseType,
		services::item::ItemPacketService_ItemUpdateType updateType);
	~SM_WAREHOUSE_UPDATE_ITEM() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
