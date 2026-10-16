#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "aion/loginserver/model/base/BannedMacEntry.h"

namespace aion::loginserver::controller {

/**
 * The banned MAC addresses, loaded from the database on first use and kept in sync with it.
 * <p>
 * <b>Threads.</b> The map is guarded by a leaf mutex (Java: an unsynchronized HashMap changed by game server packet threads and iterated while
 * writing SM_MACBAN_LIST); getMap() returns a copy. The database is updated after the lock was released.
 * <p>
 * Java: com.aionemu.loginserver.controller.BannedMacManager
 *
 * @author KID
 */
class BannedMacManager {
public:
	/** Java: getInstance() - created (and loaded from the database) on first use */
	static BannedMacManager& getInstance();

	void unban(std::string_view address, std::string_view details);

	/** @param time ban end in milliseconds since the epoch */
	void ban(std::string_view address, int64_t time, std::string_view details);

	/** @return a copy of the banned addresses */
	std::unordered_map<std::string, model::base::BannedMacEntry> getMap() const;

	/** C++ addition for tests: replaces the map with the current database content */
	void reload();

private:
	BannedMacManager();

	mutable std::mutex mutex;
	std::unordered_map<std::string, model::base::BannedMacEntry> bannedList;
};

} // namespace aion::loginserver::controller
