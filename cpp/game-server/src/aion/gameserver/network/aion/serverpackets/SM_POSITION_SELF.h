#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Instantly moves the player to the given position and cancels movement on client side (just like {@link SM_POSITION}).
 * The client responds with {@link CM_POSITION_SELF} afterward.
 *
 * @author cura
 */
class SM_POSITION_SELF : public AionServerPacket {
private:
	float x{};
	float y{};
	float z{};
	int8_t heading{};
public:
	SM_POSITION_SELF(float x, float y, float z, int8_t heading);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
