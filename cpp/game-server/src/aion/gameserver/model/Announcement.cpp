#include "aion/gameserver/model/Announcement.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/Race.h"

namespace aion::gameserver::model {

Announcement::Announcement(int32_t value, std::string_view announceValue, std::string_view factionValue, std::string_view chatTypeValue,
	int32_t delayValue)
	: id(value), faction(getFactionEnum(factionValue)), announce(std::string(announceValue)), chatType(std::string(chatTypeValue)),
	  delay(delayValue) {
}

runtime::Ref<Announcement> Announcement::create(int32_t value, std::string_view announceValue, std::string_view factionValue,
	std::string_view chatTypeValue, int32_t delayValue) {
	return runtime::makeRef<Announcement>(value, announceValue, factionValue, chatTypeValue, delayValue);
}

std::optional<Race> Announcement::getFactionEnum(std::string_view value) {
	if (commons::utils::StringUtils::equalsIgnoreCase(value, "ELYOS"))
		return Race::ELYOS;
	else if (commons::utils::StringUtils::equalsIgnoreCase(value, "ASMODIANS"))
		return Race::ASMODIANS;
	return std::nullopt;
}

ChatType Announcement::getChatType() {
	using commons::utils::StringUtils::equalsIgnoreCase;
	if (equalsIgnoreCase(chatType, "System"))
		return ChatType::GOLDEN_YELLOW;
	else if (equalsIgnoreCase(chatType, "White"))
		return ChatType::WHITE_CENTER;
	else if (equalsIgnoreCase(chatType, "Yellow"))
		return ChatType::YELLOW_CENTER;
	else if (equalsIgnoreCase(chatType, "Shout"))
		return ChatType::SHOUT;
	else if (equalsIgnoreCase(chatType, "Orange"))
		return ChatType::GROUP_LEADER;
	else
		return ChatType::BRIGHT_YELLOW_CENTER;
}

Announcement::~Announcement() = default;

} // namespace aion::gameserver::model
