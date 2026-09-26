#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author cura
 */
class SM_CHARACTER_SELECT : public AionServerPacket {
private:
	int32_t type{};
	int16_t messageType{};
	int32_t wrongCount{};

public:
	explicit SM_CHARACTER_SELECT(int32_t type);
	SM_CHARACTER_SELECT(int32_t type, int16_t messageType, int32_t wrongCount);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
