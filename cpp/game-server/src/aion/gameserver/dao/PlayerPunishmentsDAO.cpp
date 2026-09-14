#include "aion/gameserver/dao/PlayerPunishmentsDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous ParamReadStH at PlayerPunishmentsDAO.java:26 (com.aionemu.gameserver.dao.PlayerPunishmentsDAO$1); argument 2 of select(); storage: sync
//   anonymous IUStH at PlayerPunishmentsDAO.java:48 (com.aionemu.gameserver.dao.PlayerPunishmentsDAO$2); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at PlayerPunishmentsDAO.java:65 (com.aionemu.gameserver.dao.PlayerPunishmentsDAO$3); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at PlayerPunishmentsDAO.java:87 (com.aionemu.gameserver.dao.PlayerPunishmentsDAO$4); argument 2 of insertUpdate(); storage: sync
// anonymous ParamReadStH at PlayerPunishmentsDAO.java:100 (com.aionemu.gameserver.dao.PlayerPunishmentsDAO$5); argument 2 of select(); storage: sync

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view SELECT_DURATIONS_QUERY = "SELECT `punishment_type`, `duration` FROM `player_punishments` WHERE `player_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `player_id`, `start_time`, `duration`, `reason` FROM `player_punishments` WHERE `player_id`=? AND `punishment_type`=?";
constexpr std::string_view UPDATE_QUERY = "UPDATE `player_punishments` SET `duration`=? WHERE `player_id`=? AND `punishment_type`=?";
constexpr std::string_view REPLACE_QUERY = "REPLACE INTO `player_punishments` VALUES (?,?,?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_punishments` WHERE `player_id`=? AND `punishment_type`=?";

} // namespace

void PlayerPunishmentsDAO::loadPlayerPunishments(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerPunishmentsDAO::storePlayerPunishment(model::gameobjects::player::Player& player,
	services::PunishmentService_PunishmentType punishmentType) {
	AION_UNPORTED();
}

void PlayerPunishmentsDAO::punishPlayer(int32_t playerId, services::PunishmentService_PunishmentType punishmentType, int64_t duration,
	std::string_view reason) {
	AION_UNPORTED();
}

void PlayerPunishmentsDAO::punishPlayer(model::gameobjects::player::Player& player, services::PunishmentService_PunishmentType punishmentType,
	std::string_view reason) {
	AION_UNPORTED();
}

void PlayerPunishmentsDAO::unpunishPlayer(int32_t playerId, services::PunishmentService_PunishmentType punishmentType) {
	AION_UNPORTED();
}

runtime::Ref<model::account::CharacterBanInfo> PlayerPunishmentsDAO::getCharBanInfo(int32_t playerId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
