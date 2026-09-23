// P5-02 interaction tasks (wave 5a stage 2, item X-01a; header request world-engines-1 / hr-2 of the world-visibility lane): the craft-bar
// branching of AbstractCraftTask.onInteraction, the scheduling of AbstractInteractionTask.start/stop/abort and the GatheringTask that
// GatherableController needs. Expectations follow AbstractInteractionTask.java:60-100, AbstractCraftTask.java:46-60 and GatheringTask.java.
//
// Stage 3: AGatheringTaskRunsToItsFailureFinish drives a real GatheringTask through the scheduler until it completes. The scheduled body is
// `!requester.isOnline() || onInteraction()` (AbstractInteractionTask.java:69), and Player.isOnline() is `getClientConnection() != null`
// (Player.java:348), so a test player without a connection stops the task on its first run and never reaches a single GatheringTask body. The
// test therefore gives the player one accepted, real AionConnection from a loopback listener (TestClientLink below).
//
// Stage 3, fixer run: that test proved nothing about GatheringTask::onInteractionFinish. Its two statements - removing the gatherer observer and
// GatherableController::completeInteraction - were invisible to it, because a task built by hand is not the task the controller holds
// (getGatheringPlayerId() was 0 before and after) and because notifyMoveObservers() on an already stopped task changes nothing either way.
// TheControllerStartsAGatheringThatRunsToItsFailureFinish therefore takes the real entry point, GatherableController::startGathering, and both
// failure tests now assert the observer of the player directly (ObserveController::hasObservers).

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>

#include "aion/commons/network/NioServer.h"
#include "aion/commons/network/ServerCfg.h"
#include "aion/gameserver/configs/main/CraftConfig.h"
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
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/model/templates/gather/GatherableTemplate.h"
#include "aion/gameserver/model/templates/gather/Material.h"
#include "aion/gameserver/model/templates/gather/Materials.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
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

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

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
	/** onInteractionStart then throws, as a real one can (it sends packets and attaches an observer) */
	bool throwOnStart{};

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
	void onInteractionStart() override {
		++starts;
		if (throwOnStart)
			throw runtime::NullPointerException("onInteractionStart of the test task");
	}
	void onInteractionFinish() override { ++finishes; }
	void onInteractionAbort() override { ++aborts; }
};

inline std::vector<Ref<gameserver::model::gameobjects::player::PetCommonData>> noPets(Player&) {
	return {};
}

/** Sets an atomic configuration field for the scope and restores the previous value */
template <class T>
class AtomicConfigScope {
public:
	AtomicConfigScope(std::atomic<T>& configValue, T value) : config(configValue), previous(configValue.load()) { config.store(value); }
	~AtomicConfigScope() { config.store(previous); }
	AtomicConfigScope(const AtomicConfigScope&) = delete;
	AtomicConfigScope& operator=(const AtomicConfigScope&) = delete;

private:
	std::atomic<T>& config;
	const T previous;
};

/**
 * An AionConnection that skips the key exchange. The completion test only needs Player.isOnline() to be true; everything else stays the real
 * code, because AionConnection declares writeData, processData and onDisconnect `override final` (AionConnection.h:178-190). onDisconnect is
 * safe here: the connection has no account and no active player, so LoginServer.onDisconnect only scans the empty login requests
 * (LoginServer.cpp:187-193).
 */
class SilentTestConnection final : public network::aion::AionConnection {
public:
	SilentTestConnection(asio::ip::tcp::socket socket, commons::network::NioServer& server) : AionConnection(std::move(socket), server) {}

protected:
	/**
	 * Java/C++ AionConnection::initialized() sends SM_KEY and starts the ConnectionAliveChecker; neither is wanted here. The crypt key is
	 * enabled anyway (SM_KEY's writeImpl does it), because writeData encrypts every packet after the first one and would otherwise log an
	 * error for the SM_GATHER_* packets this test really sends.
	 */
	void initialized() override { enableCryptKey(); }
};

/**
 * A loopback listener on 127.0.0.1 that hands the test one accepted, real AionConnection (written after
 * tests/network/support/GameServerTestServer.h, which tests/skills may not include: it is the test support of P4-15).
 */
class TestClientLink {
public:
	TestClientLink() {
		commons::network::ServerCfg cfg{{"127.0.0.1", 0}, "gathering test client",
			[this](asio::ip::tcp::socket socket,
				commons::network::NioServer& nioServer) -> std::shared_ptr<commons::network::AConnectionBase> {
				auto accepted = std::make_shared<SilentTestConnection>(std::move(socket), nioServer);
				std::lock_guard lock(mutex);
				serverSide = accepted;
				return accepted;
			}};
		server = std::make_unique<commons::network::NioServer>(1, std::vector{cfg});
		server->connect();
		clientSocket.emplace(clientContext);
		clientSocket->connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"),
			static_cast<uint16_t>(server->getBoundAddresses().at(0).port)));
		for (int32_t i = 0; i < 1000 && !connection(); ++i)
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	~TestClientLink() {
		asio::error_code ignored;
		clientSocket->close(ignored);
		server->shutdown(std::chrono::seconds(5));
	}

	TestClientLink(const TestClientLink&) = delete;
	TestClientLink& operator=(const TestClientLink&) = delete;

	/** @return the server side of the accepted connection, empty while the accept has not run */
	std::shared_ptr<SilentTestConnection> connection() {
		std::lock_guard lock(mutex);
		return serverSide;
	}

private:
	std::mutex mutex;
	std::shared_ptr<SilentTestConnection> serverSide;
	asio::io_context clientContext;
	std::optional<asio::ip::tcp::socket> clientSocket;
	std::unique_ptr<commons::network::NioServer> server;
};

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
		// the harvest skill of template 400001 at exactly its skillLevel, so GatherableController::checkPlayerSkill passes and the skill
		// difference of startGathering is 0 (a negative difference cannot come through the controller: checkPlayerSkill rejects it)
		f.player->setSkillList(m::skill::PlayerSkillList::create(std::vector<Ptr<m::skill::PlayerSkillEntry>>{
			Ptr<m::skill::PlayerSkillEntry>(m::skill::PlayerSkillEntry::create(30002, 20, 0,
				m::gameobjects::Persistable_PersistentState::NOACTION))}));
		return f;
	}

	/** A gatherable of template 400001 at the player's spot, with its own GatherableController (the class GatheringTask.h unblocks) */
	Ref<gameserver::model::gameobjects::Gatherable> makeGatherable() {
		using gameserver::model::templates::spawns::SpawnGroup;
		using gameserver::model::templates::spawns::SpawnTemplate;
		spawnGroup = SpawnGroup::create(210010000, 400001, 0, nullptr);
		spawnTemplate = SpawnTemplate::create(*spawnGroup, 1212.94f, 1044.85f, 140.76f, int8_t{0}, 0, std::nullopt, 0);
		Ref<gameserver::model::gameobjects::Gatherable> gatherable =
			gameserver::model::gameobjects::VisibleObject::create<gameserver::model::gameobjects::Gatherable>(*spawnTemplate,
				std::make_unique<controllers::GatherableController>());
		// the Gatherable constructor takes only the map id from the spawn (Gatherable.cpp:21), and startGathering checks the distance
		gatherable->setPosition(world::WorldPosition::create(210010000, 1212.94f, 1044.85f, 140.76f, int8_t{0}));
		return gatherable;
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

TEST_F(InteractionTaskTest, AGatheringTaskRunsToItsFailureFinish) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(5107);
	TestClientLink link;
	ASSERT_TRUE(link.connection()) << "the loopback listener accepted the test client";
	f.player->setClientConnection(link.connection());
	ASSERT_TRUE(f.player->isOnline()) << "Player.isOnline(): getClientConnection() != null";

	Ref<gameserver::model::gameobjects::Gatherable> gatherable = makeGatherable();
	const gameserver::model::templates::gather::Material& material =
		gatherable->getObjectTemplate()->getMaterials()->getMaterial().front();

	// skillLvlDiff < 0 (GatheringTask.java:94-96): analyzeInteraction fills the failure bar on its first run, so the second run takes
	// AbstractCraftTask.onInteraction's failure branch and ends the interaction. Only a hand-built task reaches that branch:
	// GatherableController::checkPlayerSkill refuses a player whose skill level is below the template's, so its difference is never negative.
	Ref<GatheringTask> task = GatheringTask::create(*f.player, *gatherable, &material, -1);
	task->start();
	ASSERT_TRUE(task->isInProgress());
	EXPECT_EQ(f.player->getInteractionTask().rawPointer(), static_cast<AbstractInteractionTask*>(task.get()));
	EXPECT_TRUE(f.player->getObserveController()->hasObservers()) << "onInteractionStart attached the gatherer observer";

	// delay = Rnd.get(200, 600) and interval = 2500 - (-1 * 60) = 2560 (GatheringTask.java:36-38), so the first run is due within 600 ms
	executor->advance(std::chrono::milliseconds(600));
	EXPECT_TRUE(task->isInProgress()) << "run 1 analyzed and sent an update; onInteraction returned false, so nothing stopped the task. An "
										 "offline requester would have stopped it here without reaching a single GatheringTask body "
										 "(AnOfflineRequesterStopsTheTaskOnItsFirstRun)";
	EXPECT_EQ(f.player->getInteractionTask().rawPointer(), static_cast<AbstractInteractionTask*>(task.get()));

	executor->advance(std::chrono::milliseconds(2600));
	EXPECT_FALSE(task->isInProgress()) << "run 2 found the failure bar full, ran onFailureFinish and stopped the task";
	EXPECT_FALSE(f.player->getInteractionTask()) << "stop(): requester.setInteractionTask(null)";

	// onInteractionFinish removed the gatherer observer (GatheringTask.java:49, GatheringTask.cpp:95-98)
	EXPECT_FALSE(f.player->getObserveController()->hasObservers());
	f.player->getObserveController()->notifyMoveObservers(); // a detached observer aborts nothing
	EXPECT_FALSE(task->isInProgress());

	// a cancelled periodic task never runs again
	executor->advance(std::chrono::milliseconds(10000));
	EXPECT_FALSE(task->isInProgress());

	f.player->setClientConnection(nullptr);
}

TEST_F(InteractionTaskTest, TheControllerStartsAGatheringThatRunsToItsFailureFinish) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(5108);
	TestClientLink link;
	ASSERT_TRUE(link.connection()) << "the loopback listener accepted the test client";
	f.player->setClientConnection(link.connection());
	ASSERT_TRUE(f.player->isOnline()) << "Player.isOnline(): getClientConnection() != null";

	// Rnd.chance() is [0, 100), so `chance() >= MAX_GATHER_FAILURE_CHANCE * failReduction` (GatheringTask.java:102) is never true for a skill
	// difference of 0 (failReduction 1.0) and every analyzeInteraction fills the failure bar. The field is 33 on a server (CraftConfig.cpp:13)
	// and 0 in a test process, where every run would instead succeed and take onSuccessFinish - which needs ItemService and a WorldMapInstance.
	AtomicConfigScope<int32_t> alwaysFail(configs::main::CraftConfig::MAX_GATHER_FAILURE_CHANCE, 100);

	Ref<gameserver::model::gameobjects::Gatherable> gatherable = makeGatherable();
	ASSERT_EQ(gatherable->getController().getGatheringPlayerId(), 0) << "nobody is gathering yet";

	// the real entry point (CM_GATHER -> GatherableController.startGathering): it picks the material, computes the skill difference, creates
	// the GatheringTask, stores it in the controller and starts it (GatherableController.java:34-108, the synchronized block at :98-107)
	gatherable->getController().startGathering(*f.player);
	Ptr<AbstractInteractionTask> task = f.player->getInteractionTask();
	ASSERT_TRUE(task) << "start(): requester.setInteractionTask(this)";
	EXPECT_TRUE(task->isInProgress());
	EXPECT_EQ(gatherable->getController().getGatheringPlayerId(), f.player->getObjectId())
		<< "the controller holds this gathering: synchronized { gatheringTask = task; }";
	EXPECT_TRUE(f.player->getObserveController()->hasObservers()) << "onInteractionStart attached the gatherer observer";

	// delay = Rnd.get(200, 600) and interval = 2500 for a skill difference of 0; each run adds round(120 + 5 * Rnd(1..2)) to the failure bar,
	// so the bar (1000) is full after eight runs and the ninth takes AbstractCraftTask.onInteraction's failure branch
	for (int32_t i = 0; i < 30 && task->isInProgress(); i++)
		executor->advance(std::chrono::milliseconds(2500));
	ASSERT_FALSE(task->isInProgress()) << "onFailureFinish ran and stop() cancelled the periodic task";
	EXPECT_FALSE(f.player->getInteractionTask()) << "stop(): requester.setInteractionTask(null)";

	// the two statements of GatheringTask::onInteractionFinish (GatheringTask.java:47-51), neither of which the hand-built task above can show
	EXPECT_FALSE(f.player->getObserveController()->hasObservers()) << "requester.getObserveController().removeObserver(gathererObserver)";
	EXPECT_EQ(gatherable->getController().getGatheringPlayerId(), 0)
		<< "((Gatherable) responder).getController().completeInteraction(), which clears the controller's gatheringTask";

	// completeInteraction also counts the harvest: harvestCount is 3, so this one interaction neither deletes the gatherable nor respawns it,
	// and a second gathering of the same node starts again from a clean controller
	ASSERT_EQ(gatherable->getObjectTemplate()->getHarvestCount(), 3);
	gatherable->getController().startGathering(*f.player);
	Ptr<AbstractInteractionTask> second = f.player->getInteractionTask();
	ASSERT_TRUE(second);
	EXPECT_NE(second.rawPointer(), task.rawPointer()) << "startGathering creates a fresh task per gather";
	EXPECT_EQ(gatherable->getController().getGatheringPlayerId(), f.player->getObjectId());
	second->abort();
	EXPECT_FALSE(f.player->getObserveController()->hasObservers());

	f.player->setClientConnection(nullptr);
}

TEST_F(InteractionTaskTest, AThrowingOnInteractionStartLeavesTheTaskInTheField) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(5109);
	Ref<TestCraftTask> task = TestCraftTask::create(*f.player);
	task->throwOnStart = true;

	// start() sets Player.interactionTask before onInteractionStart() and schedules the periodic task after it
	// (AbstractInteractionTask.java:64-77), so a throwing start leaves the field pointing at a task that will never run: the
	// Player -> interactionTask -> requester cycle then has no cut left but an explicit stop() (docs/deviations/P5-02.md, m5a-plan.md §10.5).
	EXPECT_THROW(task->start(), runtime::NullPointerException);
	EXPECT_EQ(task->starts, 1);
	EXPECT_FALSE(task->isInProgress()) << "nothing was scheduled";
	EXPECT_EQ(f.player->getInteractionTask().rawPointer(), static_cast<AbstractInteractionTask*>(task.get()))
		<< "and the field still names the task";
	executor->advance(std::chrono::milliseconds(10000));
	EXPECT_EQ(task->finishes, 0) << "no run can stop it, online or not";

	task->stop(); // the cut
	EXPECT_FALSE(f.player->getInteractionTask());
	EXPECT_EQ(task->finishes, 1);
}

} // namespace
} // namespace aion::gameserver::skillengine::task::test
