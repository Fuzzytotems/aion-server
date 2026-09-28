#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author LokiReborn
 */
class SM_WINDSTREAM_ANNOUNCE : public AionServerPacket {
private:
	int32_t bidirectional{};
	int32_t mapId{};
	int32_t streamId{};
	int32_t state{};
public:
	SM_WINDSTREAM_ANNOUNCE(int32_t bidirectional, int32_t mapId, int32_t streamId, int32_t state);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
