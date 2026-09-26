#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemAddType.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author kosyachok, -Nemesiss-
 */
class SM_WAREHOUSE_ADD_ITEM : public AionServerPacket {
private:
	int32_t warehouseType{};
	std::vector<runtime::Ref<model::gameobjects::Item>> items{};
	runtime::Ref<model::gameobjects::player::Player> player{};
	services::item::ItemPacketService_ItemAddType addType{}; // Java ItemPacketService.ItemAddType (generated enum)

public:
	SM_WAREHOUSE_ADD_ITEM(model::gameobjects::Item& item, int32_t warehouseType, model::gameobjects::player::Player& player,
		services::item::ItemPacketService_ItemAddType addType);
	~SM_WAREHOUSE_ADD_ITEM() override;
protected:
	void writeImpl(AionConnection* con) override;
private:
	void writeItemInfo(model::gameobjects::Item& item);
};

} // namespace aion::gameserver::network::aion::serverpackets
