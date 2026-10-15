#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Lyahim
 */
class SM_SHOW_NPC_ON_MAP : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::player::Player> player{};
	int32_t npcid{};
	int32_t worldid{};
	float x{};
	float y{};
	float z{};
public:
	SM_SHOW_NPC_ON_MAP(model::gameobjects::player::Player& player, int32_t npcid, int32_t worldid, float x, float y, float z);
	~SM_SHOW_NPC_ON_MAP() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
