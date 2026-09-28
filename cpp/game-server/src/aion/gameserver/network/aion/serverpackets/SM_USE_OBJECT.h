#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ATracer
 */
class SM_USE_OBJECT : public AionServerPacket {
private:
	int32_t playerObjId{};
	int32_t targetObjId{};
	int32_t time{};
	int32_t actionType{};
public:
	SM_USE_OBJECT(int32_t playerObjId, int32_t targetObjId, int32_t time, int32_t actionType);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
