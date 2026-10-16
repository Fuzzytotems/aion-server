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
 * @author Avol, xTz
 */
class SM_VIEW_PLAYER_DETAILS : public AionServerPacket {
private:
	std::vector<runtime::Ref<model::gameobjects::Item>> items{};
	int32_t itemSize{};
	int32_t targetObjId{};
	runtime::Ref<model::gameobjects::player::Player> player{};
public:
	SM_VIEW_PLAYER_DETAILS(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items, model::gameobjects::player::Player& player);
	~SM_VIEW_PLAYER_DETAILS() override;
protected:
	void writeImpl(AionConnection* con) override;
private:
	void writeItemInfo(model::gameobjects::Item& item);
};

} // namespace aion::gameserver::network::aion::serverpackets
