#include "aion/gameserver/services/mail/MailFormatter.h"

#include <chrono>
#include <functional>
#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/mail/IMailFormatter.h"
#include "aion/gameserver/model/templates/mail/MailPartType.h"
#include "aion/gameserver/model/templates/mail/MailTemplate.h"
#include "aion/gameserver/model/templates/mail/Mails.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/mail/AbyssSiegeLevelInfo.h"
#include "aion/gameserver/services/mail/AuctionResultInfo.h"
#include "aion/gameserver/services/mail/SiegeResultInfo.h"
#include "aion/gameserver/services/mail/SystemMailService.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::services::mail {

namespace {

using model::Race;
using model::gameobjects::LetterType;
using model::templates::mail::IMailFormatter;
using model::templates::mail::MailPartType;
using model::templates::mail::MailTemplate;

/**
 * Java: the anonymous `new MailPart() { getParamValue ... }` formatters of MailFormatter (MailFormatter$1..$5, hub-headers.md §7.3), each a
 * local whose getParamValue reads the captured arguments. Only getParamValue is called (MailPart.getFormattedString of the template's parts
 * asks the custom formatter for the values); getType is MailPart's CUSTOM, and getFormattedString(partType) of an anonymous part with no id
 * is MailPart's NullPointerException (`id > 0` on a null Integer).
 */
class LocalMailPart final : public IMailFormatter {
public:
	explicit LocalMailPart(std::function<std::string(std::string_view)> paramValue) : paramValue(std::move(paramValue)) {}

	MailPartType getType() const override { return MailPartType::CUSTOM; }

	std::string getFormattedString(MailPartType /*partType*/) const override {
		throw runtime::NullPointerException("MailPart.id is null"); // Java: an anonymous MailPart has no id
	}

	std::string getParamValue(std::string_view name) const override { return paramValue(name); }

private:
	std::function<std::string(std::string_view)> paramValue;
};

/** Java: the template's methods on a null template (no such system mail) throw NullPointerException */
const MailTemplate& requireTemplate(const MailTemplate* template_, std::string_view name) {
	if (template_ == nullptr)
		throw runtime::NullPointerException("no mail template " + std::string(name));
	return *template_;
}

/** Java Duration.ofMillis(millis).toDays(): the seconds are floorDiv(millis, 1000), the days their truncated quotient by 86,400 */
int64_t durationDays(int64_t millis) {
	int64_t seconds = millis / 1000;
	if (millis % 1000 != 0 && millis < 0)
		--seconds;
	return seconds / 86400;
}

} // namespace

// Java MailFormatter.java:21-45
void MailFormatter::sendBlackCloudMail(std::string_view recipientName, int32_t itemObjectId, int32_t itemCount) {
	const MailTemplate& template_ = requireTemplate(
		dataholders::DataManager::SYSTEM_MAIL_TEMPLATES->getMailTemplate("$$CASH_ITEM_MAIL", "", Race::PC_ALL), "$$CASH_ITEM_MAIL");

	LocalMailPart formatter([itemObjectId, itemCount](std::string_view name) -> std::string {
		if (name == "itemid")
			return std::to_string(itemObjectId);
		else if (name == "count")
			return std::to_string(itemCount);
		else if (name == "unk1")
			return "0";
		else if (name == "purchasedate")
			return std::to_string(commons::utils::currentTimeMillis() / 1000);
		return "";
	});

	std::string title = template_.getFormattedTitle(&formatter);
	std::string body = template_.getFormattedMessage(&formatter);

	SystemMailService::sendMail("$$CASH_ITEM_MAIL", recipientName, title, body, itemObjectId, itemCount, 0, LetterType::BLACKCLOUD);
}

// Java MailFormatter.java:47-77
void MailFormatter::sendHouseMaintenanceMail(model::house::House& ownedHouse, std::string_view ownerName, int64_t impoundTimeMillis, int64_t kinah) {
	std::string templateName;
	int64_t daysUntilImpoundment = durationDays(impoundTimeMillis - commons::utils::currentTimeMillis());
	if (daysUntilImpoundment <= 0)
		templateName = "$$HS_OVERDUE_3RD";
	else if (daysUntilImpoundment <= 7)
		templateName = "$$HS_OVERDUE_2ND";
	else if (daysUntilImpoundment <= 14)
		templateName = "$$HS_OVERDUE_1ST";
	else
		return;

	const MailTemplate& template_ =
		requireTemplate(dataholders::DataManager::SYSTEM_MAIL_TEMPLATES->getMailTemplate(templateName, "", Race::PC_ALL), templateName);

	model::house::House* house = &ownedHouse;
	LocalMailPart formatter([house, impoundTimeMillis](std::string_view name) -> std::string {
		if (name == "address")
			return std::to_string(house->getAddress()->getId());
		else if (name == "datetime")
			return std::to_string(impoundTimeMillis / 60000);
		return "";
	});

	std::string title = template_.getFormattedTitle(nullptr);
	std::string message = template_.getFormattedMessage(&formatter);

	SystemMailService::sendMail(templateName, ownerName, title, message, 0, 0, kinah, LetterType::NORMAL);
}

// Java MailFormatter.java:79-104. C++: `result` is an enum value (the callers never pass Java's null)
void MailFormatter::sendHouseAuctionMail(runtime::Ptr<model::house::House> ownedHouse, model::gameobjects::player::PlayerCommonData& playerData,
	AuctionResult result, int64_t time, int64_t returnKinah) {
	const MailTemplate* template_ = dataholders::DataManager::SYSTEM_MAIL_TEMPLATES->getMailTemplate("$$HS_AUCTION_MAIL", "", playerData.getRace());
	if (ownedHouse == nullptr)
		return;

	model::house::House* house = ownedHouse.get();
	model::gameobjects::player::PlayerCommonData* data = &playerData;
	LocalMailPart formatter([house, time, result, data](std::string_view name) -> std::string {
		if (name == "address")
			return std::to_string(house->getAddress()->getId());
		else if (name == "datetime")
			return std::to_string(time / 1000);
		else if (name == "resultid")
			return std::to_string(getId(result));
		else if (name == "raceid")
			return std::to_string(model::getRaceId(data->getRace()));
		return "";
	});

	std::string title = requireTemplate(template_, "$$HS_AUCTION_MAIL").getFormattedTitle(&formatter);
	std::string message = template_->getFormattedMessage(&formatter);

	SystemMailService::sendMail("$$HS_AUCTION_MAIL", playerData.getName(), title, message, 0, 0, returnKinah, LetterType::NORMAL);
}

// Java MailFormatter.java:106-135
void MailFormatter::sendAbyssRewardMail(model::siege::SiegeLocation& siegeLocation, model::gameobjects::player::PlayerCommonData& playerData,
	AbyssSiegeLevel level, SiegeResult result, int64_t time, int32_t attachedItemObjId, int64_t attachedItemCount, int64_t attachedKinahCount) {
	const MailTemplate& template_ = requireTemplate(
		dataholders::DataManager::SYSTEM_MAIL_TEMPLATES->getMailTemplate("$$ABYSS_REWARD_MAIL", "", playerData.getRace()), "$$ABYSS_REWARD_MAIL");

	model::siege::SiegeLocation* location = &siegeLocation;
	model::gameobjects::player::PlayerCommonData* data = &playerData;
	LocalMailPart formatter([location, time, level, data, result](std::string_view name) -> std::string {
		if (name == "siegelocid")
			return std::to_string(location->getTemplate()->getId());
		else if (name == "datetime")
			return std::to_string(time / 1000);
		else if (name == "rankid")
			return std::to_string(getId(level));
		else if (name == "raceid")
			return std::to_string(model::getRaceId(data->getRace()));
		else if (name == "resultid")
			return std::to_string(getId(result));
		return "";
	});

	std::string title = template_.getFormattedTitle(&formatter);
	std::string message = template_.getFormattedMessage(&formatter);

	SystemMailService::sendMail("$$ABYSS_REWARD_MAIL", playerData.getName(), title, message, attachedItemObjId, attachedItemCount, attachedKinahCount,
		LetterType::NORMAL);
}

// Java MailFormatter.java:137-163. C++: Java's Timestamp.toLocalDateTime() is the date in the server's zone (ServerTime, GSConfig.TIME_ZONE_ID,
// which the Java server sets as the JVM default)
void MailFormatter::sendGuildDominionRewardMail(model::gameobjects::player::Player& player, int32_t territorialId,
	std::optional<commons::database::Timestamp> participantDate, int32_t itemId, int32_t itemCount) {
	const MailTemplate* template_ = dataholders::DataManager::SYSTEM_MAIL_TEMPLATES->getMailTemplate("$$GD_REWARD_MAIL", "", player.getRace());
	if (!participantDate)
		throw runtime::NullPointerException("participantDate"); // Java: participantDate.toLocalDateTime() on null
	const std::chrono::year_month_day participationDate(
		std::chrono::floor<std::chrono::days>(utils::time::ServerTime::atDate(*participantDate).get_local_time()));
	model::gameobjects::player::Player* recipient = &player;
	LocalMailPart formatter([participationDate, territorialId, recipient](std::string_view name) -> std::string {
		std::string val;
		if (name == "month") {
			val = std::to_string(static_cast<unsigned>(participationDate.month()));
		} else if (name == "day") {
			val = std::to_string(static_cast<unsigned>(participationDate.day()));
		} else if (name == "territorial") {
			val = std::to_string(territorialId);
		} else if (name == "legionName") {
			runtime::Ptr<model::team::legion::Legion> legion = recipient->getLegion();
			val = legion == nullptr ? "" : legion->getName();
		}
		return val;
	});

	std::string title = requireTemplate(template_, "$$GD_REWARD_MAIL").getFormattedTitle(&formatter);
	std::string body = template_->getFormattedMessage(&formatter);

	SystemMailService::sendMail("$$GD_REWARD_MAIL", player.getName(), title, body, itemId, itemCount, 0, LetterType::NORMAL);
}

// Java MailFormatter.java:165-169
void MailFormatter::sendCustomAbyssDefeatRewardMail(model::gameobjects::player::PlayerCommonData& playerCommonData, int32_t itemId,
	int32_t itemCount) {
	SystemMailService::sendMail(playerCommonData.getRace() == Race::ELYOS ? "%NPC:203700" : "%NPC:204052", // Fasimedes, Vidar
		playerCommonData.getName(), "$901513",                                                            // Reward Statement
		"", itemId, itemCount, 0, LetterType::NORMAL);
}

} // namespace aion::gameserver::services::mail
