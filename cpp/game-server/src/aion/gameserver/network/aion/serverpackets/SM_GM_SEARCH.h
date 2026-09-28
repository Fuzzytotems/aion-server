#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

class SM_GM_SEARCH : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::player::Player> player{};

public:
	explicit SM_GM_SEARCH(model::gameobjects::player::Player& player);
	~SM_GM_SEARCH() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
