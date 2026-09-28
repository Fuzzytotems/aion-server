#include "aion/commons/database/MariaDbLibrary.h"

#include <cstddef>
#include <mutex>

#include <mysql.h>

#include "aion/commons/database/SQLException.h"

namespace aion::commons::database::MariaDbLibrary {

namespace {

struct State {
	std::mutex mutex;
	bool initialized = false;
	size_t liveHandles = 0;
};

/**
 * Intentionally never destroyed: connections kept in static objects (e.g. the DatabaseFactory pool when shutdown() was not called) release
 * their handle during static destruction, possibly after this translation unit's statics are gone.
 */
State& state() {
	static State* instance = new State();
	return *instance;
}

} // namespace

void acquire() {
	State& s = state();
	std::scoped_lock lock(s.mutex);
	if (!s.initialized) {
		if (mysql_library_init(0, nullptr, nullptr) != 0)
			throw SQLException("Could not initialize the MariaDB client library", "HY000");
		s.initialized = true;
	}
	++s.liveHandles;
}

void release() noexcept {
	State& s = state();
	std::scoped_lock lock(s.mutex);
	if (s.liveHandles > 0)
		--s.liveHandles;
}

bool shutdownIfUnused() noexcept {
	State& s = state();
	std::scoped_lock lock(s.mutex);
	if (!s.initialized)
		return true;
	if (s.liveHandles > 0)
		return false;
	mysql_library_end();
	s.initialized = false;
	return true;
}

} // namespace aion::commons::database::MariaDbLibrary
