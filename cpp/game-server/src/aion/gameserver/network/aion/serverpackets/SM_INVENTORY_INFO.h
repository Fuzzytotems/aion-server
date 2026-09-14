#pragma once

#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * In this packet Server is sending Inventory Info
 *
 * @author -Nemesiss-, alexa026, Avol ;d, ATracer, Rolandas, Artur
 */
class SM_INVENTORY_INFO : public AionServerPacket {
private:
	bool isFirstPacket{};
	std::vector<runtime::Ref<model::gameobjects::Item>> items{};
	runtime::Ref<model::gameobjects::player::Player> player{};

public:
	SM_INVENTORY_INFO(bool isFirstPacket, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items,
		model::gameobjects::player::Player& player);
	~SM_INVENTORY_INFO() override;

protected:
	void writeImpl(AionConnection* con) override;

private:
	void writeItemInfo(model::gameobjects::Item& item);
};

} // namespace aion::gameserver::network::aion::serverpackets
