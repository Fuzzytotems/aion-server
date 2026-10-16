#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Simple, cura, Neon
 */
class SM_LEGION_UPDATE_EMBLEM : public AionServerPacket {
private:
	int32_t legionId{};
	int8_t emblemId{};
	int8_t color_a{};
	int8_t color_r{};
	int8_t color_g{};
	int8_t color_b{};
	int8_t emblemType{};
public:
	SM_LEGION_UPDATE_EMBLEM(int32_t legionId, model::team::legion::LegionEmblem& emblem);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
