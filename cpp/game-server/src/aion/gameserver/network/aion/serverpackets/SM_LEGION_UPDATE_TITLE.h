#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author sweetkr
 */
class SM_LEGION_UPDATE_TITLE : public AionServerPacket {
private:
	int32_t playerObjectId{};
	int32_t legionId{};
	std::string legionName{};
	model::team::legion::LegionRank rank{};
public:
	SM_LEGION_UPDATE_TITLE(int32_t playerObjectId, int32_t legionId, std::string_view legionName, model::team::legion::LegionRank rank);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
