#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * This packet is notify client what map should be loaded.
 *
 * @author -Nemesiss-
 */
class SM_PLAYER_SPAWN : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::player::Player> player{};
public:
	explicit SM_PLAYER_SPAWN(model::gameobjects::player::Player& player);
	~SM_PLAYER_SPAWN() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
