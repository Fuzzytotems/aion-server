#include "aion/gameserver/model/Announcement.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model {

Announcement::Announcement(int32_t value, std::string_view announceValue, std::string_view factionValue, std::string_view chatTypeValue,
	int32_t delayValue)
	: id(value), faction(), announce(std::string(announceValue)), chatType(std::string(chatTypeValue)), delay(delayValue) {
	// Java: this.faction = getFactionEnum(faction)
	AION_UNPORTED();
}

runtime::Ref<Announcement> Announcement::create(int32_t value, std::string_view announceValue, std::string_view factionValue,
	std::string_view chatTypeValue, int32_t delayValue) {
	return runtime::makeRef<Announcement>(value, announceValue, factionValue, chatTypeValue, delayValue);
}

std::optional<Race> Announcement::getFactionEnum(std::string_view value) {
	AION_UNPORTED();
}

ChatType Announcement::getChatType() {
	AION_UNPORTED();
}

Announcement::~Announcement() = default;

} // namespace aion::gameserver::model
