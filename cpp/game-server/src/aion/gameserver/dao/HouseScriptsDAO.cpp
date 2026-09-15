#include "aion/gameserver/dao/HouseScriptsDAO.h"

#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/gameobjects/player/PlayerScripts.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/utils/xml/CompressUtil.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;

namespace {

constexpr std::string_view INSERT_QUERY = "INSERT INTO `house_scripts` (`house_id`,`script_id`,`script`) VALUES (?,?,?) ON DUPLICATE KEY UPDATE house_id=VALUES(house_id), script_id=VALUES(script_id), script=VALUES(script)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `house_scripts` WHERE `house_id`=? AND `script_id`=?";
constexpr std::string_view DELETE_ALL_QUERY = "DELETE FROM `house_scripts` WHERE `house_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `script_id`, `script` FROM `house_scripts` WHERE `house_id`=? ORDER BY `date_added`";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.HouseScriptsDAO");

void HouseScriptsDAO::storeScript(int32_t houseId, int32_t scriptId, std::string_view scriptXML) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(INSERT_QUERY);
		stmt->setInt(1, houseId);
		stmt->setInt(2, scriptId);
		stmt->setString(3, scriptXML);
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
			addScript(*scripts, rset->getInt("script_id"), rset->getString("script"));
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

bool HouseScriptsDAO::addScript(model::gameobjects::player::PlayerScripts& scripts, int32_t id, std::string_view scriptXML) {
	// Java: scriptXML == null || scriptXML.isEmpty() (getString returns "" for SQL NULL)
	if (scriptXML.empty()) {
		return scripts.set(id, runtime::Array<int8_t>::make(0), 0, false);
	} else {
		// Java: scriptXML.getBytes(StandardCharsets.UTF_16LE)
		const std::u16string utf16 = commons::utils::StringUtils::toUtf16(scriptXML);
		std::vector<uint8_t> bytes;
		bytes.reserve(utf16.size() * 2);
		for (char16_t c : utf16) {
			bytes.push_back(static_cast<uint8_t>(c & 0xFF));
			bytes.push_back(static_cast<uint8_t>(c >> 8));
		}
		std::vector<uint8_t> compressed = utils::xml::CompressUtil::compress(bytes);
		runtime::Ref<runtime::Array<int8_t>> compressedBytes = runtime::Array<int8_t>::make(static_cast<int32_t>(compressed.size()));
		for (size_t i = 0; i < compressed.size(); ++i)
			(*compressedBytes)[static_cast<int32_t>(i)].set(static_cast<int8_t>(compressed[i]));
		return scripts.set(id, compressedBytes, static_cast<int32_t>(bytes.size()), false);
	}
}

} // namespace aion::gameserver::dao
