#include "aion/gameserver/dao/LegionDAO.h"

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/dao/detail/UsedIds.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionEmblem.h"
#include "aion/gameserver/model/team/legion/LegionEmblemType.h"
#include "aion/gameserver/model/team/legion/LegionHistoryAction.h"
#include "aion/gameserver/model/team/legion/LegionHistoryAction_Type.h"
#include "aion/gameserver/model/team/legion/LegionHistoryEntry.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/fields/Array.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::ResultSet;
using commons::database::SQLException;
using model::gameobjects::Persistable;
using model::team::legion::Legion;
using model::team::legion::LegionEmblem;
using model::team::legion::LegionEmblemType;
using model::team::legion::LegionHistoryAction;
using model::team::legion::LegionHistoryAction_Type;
using model::team::legion::LegionHistoryEntry;

namespace {

constexpr std::string_view INSERT_LEGION_QUERY = "INSERT INTO legions(id, `name`) VALUES (?, ?)";
constexpr std::string_view SELECT_LEGION_QUERY1 = "SELECT * FROM legions WHERE id=?";
constexpr std::string_view SELECT_LEGION_QUERY2 = "SELECT * FROM legions WHERE name=?";
constexpr std::string_view DELETE_LEGION_QUERY = "DELETE FROM legions WHERE id = ?";
constexpr std::string_view UPDATE_LEGION_QUERY = "UPDATE legions SET name=?, level=?, contribution_points=?, deputy_permission=?, centurion_permission=?, legionary_permission=?, volunteer_permission=?, disband_time=?, occupied_legion_dominion=?, last_legion_dominion=?, current_legion_dominion=? WHERE id=?";
constexpr std::string_view INSERT_ANNOUNCEMENT_QUERY = "INSERT INTO legion_announcement_list(`legion_id`, `announcement`, `date`) VALUES (?, ?, ?)";
constexpr std::string_view SELECT_ANNOUNCEMENT_QUERY = "SELECT * FROM legion_announcement_list WHERE legion_id = ? ORDER BY date DESC LIMIT 1";
constexpr std::string_view DELETE_ANNOUNCEMENT_QUERY = "DELETE FROM legion_announcement_list WHERE legion_id = ?";
constexpr std::string_view INSERT_EMBLEM_QUERY = "INSERT INTO legion_emblems(legion_id, emblem_id, color_a, color_r, color_g, color_b, emblem_type, emblem_data) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
constexpr std::string_view UPDATE_EMBLEM_QUERY = "UPDATE legion_emblems SET emblem_id=?, color_a=?, color_r=?, color_g=?, color_b=?, emblem_type=?, emblem_data=? WHERE legion_id=?";
constexpr std::string_view SELECT_EMBLEM_QUERY = "SELECT * FROM legion_emblems WHERE legion_id=?";
constexpr std::string_view INSERT_HISTORY_QUERY = "INSERT INTO legion_history(`legion_id`, `date`, `history_type`, `name`, `description`) VALUES (?, ?, ?, ?, ?)";
constexpr std::string_view SELECT_HISTORY_QUERY = "SELECT * FROM `legion_history` WHERE legion_id=? ORDER BY date DESC, id DESC";
constexpr std::string_view DELETE_HISTORY_QUERY = "DELETE FROM `legion_history` WHERE id IN (%s)";

/**
 * Stand-in for Java LegionHistoryAction.getType() (constructor data of LegionHistoryAction.java): the enum companion belongs to P5-10, which has
 * no lane in wave 3b-1 and is not on the M4 load path; a change request asks P5-10 for a companion `getType(LegionHistoryAction)` so that
 * LegionService and SM_LEGION_HISTORY share one table, and this copy is deleted then.
 */
LegionHistoryAction_Type getType(LegionHistoryAction action) {
	switch (action) {
		case LegionHistoryAction::DEFENSE:
		case LegionHistoryAction::OCCUPATION:
			return LegionHistoryAction_Type::REWARD;
		case LegionHistoryAction::ITEM_DEPOSIT:
		case LegionHistoryAction::ITEM_WITHDRAW:
		case LegionHistoryAction::KINAH_DEPOSIT:
		case LegionHistoryAction::KINAH_WITHDRAW:
			return LegionHistoryAction_Type::WAREHOUSE;
		default:
			return LegionHistoryAction_Type::LEGION;
	}
}

/** The legion columns both loadLegion bodies read after `new Legion(id, name)` */
void readLegion(Legion& legion, ResultSet& resultSet) {
	legion.setLegionLevel(resultSet.getInt("level"));
	legion.addContributionPoints(resultSet.getLong("contribution_points"));
	legion.setLegionPermissions(resultSet.getShort("deputy_permission"), resultSet.getShort("centurion_permission"),
		resultSet.getShort("legionary_permission"), resultSet.getShort("volunteer_permission"));
	legion.setDisbandTime(resultSet.getInt("disband_time"));
	legion.setOccupiedLegionDominion(resultSet.getInt("occupied_legion_dominion"));
	legion.setLastLegionDominion(resultSet.getInt("last_legion_dominion"));
	legion.setCurrentLegionDominion(resultSet.getInt("current_legion_dominion"));
}

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.LegionDAO");

bool LegionDAO::isNameUsed(std::string_view name) {
	std::unique_ptr<PreparedStatement> s = DB::prepareStatement("SELECT count(id) as cnt FROM legions WHERE ? = legions.name");
	if (!s) // Java: s.setString on the null that DB.prepareStatement returned
		throw runtime::NullPointerException("Cannot invoke \"java.sql.PreparedStatement.setString(int, String)\" because \"s\" is null");
	try {
		s->setString(1, name);
		auto rs = s->executeQuery();
		rs->next();
		bool used = rs->getInt("cnt") > 0;
		DB::close(s);
		return used;
	} catch (const SQLException& e) {
		DB::close(s);
		log.error("Can't check if name " + std::string(name) + ", is used, returning possitive result", e);
		return true;
	} catch (...) {
		DB::close(s);
		throw;
	}
}

bool LegionDAO::saveNewLegion(model::team::legion::Legion& legion) {
	bool success = DB::insertUpdate(INSERT_LEGION_QUERY, [&](PreparedStatement& preparedStatement) {
		preparedStatement.setInt(1, legion.getLegionId());
		preparedStatement.setString(2, legion.getName());
		preparedStatement.execute();
	});
	return success;
}

void LegionDAO::storeLegion(model::team::legion::Legion& legion) {
	DB::insertUpdate(UPDATE_LEGION_QUERY, [&](PreparedStatement& stmt) {
		stmt.setString(1, legion.getName());
		stmt.setInt(2, legion.getLegionLevel());
		stmt.setLong(3, legion.getContributionPoints());
		stmt.setInt(4, legion.getDeputyPermission());
		stmt.setInt(5, legion.getCenturionPermission());
		stmt.setInt(6, legion.getLegionaryPermission());
		stmt.setInt(7, legion.getVolunteerPermission());
		stmt.setInt(8, legion.getDisbandTime());
		stmt.setInt(9, legion.getOccupiedLegionDominion());
		stmt.setInt(10, legion.getLastLegionDominion());
		stmt.setInt(11, legion.getCurrentLegionDominion());
		stmt.setInt(12, legion.getLegionId());
		stmt.execute();
	});
}

runtime::Ref<model::team::legion::Legion> LegionDAO::loadLegion(std::string_view legionName) {
	runtime::Ref<Legion> legion;
	bool success = DB::select(
		SELECT_LEGION_QUERY2, [&](PreparedStatement& stmt) { stmt.setString(1, legionName); },
		[&](ResultSet& resultSet) {
			if (resultSet.next()) {
				legion = Legion::create(resultSet.getInt("id"), resultSet.getString("name"));
				readLegion(*legion, resultSet);
			}
		});

	return success ? legion : nullptr;
}

runtime::Ref<model::team::legion::Legion> LegionDAO::loadLegion(int32_t legionId) {
	runtime::Ref<Legion> legion;
	bool success = DB::select(
		SELECT_LEGION_QUERY1, [&](PreparedStatement& stmt) { stmt.setInt(1, legionId); },
		[&](ResultSet& resultSet) {
			if (resultSet.next()) {
				legion = Legion::create(legionId, resultSet.getString("name"));
				readLegion(*legion, resultSet);
			}
		});

	return success ? legion : nullptr;
}

void LegionDAO::deleteLegion(int32_t legionId) {
	std::unique_ptr<PreparedStatement> statement = DB::prepareStatement(DELETE_LEGION_QUERY);
	if (!statement) // Java: statement.setInt on the null that DB.prepareStatement returned
		throw runtime::NullPointerException("Cannot invoke \"java.sql.PreparedStatement.setInt(int, int)\" because \"statement\" is null");
	try {
		statement->setInt(1, legionId);
	} catch (const SQLException& e) {
		log.error("deleteLegion #1", e);
	}
	DB::executeUpdateAndClose(statement);
}

std::vector<int32_t> LegionDAO::getUsedIDs() {
	return detail::getUsedIDs(log, "SELECT id FROM legions", "id", "Can't get list of IDs from legions table");
}

runtime::Ref<model::team::legion::Legion::Announcement> LegionDAO::loadAnnouncement(int32_t legionId) {
	runtime::Ref<Legion::Announcement> announcement;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_ANNOUNCEMENT_QUERY);
		stmt->setInt(1, legionId);
		auto resultSet = stmt->executeQuery();
		if (resultSet->next()) {
			std::string message = resultSet->getString("announcement");
			std::optional<commons::database::Timestamp> date = resultSet->getTimestamp("date");
			// the date column is NOT NULL, so this is reachable only with a drifted schema: Java keeps a null Timestamp in the record, the frozen
			// record member is no optional (docs/deviations/P4-14.md), so the NullPointerException of its first use is thrown here
			if (!date)
				throw runtime::NullPointerException("legion_announcement.date is NULL");
			announcement = Legion::Announcement::create(message, *date);
		}
	} catch (const SQLException& e) {
		log.error("Couldn't load legion announcements for legion " + std::to_string(legionId), e);
	}
	return announcement;
}

void LegionDAO::saveAnnouncement(int32_t legionId, runtime::Ptr<model::team::legion::Legion::Announcement> announcement) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto deleteStatement = con->prepareStatement(DELETE_ANNOUNCEMENT_QUERY);
		deleteStatement->setInt(1, legionId);
		deleteStatement->executeUpdate();
		if (announcement) {
			auto insert = con->prepareStatement(INSERT_ANNOUNCEMENT_QUERY);
			insert->setInt(1, legionId);
			insert->setString(2, announcement->message());
			insert->setTimestamp(3, announcement->time());
			insert->executeUpdate();
		}
	} catch (const SQLException& e) {
		// Java: + announcement (the record's toString, "null" without an announcement)
		std::string text = "null";
		if (announcement)
			text = "Announcement[message=" + announcement->message() + ", time=" + std::to_string(detail::getTime(announcement->time())) + "]";
		log.error("Couldn't save announcement for legion " + std::to_string(legionId) + ": " + text, e);
	}
}

void LegionDAO::storeLegionEmblem(int32_t legionId, model::team::legion::LegionEmblem& legionEmblem) {
	if (!validEmblem(legionEmblem))
		return;
	if (!(checkEmblem(legionId)))
		createLegionEmblem(legionId, legionEmblem);
	else {
		switch (legionEmblem.getPersistentState()) {
			case Persistable::PersistentState::UPDATE_REQUIRED:
				updateLegionEmblem(legionId, legionEmblem);
				break;
			case Persistable::PersistentState::NEW:
				createLegionEmblem(legionId, legionEmblem);
				break;
			default:
				break;
		}
	}
	legionEmblem.setPersistentState(Persistable::PersistentState::UPDATED);
}

bool LegionDAO::validEmblem(model::team::legion::LegionEmblem& legionEmblem) {
	return legionEmblem.getEmblemType() != LegionEmblemType::CUSTOM || legionEmblem.getCustomEmblemData();
}

bool LegionDAO::checkEmblem(int32_t legionid) {
	std::unique_ptr<PreparedStatement> st = DB::prepareStatement(SELECT_EMBLEM_QUERY);
	if (!st) // Java: st.setInt on the null that DB.prepareStatement returned
		throw runtime::NullPointerException("Cannot invoke \"java.sql.PreparedStatement.setInt(int, int)\" because \"st\" is null");
	try {
		st->setInt(1, legionid);
		auto rs = st->executeQuery();
		if (rs->next()) {
			DB::close(st);
			return true;
		}
	} catch (const SQLException& e) {
		log.error("Can't check " + std::to_string(legionid) + " legion emblem: ", e);
	} catch (...) {
		DB::close(st);
		throw;
	}
	DB::close(st);
	return false;
}

void LegionDAO::createLegionEmblem(int32_t legionId, model::team::legion::LegionEmblem& legionEmblem) {
	DB::insertUpdate(INSERT_EMBLEM_QUERY, [&](PreparedStatement& preparedStatement) {
		preparedStatement.setInt(1, legionId);
		preparedStatement.setInt(2, legionEmblem.getEmblemId());
		preparedStatement.setByte(3, legionEmblem.getColor_a());
		preparedStatement.setByte(4, legionEmblem.getColor_r());
		preparedStatement.setByte(5, legionEmblem.getColor_g());
		preparedStatement.setByte(6, legionEmblem.getColor_b());
		preparedStatement.setString(7, detail::enumName(legionEmblem.getEmblemType()));
		preparedStatement.setBytes(8, detail::toBytes(legionEmblem.getCustomEmblemData()));
		preparedStatement.execute();
	});
}

void LegionDAO::updateLegionEmblem(int32_t legionId, model::team::legion::LegionEmblem& legionEmblem) {
	DB::insertUpdate(UPDATE_EMBLEM_QUERY, [&](PreparedStatement& stmt) {
		stmt.setInt(1, legionEmblem.getEmblemId());
		stmt.setByte(2, legionEmblem.getColor_a());
		stmt.setByte(3, legionEmblem.getColor_r());
		stmt.setByte(4, legionEmblem.getColor_g());
		stmt.setByte(5, legionEmblem.getColor_b());
		stmt.setString(6, detail::enumName(legionEmblem.getEmblemType()));
		stmt.setBytes(7, detail::toBytes(legionEmblem.getCustomEmblemData()));
		stmt.setInt(8, legionId);
		stmt.execute();
	});
}

runtime::Ref<model::team::legion::LegionEmblem> LegionDAO::loadLegionEmblem(int32_t legionId) {
	runtime::Ref<LegionEmblem> legionEmblem = LegionEmblem::create();

	DB::select(
		SELECT_EMBLEM_QUERY, [&](PreparedStatement& stmt) { stmt.setInt(1, legionId); },
		[&](ResultSet& resultSet) {
			while (resultSet.next()) {
				runtime::Ref<runtime::Array<int8_t>> emblemData = detail::toArray(resultSet.getObject<std::vector<uint8_t>>("emblem_data"));
				legionEmblem->setEmblem(resultSet.getByte("emblem_id"), resultSet.getByte("color_a"), resultSet.getByte("color_r"),
					resultSet.getByte("color_g"), resultSet.getByte("color_b"),
					detail::enumValueOf<LegionEmblemType>(resultSet.getString("emblem_type"), "com.aionemu.gameserver.model.team.legion.LegionEmblemType"),
					emblemData);
			}
		});
	legionEmblem->setPersistentState(Persistable::PersistentState::UPDATED);

	return legionEmblem;
}

void LegionDAO::loadHistory(model::team::legion::Legion& legion) {
	std::map<LegionHistoryAction_Type, std::vector<runtime::Ref<LegionHistoryEntry>>> history;
	for (LegionHistoryAction_Type type : {LegionHistoryAction_Type::LEGION, LegionHistoryAction_Type::REWARD, LegionHistoryAction_Type::WAREHOUSE})
		history.try_emplace(type);

	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_HISTORY_QUERY);
		stmt->setInt(1, legion.getLegionId());
		auto resultSet = stmt->executeQuery();
		while (resultSet->next()) {
			int32_t id = resultSet->getInt("id");
			int32_t epochSeconds = static_cast<int32_t>(detail::getTime(resultSet->getTimestamp("date")) / 1000);
			LegionHistoryAction action =
				detail::enumValueOf<LegionHistoryAction>(resultSet->getString("history_type"), "com.aionemu.gameserver.model.team.legion.LegionHistoryAction");
			std::string name = resultSet->getString("name");
			std::string description = resultSet->getString("description");
			history.at(getType(action)).push_back(LegionHistoryEntry::create(id, epochSeconds, action, name, description));
		}
	} catch (const std::exception& e) {
		log.error("Could not load history of legion " + legion.toString(), e);
	}
	legion.setHistory(history);
}

runtime::Ref<model::team::legion::LegionHistoryEntry> LegionDAO::insertHistory(int32_t legionId, model::team::legion::LegionHistoryAction action,
	std::string_view name, std::string_view description) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(INSERT_HISTORY_QUERY, commons::database::Statement::RETURN_GENERATED_KEYS);
		int64_t nowMillis = commons::utils::currentTimeMillis();
		stmt->setInt(1, legionId);
		stmt->setTimestamp(2, detail::toTimestamp(nowMillis));
		stmt->setString(3, detail::enumName(action));
		stmt->setString(4, name);
		stmt->setString(5, description);
		stmt->execute();
		auto result = stmt->getGeneratedKeys();
		result->next();
		return LegionHistoryEntry::create(result->getInt(1), static_cast<int32_t>(nowMillis / 1000), action, name, description);
	} catch (const std::exception& e) {
		log.error("Could not add history entry for legion " + std::to_string(legionId), e);
		return nullptr;
	}
}

void LegionDAO::deleteHistory(int32_t legionId, const std::vector<runtime::Ptr<model::team::legion::LegionHistoryEntry>>& entries) {
	if (entries.empty())
		return;
	// Java: DELETE_HISTORY_QUERY.formatted(",?".repeat(entries.size()).substring(1))
	std::string placeholders;
	for (size_t i = 0; i < entries.size(); i++)
		placeholders += ",?";
	std::string query(DELETE_HISTORY_QUERY);
	query.replace(query.find("%s"), 2, placeholders.substr(1));
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(query);
		for (size_t i = 0; i < entries.size(); i++)
			stmt->setInt(static_cast<int32_t>(i + 1), entries[i]->id());
		stmt->executeUpdate();
	} catch (const std::exception& e) {
		log.error("Could not delete " + std::to_string(entries.size()) + " history entries for legion " + std::to_string(legionId), e);
	}
}

} // namespace aion::gameserver::dao
