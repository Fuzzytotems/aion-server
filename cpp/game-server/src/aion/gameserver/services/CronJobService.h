#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * This class is used for miscellaneous long-time schedules like specific spawns.
 * <p>
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 *
 * @author Estrayl
 */
class CronJobService : public runtime::Immortal {
private:
	class IdianDepthPortalSpawner;
	CronJobService();
	~CronJobService();
public:
	static CronJobService& getInstance(); // Java singleton
private:
	void scheduleMoltenusSpawn();
	void scheduleAhserionsFlight();
	void scheduleIdianDepthPortalSpawns();
	void scheduleLegionDominionCalculation();
};

} // namespace aion::gameserver::services
