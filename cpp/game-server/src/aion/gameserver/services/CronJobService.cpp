#include "aion/gameserver/services/CronJobService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

CronJobService::CronJobService() {
	AION_UNPORTED();
}

CronJobService::~CronJobService() = default;

CronJobService& CronJobService::getInstance() {
	static CronJobService instance; // Java SingletonHolder
	return instance;
}

// Defined here (hub-headers.md §9.3): only CronJobService bodies use it. The members are what fieldmap.py prints (K5); the
// port stores the spawner in CronService, so it becomes a PinnedCallback task with Ref members (fieldmap change request).
// Java implements Runnable
class CronJobService::IdianDepthPortalSpawner {
public:
	runtime::Ptr<model::gameobjects::Npc> asmodianUndergroundEntrance{};
	runtime::Ptr<model::gameobjects::Npc> elyosUndergroundEntrance{};
	void run(); // @Override of a Java library type
};

void CronJobService::IdianDepthPortalSpawner::run() {
	AION_UNPORTED();
}

// anonymous Runnable at CronJobService.java:35 (fieldmap key CronJobService$1); argument 1 of schedule(); storage: stored in CronService
void CronJobService::scheduleMoltenusSpawn() {
	AION_UNPORTED();
}

void CronJobService::scheduleAhserionsFlight() {
	AION_UNPORTED();
}

void CronJobService::scheduleIdianDepthPortalSpawns() {
	AION_UNPORTED();
}

void CronJobService::scheduleLegionDominionCalculation() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
