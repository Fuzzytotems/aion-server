#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/storage/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Sweetkr
 */
class SM_CUBE_UPDATE : public AionServerPacket {
private:
	int32_t action{};
	int32_t actionValue{};
	int32_t itemsCount{};
	int32_t npcExpands{};
	int32_t questExpands{};
	int32_t itemExpands{};

public:
	static SM_CUBE_UPDATE stigmaSlots(int32_t slots);
	static SM_CUBE_UPDATE cubeSize(model::items::storage::StorageType type, model::gameobjects::player::Player& player);

private:
	SM_CUBE_UPDATE(int32_t action, int32_t actionValue, int32_t itemsCount, int32_t npcExpands, int32_t questExpands, int32_t itemExpands);
	SM_CUBE_UPDATE(int32_t action, int32_t actionValue);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
