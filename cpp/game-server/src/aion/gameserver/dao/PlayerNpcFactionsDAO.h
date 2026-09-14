#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author MrPoke
 */
class PlayerNpcFactionsDAO {
public:
	static void loadNpcFactions(model::gameobjects::player::Player& player);
	static void storeNpcFactions(model::gameobjects::player::Player& player);
private:
	static void insertNpcFaction(int32_t playerObjectId, model::gameobjects::player::npcFaction::NpcFaction& faction);
	static void updateNpcFaction(int32_t playerObjectId, model::gameobjects::player::npcFaction::NpcFaction& faction);
};

} // namespace aion::gameserver::dao
