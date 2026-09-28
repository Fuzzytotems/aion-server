#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * Automatic Announcement System
 * <p>
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 *
 * @author Divinity
 */
class AnnouncementService : public runtime::Immortal {
private:
	runtime::ConcurrentHashMap<int32_t, runtime::FutureRef> delays{AION_LOCK_CLASS(AnnouncementService::delays#stripe)}; // Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::Announcement>> announcements{AION_LOCK_CLASS(AnnouncementService::announcements#stripe)}; // Java: = new ConcurrentHashMap<>()
	AnnouncementService();
	~AnnouncementService();
public:
	static AnnouncementService& getInstance(); // Java singleton
	/** Reload the announcements system */
	void reload();
private:
	void schedule(model::Announcement& announce);
public:
	bool addAnnouncement(std::string_view message, std::string_view faction, std::string_view chatType, int32_t delay);
	bool delAnnouncement(int32_t id);
	std::vector<runtime::Ptr<model::Announcement>> getAnnouncements();
};

} // namespace aion::gameserver::services
