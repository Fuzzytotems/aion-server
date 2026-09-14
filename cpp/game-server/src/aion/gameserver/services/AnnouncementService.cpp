#include "aion/gameserver/services/AnnouncementService.h"

#include "aion/gameserver/model/Announcement.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

AnnouncementService::AnnouncementService() {
	AION_UNPORTED();
}

AnnouncementService::~AnnouncementService() = default;

AnnouncementService& AnnouncementService::getInstance() {
	static AnnouncementService instance; // Java SingletonHolder
	return instance;
}

void AnnouncementService::reload() {
	AION_UNPORTED();
}

// anonymous Runnable at AnnouncementService.java:55 (fieldmap key AnnouncementService$1); argument 1 of scheduleAtFixedRate(); storage: task in AnnouncementService
void AnnouncementService::schedule(model::Announcement& announce) {
	AION_UNPORTED();
}

bool AnnouncementService::addAnnouncement(std::string_view message, std::string_view faction, std::string_view chatType, int32_t delay) {
	AION_UNPORTED();
}

bool AnnouncementService::delAnnouncement(int32_t id) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::Announcement>> AnnouncementService::getAnnouncements() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
