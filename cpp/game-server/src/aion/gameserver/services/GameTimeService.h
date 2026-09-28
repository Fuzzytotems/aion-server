#pragma once

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/services/fwd.h"
#include "aion/gameserver/utils/time/gametime/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 *
 * @author ATracer, Neon
 */
class GameTimeService : public runtime::Immortal {
private:
	const runtime::Ref<utils::time::gametime::GameTime> gameTime; // Java: = new GameTime(ServerVariablesDAO.loadInt("time"))
	runtime::AtomicBoolean isStarted{AION_LOCK_CLASS(GameTimeService::isStarted)}; // Java: = new AtomicBoolean()
	GameTimeService();
	~GameTimeService();
public:
	runtime::Ptr<utils::time::gametime::GameTime> getGameTime() const { return this->gameTime; }
	/** Saves the current time to the database */
	bool saveGameTime();
	void startClock();
	static GameTimeService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services
