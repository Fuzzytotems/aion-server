#include "aion/gameserver/dao/PlayerSkillListDAO.h"

#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;
using model::gameobjects::Persistable;
using model::skill::PlayerSkillEntry;

namespace {

constexpr std::string_view INSERT_QUERY = "REPLACE INTO `player_skills` (`player_id`, `skill_id`, `skill_level`) VALUES (?, ?, ?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE `player_skills` set skill_level=? where player_id=? AND skill_id=?";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_skills` WHERE `player_id`=? AND skill_id=?";
constexpr std::string_view SELECT_QUERY = "SELECT `skill_id`, `skill_level` FROM `player_skills` WHERE `player_id`=?";

/** Java skills.stream().filter(Persistable.<state>) */
std::vector<runtime::Ptr<PlayerSkillEntry>> filterByState(const std::vector<runtime::Ptr<PlayerSkillEntry>>& skills, Persistable::PersistentState state) {
	std::vector<runtime::Ptr<PlayerSkillEntry>> filtered;
	for (const runtime::Ptr<PlayerSkillEntry>& skill : skills) {
		if (skill && skill->getPersistentState() == state)
			filtered.push_back(skill);
	}
	return filtered;
}

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerSkillListDAO");

runtime::Ref<model::skill::PlayerSkillList> PlayerSkillListDAO::loadSkillList(int32_t playerId) {
	std::vector<runtime::Ref<PlayerSkillEntry>> skills;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, playerId);
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			int32_t id = rset->getInt("skill_id");
			int32_t lv = rset->getInt("skill_level");
			skills.push_back(PlayerSkillEntry::create(id, lv, 0, Persistable::PersistentState::UPDATED));
		}
	} catch (const std::exception& e) {
		log.error("Could not restore SkillList data for player: " + std::to_string(playerId) + " from DB: " + e.what(), e);
	}
	return model::skill::PlayerSkillList::create(std::vector<runtime::Ptr<PlayerSkillEntry>>(skills.begin(), skills.end()));
}

bool PlayerSkillListDAO::storeSkills(model::gameobjects::player::Player& player) {
	store(player, player.getSkillList()->getDeletedSkills());
	store(player, player.getSkillList()->getAllSkills());
	return true;
}

void PlayerSkillListDAO::store(model::gameobjects::player::Player& player, const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skills) {
	try {
		auto con = DatabaseFactory::getConnection();
		con->setAutoCommit(false);
		deleteSkills(*con, player, skills);
		addSkills(*con, player, skills);
		updateSkills(*con, player, skills);
	} catch (const SQLException&) {
		log.error("Failed to open connection to database while saving SkillList for player " + std::to_string(player.getObjectId()));
	}

	for (const runtime::Ptr<PlayerSkillEntry>& skill : skills) {
		skill->setPersistentState(Persistable::PersistentState::UPDATED);
	}
}

void PlayerSkillListDAO::addSkills(commons::database::Connection& con, model::gameobjects::player::Player& player,
	const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skills) {
	std::vector<runtime::Ptr<PlayerSkillEntry>> newSkills = filterByState(skills, Persistable::PersistentState::NEW);
	if (newSkills.empty())
		return;

	try {
		auto ps = con.prepareStatement(INSERT_QUERY);
		for (const runtime::Ptr<PlayerSkillEntry>& skill : newSkills) {
			ps->setInt(1, player.getObjectId());
			ps->setInt(2, skill->getSkillId());
			ps->setInt(3, skill->getSkillLevel());
			ps->addBatch();
		}
		ps->executeBatch();
		con.commit();
	} catch (const SQLException& e) {
		log.error("Can't add skills for player: " + std::to_string(player.getObjectId()), e);
	}
}

void PlayerSkillListDAO::updateSkills(commons::database::Connection& con, model::gameobjects::player::Player& player,
	const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skills) {
	std::vector<runtime::Ptr<PlayerSkillEntry>> changedSkills = filterByState(skills, Persistable::PersistentState::UPDATE_REQUIRED);
	if (changedSkills.empty())
		return;

	try {
		auto ps = con.prepareStatement(UPDATE_QUERY);
		for (const runtime::Ptr<PlayerSkillEntry>& skill : changedSkills) {
			ps->setInt(1, skill->getSkillLevel());
			ps->setInt(2, player.getObjectId());
			ps->setInt(3, skill->getSkillId());
			ps->addBatch();
		}
		ps->executeBatch();
		con.commit();
	} catch (const SQLException&) {
		log.error("Can't update skills for player: " + std::to_string(player.getObjectId()));
	}
}

void PlayerSkillListDAO::deleteSkills(commons::database::Connection& con, model::gameobjects::player::Player& player,
	const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skills) {
	std::vector<runtime::Ptr<PlayerSkillEntry>> deletedSkills = filterByState(skills, Persistable::PersistentState::DELETED);
	if (deletedSkills.empty())
		return;

	try {
		auto ps = con.prepareStatement(DELETE_QUERY);
		for (const runtime::Ptr<PlayerSkillEntry>& skill : deletedSkills) {
			ps->setInt(1, player.getObjectId());
			ps->setInt(2, skill->getSkillId());
			ps->addBatch();
		}
		ps->executeBatch();
		con.commit();
	} catch (const SQLException&) {
		log.error("Can't delete skills for player: " + std::to_string(player.getObjectId()));
	}
}

} // namespace aion::gameserver::dao
