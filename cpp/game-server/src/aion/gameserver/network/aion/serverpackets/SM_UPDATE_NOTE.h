#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author xavier
 */
class SM_UPDATE_NOTE : public AionServerPacket {
private:
	int32_t targetObjId{};
	std::string note{};
public:
	explicit SM_UPDATE_NOTE(model::gameobjects::player::Player& player);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
