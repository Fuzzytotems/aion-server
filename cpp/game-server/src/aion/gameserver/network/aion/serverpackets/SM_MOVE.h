#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * This packet is displaying movement of players etc.
 *
 * @author -Nemesiss-
 */
class SM_MOVE : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::Creature> creature{};
	int8_t movementMask{};
public:
	explicit SM_MOVE(model::gameobjects::Creature& creature);
	SM_MOVE(model::gameobjects::Creature& creature, int8_t movementMask);
	~SM_MOVE() override;
protected:
	void writeImpl(AionConnection* client) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
