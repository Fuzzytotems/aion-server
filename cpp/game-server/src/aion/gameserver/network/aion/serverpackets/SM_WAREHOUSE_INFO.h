#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author kosyachok
 */
class SM_WAREHOUSE_INFO : public AionServerPacket {
private:
	int32_t warehouseType{};
	std::vector<runtime::Ref<model::gameobjects::Item>> itemList{};
	bool firstPacket{};
	int32_t expandLvl{};
	runtime::Ref<model::gameobjects::player::Player> player{};
public:
	/** @param items the items of this part; Java null (the closing packet of a warehouse) is the empty vector (Java Collections.emptyList()) */
	SM_WAREHOUSE_INFO(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items, int32_t warehouseType, int32_t expandLvl, bool firstPacket,
		model::gameobjects::player::Player& player);
	~SM_WAREHOUSE_INFO() override;
protected:
	void writeImpl(AionConnection* con) override;
private:
	void writeItemInfo(model::gameobjects::Item& item);
};

} // namespace aion::gameserver::network::aion::serverpackets
