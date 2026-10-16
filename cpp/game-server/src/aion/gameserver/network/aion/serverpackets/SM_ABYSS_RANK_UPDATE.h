#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Nemiroff Date: 17.02.2010
 */
class SM_ABYSS_RANK_UPDATE : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::player::Player> player{};
	int32_t action{};

public:
	SM_ABYSS_RANK_UPDATE(int32_t action, model::gameobjects::player::Player& player);
	~SM_ABYSS_RANK_UPDATE() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
