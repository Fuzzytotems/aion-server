#include "aion/gameserver/dao/PlayerQuestListDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view SELECT_QUERY = "SELECT `quest_id`, `status`, `quest_vars`, `flags`, `complete_count`, `next_repeat_time`, `reward`, `complete_time` FROM `player_quests` WHERE `player_id`=?";
constexpr std::string_view UPDATE_QUERY = "UPDATE `player_quests` SET `status`=?, `quest_vars`=?, `flags`=?, `complete_count`=?, `next_repeat_time`=?, `reward`=?, `complete_time`=? WHERE `player_id`=? AND `quest_id`=?";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_quests` WHERE `player_id`=? AND `quest_id`=?";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_quests` (`player_id`, `quest_id`, `status`, `quest_vars`, `flags`, `complete_count`, `next_repeat_time`, `reward`, `complete_time`) VALUES (?,?,?,?,?,?,?,?,?)";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerQuestListDAO");

runtime::Ref<model::gameobjects::player::QuestStateList> PlayerQuestListDAO::load(int32_t playerObjId) {
	AION_UNPORTED();
}

void PlayerQuestListDAO::store(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerQuestListDAO::addQuests(commons::database::Connection& con, int32_t playerId,
	const std::vector<runtime::Ptr<questEngine::model::QuestState>>& states) {
	AION_UNPORTED();
}

void PlayerQuestListDAO::updateQuests(commons::database::Connection& con, int32_t playerId,
	const std::vector<runtime::Ptr<questEngine::model::QuestState>>& states) {
	AION_UNPORTED();
}

void PlayerQuestListDAO::deleteQuest(commons::database::Connection& con, int32_t playerId,
	const std::vector<runtime::Ptr<questEngine::model::QuestState>>& states, const std::unordered_set<int32_t>& questIds) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
