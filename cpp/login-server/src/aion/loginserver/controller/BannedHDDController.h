#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "aion/commons/database/SqlTypes.h"

namespace aion::loginserver::controller {

/**
 * The banned HDD serials, loaded from the database on first use and kept in sync with it.
 * <p>
 * <b>Threads.</b> The map is guarded by a leaf mutex (Java: an unsynchronized HashMap changed by game server packet threads and iterated while
 * writing SM_HDDBAN_LIST); getMap() returns a copy. The database is updated after the lock was released.
 * <p>
 * Java: com.aionemu.loginserver.controller.BannedHDDController
 *
 * @author ViAl
 */
class BannedHDDController {
public:
	/** Java: getInstance() - created (and loaded from the database) on first use */
	static BannedHDDController& getInstance();

	void unban(std::string_view serial);

	/** @param time ban end in milliseconds since the epoch */
	void ban(std::string_view serial, int64_t time);

	/** @return a copy of the banned serials and their ban end times */
	std::unordered_map<std::string, commons::database::Timestamp> getMap() const;

	/** C++ addition for tests: replaces the map with the current database content */
	void reload();

private:
	BannedHDDController();

	mutable std::mutex mutex;
	std::unordered_map<std::string, commons::database::Timestamp> bannedList;
};

} // namespace aion::loginserver::controller
