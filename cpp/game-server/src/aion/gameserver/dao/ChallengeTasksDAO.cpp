#include "aion/gameserver/dao/ChallengeTasksDAO.h"

#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/dataholders/ChallengeData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/challenge/ChallengeQuest.h"
#include "aion/gameserver/model/challenge/ChallengeTask.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/templates/challenge/ChallengeTaskTemplate.h"
#include "aion/gameserver/model/templates/challenge/ChallengeType.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;
using model::challenge::ChallengeQuest;
using model::challenge::ChallengeTask;
using model::gameobjects::Persistable;

namespace {

constexpr std::string_view SELECT_QUERY = "SELECT * FROM `challenge_tasks` WHERE `owner_id` = ? AND `owner_type` = ?";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `challenge_tasks` (`task_id`, `quest_id`, `owner_id`, `owner_type`, `complete_count`, `complete_time`) VALUES (?, ?, ?, ?, ?, ?);";
constexpr std::string_view UPDATE_QUERY = "UPDATE `challenge_tasks` SET `complete_count` = ?, `complete_time`= ? WHERE `task_id` = ? AND `quest_id` = ? AND `owner_id` = ?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.ChallengeTasksDAO");

std::unordered_map<int32_t, runtime::Ref<model::challenge::ChallengeTask>> ChallengeTasksDAO::load(int32_t ownerId,
	model::templates::challenge::ChallengeType type) {
	std::unordered_map<int32_t, runtime::Ref<ChallengeTask>> tasks;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, ownerId);
		stmt->setString(2, detail::enumName(type));
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			int32_t taskId = rset->getInt("task_id");
			int32_t questId = rset->getInt("quest_id");
			int32_t completeCount = rset->getInt("complete_count");
			std::optional<commons::database::Timestamp> date = rset->getTimestamp("complete_time");
			const model::templates::challenge::ChallengeQuestTemplate* questTemplate = dataholders::DataManager::CHALLENGE_DATA->getQuestByQuestId(questId);
			runtime::Ref<ChallengeQuest> quest = ChallengeQuest::create(questTemplate, completeCount);
			quest->setPersistentState(Persistable::PersistentState::UPDATED);
			auto it = tasks.find(taskId);
			if (it == tasks.end()) {
				std::unordered_map<int32_t, runtime::Ref<ChallengeQuest>> quests;
				quests.reserve(2);
				quests.insert_or_assign(quest->getQuestId(), quest);
				runtime::Ref<ChallengeTask> task = ChallengeTask::create(taskId, ownerId, std::move(quests), date);
				tasks.try_emplace(taskId, std::move(task));
			} else {
				it->second->getQuests().put(questId, quest);
			}
		}
	} catch (const SQLException& e) {
		log.error("Could not load " + detail::enumName(type) + " challenge tasks of owner " + std::to_string(ownerId), e);
	}
	return tasks;
}

void ChallengeTasksDAO::storeTask(model::challenge::ChallengeTask& task) {
	for (const auto& quest : task.getQuests().values().toVector()) {
		switch (quest->getPersistentState()) {
			case Persistable::PersistentState::NEW:
				insertQuestEntry(task, *quest);
				break;
			case Persistable::PersistentState::UPDATE_REQUIRED:
				updateQuestEntry(task, *quest);
				break;
			default:
				break;
		}
	}
}

void ChallengeTasksDAO::insertQuestEntry(model::challenge::ChallengeTask& task, model::challenge::ChallengeQuest& quest) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(INSERT_QUERY);
		stmt->setInt(1, task.getTaskId());
		stmt->setInt(2, quest.getQuestId());
		stmt->setInt(3, task.getOwnerId());
		stmt->setString(4, detail::enumName(task.getTemplate()->getType()));
		stmt->setInt(5, quest.getCompleteCount());
		stmt->setTimestamp(6, task.getCompleteTime());
		stmt->executeUpdate();
		quest.setPersistentState(Persistable::PersistentState::UPDATED);
	} catch (const SQLException& e) {
		log.error("Could not insert challenge task " + std::to_string(task.getTaskId()) + " of owner " + std::to_string(task.getOwnerId()), e);
	}
}

void ChallengeTasksDAO::updateQuestEntry(model::challenge::ChallengeTask& task, model::challenge::ChallengeQuest& quest) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_QUERY);
		stmt->setInt(1, quest.getCompleteCount());
		stmt->setTimestamp(2, task.getCompleteTime());
		stmt->setInt(3, task.getTaskId());
		stmt->setInt(4, quest.getQuestId());
		stmt->setInt(5, task.getOwnerId());
		stmt->executeUpdate();
		quest.setPersistentState(Persistable::PersistentState::UPDATED);
	} catch (const SQLException& e) {
		log.error("Could not update challenge task " + std::to_string(task.getTaskId()) + " of owner " + std::to_string(task.getOwnerId()), e);
	}
}

} // namespace aion::gameserver::dao
