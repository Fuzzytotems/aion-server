#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Sweetkr
 */
class SM_FORCED_MOVE : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::Creature> creature{};
	int32_t objectId{};
	float x{};
	float y{};
	float z{};

public:
	SM_FORCED_MOVE(model::gameobjects::Creature& creature, model::gameobjects::Creature& target);
	SM_FORCED_MOVE(model::gameobjects::Creature& creature, int32_t objectId, float x, float y, float z);
	~SM_FORCED_MOVE() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
