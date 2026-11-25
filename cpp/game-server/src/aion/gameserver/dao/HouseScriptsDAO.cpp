#include "aion/gameserver/dao/HouseScriptsDAO.h"

#include <string_view>
#include <vector>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/model/gameobjects/player/PlayerScripts.h"
#include "aion/gameserver/runtime/fields/Array.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;

namespace {

constexpr std::string_view INSERT_QUERY = "INSERT INTO `house_scripts` (`house_id`,`script_id`,`script`,`uncompressed_size`) VALUES (?,?,?,?) ON DUPLICATE KEY UPDATE script=VALUES(script), uncompressed_size=VALUES(uncompressed_size)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `house_scripts` WHERE `house_id`=? AND `script_id`=?";
constexpr std::string_view DELETE_ALL_QUERY = "DELETE FROM `house_scripts` WHERE `house_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `script_id`, `script`, `uncompressed_size` FROM `house_scripts` WHERE `house_id`=? ORDER BY `date_added`";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.HouseScriptsDAO");

void HouseScriptsDAO::storeScript(int32_t houseId, int32_t scriptId, runtime::Ptr<runtime::Array<int8_t>> compressedXML, int32_t uncompressedSize) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(INSERT_QUERY);
		stmt->setInt(1, houseId);
		stmt->setInt(2, scriptId);
		stmt->setBytes(3, detail::toBytes(compressedXML)); // Java setBytes(3, null) binds NULL, which the NOT NULL column refuses
		stmt->setInt(4, uncompressedSize);
		stmt->executeUpdate();
	} catch (const std::exception& e) {
		log.error("Could not save script data for houseId: {}", houseId, e);
	}
}

runtime::Ref<model::gameobjects::player::PlayerScripts> HouseScriptsDAO::getPlayerScripts(int32_t houseId) {
	runtime::Ref<model::gameobjects::player::PlayerScripts> scripts = model::gameobjects::player::PlayerScripts::create(houseId);
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, houseId);
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			scripts->set(rset->getInt("script_id"), detail::toArray(rset->getObject<std::vector<uint8_t>>("script")), rset->getInt("uncompressed_size"),
				false);
		}
	} catch (const std::exception& e) {
		log.error("Could not restore script data for houseId: {}", houseId, e);
	}
	return scripts;
}

void HouseScriptsDAO::deleteScript(int32_t houseId, int32_t scriptId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(DELETE_QUERY);
		stmt->setInt(1, houseId);
		stmt->setInt(2, scriptId);
		stmt->executeUpdate();
	} catch (const std::exception& e) {
		log.error("Could not delete script for houseId: {}", houseId, e);
	}
}

void HouseScriptsDAO::deleteScriptsForHouse(int32_t houseId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(DELETE_ALL_QUERY);
		stmt->setInt(1, houseId);
		stmt->executeUpdate();
	} catch (const std::exception& e) {
		log.error("Could not delete script for houseId: {}", houseId, e);
	}
}

} // namespace aion::gameserver::dao
