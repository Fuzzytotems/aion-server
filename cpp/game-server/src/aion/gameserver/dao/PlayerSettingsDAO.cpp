#include "aion/gameserver/dao/PlayerSettingsDAO.h"

#include <string>

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::PreparedStatement;
using model::gameobjects::Persistable;
using model::gameobjects::player::PlayerSettings;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerSettingsDAO");

runtime::Ref<model::gameobjects::player::PlayerSettings> PlayerSettingsDAO::loadSettings(int32_t playerId) {
	runtime::Ref<PlayerSettings> playerSettings = PlayerSettings::create();
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("SELECT * FROM player_settings WHERE player_id = ?");
		stmt->setInt(1, playerId);
		auto resultSet = stmt->executeQuery();
		while (resultSet->next()) {
			int32_t type = resultSet->getInt("settings_type");
			switch (type) {
				case 0:
					playerSettings->setUiSettings(detail::toArray(resultSet->getObject<std::vector<uint8_t>>("settings")));
					break;
				case 1:
					playerSettings->setShortcuts(detail::toArray(resultSet->getObject<std::vector<uint8_t>>("settings")));
					break;
				case 2:
					playerSettings->setHouseBuddies(detail::toArray(resultSet->getObject<std::vector<uint8_t>>("settings")));
					break;
				case -1:
					playerSettings->setDisplay(resultSet->getInt("settings"));
					break;
				case -2:
					playerSettings->setDeny(resultSet->getInt("settings"));
					break;
				default:
					break;
			}
		}
	} catch (const std::exception& e) {
		log.error("Could not restore PlayerSettings data for player " + std::to_string(playerId) + " from DB: " + e.what(), e);
	}
	playerSettings->setPersistentState(Persistable::PersistentState::UPDATED);
	return playerSettings;
}

void PlayerSettingsDAO::saveSettings(model::gameobjects::player::Player& player) {
	const int32_t playerId = player.getObjectId();

	runtime::Ptr<PlayerSettings> playerSettings = player.getPlayerSettings();
	if (playerSettings->getPersistentState() == Persistable::PersistentState::UPDATED)
		return;

	const std::optional<std::vector<uint8_t>> uiSettings = detail::toBytes(playerSettings->getUiSettings());
	const std::optional<std::vector<uint8_t>> shortcuts = detail::toBytes(playerSettings->getShortcuts());
	const std::optional<std::vector<uint8_t>> houseBuddies = detail::toBytes(playerSettings->getHouseBuddies());
	const int32_t display = playerSettings->getDisplay();
	const int32_t deny = playerSettings->getDeny();

	if (uiSettings) {
		DB::insertUpdate("REPLACE INTO player_settings values (?, ?, ?)", [&](PreparedStatement& stmt) {
			stmt.setInt(1, playerId);
			stmt.setInt(2, 0);
			stmt.setBytes(3, uiSettings);
			stmt.execute();
		});
	}

	if (shortcuts) {
		DB::insertUpdate("REPLACE INTO player_settings values (?, ?, ?)", [&](PreparedStatement& stmt) {
			stmt.setInt(1, playerId);
			stmt.setInt(2, 1);
			stmt.setBytes(3, shortcuts);
			stmt.execute();
		});
	}

	if (houseBuddies) {
		DB::insertUpdate("REPLACE INTO player_settings values (?, ?, ?)", [&](PreparedStatement& stmt) {
			stmt.setInt(1, playerId);
			stmt.setInt(2, 2);
			stmt.setBytes(3, houseBuddies);
			stmt.execute();
		});
	}

	DB::insertUpdate("REPLACE INTO player_settings values (?, ?, ?)", [&](PreparedStatement& stmt) {
		stmt.setInt(1, playerId);
		stmt.setInt(2, -1);
		stmt.setInt(3, display);
		stmt.execute();
	});

	DB::insertUpdate("REPLACE INTO player_settings values (?, ?, ?)", [&](PreparedStatement& stmt) {
		stmt.setInt(1, playerId);
		stmt.setInt(2, -2);
		stmt.setInt(3, deny);
		stmt.execute();
	});
}

} // namespace aion::gameserver::dao
