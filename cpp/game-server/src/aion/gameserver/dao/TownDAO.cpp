#include "aion/gameserver/dao/TownDAO.h"

#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/town/Town.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;
using model::gameobjects::Persistable;

namespace {

constexpr std::string_view SELECT_QUERY = "SELECT * FROM `towns` WHERE `race` = ?";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `towns`(`id`,`level`,`points`, `race`) VALUES (?,?,?,?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE `towns` SET `level` = ?, `points` = ?, `level_up_date` = ? WHERE `id` = ?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.TownDAO");

std::unordered_map<int32_t, runtime::Ref<model::town::Town>> TownDAO::load(model::Race race) {
	std::unordered_map<int32_t, runtime::Ref<model::town::Town>> towns;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setString(1, detail::enumName(race));
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			int32_t id = rset->getInt("id");
			int32_t level = rset->getInt("level");
			int32_t points = rset->getInt("points");
			std::optional<commons::database::Timestamp> levelUpDate = rset->getTimestamp("level_up_date");
			runtime::Ref<model::town::Town> town = model::town::Town::create(id, level, points, race, levelUpDate);
			towns.insert_or_assign(town->getId(), std::move(town));
		}
	} catch (const SQLException& e) {
		log.error("Could not load towns", e);
	}
	return towns;
}

void TownDAO::store(model::town::Town& town) {
	switch (town.getPersistentState()) {
		case Persistable::PersistentState::NEW:
			insertTown(town);
			break;
		case Persistable::PersistentState::UPDATE_REQUIRED:
			updateTown(town);
			break;
		default:
			break;
	}
}

void TownDAO::insertTown(model::town::Town& town) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(INSERT_QUERY);
		stmt->setInt(1, town.getId());
		stmt->setInt(2, town.getLevel());
		stmt->setInt(3, town.getPoints());
		stmt->setString(4, detail::enumName(town.getRace()));
		stmt->executeUpdate();
		town.setPersistentState(Persistable::PersistentState::UPDATED);
	} catch (const SQLException& e) {
		log.error("Could not insert town " + std::to_string(town.getId()), e);
	}
}

void TownDAO::updateTown(model::town::Town& town) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_QUERY);
		stmt->setInt(1, town.getLevel());
		stmt->setInt(2, town.getPoints());
		stmt->setTimestamp(3, town.getLevelUpDate());
		stmt->setInt(4, town.getId());
		stmt->executeUpdate();
		town.setPersistentState(Persistable::PersistentState::UPDATED);
	} catch (const SQLException& e) {
		log.error("Could not update town " + std::to_string(town.getId()), e);
	}
}

} // namespace aion::gameserver::dao
