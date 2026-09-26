#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ATracer
 */
class SM_ITEM_USAGE_ANIMATION : public AionServerPacket {
private:
	int32_t playerObjId{};
	int32_t targetObjId{};
	int32_t itemObjId{};
	int32_t itemId{};
	int32_t time{};
	int32_t end{};
	int32_t unk{};
	int32_t unk1{};
	int32_t unk2{1};
	int32_t unk3{};

public:
	SM_ITEM_USAGE_ANIMATION(int32_t playerObjId, int32_t itemObjId, int32_t itemId);
	SM_ITEM_USAGE_ANIMATION(int32_t playerObjId, int32_t itemObjId, int32_t itemId, int32_t time, int32_t end);
	SM_ITEM_USAGE_ANIMATION(int32_t playerObjId, int32_t itemObjId, int32_t itemId, int32_t time, int32_t end, int32_t unk);
	SM_ITEM_USAGE_ANIMATION(int32_t playerObjId, int32_t targetObjId, int32_t itemObjId, int32_t itemId, int32_t time, int32_t end, int32_t unk);
	SM_ITEM_USAGE_ANIMATION(int32_t playerObjId, int32_t targetObjId, int32_t itemObjId, int32_t itemId, int32_t time, int32_t end, int32_t unk,
		int32_t unk1, int32_t unk2, int32_t unk3);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
