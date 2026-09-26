#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/account/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/PunishmentService_PunishmentType.h"

namespace aion::gameserver::dao {

/**
 * @author lord_rex, Cura, nrg
 */
class PlayerPunishmentsDAO {
public:
	static void loadPlayerPunishments(model::gameobjects::player::Player& player);
	static void storePlayerPunishment(model::gameobjects::player::Player& player, services::PunishmentService_PunishmentType punishmentType);
	static void punishPlayer(int32_t playerId, services::PunishmentService_PunishmentType punishmentType, int64_t duration, std::string_view reason);
	static void punishPlayer(model::gameobjects::player::Player& player, services::PunishmentService_PunishmentType punishmentType,
		std::string_view reason);
	static void unpunishPlayer(int32_t playerId, services::PunishmentService_PunishmentType punishmentType);
	static runtime::Ref<model::account::CharacterBanInfo> getCharBanInfo(int32_t playerId);
};

} // namespace aion::gameserver::dao
