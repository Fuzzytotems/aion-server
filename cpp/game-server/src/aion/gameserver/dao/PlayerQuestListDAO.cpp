#include "aion/gameserver/dao/PlayerQuestListDAO.h"

#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;
namespace Types = commons::database::Types;
using model::gameobjects::Persistable;
using questEngine::model::QuestState;
using questEngine::model::QuestStatus;

namespace {

constexpr std::string_view SELECT_QUERY = "SELECT `quest_id`, `status`, `quest_vars`, `flags`, `complete_count`, `next_repeat_time`, `reward`, `complete_time` FROM `player_quests` WHERE `player_id`=?";
constexpr std::string_view UPDATE_QUERY = "UPDATE `player_quests` SET `status`=?, `quest_vars`=?, `flags`=?, `complete_count`=?, `next_repeat_time`=?, `reward`=?, `complete_time`=? WHERE `player_id`=? AND `quest_id`=?";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_quests` WHERE `player_id`=? AND `quest_id`=?";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_quests` (`player_id`, `quest_id`, `status`, `quest_vars`, `flags`, `complete_count`, `next_repeat_time`, `reward`, `complete_time`) VALUES (?,?,?,?,?,?,?,?,?)";

/** Java states.stream().filter(Persistable.<state>) */
std::vector<runtime::Ptr<QuestState>> filterByState(const std::vector<runtime::Ptr<QuestState>>& states, Persistable::PersistentState state) {
	std::vector<runtime::Ptr<QuestState>> filtered;
	for (const runtime::Ptr<QuestState>& qs : states) {
		if (qs && qs->getPersistentState() == state)
			filtered.push_back(qs);
	}
	return filtered;
}

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerQuestListDAO");

runtime::Ref<model::gameobjects::player::QuestStateList> PlayerQuestListDAO::load(int32_t playerObjId) {
	runtime::Ref<model::gameobjects::player::QuestStateList> questStateList = model::gameobjects::player::QuestStateList::create();
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, playerObjId);
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			int32_t questId = rset->getInt("quest_id");
			int32_t questVars = rset->getInt("quest_vars");
			int32_t flags = rset->getInt("flags");
			int32_t completeCount = rset->getInt("complete_count");
			std::optional<commons::database::Timestamp> nextRepeatTime = rset->getTimestamp("next_repeat_time");
			std::optional<int32_t> reward = rset->getInt("reward");
			if (rset->wasNull())
				reward = std::nullopt;
			std::optional<commons::database::Timestamp> completeTime = rset->getTimestamp("complete_time");
			QuestStatus status = detail::enumValueOf<QuestStatus>(rset->getString("status"), "com.aionemu.gameserver.questEngine.model.QuestStatus");
			runtime::Ref<QuestState> questState = QuestState::create(questId, status, questVars, flags, completeCount, nextRepeatTime, reward, completeTime);
			questState->setPersistentState(Persistable::PersistentState::UPDATED);
			questStateList->addQuest(questId, *questState);
		}
	} catch (const std::exception& e) {
		log.error("Could not restore QuestStateList data for player: " + std::to_string(playerObjId) + " from DB: " + e.what(), e);
	}
	return questStateList;
}

void PlayerQuestListDAO::store(model::gameobjects::player::Player& player) {
	std::vector<runtime::Ptr<QuestState>> qsList = player.getQuestStateList()->getAllQuestState();
	runtime::HashSet<int32_t>& delQsList = player.getQuestStateList()->getDeletedQuestIds();
	std::vector<int32_t> deletedQuestIds = delQsList.snapshot();
	if (qsList.empty() && deletedQuestIds.empty())
		return;

	try {
		auto con = DatabaseFactory::getConnection();
		con->setAutoCommit(false);
		std::unordered_set<int32_t> questIds(deletedQuestIds.begin(), deletedQuestIds.end());
		deleteQuest(*con, player.getObjectId(), qsList, questIds);
		// Java: deleteQuest ends with questIds.clear() on the player's set unless it returned early (no deleted state and no deleted id); the
		// frozen parameter is a const copy of the ids, so the set is emptied here. D6 fix (docs/deviations/P4-14.md): only the snapshotted ids
		// are removed, so an id the player's thread deletes while the connection is acquired or the batch runs stays for the next store instead
		// of being dropped without its DELETE (its row would come back on the next login)
		if (!questIds.empty() || !filterByState(qsList, Persistable::PersistentState::DELETED).empty())
			delQsList.removeAll(deletedQuestIds);
		addQuests(*con, player.getObjectId(), qsList);
		updateQuests(*con, player.getObjectId(), qsList);
	} catch (const SQLException& e) {
		log.error("Can't save quests for player " + std::to_string(player.getObjectId()), e);
	}

	for (const runtime::Ptr<QuestState>& qs : qsList) {
		qs->setPersistentState(Persistable::PersistentState::UPDATED);
	}
}

void PlayerQuestListDAO::addQuests(commons::database::Connection& con, int32_t playerId, const std::vector<runtime::Ptr<questEngine::model::QuestState>>& states) {
	std::vector<runtime::Ptr<QuestState>> newStates = filterByState(states, Persistable::PersistentState::NEW);
	if (newStates.empty())
		return;

	try {
		auto ps = con.prepareStatement(INSERT_QUERY);
		for (const runtime::Ptr<QuestState>& qs : newStates) {
			ps->setInt(1, playerId);
			ps->setInt(2, qs->getQuestId());
			ps->setString(3, detail::enumName(qs->getStatus()));
			ps->setInt(4, qs->getQuestVars()->getQuestVars());
			ps->setInt(5, qs->getFlags());
			ps->setInt(6, qs->getCompleteCount());
			ps->setObject(7, qs->getNextRepeatTime(), Types::TIMESTAMP); // supports inserting null value
			ps->setObject(8, qs->getRewardGroup(), Types::SMALLINT); // supports inserting null value
			ps->setObject(9, qs->getLastCompleteTime(), Types::TIMESTAMP); // supports inserting null value
			ps->addBatch();
		}
		ps->executeBatch();
		con.commit();
	} catch (const SQLException&) {
		log.error("Failed to insert new quests for player " + std::to_string(playerId));
	}
}

void PlayerQuestListDAO::updateQuests(commons::database::Connection& con, int32_t playerId,
	const std::vector<runtime::Ptr<questEngine::model::QuestState>>& states) {
	std::vector<runtime::Ptr<QuestState>> changedStates = filterByState(states, Persistable::PersistentState::UPDATE_REQUIRED);
	if (changedStates.empty())
		return;

	try {
		auto ps = con.prepareStatement(UPDATE_QUERY);
		for (const runtime::Ptr<QuestState>& qs : changedStates) {
			ps->setString(1, detail::enumName(qs->getStatus()));
			ps->setInt(2, qs->getQuestVars()->getQuestVars());
			ps->setInt(3, qs->getFlags());
			ps->setInt(4, qs->getCompleteCount());
			ps->setObject(5, qs->getNextRepeatTime(), Types::TIMESTAMP); // supports inserting null value
			ps->setObject(6, qs->getRewardGroup(), Types::SMALLINT); // supports inserting null value
			ps->setObject(7, qs->getLastCompleteTime(), Types::TIMESTAMP); // supports inserting null value
			ps->setInt(8, playerId);
			ps->setInt(9, qs->getQuestId());
			ps->addBatch();
		}
		ps->executeBatch();
		con.commit();
	} catch (const SQLException&) {
		log.error("Failed to update existing quests for player " + std::to_string(playerId));
	}
}

void PlayerQuestListDAO::deleteQuest(commons::database::Connection& con, int32_t playerId,
	const std::vector<runtime::Ptr<questEngine::model::QuestState>>& states, const std::unordered_set<int32_t>& questIds) {
	std::vector<runtime::Ptr<QuestState>> deletedStates = filterByState(states, Persistable::PersistentState::DELETED);
	if (deletedStates.empty() && questIds.empty())
		return;

	try {
		auto ps = con.prepareStatement(DELETE_QUERY);
		for (const runtime::Ptr<QuestState>& qs : deletedStates) {
			ps->setInt(1, playerId);
			ps->setInt(2, qs->getQuestId());
			ps->addBatch();
		}
		for (int32_t questId : questIds) {
			ps->setInt(1, playerId);
			ps->setInt(2, questId);
			ps->addBatch();
		}
		ps->executeBatch();
		con.commit();
	} catch (const SQLException&) {
		log.error("Failed to delete existing quests for player " + std::to_string(playerId));
	}
	// Java: questIds.clear() (the frozen parameter is const: store() clears the player's set)
}

} // namespace aion::gameserver::dao
