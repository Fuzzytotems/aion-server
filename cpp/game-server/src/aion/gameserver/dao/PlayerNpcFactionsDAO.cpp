#include "aion/gameserver/dao/PlayerNpcFactionsDAO.h"

#include <memory>
#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/ENpcFactionQuestState.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFaction.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using model::gameobjects::Persistable;
using model::gameobjects::player::npcFaction::ENpcFactionQuestState;
using model::gameobjects::player::npcFaction::NpcFaction;

namespace {

constexpr std::string_view SELECT_QUERY = "SELECT `faction_id`, `active`, `time`, `state`, `quest_id` FROM player_npc_factions WHERE `player_id`=?";
constexpr std::string_view INSERT_QUERY = "INSERT INTO player_npc_factions (`player_id`, `faction_id`, `active`, `time`, `state`, `quest_id`) VALUES (?,?,?,?,?,?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE player_npc_factions SET `active`=?, `time`=?, `state`=?, `quest_id`=?  WHERE `player_id`=? AND `faction_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerNpcFactionsDAO");

void PlayerNpcFactionsDAO::loadNpcFactions(model::gameobjects::player::Player& player) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, player.getObjectId());
		auto rset = stmt->executeQuery();
		player.setNpcFactions(std::make_unique<model::gameobjects::player::npcFaction::NpcFactions>(player));
		model::gameobjects::player::npcFaction::NpcFactions& factions = player.getNpcFactions();
		while (rset->next()) {
			int32_t faction_id = rset->getInt("faction_id");
			bool active = rset->getBoolean("active");
			int32_t time = rset->getInt("time");
			int32_t questId = rset->getInt("quest_id");
			ENpcFactionQuestState state = detail::enumValueOf<ENpcFactionQuestState>(rset->getString("state"),
				"com.aionemu.gameserver.model.gameobjects.player.npcFaction.ENpcFactionQuestState");
			runtime::Ref<NpcFaction> faction = NpcFaction::create(faction_id, time, active, state, questId);
			faction->setPersistentState(Persistable::PersistentState::UPDATED);
			factions.addNpcFaction(*faction);
		}
	} catch (const std::exception& e) {
		log.error("Could not restore Npc faction data for playerObjId: " + std::to_string(player.getObjectId()) + " from DB: " + e.what(), e);
	}
}

void PlayerNpcFactionsDAO::storeNpcFactions(model::gameobjects::player::Player& player) {
	for (const runtime::Ptr<NpcFaction>& npcFaction : player.getNpcFactions().getNpcFactions()) {
		switch (npcFaction->getPersistentState()) {
			case Persistable::PersistentState::NEW:
				insertNpcFaction(player.getObjectId(), *npcFaction);
				break;
			case Persistable::PersistentState::UPDATE_REQUIRED:
				updateNpcFaction(player.getObjectId(), *npcFaction);
				break;
			default:
				break;
		}
	}
}

void PlayerNpcFactionsDAO::insertNpcFaction(int32_t playerObjectId, model::gameobjects::player::npcFaction::NpcFaction& faction) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(INSERT_QUERY);
		stmt->setInt(1, playerObjectId);
		stmt->setInt(2, faction.getId());
		stmt->setBoolean(3, faction.isActive());
		stmt->setInt(4, faction.getTime());
		stmt->setString(5, detail::enumName(faction.getState()));
		stmt->setInt(6, faction.getQuestId());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Could not insert Npc faction data for playerObjId: " + std::to_string(playerObjectId) + " from DB: " + e.what(), e);
	}
}

void PlayerNpcFactionsDAO::updateNpcFaction(int32_t playerObjectId, model::gameobjects::player::npcFaction::NpcFaction& faction) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_QUERY);
		stmt->setBoolean(1, faction.isActive());
		stmt->setInt(2, faction.getTime());
		stmt->setString(3, detail::enumName(faction.getState()));
		stmt->setInt(4, faction.getQuestId());
		stmt->setInt(5, playerObjectId);
		stmt->setInt(6, faction.getId());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Could not update Npc faction data for playerObjId: " + std::to_string(playerObjectId) + " from DB: " + e.what(), e);
	}
}

} // namespace aion::gameserver::dao
