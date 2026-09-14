#include "aion/gameserver/dao/AnnouncementsDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous ReadStH at AnnouncementsDAO.java:24 (com.aionemu.gameserver.dao.AnnouncementsDAO$1); argument 2 of select(); storage: sync
//   anonymous IUStH at AnnouncementsDAO.java:65 (com.aionemu.gameserver.dao.AnnouncementsDAO$2); argument 2 of insertUpdate(); storage: sync

std::vector<runtime::Ref<model::Announcement>> AnnouncementsDAO::loadAnnouncements() {
	AION_UNPORTED();
}

runtime::Ref<model::Announcement> AnnouncementsDAO::getAnnouncement(commons::database::ResultSet& resultSet) {
	AION_UNPORTED();
}

int32_t AnnouncementsDAO::addAnnouncement(std::string_view message, std::string_view faction, std::string_view chatType, int32_t delay) {
	AION_UNPORTED();
}

bool AnnouncementsDAO::delAnnouncement(int32_t id) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
