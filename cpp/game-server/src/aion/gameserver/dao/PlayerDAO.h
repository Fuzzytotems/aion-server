#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/account/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/legion/fwd.h"

namespace aion::gameserver::dao {

/**
 * Class that is responsible for storing/loading player data
 *
 * @author SoulKeeper, Saelya, cura, KID, xTz
 */
class PlayerDAO {
public:
	class PlayerAndLegionInfo {
	private:
		int32_t playerId_{};
		std::string name_{};
		int32_t legionId_{};
		/** null (std::nullopt) for a player without a legion (the LEFT JOIN of getPlayersOnInactiveAccounts) */
		std::optional<model::team::legion::LegionRank> legionRank_{};
	public:
		// canonical record constructor
		PlayerAndLegionInfo(int32_t playerId, std::string_view name, int32_t legionId, std::optional<model::team::legion::LegionRank> legionRank);
		int32_t playerId() const { return this->playerId_; }
		std::string name() const { return this->name_; }
		int32_t legionId() const { return this->legionId_; }
		std::optional<model::team::legion::LegionRank> legionRank() const { return this->legionRank_; }
		/** Java record equals: all components */
		bool equals(const PlayerAndLegionInfo& obj) const;
		int32_t hashCode() const;
	};
public:
	static bool isNameUsed(std::string_view name);
	static void storePlayer(model::gameobjects::player::Player& player);
	static bool saveNewPlayer(model::gameobjects::player::Player& player, int32_t accountId, std::string_view accountName);
	static runtime::Ref<model::gameobjects::player::PlayerCommonData> loadPlayerCommonDataByName(std::string_view name);
	static runtime::Ref<model::gameobjects::player::PlayerCommonData> loadPlayerCommonData(int32_t playerObjId);
	/** Removes player and all related data (via CASCADE DELETION) */
	static void deletePlayer(int32_t playerId);
	static std::vector<int32_t> getPlayerOidsOnAccount(int32_t accountId);
	static std::vector<int32_t> getPlayerOidsOnAccount(int32_t accountId, int64_t exp);
	static void setCreationDeletionTime(model::account::PlayerAccountData& acData);
	static void updateDeletionTime(int32_t objectId, std::optional<commons::database::Timestamp> deletionDate);
	static void storeCreationTime(int32_t objectId, std::optional<commons::database::Timestamp> creationDate);
	static void storeLastOnlineTime(int32_t objectId, std::optional<commons::database::Timestamp> lastOnline);
	static std::vector<int32_t> getUsedIDs();
	static bool isOnline(int32_t playerId);
	static void onlinePlayer(model::gameobjects::player::Player& player, bool online);
	static void setAllPlayersOffline();
	/** @return the name, null if no such player (BlockListDAO.java:67 tests PlayerService.getPlayerName) */
	static std::optional<std::string> getPlayerNameByObjId(int32_t playerObjId);
	static int32_t getPlayerIdByName(std::string_view playerName);
	static int32_t getAccountIdByName(std::string_view name);
	static int32_t getAccountId(int32_t playerId);
	static void storePlayerName(model::gameobjects::player::PlayerCommonData& recipientCommonData);
	static int32_t getCharacterCountOnAccount(int32_t accountId);
	static int32_t getCharacterCountForRace(model::Race race);
	static std::vector<PlayerDAO::PlayerAndLegionInfo> getPlayersOnInactiveAccounts(int64_t maxExp, int32_t daysOfAccountInactivity);
	static void setPlayerLastTransferTime(int32_t playerId, int64_t time);
	static int32_t getOldCharacterLevel(int32_t playerObjectId);
	static void storeOldCharacterLevel(int32_t playerObjectId, int32_t level);
};

} // namespace aion::gameserver::dao
