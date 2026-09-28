#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Simple
 */
class SM_LEGION_UPDATE_MEMBER : public AionServerPacket {
private:
	runtime::Ref<model::team::legion::LegionMember> legionMember{};
	int32_t msgId{};
	std::string text{};
public:
	SM_LEGION_UPDATE_MEMBER(model::gameobjects::player::Player& player, int32_t msgId, std::string_view text);
	SM_LEGION_UPDATE_MEMBER(model::team::legion::LegionMember& legionMember, int32_t msgId, std::string_view text);
	explicit SM_LEGION_UPDATE_MEMBER(model::team::legion::LegionMember& legionMember);
	~SM_LEGION_UPDATE_MEMBER() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
