#include "aion/gameserver/services/player/PlayerService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::player {

bool PlayerService::isNameUsedOrReserved(std::optional<std::string_view> oldName, std::string_view newName) {
	AION_UNPORTED();
}

bool PlayerService::isNameUsedOrReserved(std::optional<std::string_view> oldName, std::string_view newName, int32_t nameReservationDurationDays) {
	AION_UNPORTED();
}

bool PlayerService::storeNewPlayer(model::gameobjects::player::Player& player, std::string_view accountName, int32_t accountId) {
	AION_UNPORTED();
}

void PlayerService::storePlayer(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

runtime::Ref<model::gameobjects::player::Player> PlayerService::getPlayer(int32_t playerObjId, runtime::Ptr<model::account::Account> account) {
	AION_UNPORTED();
}

runtime::Ref<model::gameobjects::player::Player> PlayerService::newPlayer(model::account::PlayerAccountData& playerAccountData,
	model::account::Account& account) {
	AION_UNPORTED();
}

runtime::Ref<model::gameobjects::player::PlayerCommonData> PlayerService::getOrLoadPlayerCommonData(int32_t playerObjId) {
	AION_UNPORTED();
}

runtime::Ref<model::gameobjects::player::PlayerCommonData> PlayerService::getOrLoadPlayerCommonData(std::string_view name) {
	AION_UNPORTED();
}

bool PlayerService::cancelPlayerDeletion(model::account::PlayerAccountData& accData) {
	AION_UNPORTED();
}

void PlayerService::deletePlayer(model::account::PlayerAccountData& accData) {
	AION_UNPORTED();
}

void PlayerService::deletePlayerFromDB(int32_t playerId) {
	AION_UNPORTED();
}

void PlayerService::deletePlayerFromDB(int32_t playerId, bool notifyServices) {
	AION_UNPORTED();
}

void PlayerService::storeDeletionTime(model::account::PlayerAccountData& accData) {
	AION_UNPORTED();
}

void PlayerService::storeCreationTime(int32_t objectId, std::optional<commons::database::Timestamp> creationDate) {
	AION_UNPORTED();
}

void PlayerService::addMacro(model::gameobjects::player::Player& player, int32_t macroOrder, std::string_view macroXML) {
	AION_UNPORTED();
}

void PlayerService::removeMacro(model::gameobjects::player::Player& player, int32_t macroOrder) {
	AION_UNPORTED();
}

std::optional<std::string> PlayerService::getPlayerName(int32_t objectId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::player
