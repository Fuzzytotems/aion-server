#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

class SM_BIND_POINT_INFO : public AionServerPacket {
private:
	int32_t mapId{};
	float x{};
	float y{};
	float z{};
	int8_t bindPointType{};
	int32_t kiskObjId{};

public:
	SM_BIND_POINT_INFO(int32_t mapId, float x, float y, float z);
	explicit SM_BIND_POINT_INFO(runtime::Ptr<model::gameobjects::Kisk> kisk);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
