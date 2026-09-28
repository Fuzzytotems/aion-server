#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 *
 * @author ATracer
 */
class DebugService : public runtime::Immortal {
private:
	static constexpr int32_t ANALYZE_PLAYERS_INTERVAL = 30 * 60 * 1000;
public:
	static DebugService& getInstance(); // Java singleton
private:
	DebugService();
	~DebugService();
	void analyzeWorldPlayers();
};

} // namespace aion::gameserver::services
