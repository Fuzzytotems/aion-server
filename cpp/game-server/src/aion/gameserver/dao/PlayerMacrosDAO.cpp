#include "aion/gameserver/dao/PlayerMacrosDAO.h"

#include <string>
#include <string_view>

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/Macros.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::PreparedStatement;

namespace {

constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_macrosses` (`player_id`, `order`, `macro`) VALUES (?,?,?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE `player_macrosses` SET `macro`=? WHERE `player_id`=? AND `order`=?";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_macrosses` WHERE `player_id`=? AND `order`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `order`, `macro` FROM `player_macrosses` WHERE `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerMacrosDAO");

void PlayerMacrosDAO::addMacro(int32_t playerId, int32_t macroPosition, std::string_view macro) {
	DB::insertUpdate(INSERT_QUERY, [&](PreparedStatement& stmt) {
		stmt.setInt(1, playerId);
		stmt.setInt(2, macroPosition);
		stmt.setString(3, macro);
		stmt.execute();
	});
}

void PlayerMacrosDAO::updateMacro(int32_t playerId, int32_t macroPosition, std::string_view macro) {
	DB::insertUpdate(UPDATE_QUERY, [&](PreparedStatement& stmt) {
		stmt.setString(1, macro);
		stmt.setInt(2, playerId);
		stmt.setInt(3, macroPosition);
		stmt.execute();
	});
}

void PlayerMacrosDAO::deleteMacro(int32_t playerId, int32_t macroPosition) {
	DB::insertUpdate(DELETE_QUERY, [&](PreparedStatement& stmt) {
		stmt.setInt(1, playerId);
		stmt.setInt(2, macroPosition);
		stmt.execute();
	});
}

runtime::Ref<model::gameobjects::player::Macros> PlayerMacrosDAO::loadMacros(int32_t playerId) {
	runtime::Ref<model::gameobjects::player::Macros> macros = model::gameobjects::player::Macros::create();
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, playerId);
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			macros->add(rset->getInt("order"), rset->getString("macro"));
		}
	} catch (const std::exception& e) {
		log.error("Could not load macros for player " + std::to_string(playerId), e);
	}
	return macros;
}

} // namespace aion::gameserver::dao
