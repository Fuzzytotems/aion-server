#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Simple
 */
class SM_LEGION_UPDATE_SELF_INTRO : public AionServerPacket {
private:
	std::string selfintro{};
	int32_t playerObjId{};
public:
	SM_LEGION_UPDATE_SELF_INTRO(int32_t playerObjId, std::string_view selfintro);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
