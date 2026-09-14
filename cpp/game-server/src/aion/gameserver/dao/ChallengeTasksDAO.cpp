#include "aion/gameserver/dao/ChallengeTasksDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view SELECT_QUERY = "SELECT * FROM `challenge_tasks` WHERE `owner_id` = ? AND `owner_type` = ?";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `challenge_tasks` (`task_id`, `quest_id`, `owner_id`, `owner_type`, `complete_count`, `complete_time`) VALUES (?, ?, ?, ?, ?, ?);";
constexpr std::string_view UPDATE_QUERY = "UPDATE `challenge_tasks` SET `complete_count` = ?, `complete_time`= ? WHERE `task_id` = ? AND `quest_id` = ? AND `owner_id` = ?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.ChallengeTasksDAO");

std::unordered_map<int32_t, runtime::Ref<model::challenge::ChallengeTask>> ChallengeTasksDAO::load(int32_t ownerId,
	model::templates::challenge::ChallengeType type) {
	AION_UNPORTED();
}

void ChallengeTasksDAO::storeTask(model::challenge::ChallengeTask& task) {
	AION_UNPORTED();
}

void ChallengeTasksDAO::insertQuestEntry(model::challenge::ChallengeTask& task, model::challenge::ChallengeQuest& quest) {
	AION_UNPORTED();
}

void ChallengeTasksDAO::updateQuestEntry(model::challenge::ChallengeTask& task, model::challenge::ChallengeQuest& quest) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
