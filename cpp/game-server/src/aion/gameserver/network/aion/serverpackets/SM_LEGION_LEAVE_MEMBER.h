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
class SM_LEGION_LEAVE_MEMBER : public AionServerPacket {
private:
	std::string name{};
	std::string name1{};
	int32_t playerObjId{};
	int32_t msgId{};
public:
	SM_LEGION_LEAVE_MEMBER(int32_t msgId, int32_t playerObjId, std::string_view name);
	SM_LEGION_LEAVE_MEMBER(int32_t msgId, int32_t playerObjId, std::string_view name, std::string_view name1);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
