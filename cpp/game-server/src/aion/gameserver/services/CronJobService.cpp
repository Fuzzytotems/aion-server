#include "aion/gameserver/services/CronJobService.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/services/LegionDominionService.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/panesterra/PanesterraService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::services {

namespace {

namespace Rnd = commons::utils::Rnd;
using model::gameobjects::Npc;
using spawnengine::SpawnEngine;

/** Java CronService.schedule(Runnable, CronExpression): a null expression (unset config) is dereferenced by Quartz */
const cron::CronExpression& required(const cron::CronExpression* expression, const char* key) {
	if (expression == nullptr)
		throw runtime::NullPointerException(std::string("Cron expression ") + key + " is null");
	return *expression;
}

/** Java: (Npc) SpawnEngine.spawnObject(template, 1) (ClassCastException for another object type) */
runtime::Ref<Npc> spawnNpc(model::templates::spawns::SpawnTemplate& spawn) {
	runtime::Ptr<model::gameobjects::VisibleObject> object = SpawnEngine::spawnObject(spawn, 1);
	return runtime::Ref<Npc>(runtime::cast<Npc>(object));
}

// anonymous Runnable at CronJobService.java:35 (fieldmap key CronJobService$1, fieldmap.json: K4 CronJobService_Runnable with Field<Ref<Npc>>
// moltenus); argument 1 of schedule(); storage: stored in CronService (the cron job holds the only Ref)
class CronJobService_Runnable final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<Npc>> moltenus{};

	CronJobService_Runnable() = default;
	~CronJobService_Runnable() override = default;

public:
	static runtime::Ref<CronJobService_Runnable> create() { return runtime::makeRef<CronJobService_Runnable>(); }

	void run() {
		runtime::Ptr<Npc> current = moltenus.get();
		if (current && current->isSpawned())
			return;

		runtime::Ref<model::templates::spawns::SpawnTemplate> template_;
		switch (Rnd::get(1, 3)) {
			case 1:
				template_ = SpawnEngine::newSingleTimeSpawn(400010000, 251045, 2464.9199f, 1689.0f, 2882.221f, int8_t{0});
				break;
			case 2:
				template_ = SpawnEngine::newSingleTimeSpawn(400010000, 251045, 2263.4812f, 2587.1633f, 2879.5447f, int8_t{0});
				break;
			default:
				template_ = SpawnEngine::newSingleTimeSpawn(400010000, 251045, 1692.96f, 1809.04f, 2886.027f, int8_t{0});
				break;
		}
		moltenus.set(spawnNpc(*template_));
		// Despawn task
		utils::ThreadPoolManager::getInstance().schedule({this}, [this] {
			runtime::Ptr<Npc> spawned = moltenus.get();
			if (spawned && !spawned->isDead()) {
				spawned->getController().delete_();
				moltenus.set(nullptr);
			}
		}, 3600 * 1000);
	}
};

} // namespace

CronJobService::CronJobService() {
	scheduleMoltenusSpawn();
	scheduleAhserionsFlight();
	scheduleIdianDepthPortalSpawns();
	scheduleLegionDominionCalculation();
}

CronJobService::~CronJobService() = default;

CronJobService& CronJobService::getInstance() {
	static CronJobService instance; // Java: private static final CronJobService INSTANCE
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
	runtime::Ref<model::templates::spawns::SpawnTemplate> elyosSpawn;
	switch (Rnd::get(1, 4)) {
		case 1:
			elyosSpawn = SpawnEngine::newSingleTimeSpawn(600100000, 731631, 721.39f, 268.67f, 291.636f, int8_t{60}); // Levinshor
			break;
		case 2:
			elyosSpawn = SpawnEngine::newSingleTimeSpawn(600100000, 731631, 332.40f, 1903.37f, 232.000f, int8_t{110}); // Levinshor
			break;
		case 3:
			elyosSpawn = SpawnEngine::newSingleTimeSpawn(600090000, 731631, 1179.58f, 687.52f, 190.625f, int8_t{0}); // Kaldor
			break;
		default:
			elyosSpawn = SpawnEngine::newSingleTimeSpawn(210070000, 731631, 777.01f, 1479.86f, 457.375f, int8_t{30}); // Cygnea
			break;
	}
	runtime::Ref<model::templates::spawns::SpawnTemplate> asmodianSpawn;
	switch (Rnd::get(1, 4)) {
		case 1:
			asmodianSpawn = SpawnEngine::newSingleTimeSpawn(600100000, 731632, 1478.78f, 1844.20f, 225.987f, int8_t{45}); // Levinshor
			break;
		case 2:
			asmodianSpawn = SpawnEngine::newSingleTimeSpawn(600100000, 731632, 1870.49f, 41.64f, 244.711f, int8_t{15}); // Levinshor
			break;
		case 3:
			asmodianSpawn = SpawnEngine::newSingleTimeSpawn(600090000, 731632, 415.01f, 564.42f, 142.0f, int8_t{100}); // Kaldor
			break;
		default:
			asmodianSpawn = SpawnEngine::newSingleTimeSpawn(220080000, 731632, 233.39f, 1137.03f, 225.875f, int8_t{105}); // Enshar
			break;
	}
	if (runtime::Ptr<model::gameobjects::Npc> asmodian = asmodianUndergroundEntrance.get())
		asmodian->getController().delete_();
	if (runtime::Ptr<model::gameobjects::Npc> elyos = elyosUndergroundEntrance.get())
		elyos->getController().delete_();
	elyosUndergroundEntrance.set(spawnNpc(*elyosSpawn));
	asmodianUndergroundEntrance.set(spawnNpc(*asmodianSpawn));
	// Java reschedules after 3.6 to 18 seconds (Rnd.get(3600, 18000) with TimeUnit.MILLISECONDS); kept as it is
	utils::ThreadPoolManager::getInstance().schedule({this}, [this] { run(); }, Rnd::get(3600, 18000), utils::TimeUnit::MILLISECONDS);
}

// anonymous Runnable at CronJobService.java:35 (fieldmap key CronJobService$1); argument 1 of schedule(); storage: stored in CronService
void CronJobService::scheduleMoltenusSpawn() {
	cron::CronService::getInstance().schedule(
		cron::CronJob(runtime::Pin(), [runnable = CronJobService_Runnable::create()] { runnable->run(); }),
		required(configs::main::SiegeConfig::MOLTENUS_SPAWN_SCHEDULE.load(), "gameserver.moltenus.time"));
}

// lambda at CronJobService.java:62 (fieldmap key CronJobService@L62:38); stored in CronService; captures nothing
void CronJobService::scheduleAhserionsFlight() {
	cron::CronService::getInstance().schedule(cron::CronJob([] { panesterra::PanesterraService::getInstance().startAhserionRaid(); }),
		required(configs::main::SiegeConfig::AHSERION_START_SCHEDULE.load(), "gameserver.siege.panesterra.ahserion.time"));
}

void CronJobService::scheduleIdianDepthPortalSpawns() {
	IdianDepthPortalSpawner::create()->run(); // not a cronjob anymore, but let's keep it here
}

// lambda at CronJobService.java:70 (fieldmap key CronJobService@L70:38); stored in CronService; captures nothing
void CronJobService::scheduleLegionDominionCalculation() {
	cron::CronService::getInstance().schedule(cron::CronJob([] { LegionDominionService::getInstance().startWeeklyCalculation(); }),
		"0 0 9 ? * WED *");
}

} // namespace aion::gameserver::services
