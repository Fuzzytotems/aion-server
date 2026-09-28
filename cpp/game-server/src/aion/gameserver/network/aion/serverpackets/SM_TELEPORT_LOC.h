#pragma once

#include <cstdint>

#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * This packet is used to teleport player and port animation
 *
 * @author Luno, orz, xTz
 */
class SM_TELEPORT_LOC : public AionServerPacket {
private:
	int8_t portAnimation{};
	int32_t mapId{};
	int32_t instanceId{};
	float x{};
	float y{};
	float z{};
	int8_t heading{};
	bool isInstance{};
public:
	SM_TELEPORT_LOC(int32_t mapId, int32_t instanceId, float x, float y, float z, int8_t heading, model::animations::TeleportAnimation portAnimation);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
