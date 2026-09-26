#pragma once

// Shared fixture of the P4-11b controller tests: real Npcs created through VisibleObject::create<T> with the real constructor and postConstruct
// chain (NpcController part, NpcMoveController part, ObserveController, dummy AI). Test doubles standing in for bodies of later chunks: the npc
// template is bound from XML text, NPC_SKILL_DATA is an empty holder, the life stats are a CreatureLifeStats with fixed HP (NpcLifeStats reads the
// stat calculation of P5-01), and the Npc controller is a RecordingNpcController that records onStartMove/onStopMove instead of reaching the
// movement task managers of P4-10 (the tests do not run their periodic tasks). The scheduler is a DeterministicExecutor on a ManualClock.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::controllers::testing {

#define CONTROLLERS_TEST_SCOPE runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST))

inline std::atomic<int32_t> destroyedTestNpcs{0};

/** Life stats with fixed HP (NpcLifeStats reads the stat calculation of P5-01) */
class FixedLifeStats final : public model::stats::container::CreatureLifeStats {
public:
	explicit FixedLifeStats(model::gameobjects::Creature& owner) : CreatureLifeStats(owner, 1000, 100) {}
};

/** NpcController whose move notifications are recorded (CreatureController::onStartMove reaches MovementNotifyTask, P4-10) */
class RecordingNpcController final : public NpcController {
public:
	std::atomic<int32_t> startMoves{0};
	std::atomic<int32_t> stopMoves{0};

	void onStartMove() override { startMoves.fetch_add(1); }
	void onStopMove() override { stopMoves.fetch_add(1); }

	/** CreatureController::onDespawn without NpcController's service calls (effects, drops, instance handler of later chunks) */
	void despawnAsCreature() { CreatureController::onDespawn(); }
};

/** An Npc with the real constructor and postConstruct chain; only the stat containers are doubles. */
class ControllersTestNpc final : public model::gameobjects::Npc {
	AION_MAKE_REF_FRIEND
public:
	ControllersTestNpc(CreateKey key, std::unique_ptr<NpcController> controller, model::templates::spawns::SpawnTemplate& spawnTemplate,
		const model::templates::npc::NpcTemplate* objectTemplate)
		: Npc(key, std::move(controller), spawnTemplate, objectTemplate) {}

	RecordingNpcController& recordingController() const { return static_cast<RecordingNpcController&>(getController()); }

protected:
	~ControllersTestNpc() override { destroyedTestNpcs.fetch_add(1); }

	void setupStatContainers() override {
		setGameStats(std::make_unique<model::stats::container::NpcGameStats>(*this));
		setLifeStats(std::make_unique<FixedLifeStats>(*this));
	}
};

class ControllersTestSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	explicit ControllersTestSpawnTemplate(model::templates::spawns::SpawnGroup& group)
		: SpawnTemplate(group, 10.0f, 20.0f, 30.0f, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** Npc templates are immortal static data: kept for the process like the DataManager holder keeps them. */
inline const model::templates::npc::NpcTemplate* npcTemplate(std::string_view attributes) {
	xml::LoadContext context;
	return xml::bindString<model::templates::npc::NpcTemplate>(context, "<npc_template name_id=\"1\" " + std::string(attributes) + "/>").release();
}

class ControllersTest : public ::testing::Test {
protected:
	void SetUp() override {
		destroyedTestNpcs = 0;
		utils::ThreadPoolManager::installBackend(nullptr);
		auto backend = std::make_unique<runtime::DeterministicExecutor>(clock, 11);
		executor = backend.get(); // owned by ThreadPoolManager until TearDown installs no backend
		utils::ThreadPoolManager::installBackend(std::move(backend));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		runtime::LeakCensus::getInstance().install();
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		CONTROLLERS_TEST_SCOPE;
		group = model::templates::spawns::SpawnGroup::create(210010000, 700000, 0, nullptr);
		spawnTemplate = runtime::Ref<ControllersTestSpawnTemplate>(
			static_cast<ControllersTestSpawnTemplate&>(group->addSpawnTemplate(std::make_unique<ControllersTestSpawnTemplate>(*group))));
	}

	void TearDown() override {
		spawnTemplate.reset();
		group.reset();
		runtime::Reclaimer::getInstance().drain();
		runtime::LeakCensus::getInstance().uninstall();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
	}

	runtime::Ref<ControllersTestNpc> createNpc() {
		return model::gameobjects::VisibleObject::create<ControllersTestNpc>(std::make_unique<RecordingNpcController>(), *spawnTemplate, guardTemplate);
	}

	/** Moves the clock, running the due tasks of the deterministic executor at their due times */
	size_t advance(std::chrono::milliseconds dt) { return executor->advance(dt); }

	runtime::ManualClock clock{0};
	runtime::DeterministicExecutor* executor = nullptr;
	runtime::Ref<model::templates::spawns::SpawnGroup> group;
	runtime::Ref<ControllersTestSpawnTemplate> spawnTemplate;
	static inline const model::templates::npc::NpcTemplate* guardTemplate =
		npcTemplate(R"(npc_id="210001" level="12" name="guard" rating="NORMAL" rank="VETERAN" srange="5" tribe="GUARD")");
};

} // namespace aion::gameserver::controllers::testing
