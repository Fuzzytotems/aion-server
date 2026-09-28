#pragma once

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
 * @author ATracer
 */
class SM_INVENTORY_ADD_ITEM : public AionServerPacket {
private:
	std::vector<runtime::Ref<model::gameobjects::Item>> items{};
	runtime::Ref<model::gameobjects::player::Player> player{};
	services::item::ItemPacketService_ItemAddType addType{};

public:
	SM_INVENTORY_ADD_ITEM(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items, model::gameobjects::player::Player& player,
		services::item::ItemPacketService_ItemAddType addType);
	~SM_INVENTORY_ADD_ITEM() override;

protected:
	void writeImpl(AionConnection* con) override;

private:
	void writeItemInfo(model::gameobjects::Item& item);
};

} // namespace aion::gameserver::network::aion::serverpackets
