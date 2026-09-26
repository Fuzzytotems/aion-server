#include "aion/gameserver/services/mail/MailFormatter.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::mail {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous MailPart at MailFormatter.java:25 (com.aionemu.gameserver.services.mail.MailFormatter$1); local formatter; storage: local
//   anonymous MailPart at MailFormatter.java:62 (com.aionemu.gameserver.services.mail.MailFormatter$2); local formatter; storage: local
//   anonymous MailPart at MailFormatter.java:86 (com.aionemu.gameserver.services.mail.MailFormatter$3); local formatter; storage: local
//   anonymous MailPart at MailFormatter.java:113 (com.aionemu.gameserver.services.mail.MailFormatter$4); local formatter; storage: local
//   anonymous MailPart at MailFormatter.java:141 (com.aionemu.gameserver.services.mail.MailFormatter$5); local formatter; storage: local

void MailFormatter::sendBlackCloudMail(std::string_view recipientName, int32_t itemObjectId, int32_t itemCount) {
	AION_UNPORTED();
}

void MailFormatter::sendHouseMaintenanceMail(model::house::House& ownedHouse, std::string_view ownerName, int64_t impoundTimeMillis, int64_t kinah) {
	AION_UNPORTED();
}

void MailFormatter::sendHouseAuctionMail(runtime::Ptr<model::house::House> ownedHouse, model::gameobjects::player::PlayerCommonData& playerData,
	AuctionResult result, int64_t time, int64_t returnKinah) {
	AION_UNPORTED();
}

void MailFormatter::sendAbyssRewardMail(model::siege::SiegeLocation& siegeLocation, model::gameobjects::player::PlayerCommonData& playerData,
	AbyssSiegeLevel level, SiegeResult result, int64_t time, int32_t attachedItemObjId, int64_t attachedItemCount, int64_t attachedKinahCount) {
	AION_UNPORTED();
}

void MailFormatter::sendGuildDominionRewardMail(model::gameobjects::player::Player& player, int32_t territorialId,
	std::optional<commons::database::Timestamp> participantDate, int32_t itemId, int32_t itemCount) {
	AION_UNPORTED();
}

void MailFormatter::sendCustomAbyssDefeatRewardMail(model::gameobjects::player::PlayerCommonData& playerCommonData, int32_t itemId,
	int32_t itemCount) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::mail
