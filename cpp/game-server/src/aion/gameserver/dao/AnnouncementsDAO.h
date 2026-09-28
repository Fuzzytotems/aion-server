#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/fwd.h"

namespace aion::gameserver::dao {

/**
 * DAO that manages Announcements
 *
 * @author Divinity
 */
class AnnouncementsDAO {
public:
	static std::vector<runtime::Ref<model::Announcement>> loadAnnouncements();
private:
	static runtime::Ref<model::Announcement> getAnnouncement(commons::database::ResultSet& resultSet);
public:
	static int32_t addAnnouncement(std::string_view message, std::string_view faction, std::string_view chatType, int32_t delay);
	static bool delAnnouncement(int32_t id);
};

} // namespace aion::gameserver::dao
