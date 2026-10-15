#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Avol, ATracer
 */
class SM_EXCHANGE_ADD_ITEM : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::player::Player> player{};
	int32_t action{};
	runtime::Ref<model::gameobjects::Item> item{};

public:
	SM_EXCHANGE_ADD_ITEM(int32_t action, model::gameobjects::Item& item, model::gameobjects::player::Player& player);
	~SM_EXCHANGE_ADD_ITEM() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
