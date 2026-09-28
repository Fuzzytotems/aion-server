#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Simple, cura, Neon
 */
class SM_LEGION_SEND_EMBLEM : public AionServerPacket {
private:
	int32_t legionId{};
	int32_t emblemId{};
	int8_t emblemType{};
	int32_t emblemDataSize{};
	int8_t color_a{};
	int8_t color_r{};
	int8_t color_g{};
	int8_t color_b{};
	std::string legionName{};
public:
	SM_LEGION_SEND_EMBLEM(int32_t legionId, model::team::legion::LegionEmblem& emblem, int32_t emblemDataSize, std::string_view legionName);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
