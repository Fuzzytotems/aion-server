#include "aion/gameserver/dao/PlayerNpcFactionsDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view SELECT_QUERY = "SELECT `faction_id`, `active`, `time`, `state`, `quest_id` FROM player_npc_factions WHERE `player_id`=?";
constexpr std::string_view INSERT_QUERY = "INSERT INTO player_npc_factions (`player_id`, `faction_id`, `active`, `time`, `state`, `quest_id`) VALUES (?,?,?,?,?,?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE player_npc_factions SET `active`=?, `time`=?, `state`=?, `quest_id`=?  WHERE `player_id`=? AND `faction_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerNpcFactionsDAO");

void PlayerNpcFactionsDAO::loadNpcFactions(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerNpcFactionsDAO::storeNpcFactions(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerNpcFactionsDAO::insertNpcFaction(int32_t playerObjectId, model::gameobjects::player::npcFaction::NpcFaction& faction) {
	AION_UNPORTED();
}

void PlayerNpcFactionsDAO::updateNpcFaction(int32_t playerObjectId, model::gameobjects::player::npcFaction::NpcFaction& faction) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
