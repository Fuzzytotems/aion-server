#pragma once

// Test support of the AI framework chunk (P5-05): real Npcs created through VisibleObject::create<T> (Creature::postConstruct creates the AI
// through AIEngine::newAI), with the test doubles of the controller tests: the npc template is bound from XML text, NPC_SKILL_DATA is an empty
// holder and the life stats are a CreatureLifeStats with fixed HP (NpcLifeStats reads the stat calculation of P5-01). The AI registry of this
// test executable is the empty table (aion_gs_registry_empty), so every AI name is a missing handler.

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
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
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::ai::testing {

#define AI_TEST_SCOPE runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST))

/** Life stats with fixed HP (NpcLifeStats reads the stat calculation of P5-01) */
class FixedLifeStats final : public model::stats::container::CreatureLifeStats {
public:
	explicit FixedLifeStats(model::gameobjects::Creature& owner) : CreatureLifeStats(owner, 1000, 100) {}
};

/** An Npc with the real constructor and postConstruct chain (the AI included); only the stat containers are doubles. */
class AiTestNpc final : public model::gameobjects::Npc {
	AION_MAKE_REF_FRIEND
public:
	AiTestNpc(CreateKey key, std::unique_ptr<controllers::NpcController> controller, model::templates::spawns::SpawnTemplate& spawnTemplate,
		const model::templates::npc::NpcTemplate* objectTemplate)
		: Npc(key, std::move(controller), spawnTemplate, objectTemplate) {}

protected:
	~AiTestNpc() override = default;

	void setupStatContainers() override {
		setGameStats(std::make_unique<model::stats::container::NpcGameStats>(*this));
		setLifeStats(std::make_unique<FixedLifeStats>(*this));
	}
};

class AiTestSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	explicit AiTestSpawnTemplate(model::templates::spawns::SpawnGroup& group)
		: SpawnTemplate(group, 10.0f, 20.0f, 30.0f, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** Npc templates are immortal static data: kept for the process like the DataManager holder keeps them. */
inline const model::templates::npc::NpcTemplate* npcTemplate(std::string_view attributes) {
	xml::LoadContext context;
	return xml::bindString<model::templates::npc::NpcTemplate>(context, "<npc_template name_id=\"1\" " + std::string(attributes) + "/>").release();
}

/** Captures the messages of one logger ("level|message" per line) while it exists */
class LogCapture {
public:
	explicit LogCapture(std::string loggerName) : name(std::move(loggerName)) {
		auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
		sink->set_pattern("%l|%v");
		commons::logging::LoggerFactory::configure(name, {.sinks = {sink}, .additive = false});
	}
	~LogCapture() { commons::logging::LoggerFactory::removeConfig(name); }
	LogCapture(const LogCapture&) = delete;
	LogCapture& operator=(const LogCapture&) = delete;

	std::string text() const { return stream.str(); }

private:
	std::string name;
	std::ostringstream stream;
};

/** DeterministicExecutor, IDFactory, NPC_SKILL_DATA and a spawn template; restores the AI config keys the tests change. */
class AiTest : public ::testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		auto backend = std::make_unique<runtime::DeterministicExecutor>(clock, 5);
		utils::ThreadPoolManager::installBackend(std::move(backend));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("fail");
		AI_TEST_SCOPE;
		group = model::templates::spawns::SpawnGroup::create(210010000, 700000, 0, nullptr);
		spawnTemplate = runtime::Ref<AiTestSpawnTemplate>(
			static_cast<AiTestSpawnTemplate&>(group->addSpawnTemplate(std::make_unique<AiTestSpawnTemplate>(*group))));
	}

	void TearDown() override {
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("fail");
		configs::main::AIConfig::EVENT_DEBUG.store(false);
		configs::main::AIConfig::ONCREATE_DEBUG.store(false);
		spawnTemplate.reset();
		group.reset();
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
	}

	runtime::Ref<AiTestNpc> createNpc(const model::templates::npc::NpcTemplate* objectTemplate) {
		return model::gameobjects::VisibleObject::create<AiTestNpc>(std::make_unique<controllers::NpcController>(), *spawnTemplate, objectTemplate);
	}

	runtime::ManualClock clock{0};
	runtime::Ref<model::templates::spawns::SpawnGroup> group;
	runtime::Ref<AiTestSpawnTemplate> spawnTemplate;
	static inline const model::templates::npc::NpcTemplate* plainTemplate =
		npcTemplate(R"(npc_id="210001" level="12" name="plain" rating="NORMAL" rank="VETERAN" tribe="GUARD")");
	static inline const model::templates::npc::NpcTemplate* generalTemplate =
		npcTemplate(R"(npc_id="210002" level="12" name="general" rating="NORMAL" rank="VETERAN" tribe="GUARD" ai="general")");
};

} // namespace aion::gameserver::ai::testing
