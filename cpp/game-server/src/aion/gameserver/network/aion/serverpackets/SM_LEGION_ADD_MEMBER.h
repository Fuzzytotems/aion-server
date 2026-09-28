#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Simple
 */
class SM_LEGION_ADD_MEMBER : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::player::Player> player{};
	bool isMember{};
	int32_t msgId{};
	std::string text{};
public:
	SM_LEGION_ADD_MEMBER(model::gameobjects::player::Player& player, bool isMember, int32_t msgId, std::string_view text);
	~SM_LEGION_ADD_MEMBER() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
