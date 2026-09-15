#include "aion/gameserver/dao/PlayerPunishmentsDAO.h"

#include <string_view>

#include "aion/commons/database/DB.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/model/account/CharacterBanInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/services/PunishmentService_PunishmentType.h"

namespace aion::gameserver::dao {

using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::ResultSet;
using services::PunishmentService_PunishmentType;

namespace {

constexpr std::string_view SELECT_DURATIONS_QUERY = "SELECT `punishment_type`, `duration` FROM `player_punishments` WHERE `player_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `player_id`, `start_time`, `duration`, `reason` FROM `player_punishments` WHERE `player_id`=? AND `punishment_type`=?";
constexpr std::string_view UPDATE_QUERY = "UPDATE `player_punishments` SET `duration`=? WHERE `player_id`=? AND `punishment_type`=?";
constexpr std::string_view REPLACE_QUERY = "REPLACE INTO `player_punishments` VALUES (?,?,?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_punishments` WHERE `player_id`=? AND `punishment_type`=?";

} // namespace

void PlayerPunishmentsDAO::loadPlayerPunishments(model::gameobjects::player::Player& player) {
	DB::select(
		SELECT_DURATIONS_QUERY, [&](PreparedStatement& ps) { ps.setInt(1, player.getObjectId()); },
		[&](ResultSet& rs) {
			while (rs.next()) {
				PunishmentService_PunishmentType punishmentType = detail::enumValueOf<PunishmentService_PunishmentType>(
					rs.getString("punishment_type"), "com.aionemu.gameserver.services.PunishmentService.PunishmentType");
				if (punishmentType == PunishmentService_PunishmentType::PRISON) {
					player.setPrisonEndTimeMillis(commons::utils::currentTimeMillis() + rs.getLong("duration") * 1000);
				} else if (punishmentType == PunishmentService_PunishmentType::GATHER) {
					player.setGatherRestrictionExpirationTime(commons::utils::currentTimeMillis() + rs.getLong("duration") * 1000);
				}
			}
		});
}

void PlayerPunishmentsDAO::storePlayerPunishment(model::gameobjects::player::Player& player, services::PunishmentService_PunishmentType punishmentType) {
	DB::insertUpdate(UPDATE_QUERY, [&](PreparedStatement& ps) {
		if (punishmentType == PunishmentService_PunishmentType::PRISON) {
			ps.setLong(1, player.getPrisonDurationSeconds());
		} else if (punishmentType == PunishmentService_PunishmentType::GATHER) {
			ps.setLong(1, player.getGatherRestrictionDurationSeconds());
		}
		ps.setInt(2, player.getObjectId());
		ps.setString(3, detail::enumName(punishmentType));
		ps.execute();
	});
}

void PlayerPunishmentsDAO::punishPlayer(int32_t playerId, services::PunishmentService_PunishmentType punishmentType, int64_t duration,
	std::string_view reason) {
	DB::insertUpdate(REPLACE_QUERY, [&](PreparedStatement& ps) {
		ps.setInt(1, playerId);
		ps.setString(2, detail::enumName(punishmentType));
		ps.setLong(3, commons::utils::currentTimeMillis() / 1000);
		ps.setLong(4, duration);
		ps.setString(5, reason);
		ps.execute();
	});
}

void PlayerPunishmentsDAO::punishPlayer(model::gameobjects::player::Player& player, services::PunishmentService_PunishmentType punishmentType,
	std::string_view reason) {
	if (punishmentType == PunishmentService_PunishmentType::PRISON)
		punishPlayer(player.getObjectId(), punishmentType, player.getPrisonDurationSeconds(), reason);
	else if (punishmentType == PunishmentService_PunishmentType::GATHER)
		punishPlayer(player.getObjectId(), punishmentType, player.getGatherRestrictionDurationSeconds(), reason);
}

void PlayerPunishmentsDAO::unpunishPlayer(int32_t playerId, services::PunishmentService_PunishmentType punishmentType) {
	DB::insertUpdate(DELETE_QUERY, [&](PreparedStatement& ps) {
		ps.setInt(1, playerId);
		ps.setString(2, detail::enumName(punishmentType));
		ps.execute();
	});
}

runtime::Ref<model::account::CharacterBanInfo> PlayerPunishmentsDAO::getCharBanInfo(int32_t playerId) {
	runtime::Ref<model::account::CharacterBanInfo> charBan;
	DB::select(
		SELECT_QUERY,
		[&](PreparedStatement& ps) {
			ps.setInt(1, playerId);
			ps.setString(2, detail::enumName(PunishmentService_PunishmentType::CHARBAN));
		},
		[&](ResultSet& rs) {
			while (rs.next()) {
				charBan = model::account::CharacterBanInfo::create(rs.getLong("start_time"), rs.getLong("duration"), rs.getString("reason"));
			}
		});
	return charBan;
}

} // namespace aion::gameserver::dao
