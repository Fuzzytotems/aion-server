// P5-02 interaction tasks (wave 5a stage 2, item X-01a; header request world-engines-1 / hr-2 of the world-visibility lane): the craft-bar
// branching of AbstractCraftTask.onInteraction, the scheduling of AbstractInteractionTask.start/stop/abort and the GatheringTask that
// GatherableController needs. Expectations follow AbstractInteractionTask.java:60-100, AbstractCraftTask.java:46-60 and GatheringTask.java.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/GatherableController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GatherableData.bind.h"
#include "aion/gameserver/dataholders/GatherableData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/Gatherable.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/templates/gather/GatherableTemplate.h"
#include "aion/gameserver/model/templates/gather/Material.h"
#include "aion/gameserver/model/templates/gather/Materials.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/skillengine/task/AbstractCraftTask.h"
#include "aion/gameserver/skillengine/task/AbstractInteractionTask.h"
#include "aion/gameserver/skillengine/task/GatheringTask.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::skillengine::task::test {
namespace {

using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;
using runtime::Ref;

/**
 * A minimal AbstractCraftTask that counts the template-method calls and exposes the protected bar values, so the Java branching of
 * onInteraction and the scheduling of the base class can be driven without a Gatherable.
 */
class TestCraftTask final : public AbstractCraftTask {
	AION_MAKE_REF_FRIEND
public:
	static Ref<TestCraftTask> create(Player& requester) { return runtime::makeRef<TestCraftTask>(requester); }

	int32_t analyzed{};
	int32_t updates{};
	int32_t successes{};
	int32_t failures{};
	int32_t starts{};
	int32_t finishes{};
	int32_t aborts{};

	bool interact() { return onInteraction(); }
	void setSuccessValue(int32_t value) { currentSuccessValue.set(value); }
	void setFailureValue(int32_t value) { currentFailureValue.set(value); }
	static constexpr int32_t fullBar() { return fullBarValue; }

protected:
	explicit TestCraftTask(Player& requester) : AbstractCraftTask(requester, nullptr, 0) {}
	~TestCraftTask() override = default;

	void analyzeInteraction() override { ++analyzed; }
	void sendInteractionUpdate() override { ++updates; }
	bool onSuccessFinish() override {
		++successes;
		return true;
	}
	void onFailureFinish() override { ++failures; }
	void onInteractionStart() override { ++starts; }
	void onInteractionFinish() override { ++finishes; }
	void onInteractionAbort() override { ++aborts; }
};

inline std::vector<Ref<gameserver::model::gameobjects::player::PetCommonData>> noPets(Player&) {
	return {};
}

class InteractionTaskTest : public testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		executor = new runtime::DeterministicExecutor(clock, 23);
		utils::ThreadPoolManager::installBackend(std::unique_ptr<runtime::DeterministicExecutor>(executor));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(&noPets);
		// one gatherable template: 3 harvests, so a single completed interaction does not delete the object
		dataholders::DataManager::GATHERABLE_DATA.publish(xml::bindString<dataholders::GatherableData>(gatherContext,
			R"(<gatherable_templates>)"
			R"(<gatherable_template id="400001" name="Kukuru" nameId="701957" sourceType="VEGETABLE" harvestCount="3" skillLevel="20" harvestSkill="30002" successAdj="100" failureAdj="100" aerialAdj="100">)"
			R"(<materials><material rate="10000000" nameid="702021" itemid="152000001" name="Kukuru"/></materials>)"
			R"(</gatherable_template>)"
			R"(</gatherable_templates>)"));
	}

	void TearDown() override {
		dataholders::DataManager::GATHERABLE_DATA.resetForTests();
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		spawnTemplate = nullptr;
		spawnGroup = nullptr;
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		executor = nullptr;
		runtime::Reclaimer::getInstance().drain();
	}

	struct PlayerFixture {
		Ref<gameserver::model::account::Account> account;
		Ref<gameserver::model::gameobjects::player::PlayerCommonData> commonData;
		Ref<Player> player;
	};

	static PlayerFixture makePlayer(int32_t objectId) {
		namespace m = gameserver::model;
		PlayerFixture f;
		f.account = m::account::Account::create(7000 + objectId);
		f.commonData = m::gameobjects::player::PlayerCommonData::create(objectId);
		f.commonData->setName("Gather" + std::to_string(objectId));
		f.commonData->setRace(m::Race::ELYOS);
		f.commonData->setPlayerClass(m::PlayerClass::WARRIOR);
		Ref<m::gameobjects::player::PlayerAppearance> appearance = m::gameobjects::player::PlayerAppearance::create();
		f.account->addPlayerAccountData(std::make_unique<m::account::PlayerAccountData>(*f.account, *f.commonData, *appearance));
		f.account->setAccountWarehouse(
			std::make_unique<m::items::storage::PlayerStorage>(*f.account, m::items::storage::StorageType::ACCOUNT_WAREHOUSE));
		f.player = m::gameobjects::VisibleObject::create<m::gameobjects::player::Player>(*f.account->getPlayerAccountData(objectId), *f.account);
		f.player->setPosition(world::WorldPosition::create(210010000, 1212.94f, 1044.85f, 140.76f, int8_t{0}));
		f.player->setKnownlist(std::make_unique<world::knownlist::KnownList>(*f.player));
		return f;
	}

	/** A gatherable of template 400001 at the player's spot, with its own GatherableController (the class GatheringTask.h unblocks) */
	Ref<gameserver::model::gameobjects::Gatherable> makeGatherable() {
		using gameserver::model::templates::spawns::SpawnGroup;
		using gameserver::model::templates::spawns::SpawnTemplate;
		spawnGroup = SpawnGroup::create(210010000, 400001, 0, nullptr);
		spawnTemplate = SpawnTemplate::create(*spawnGroup, 1212.94f, 1044.85f, 140.76f, int8_t{0}, 0, std::nullopt, 0);
		return gameserver::model::gameobjects::VisibleObject::create<gameserver::model::gameobjects::Gatherable>(*spawnTemplate,
			std::make_unique<controllers::GatherableController>());
	}

	runtime::ManualClock clock{0};
	runtime::DeterministicExecutor* executor = nullptr; // owned by ThreadPoolManager
	xml::LoadContext gatherContext;
	Ref<gameserver::model::templates::spawns::SpawnGroup> spawnGroup;
	Ref<gameserver::model::templates::spawns::SpawnTemplate> spawnTemplate;
};

TEST_F(InteractionTaskTest, CraftBarBranchesLikeJava) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(5101);
	Ref<TestCraftTask> task = TestCraftTask::create(*f.player);

	// neither bar is full: analyze, send an update and keep going
	EXPECT_FALSE(task->interact());
	EXPECT_EQ(task->analyzed, 1);
	EXPECT_EQ(task->updates, 1);
	EXPECT_EQ(task->successes, 0);
	EXPECT_EQ(task->failures, 0);

	// the success bar is full: onSuccessFinish decides, nothing is analyzed
	task->setSuccessValue(TestCraftTask::fullBar());
	EXPECT_TRUE(task->interact());
	EXPECT_EQ(task->successes, 1);
	EXPECT_EQ(task->analyzed, 1);
	EXPECT_EQ(task->updates, 1);

	// the failure bar is checked only after the success bar
	task->setSuccessValue(0);
	task->setFailureValue(TestCraftTask::fullBar());
	EXPECT_TRUE(task->interact());
	EXPECT_EQ(task->failures, 1);
	EXPECT_EQ(task->analyzed, 1);
}

TEST_F(InteractionTaskTest, StartSchedulesTheTaskAndAbortStopsIt) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(5102);
	Ref<TestCraftTask> task = TestCraftTask::create(*f.player);
	EXPECT_FALSE(task->isInProgress());

	task->start();
	EXPECT_EQ(task->starts, 1);
	EXPECT_TRUE(task->isInProgress());
	EXPECT_EQ(f.player->getInteractionTask().rawPointer(), task.get()) << "Java: requester.setInteractionTask(this)";

	task->abort();
	EXPECT_EQ(task->aborts, 1);
	EXPECT_EQ(task->finishes, 1) << "Java abort(): onInteractionAbort() then stop()";
	EXPECT_FALSE(task->isInProgress());
	EXPECT_FALSE(f.player->getInteractionTask()) << "Java stop(): setInteractionTask(null)";

	// the default delay is 1000 ms and the interval 2500 ms (AbstractInteractionTask.java:17-18); the cancelled task never runs
	executor->advance(std::chrono::milliseconds(10000));
	EXPECT_EQ(task->analyzed, 0);
	EXPECT_EQ(task->finishes, 1);
}

TEST_F(InteractionTaskTest, AnOfflineRequesterStopsTheTaskOnItsFirstRun) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(5105);
	Ref<TestCraftTask> task = TestCraftTask::create(*f.player);
	task->start();

	// Java: boolean stopTask = !requester.isOnline() || onInteraction(); the test player has no client connection
	executor->advance(std::chrono::milliseconds(1000));
	EXPECT_EQ(task->analyzed, 0) << "onInteraction is not even reached";
	EXPECT_EQ(task->finishes, 1) << "the run stops the task";
	EXPECT_EQ(task->aborts, 0) << "stop() is not abort()";
	EXPECT_FALSE(task->isInProgress());
	EXPECT_FALSE(f.player->getInteractionTask());

	executor->advance(std::chrono::milliseconds(10000));
	EXPECT_EQ(task->finishes, 1) << "a cancelled task does not run again";
}

TEST_F(InteractionTaskTest, StartingASecondTaskAbortsTheFirst) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(5103);
	Ref<TestCraftTask> first = TestCraftTask::create(*f.player);
	Ref<TestCraftTask> second = TestCraftTask::create(*f.player);
	first->start();
	second->start();

	EXPECT_EQ(first->aborts, 1) << "Java start(): the old interaction task is aborted";
	EXPECT_FALSE(first->isInProgress());
	EXPECT_TRUE(second->isInProgress());
	EXPECT_EQ(f.player->getInteractionTask().rawPointer(), second.get());
	second->abort();
}

TEST_F(InteractionTaskTest, AGatherableAndItsControllerAreConstructible) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<gameserver::model::gameobjects::Gatherable> gatherable = makeGatherable();
	ASSERT_TRUE(gatherable);
	EXPECT_EQ(gatherable->getObjectTemplate()->getHarvestCount(), 3);
	EXPECT_EQ(gatherable->getObjectTemplate()->getHarvestSkill(), 30002);
	EXPECT_EQ(gatherable->getController().getGatheringPlayerId(), 0) << "nobody is gathering";
}

TEST_F(InteractionTaskTest, TheGathererObserverAbortsTheGathering) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(5104);
	Ref<gameserver::model::gameobjects::Gatherable> gatherable = makeGatherable();
	const gameserver::model::templates::gather::Material& material =
		gatherable->getObjectTemplate()->getMaterials()->getMaterial().front();

	Ref<GatheringTask> task = GatheringTask::create(*f.player, *gatherable, &material, 5);
	EXPECT_EQ(task->getGathererId(), f.player->getObjectId());
	EXPECT_FALSE(task->isInProgress());

	task->start();
	EXPECT_TRUE(task->isInProgress());
	EXPECT_EQ(f.player->getInteractionTask().rawPointer(), static_cast<AbstractInteractionTask*>(task.get()));

	// Java GatheringTask$1: every observed event aborts the gathering
	f.player->getObserveController()->notifyMoveObservers();
	EXPECT_FALSE(task->isInProgress());
	EXPECT_FALSE(f.player->getInteractionTask());

	// the observer was detached, so a second move changes nothing
	f.player->getObserveController()->notifyMoveObservers();
	EXPECT_FALSE(task->isInProgress());
}

TEST_F(InteractionTaskTest, ARestartedGatheringTaskGetsItsObserverBack) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(5106);
	Ref<gameserver::model::gameobjects::Gatherable> gatherable = makeGatherable();
	const gameserver::model::templates::gather::Material& material =
		gatherable->getObjectTemplate()->getMaterials()->getMaterial().front();

	// Java's gathererObserver is final and survives onInteractionFinish, so starting the same task again re-attaches it. The C++ field is
	// cleared when the observer is removed (cycles.toml cpp-breaker), so onInteractionStart has to create it again instead of dereferencing
	// an empty Ref.
	Ref<GatheringTask> task = GatheringTask::create(*f.player, *gatherable, &material, 5);
	task->start();
	f.player->getObserveController()->notifyMoveObservers(); // aborts and removes the observer
	ASSERT_FALSE(task->isInProgress());

	task->start();
	EXPECT_TRUE(task->isInProgress());
	EXPECT_EQ(f.player->getInteractionTask().rawPointer(), static_cast<AbstractInteractionTask*>(task.get()));

	// the new observer is attached to the same player and still aborts the gathering
	f.player->getObserveController()->notifyMoveObservers();
	EXPECT_FALSE(task->isInProgress());
	EXPECT_FALSE(f.player->getInteractionTask());
}

} // namespace
} // namespace aion::gameserver::skillengine::task::test
