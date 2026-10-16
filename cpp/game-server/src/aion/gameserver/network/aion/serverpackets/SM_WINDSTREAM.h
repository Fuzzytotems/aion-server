#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

class SM_WINDSTREAM : public AionServerPacket {
private:
	int32_t unk1{};
	int32_t unk2{};
public:
	SM_WINDSTREAM(int32_t unk1, int32_t unk2);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
