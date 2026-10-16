// P5-10a: the correction of PlayerTeamDistributionService.doReward's quest kills (the owner's decision of 2026-10-05, both branches;
// docs/deviations/P5-10a.md). Java calls QuestEngine.onKill for every in-range member inside team.forEach, under the team lock
// (PlayerTeamDistributionService.java:118-123); the port collects the members there and calls onKill after the forEach released the lock.
//
// The proof is a lock-order one: a quest probe takes a lock of its own class in onKillEvent, and the test first takes that lock outside the team
// lock (probe -> team). Java's order (team -> probe inside onKill) would close a CYCLE; the corrected order records no edge from the team lock.
// Both members are mentors, so doReward returns right after the quest kills (no reward arithmetic, no damage list read).

#include "../../cm_ak/EconomyPacketTestSupport.h"
#include "../P5-10b/TeamTestSupport.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/controllers/attack/DamageList.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/controllers/attack/TeamDamageList.h"
#include "aion/gameserver/model/team/common/service/PlayerTeamDistributionService.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

constexpr int32_t KILLED_NPC = 798007; // a row of the economy npc templates
constexpr int32_t PROBE_QUEST = 99001;

/** The probe lock: its own lock class, taken by the quest handler and, first, by the test outside the team lock */
runtime::Monitor& probeLock() {
	static runtime::Monitor monitor{AION_LOCK_CLASS(TeamQuestKillOrderTest::probeLock)};
	return monitor;
}

class KillProbe final : public questEngine::handlers::AbstractQuestHandler {
public:
	explicit KillProbe(std::vector<int32_t>& seen) : AbstractQuestHandler(PROBE_QUEST), seen(seen) {}

	void register_() override { qe.registerQuestNpc(KILLED_NPC)->addOnKillEvent(questId); }

	bool onKillEvent(questEngine::model::QuestEnv& env) override {
		probeLock().lock();
		seen.push_back(env.getPlayer()->getObjectId());
		probeLock().unlock();
		return true;
	}

private:
	std::vector<int32_t>& seen;
};

class TeamQuestKillOrderTest : public TeamTest {
protected:
	void SetUp() override {
		TeamTest::SetUp();
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn"); // the npc row names ai="general", whose handler is not linked here
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, items::ECONOMY_NPC_TEMPLATES_XML));
		// the probe's quest needs no template; the engine asks the holder (an empty quest list)
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context, R"xml(<quests>
	<quest id="99001" name="Kill order probe" nameId="1" quest_zone="Poeta" category="QUEST"/>
</quests>)xml"));
	}

	void TearDown() override {
		npc = nullptr;
		spawnGroup = nullptr;
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		TeamTest::TearDown();
		dataholders::DataManager::QUEST_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
	}

	/** the killed npc at (x, 100, 50) of the fixture's map instance (EconomyPacketTestSupport.h's npcAt, without a known list) */
	model::gameobjects::Npc& npcAt(float x) {
		spawnGroup = model::templates::spawns::SpawnGroup::create(210010000, KILLED_NPC, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn =
			spawnGroup->addSpawnTemplate(std::make_unique<items::EconomySpawnTemplate>(*spawnGroup, x, 100.0f, 50.0f));
		npc = model::gameobjects::VisibleObject::create<model::gameobjects::Npc>(std::make_unique<controllers::NpcController>(), spawn,
			dataholders::DataManager::NPC_DATA->getNpcTemplate(KILLED_NPC));
		npc->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(x, 100.0f, 50.0f)));
		return *npc;
	}

	runtime::Ref<model::templates::spawns::SpawnGroup> spawnGroup;
	runtime::Ref<model::gameobjects::Npc> npc;
	std::shared_ptr<const std::string> savedMissingAiHandlers;
	xml::LoadContext context;
};

/** doReward calls QuestEngine.onKill for every in-range member, and no longer under the team lock */
TEST_F(TeamQuestKillOrderTest, QuestKillsRunOutsideTheTeamLock) {
	runtime::LockOrderValidator& validator = runtime::LockOrderValidator::getInstance();
	if (!validator.isEnabled())
		GTEST_SKIP() << "the lock-order validator runs in checked builds only";
	ConfigScope<int32_t> distance(configs::main::GroupConfig::GROUP_MAX_DISTANCE, 100);
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo", 102.0f);
	PlayerGroup& group = form({&a, &b});
	a.player().setMentor(true); // mentors: doReward returns after the quest kills
	b.player().setMentor(true);
	model::gameobjects::Npc& owner = npcAt(101.0f);
	std::vector<int32_t> seen;
	questEngine::QuestEngine::getInstance().addQuestHandler(std::make_unique<KillProbe>(seen));
	validator.clearReports(true);

	// the order every other path uses: the probe lock, then the team lock
	probeLock().lock();
	group.forEach([](model::gameobjects::AionObject&) {});
	probeLock().unlock();

	controllers::attack::DamageList damageList({}, owner);
	controllers::attack::TeamDamageList teamDamageList(damageList);
	model::team::common::service::PlayerTeamDistributionService::doReward(group, 1.0f, owner, a.player(), teamDamageList);

	EXPECT_EQ(seen.size(), 2u) << "onKill for both in-range members";
	EXPECT_EQ(validator.reportCount(runtime::LockOrderValidator::ReportKind::CYCLE), 0u)
		<< "QuestEngine.onKill ran under the team lock (Java's order, PlayerTeamDistributionService.java:122)";
	for (const runtime::LockOrderValidator::Report& report : validator.getReports())
		ADD_FAILURE() << report.text;
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
