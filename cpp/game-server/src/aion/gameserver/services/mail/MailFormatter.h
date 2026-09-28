#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/services/mail/fwd.h"

namespace aion::gameserver::services::mail {

/**
 * @author Rolandas
 */
class MailFormatter final {
public:
	static void sendBlackCloudMail(std::string_view recipientName, int32_t itemObjectId, int32_t itemCount);
	static void sendHouseMaintenanceMail(model::house::House& ownedHouse, std::string_view ownerName, int64_t impoundTimeMillis, int64_t kinah);
	static void sendHouseAuctionMail(runtime::Ptr<model::house::House> ownedHouse, model::gameobjects::player::PlayerCommonData& playerData,
		AuctionResult result, int64_t time, int64_t returnKinah);
	static void sendAbyssRewardMail(model::siege::SiegeLocation& siegeLocation, model::gameobjects::player::PlayerCommonData& playerData,
		AbyssSiegeLevel level, SiegeResult result, int64_t time, int32_t attachedItemObjId, int64_t attachedItemCount, int64_t attachedKinahCount);
	static void sendGuildDominionRewardMail(model::gameobjects::player::Player& player, int32_t territorialId,
		std::optional<commons::database::Timestamp> participantDate, int32_t itemId, int32_t itemCount);
	static void sendCustomAbyssDefeatRewardMail(model::gameobjects::player::PlayerCommonData& playerCommonData, int32_t itemId, int32_t itemCount);
};

} // namespace aion::gameserver::services::mail
