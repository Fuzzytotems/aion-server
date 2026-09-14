#include "aion/gameserver/dao/PlayerSkillListDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view INSERT_QUERY = "REPLACE INTO `player_skills` (`player_id`, `skill_id`, `skill_level`) VALUES (?, ?, ?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE `player_skills` set skill_level=? where player_id=? AND skill_id=?";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_skills` WHERE `player_id`=? AND skill_id=?";
constexpr std::string_view SELECT_QUERY = "SELECT `skill_id`, `skill_level` FROM `player_skills` WHERE `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerSkillListDAO");

runtime::Ref<model::skill::PlayerSkillList> PlayerSkillListDAO::loadSkillList(int32_t playerId) {
	AION_UNPORTED();
}

bool PlayerSkillListDAO::storeSkills(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerSkillListDAO::store(model::gameobjects::player::Player& player, const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skills) {
	AION_UNPORTED();
}

void PlayerSkillListDAO::addSkills(commons::database::Connection& con, model::gameobjects::player::Player& player,
	const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skills) {
	AION_UNPORTED();
}

void PlayerSkillListDAO::updateSkills(commons::database::Connection& con, model::gameobjects::player::Player& player,
	const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skills) {
	AION_UNPORTED();
}

void PlayerSkillListDAO::deleteSkills(commons::database::Connection& con, model::gameobjects::player::Player& player,
	const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skills) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
