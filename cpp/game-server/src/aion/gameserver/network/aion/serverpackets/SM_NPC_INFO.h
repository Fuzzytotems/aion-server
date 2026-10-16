#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * This packet is displaying visible npc/monsters.
 *
 * @author -Nemesiss-
 */
class SM_NPC_INFO : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::Creature> npc{};
	int32_t creatorId{};
	std::string masterName{};
	model::CreatureType creatureType{};
public:
	SM_NPC_INFO(model::gameobjects::Npc& npc, model::gameobjects::player::Player& player);
	SM_NPC_INFO(model::gameobjects::Summon& summon, model::gameobjects::player::Player& player);
	~SM_NPC_INFO() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
