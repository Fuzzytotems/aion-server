#include "aion/gameserver/services/CronJobService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/Npc.h"

namespace aion::gameserver::services {

CronJobService::CronJobService() {
	AION_UNPORTED();
}

CronJobService::~CronJobService() = default;

CronJobService& CronJobService::getInstance() {
	static CronJobService instance; // Java SingletonHolder
	return instance;
}

// Defined here (hub-headers.md §9.3): only CronJobService bodies use it. It reschedules itself (schedule(this, ...)) and keeps the two
// spawned portal Npcs between runs, so it is K4 (fieldmap.toml [kinds]): RefCounted, created with create(), the Npcs Field<Ref>s. The
// pending Future (pin: the spawner) is its only holder; it runs until shutdown, like Java.
// Java implements Runnable
class CronJobService::IdianDepthPortalSpawner final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<model::gameobjects::Npc>> asmodianUndergroundEntrance{};
	runtime::Field<runtime::Ref<model::gameobjects::Npc>> elyosUndergroundEntrance{};

protected:
	IdianDepthPortalSpawner();
	~IdianDepthPortalSpawner() override;

public:
	/** Java: new IdianDepthPortalSpawner() */
	static runtime::Ref<IdianDepthPortalSpawner> create();
	void run(); // @Override of a Java library type
};

CronJobService::IdianDepthPortalSpawner::IdianDepthPortalSpawner() = default;

CronJobService::IdianDepthPortalSpawner::~IdianDepthPortalSpawner() = default;

runtime::Ref<CronJobService::IdianDepthPortalSpawner> CronJobService::IdianDepthPortalSpawner::create() {
	return runtime::makeRef<IdianDepthPortalSpawner>();
}

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
