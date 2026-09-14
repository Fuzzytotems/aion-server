#include "aion/gameserver/dao/PlayerDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous ParamReadStH at PlayerDAO.java:193 (com.aionemu.gameserver.dao.PlayerDAO$1); argument 2 of select(); storage: sync
//   anonymous ParamReadStH at PlayerDAO.java:358 (com.aionemu.gameserver.dao.PlayerDAO$10); argument 2 of select(); storage: sync
//   anonymous IUStH at PlayerDAO.java:465 (com.aionemu.gameserver.dao.PlayerDAO$11); argument 2 of insertUpdate(); storage: sync
//   anonymous ParamReadStH at PlayerDAO.java:213 (com.aionemu.gameserver.dao.PlayerDAO$2); argument 2 of select(); storage: sync
//   anonymous ParamReadStH at PlayerDAO.java:233 (com.aionemu.gameserver.dao.PlayerDAO$3); argument 2 of select(); storage: sync
//   anonymous IUStH at PlayerDAO.java:251 (com.aionemu.gameserver.dao.PlayerDAO$4); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at PlayerDAO.java:263 (com.aionemu.gameserver.dao.PlayerDAO$5); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at PlayerDAO.java:275 (com.aionemu.gameserver.dao.PlayerDAO$6); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at PlayerDAO.java:316 (com.aionemu.gameserver.dao.PlayerDAO$7); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at PlayerDAO.java:328 (com.aionemu.gameserver.dao.PlayerDAO$8); argument 2 of insertUpdate(); storage: sync
//   anonymous ParamReadStH at PlayerDAO.java:340 (com.aionemu.gameserver.dao.PlayerDAO$9); argument 2 of select(); storage: sync

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerDAO");

PlayerDAO::PlayerAndLegionInfo::PlayerAndLegionInfo(int32_t playerId, std::string_view name, int32_t legionId,
	model::team::legion::LegionRank legionRank)
	: playerId_(playerId), name_(name), legionId_(legionId), legionRank_(legionRank) {
}

bool PlayerDAO::PlayerAndLegionInfo::equals(const PlayerAndLegionInfo& obj) const {
	return playerId_ == obj.playerId_ && name_ == obj.name_ && legionId_ == obj.legionId_ && legionRank_ == obj.legionRank_;
}

int32_t PlayerDAO::PlayerAndLegionInfo::hashCode() const {
	// Java: String.hashCode/Enum identity hash of the components; ported with the first hash collection that holds the record
	AION_UNPORTED();
}

bool PlayerDAO::isNameUsed(std::string_view name) {
	AION_UNPORTED();
}

void PlayerDAO::storePlayer(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool PlayerDAO::saveNewPlayer(model::gameobjects::player::Player& player, int32_t accountId, std::string_view accountName) {
	AION_UNPORTED();
}

runtime::Ref<model::gameobjects::player::PlayerCommonData> PlayerDAO::loadPlayerCommonDataByName(std::string_view name) {
	AION_UNPORTED();
}

runtime::Ref<model::gameobjects::player::PlayerCommonData> PlayerDAO::loadPlayerCommonData(int32_t playerObjId) {
	AION_UNPORTED();
}

void PlayerDAO::deletePlayer(int32_t playerId) {
	AION_UNPORTED();
}

std::vector<int32_t> PlayerDAO::getPlayerOidsOnAccount(int32_t accountId) {
	AION_UNPORTED();
}

std::vector<int32_t> PlayerDAO::getPlayerOidsOnAccount(int32_t accountId, int64_t exp) {
	AION_UNPORTED();
}

void PlayerDAO::setCreationDeletionTime(model::account::PlayerAccountData& acData) {
	AION_UNPORTED();
}

void PlayerDAO::updateDeletionTime(int32_t objectId, std::optional<commons::database::Timestamp> deletionDate) {
	AION_UNPORTED();
}

void PlayerDAO::storeCreationTime(int32_t objectId, std::optional<commons::database::Timestamp> creationDate) {
	AION_UNPORTED();
}

void PlayerDAO::storeLastOnlineTime(int32_t objectId, std::optional<commons::database::Timestamp> lastOnline) {
	AION_UNPORTED();
}

std::vector<int32_t> PlayerDAO::getUsedIDs() {
	AION_UNPORTED();
}

bool PlayerDAO::isOnline(int32_t playerId) {
	AION_UNPORTED();
}

void PlayerDAO::onlinePlayer(model::gameobjects::player::Player& player, bool online) {
	AION_UNPORTED();
}

void PlayerDAO::setAllPlayersOffline() {
	AION_UNPORTED();
}

std::optional<std::string> PlayerDAO::getPlayerNameByObjId(int32_t playerObjId) {
	AION_UNPORTED();
}

int32_t PlayerDAO::getPlayerIdByName(std::string_view playerName) {
	AION_UNPORTED();
}

int32_t PlayerDAO::getAccountIdByName(std::string_view name) {
	AION_UNPORTED();
}

int32_t PlayerDAO::getAccountId(int32_t playerId) {
	AION_UNPORTED();
}

void PlayerDAO::storePlayerName(model::gameobjects::player::PlayerCommonData& recipientCommonData) {
	AION_UNPORTED();
}

int32_t PlayerDAO::getCharacterCountOnAccount(int32_t accountId) {
	AION_UNPORTED();
}

int32_t PlayerDAO::getCharacterCountForRace(model::Race race) {
	AION_UNPORTED();
}

std::vector<PlayerDAO::PlayerAndLegionInfo> PlayerDAO::getPlayersOnInactiveAccounts(int64_t maxExp, int32_t daysOfAccountInactivity) {
	AION_UNPORTED();
}

void PlayerDAO::setPlayerLastTransferTime(int32_t playerId, int64_t time) {
	AION_UNPORTED();
}

int32_t PlayerDAO::getOldCharacterLevel(int32_t playerObjectId) {
	AION_UNPORTED();
}

void PlayerDAO::storeOldCharacterLevel(int32_t playerObjectId, int32_t level) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
