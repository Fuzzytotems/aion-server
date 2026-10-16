// The three client packets of the ascension route (P5-16, m5f-plan.md §15, P-02, P-05 and CM_PLAY_MOVIE_END), run in-process against a real
// Player in the World of tests/instance/AscensionTestSupport.h (the real map, spawn, cooltime and exit rows):
// - CM_TELEPORT_ANIMATION_DONE (CM_TELEPORT_ANIMATION_DONE.java:30-50): every beam teleport waits for it, 1006's same-map beams as much as its
//   exit from Karamatis B;
// - CM_MOVE_IN_AIR (CM_MOVE_IN_AIR.java:34-60): the scripted flight's position updates;
// - CM_PLAY_MOVIE_END (CM_PLAY_MOVIE_END.java:33-56): the only packet that clears WATCHING_CUTSCENE, which SM_PLAY_MOVIE sets and CM_MOVE obeys,
//   and the dispatch that moves 1006 and 2008 on after their movies: QuestEngine.onMovieEnd finds the handler of the packet's quest id. The
//   test executables link the empty quest registry, so the movie cases add a recording handler for 1006 to the QuestEngine singleton, as
//   tests/quest/QuestEngineTest.cpp does, and clear the engine again.
//
// Not covered here: P-05's known-list case of m5f-plan.md §15.6 (an npc 150 m from flypath 1's start seen after one packet); the flight's
// onMoveFromClient is asserted through the PlayerMoveController state it sets instead (docs/deviations/P5-16.md).

#include "../instance/AscensionTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/configs/main/InstanceConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/controllers/observer/AbstractCollisionObserver.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/scene/Node.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/animations/ArrivalAnimation.h"
#include "aion/gameserver/model/animations/ArrivalAnimationInfo.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/network/aion/clientpackets/CM_MOVE_IN_AIR.h"
#include "aion/gameserver/network/aion/clientpackets/CM_PLAY_MOVIE_END.h"
#include "aion/gameserver/network/aion/clientpackets/CM_TELEPORT_ANIMATION_DONE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHANNEL_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_SPAWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATS_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/cron/ThreadPoolManagerRunnableRunner.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

/** The fields CM_MOVE_IN_AIR::readImpl decoded (Java keeps them private without a getter) */
struct CM_MOVE_IN_AIRTestAccess {
	static int32_t worldId(const CM_MOVE_IN_AIR& p) { return p.worldId; }
	static float x(const CM_MOVE_IN_AIR& p) { return p.x; }
	static float y(const CM_MOVE_IN_AIR& p) { return p.y; }
	static float z(const CM_MOVE_IN_AIR& p) { return p.z; }
	static int8_t heading(const CM_MOVE_IN_AIR& p) { return p.heading; }
	static int32_t distance(const CM_MOVE_IN_AIR& p) { return p.distance; }
};

/** The fields CM_PLAY_MOVIE_END::readImpl decoded */
struct CM_PLAY_MOVIE_ENDTestAccess {
	static int8_t type(const CM_PLAY_MOVIE_END& p) { return p.type; }
	static int32_t targetObjectId(const CM_PLAY_MOVIE_END& p) { return p.targetObjectId; }
	static int32_t questId(const CM_PLAY_MOVIE_END& p) { return p.questId; }
	static int32_t movieId(const CM_PLAY_MOVIE_END& p) { return p.movieId; }
	static bool canSkip(const CM_PLAY_MOVIE_END& p) { return p.canSkip; }
};

} // namespace aion::gameserver::network::aion::clientpackets

namespace aion::gameserver::instance::test {
namespace {

using model::animations::TeleportAnimation;
using model::gameobjects::player::CustomPlayerState;
using model::gameobjects::state::CreatureState;
using network::aion::clientpackets::CM_MOVE_IN_AIR;
using network::aion::clientpackets::CM_MOVE_IN_AIRTestAccess;
using network::aion::clientpackets::CM_PLAY_MOVIE_END;
using network::aion::clientpackets::CM_PLAY_MOVIE_ENDTestAccess;
using network::aion::clientpackets::CM_TELEPORT_ANIMATION_DONE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using network::test::PacketWriter;
using services::instance::InstanceService;
using services::teleport::TeleportService;

// ClientPacketInfo.gen.inc:31, :60, :88
constexpr int32_t TELEPORT_ANIMATION_DONE_OPCODE = 15;
constexpr int32_t MOVE_IN_AIR_OPCODE = 49;
constexpr int32_t PLAY_MOVIE_END_OPCODE = 81;

/** An instance script that records the movie ends it is told about (GeneralInstanceHandler.onPlayMovieEnd is empty) */
class MovieInstanceHandler final : public handlers::GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
public:
	explicit MovieInstanceHandler(world::WorldMapInstance& instance) : GeneralInstanceHandler(instance) {}

	static runtime::Ref<MovieInstanceHandler> create(world::WorldMapInstance& instance) { return runtime::makeRef<MovieInstanceHandler>(instance); }

	void onPlayMovieEnd(model::gameobjects::player::Player&, int32_t movieId) override { movies.push_back(movieId); }

	std::vector<int32_t> movies;

protected:
	~MovieInstanceHandler() override = default;
};

/** Counts the MOVE notifications of the ObserveController (CreatureController.onMove -> notifyMoveObservers) */
class MoveCounter final : public controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND
public:
	MoveCounter() : ActionObserver(controllers::observer::ObserverType::MOVE) {}

	static runtime::Ref<MoveCounter> create() { return runtime::makeRef<MoveCounter>(); }

	void moved() override { ++count; }

	int count = 0;

protected:
	~MoveCounter() override = default;
};

/** What QuestEngine.onMovieEnd handed a quest handler: the QuestEnv's quest, player and target (0 for null), and the movie */
struct MovieEnd {
	int32_t questId;
	int32_t playerId;
	int32_t targetId;
	int32_t movieId;

	bool operator==(const MovieEnd&) const = default;
};

/** A quest handler that records its onMovieEndEvent calls (quest handlers are Immortal: QuestEngine keeps them, so the record is static) */
class RecordingQuestHandler final : public questEngine::handlers::AbstractQuestHandler {
public:
	explicit RecordingQuestHandler(int32_t questId) : AbstractQuestHandler(questId) {}

	static inline std::vector<MovieEnd> movieEnds;

	void register_() override {} // QuestEngine.onMovieEnd looks the handler up by the QuestEnv's quest id, no registration needed

	void onMovieEndEvent(questEngine::model::QuestEnv& env, int32_t movieId) override {
		runtime::Ptr<model::gameobjects::VisibleObject> target = env.getVisibleObject();
		movieEnds.push_back({env.getQuestId(), env.getPlayer()->getObjectId(), target ? target->getObjectId() : 0, movieId});
	}
};

constexpr int32_t ASCENSION_ELYOS = 1006;

/** quest_data/quest_data.xml:65-67, verbatim (the holder requires one quest at least) */
const char* const QUEST_1006_XML = R"x(<quests>
	<quest id="1006" name="Ascension" nameId="1102006" quest_zone="Ascension Quests" minlevel_permitted="9" max_repeat_count="1" cannot_share="true" cannot_giveup="true" race_permitted="ELYOS" category="MISSION">
		<rewards exp="73200"/>
	</quest>
</quests>)x";

class AscensionPacketsTest : public AscensionWorldTest {
protected:
	void SetUp() override {
		AscensionWorldTest::SetUp();
		if (IsSkipped())
			return;
		configs::main::InstanceConfig::SOLO_INSTANCE_DESTROY_DELAY_SECONDS.store(600);
		// somewhere on Poeta that is not 1006's beam spot
		spawnActor(POETA, 1, 600.0f, 1000.0f, 100.0f, int8_t{0});
	}

	void TearDown() override {
		configs::main::LoggingConfig::LOG_AUDIT.store(false);
		if (questHandlerAdded) {
			questEngine::QuestEngine::getInstance().clear(); // the handlers themselves stay (Immortal), unreachable
			services::cron::CronService::resetForTests();
			dataholders::DataManager::QUEST_DATA.resetForTests();
			RecordingQuestHandler::movieEnds.clear();
		}
		AscensionWorldTest::TearDown();
	}

	/**
	 * Adds the recording handler of 1006 to the QuestEngine singleton. AbstractQuestHandler's constructor asks QUEST_DATA for the quest's work
	 * and action items, and QuestEngine.clear cancels the daily message through the CronService: both are set up here as QuestEngineTest's
	 * fixture does, QUEST_DATA with 1006's row (quest_data/quest_data.xml:65-67, verbatim; onMovieEnd reads no quest template).
	 */
	void addRecordingQuestHandler() {
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context, QUEST_1006_XML));
		services::cron::CronService::resetForTests();
		services::cron::CronService::initSingleton(std::make_unique<utils::cron::ThreadPoolManagerRunnableRunner>(), std::chrono::locate_zone("UTC"),
			services::cron::CronService::Driver::EXECUTOR);
		questHandlerAdded = true;
		RecordingQuestHandler::movieEnds.clear();
		questEngine::QuestEngine::getInstance().addQuestHandler(std::make_unique<RecordingQuestHandler>(ASCENSION_ELYOS));
		ASSERT_TRUE(questEngine::QuestEngine::getInstance().isHaveHandler(ASCENSION_ELYOS));
	}

	/** A Karamatis B instance with MovieInstanceHandler as its script, the actor inside it and spawned */
	runtime::Ptr<world::WorldMapInstance> enterMovieInstance() {
		runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, 0, int8_t{0},
			[](world::WorldMapInstance& created) -> runtime::Ref<handlers::InstanceHandler> { return MovieInstanceHandler::create(created); }, 1,
			false);
		TeleportService::teleportTo(*actor.player, *instance, 52, 174, 229, int8_t{10}, TeleportAnimation::NONE);
		world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
		return instance;
	}

	bool questHandlerAdded = false;

	void animationDone() {
		cptest::Driver<CM_TELEPORT_ANIMATION_DONE> packet(TELEPORT_ANIMATION_DONE_OPCODE);
		packet.readAndRun({}, client->get());
	}

	void moveInAir(int32_t worldId, float x, float y, float z, int8_t heading, int32_t distance) {
		cptest::Driver<CM_MOVE_IN_AIR> packet(MOVE_IN_AIR_OPCODE);
		packet.readAndRun(PacketWriter().D(worldId).F(x).F(y).F(z).C(heading).D(distance).data, client->get());
	}

	void playMovieEnd(int8_t type, int32_t target, int32_t questId, int32_t movieId) {
		cptest::Driver<CM_PLAY_MOVIE_END> packet(PLAY_MOVIE_END_OPCODE);
		packet.readAndRun(PacketWriter().C(type).D(target).D(questId).D(movieId).C(0).C(0).data, client->get());
	}

	void expectAt(int32_t mapId, float x, float y, float z) {
		EXPECT_EQ(actor.player->getWorldId(), mapId);
		EXPECT_FLOAT_EQ(actor.player->getX(), x);
		EXPECT_FLOAT_EQ(actor.player->getY(), y);
		EXPECT_FLOAT_EQ(actor.player->getZ(), z);
	}
};

// ---- CM_TELEPORT_ANIMATION_DONE -------------------------------------------------------------------------------------------------------------

TEST_F(AscensionPacketsTest, WithoutATeleportTaskTheEmptyPacketDoesNothing) {
	EXPECT_NO_THROW(animationDone());
	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(actor.player->isSpawned());
}

TEST_F(AscensionPacketsTest, The1006SameMapBeamCompletesOnlyAfterTheAnimation) {
	// _1006Ascension.java:95, verbatim: TeleportService.teleportTo(player, 210010000, 657f, 1071f, 99.375f, (byte) 72, FADE_OUT_BEAM)
	TeleportService::teleportTo(*actor.player, POETA, 657.0f, 1071.0f, 99.375f, int8_t{72}, TeleportAnimation::FADE_OUT_BEAM);
	ASSERT_FALSE(actor.player->isSpawned()) << "sendLoc despawned him and waits for the client";
	ASSERT_TRUE(actor.player->getController().hasTask(model::TaskId::TELEPORT));
	(*client)->clearSent();

	animationDone();

	EXPECT_FALSE(actor.player->getController().hasTask(model::TaskId::TELEPORT)) << "getAndRemoveTask";
	EXPECT_TRUE(actor.player->isSpawned());
	expectAt(POETA, 657.0f, 1071.0f, 99.375f);
	// SpawnTask.run's same-map arm: spawnOnSameMap (TeleportService.java:208-219)
	std::vector<std::vector<uint8_t>> bytes = sent();
	ASSERT_GE(bytes.size(), 4u);
	EXPECT_EQ(headerOf(bytes[0]), headerOf(cptest::serialized(network::aion::serverpackets::SM_CHANNEL_INFO(actor.player->getPosition()), client->con())));
	EXPECT_EQ(headerOf(bytes[1]), headerOf(cptest::serialized(network::aion::serverpackets::SM_PLAYER_INFO(*actor.player), client->con())));
	EXPECT_EQ(headerOf(bytes[2]), headerOf(cptest::serialized(network::aion::serverpackets::SM_STATS_INFO(*actor.player), client->con())));
	EXPECT_EQ(headerOf(bytes[3]), headerOf(cptest::serialized(network::aion::serverpackets::SM_MOTION(actor.player->getObjectId(),
		std::unordered_map<int32_t, runtime::Ptr<model::gameobjects::player::motion::Motion>>{}), client->con())));
}

TEST_F(AscensionPacketsTest, The1006ExitBeamLeavesKaramatisBForPoeta) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	TeleportService::teleportTo(*actor.player, *instance, 52, 174, 229, int8_t{10}, TeleportAnimation::NONE);
	world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
	// _1006Ascension.java:185 beams the Daeva back to Poeta with FADE_OUT_BEAM
	TeleportService::teleportTo(*actor.player, POETA, 657.0f, 1071.0f, 99.375f, int8_t{72}, TeleportAnimation::FADE_OUT_BEAM);
	(*client)->clearSent();

	animationDone();

	// SpawnTask.run's map-reloading arm: onLeaveInstance (the solo message), SM_CHANNEL_INFO, SM_PLAYER_SPAWN; the player spawns at CM_LEVEL_READY
	expectAt(POETA, 657.0f, 1071.0f, 99.375f);
	EXPECT_TRUE(wasSent(cptest::serialized(SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE(10), client->con())));
	EXPECT_TRUE(wasSent(cptest::serialized(network::aion::serverpackets::SM_CHANNEL_INFO(actor.player->getPosition()), client->con())));
	EXPECT_TRUE(wasSentClass(cptest::serialized(network::aion::serverpackets::SM_PLAYER_SPAWN(*actor.player), client->con())));
	// TeleportService.java:522, the first setPortAnimation: on this arm nothing resets it (TeleportStatementsTest.cpp:17-20's gap)
	EXPECT_EQ(actor.player->getPortAnimationId(), model::animations::getId(model::animations::ArrivalAnimation::FADE_IN_BEAM));
}

TEST_F(AscensionPacketsTest, ACancelledTeleportTaskIsNotRun) {
	int runs = 0;
	runtime::Ref<runtime::Future> task = runtime::Future::deferred(runtime::Pin(), [&runs] { ++runs; });
	actor.player->getController().addTask(model::TaskId::TELEPORT, task);
	task->cancel(false);

	EXPECT_NO_THROW(animationDone()) << "isDone() is true for a cancelled task: no run, no get()";
	EXPECT_EQ(runs, 0);
}

TEST_F(AscensionPacketsTest, AFailedTeleportIsLoggedAndTheDespawnedPlayerSpawnedInPlace) {
	network::test::LogCapture capture({"com.aionemu.gameserver.network.aion.clientpackets.CM_TELEPORT_ANIMATION_DONE"}, spdlog::level::info);
	world::World::getInstance().despawn(*actor.player);
	actor.player->getController().addTask(model::TaskId::TELEPORT,
		runtime::Future::deferred(runtime::Pin(), [] { throw runtime::IllegalStateException("spawn task failed"); }));
	(*client)->clearSent();

	animationDone();

	EXPECT_TRUE(capture.contains("spawn task failed")) << "log.error(\"\", e.getCause())";
	EXPECT_TRUE(wasSentClass(cptest::serialized(network::aion::serverpackets::SM_PLAYER_INFO(*actor.player), client->con())));
	EXPECT_TRUE(actor.player->isSpawned()) << "World.spawn(player) where he stands";
	expectAt(POETA, 600.0f, 1000.0f, 100.0f);
}

TEST_F(AscensionPacketsTest, AFailedTeleportOfASpawnedPlayerOnlyLogs) {
	network::test::LogCapture capture({"com.aionemu.gameserver.network.aion.clientpackets.CM_TELEPORT_ANIMATION_DONE"}, spdlog::level::info);
	actor.player->getController().addTask(model::TaskId::TELEPORT,
		runtime::Future::deferred(runtime::Pin(), [] { throw runtime::IllegalStateException("spawn task failed"); }));
	(*client)->clearSent();

	EXPECT_NO_THROW(animationDone());

	EXPECT_TRUE(capture.contains("spawn task failed"));
	EXPECT_TRUE(sent().empty()) << "`if (!player.isSpawned())`: no SM_PLAYER_INFO, no second spawn";
}

// ---- CM_MOVE_IN_AIR -----------------------------------------------------------------------------------------------------------------------

TEST_F(AscensionPacketsTest, MoveInAirReadsItsTwentyOneBytes) {
	cptest::Driver<CM_MOVE_IN_AIR> packet(MOVE_IN_AIR_OPCODE);
	std::vector<uint8_t> body = PacketWriter().D(210010000).F(1.5f).F(2.5f).F(3.5f).C(-7).D(123456).data;
	ASSERT_EQ(body.size(), 21u);
	packet.readAndRun(body, client->get());

	EXPECT_EQ(CM_MOVE_IN_AIRTestAccess::worldId(packet), 210010000);
	EXPECT_FLOAT_EQ(CM_MOVE_IN_AIRTestAccess::x(packet), 1.5f);
	EXPECT_FLOAT_EQ(CM_MOVE_IN_AIRTestAccess::y(packet), 2.5f);
	EXPECT_FLOAT_EQ(CM_MOVE_IN_AIRTestAccess::z(packet), 3.5f);
	EXPECT_EQ(CM_MOVE_IN_AIRTestAccess::heading(packet), -7);
	EXPECT_EQ(CM_MOVE_IN_AIRTestAccess::distance(packet), 123456);
}

TEST_F(AscensionPacketsTest, MoveInAirIsIgnoredUnlessFlying) {
	runtime::Ref<MoveCounter> moves = MoveCounter::create();
	actor.player->getObserveController()->attach(*moves);

	moveInAir(POETA, 650.0f, 1050.0f, 150.0f, int8_t{20}, 10);
	expectAt(POETA, 600.0f, 1000.0f, 100.0f);

	world::World::getInstance().despawn(*actor.player);
	actor.player->setState(CreatureState::FLYING);
	moveInAir(POETA, 650.0f, 1050.0f, 150.0f, int8_t{20}, 10);
	expectAt(POETA, 600.0f, 1000.0f, 100.0f);
	EXPECT_EQ(moves->count, 0) << "both returns come before onMove";
	actor.player->getObserveController()->removeObserver(*moves);
}

TEST_F(AscensionPacketsTest, TheScriptedFlightFollowsTheClient) {
	// _1006Ascension.java:157-175: FLYING, flight 1001 of flypath_template.xml:3, the protection of the last spawn still blinking
	actor.player->setState(CreatureState::FLYING);
	runtime::Ref<model::templates::flypath::FlightPath> flightPath =
		model::templates::flypath::FlightPath::create(model::templates::flypath::FlightPath::Type::FLIGHT_TRANSPORTER, 1001, 0);
	actor.player->setFlightPath(flightPath);
	actor.player->getController().startProtectionActiveTask();
	ASSERT_TRUE(actor.player->isProtectionActive());
	runtime::Ref<MoveCounter> moves = MoveCounter::create();
	actor.player->getObserveController()->attach(*moves);

	moveInAir(POETA, 650.0f, 1050.0f, 150.0f, int8_t{20}, 37);

	EXPECT_EQ(flightPath->getDistance(), 37) << "FlightPath.setDistance(distance)";
	EXPECT_FALSE(actor.player->isProtectionActive()) << "stopProtectionActiveTask";
	expectAt(POETA, 650.0f, 1050.0f, 150.0f);
	EXPECT_EQ(actor.player->getHeading(), 20);
	EXPECT_EQ(moves->count, 1) << "PlayerController.onMove -> notifyMoveObservers";
	// PlayerMoveController.onMoveFromClient: the position the client reported, the check point of resetToLastPositionFromClient
	runtime::Ptr<world::WorldPosition> fromClient = actor.player->getMoveController()->getLastPositionFromClient();
	ASSERT_TRUE(fromClient);
	EXPECT_EQ(fromClient->getMapId(), POETA);
	EXPECT_FLOAT_EQ(fromClient->getX(), 650.0f);
	EXPECT_FLOAT_EQ(fromClient->getY(), 1050.0f);
	EXPECT_FLOAT_EQ(fromClient->getZ(), 150.0f);
	EXPECT_EQ(fromClient->getHeading(), 20);
	EXPECT_GT(actor.player->getMoveController()->getLastPositionFromClientMillis(), 0);
	actor.player->getObserveController()->removeObserver(*moves);
}

/**
 * m5f-plan.md §10.5 G2c: Player.setPosition resets the last position from the client (Player.java:1592-1600, "material collision handlers
 * (such as shields) affect you on teleport"), so a collision observer created after a teleport takes the NEW position as its oldPos
 * (AbstractCollisionObserver.java:33-37) - not the point the client last reported before the teleport, which would make the observer's first
 * ray run from there across the whole map. Neither gate can see this row (§10.4's last row); it lives here, beside the CM_MOVE_IN_AIR case
 * that sets the client position, and not in tests/geo as the plan placed it (no fixture there has a Player; docs/deviations/P5-SC.md).
 */
class OldPositionProbe final : public controllers::observer::AbstractCollisionObserver {
	AION_MAKE_REF_FRIEND
public:
	explicit OldPositionProbe(model::gameobjects::Creature& creature)
		: AbstractCollisionObserver(creature, geoEngine::scene::Node::create(), 0, CheckType::TOUCH) {}

	static runtime::Ref<OldPositionProbe> create(model::gameobjects::Creature& creature) { return runtime::makeRef<OldPositionProbe>(creature); }

	geoEngine::math::Vector3f oldPosition() const { return oldPos.get(); }
	void onMoved(geoEngine::collision::CollisionResults&) override {}

protected:
	~OldPositionProbe() override = default;
};

TEST_F(AscensionPacketsTest, ATeleportResetsTheClientPositionACollisionObserverStartsFrom) {
	actor.player->setState(CreatureState::FLYING);
	actor.player->setFlightPath(
		model::templates::flypath::FlightPath::create(model::templates::flypath::FlightPath::Type::FLIGHT_TRANSPORTER, 1001, 0));
	moveInAir(POETA, 650.0f, 1050.0f, 150.0f, int8_t{20}, 37);
	ASSERT_TRUE(actor.player->getMoveController()->getLastPositionFromClient()) << "CM_MOVE_IN_AIR recorded the client's position";
	{
		const geoEngine::math::Vector3f before = OldPositionProbe::create(*actor.player)->oldPosition();
		EXPECT_FLOAT_EQ(before.getX(), 650.0f) << "without a teleport the observer starts from the client's last position";
	}
	actor.player->unsetState(CreatureState::FLYING);
	actor.player->setFlightPath(nullptr);

	// a same-map teleport with no animation: sendLoc -> SpawnTask.run -> World.setPosition -> Player.setPosition
	TeleportService::teleportTo(*actor.player, POETA, 900.0f, 1300.0f, 120.0f, int8_t{0}, TeleportAnimation::NONE);
	ASSERT_TRUE(actor.player->isSpawned());
	expectAt(POETA, 900.0f, 1300.0f, 120.0f);
	EXPECT_FALSE(actor.player->getMoveController()->getLastPositionFromClient()) << "Player.setPosition resets it";
	const geoEngine::math::Vector3f after = OldPositionProbe::create(*actor.player)->oldPosition();
	EXPECT_FLOAT_EQ(after.getX(), 900.0f) << "G2c: oldPos is the new position, not the client's pre-teleport point";
	EXPECT_FLOAT_EQ(after.getY(), 1300.0f);
	EXPECT_FLOAT_EQ(after.getZ(), 120.0f);
}

// ---- CM_PLAY_MOVIE_END ---------------------------------------------------------------------------------------------------------------------

TEST_F(AscensionPacketsTest, PlayMovieEndReadsItsFifteenBytes) {
	cptest::Driver<CM_PLAY_MOVIE_END> packet(PLAY_MOVIE_END_OPCODE);
	std::vector<uint8_t> body = PacketWriter().C(1).D(4242).D(1006).D(14).C(9).C(0).data;
	ASSERT_EQ(body.size(), 15u);
	packet.readAndRun(body, client->get());

	EXPECT_EQ(CM_PLAY_MOVIE_ENDTestAccess::type(packet), 1);
	EXPECT_EQ(CM_PLAY_MOVIE_ENDTestAccess::targetObjectId(packet), 4242);
	EXPECT_EQ(CM_PLAY_MOVIE_ENDTestAccess::questId(packet), 1006);
	EXPECT_EQ(CM_PLAY_MOVIE_ENDTestAccess::movieId(packet), 14);
	EXPECT_TRUE(CM_PLAY_MOVIE_ENDTestAccess::canSkip(packet)) << "canSkip = readC() == 0";
}

TEST_F(AscensionPacketsTest, TheEndOfAWatchedCutsceneClearsTheLockAndTellsTheQuestAndTheInstance) {
	addRecordingQuestHandler();
	runtime::Ptr<world::WorldMapInstance> instance = enterMovieInstance();
	actor.player->setCustomState(CustomPlayerState::WATCHING_CUTSCENE); // what SM_PLAY_MOVIE's writeImpl sets (SM_PLAY_MOVIE.cpp:18)

	playMovieEnd(0, 0, ASCENSION_ELYOS, 14); // 1006's first movie

	EXPECT_FALSE(actor.player->isInCustomState(CustomPlayerState::WATCHING_CUTSCENE)) << "CM_MOVE is accepted again (CM_MOVE.cpp:146-148)";
	// CM_PLAY_MOVIE_END.java:53: QuestEngine.onMovieEnd(new QuestEnv(target, player, questId), movieId), no target selected
	EXPECT_EQ(RecordingQuestHandler::movieEnds, (std::vector<MovieEnd>{{ASCENSION_ELYOS, actor.player->getObjectId(), 0, 14}}));
	EXPECT_EQ(dynamic_cast<MovieInstanceHandler&>(*instance->getInstanceHandler()).movies, std::vector<int32_t>{14})
		<< "InstanceHandler.onPlayMovieEnd(player, movieId)";
}

TEST_F(AscensionPacketsTest, TheQuestIsGivenTheTargetOnlyWhenThePlayerTargetsTheObjectThePacketNames) {
	addRecordingQuestHandler();
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	TeleportService::teleportTo(*actor.player, *instance, 52, 174, 229, int8_t{10}, TeleportAnimation::NONE);
	world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
	runtime::Ptr<model::gameobjects::Npc> legionary = instance->getNpc(205009); // an Archon Legionary of the map's spawns
	ASSERT_TRUE(legionary);
	actor.player->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*legionary));
	const int32_t me = actor.player->getObjectId();

	// `player.isTargeting(targetObjectId) ? player.getTarget() : null` (CM_PLAY_MOVIE_END.java:52)
	actor.player->setCustomState(CustomPlayerState::WATCHING_CUTSCENE);
	playMovieEnd(0, legionary->getObjectId(), ASCENSION_ELYOS, 14); // the targeted npc
	actor.player->setCustomState(CustomPlayerState::WATCHING_CUTSCENE);
	playMovieEnd(0, legionary->getObjectId() + 100000, ASCENSION_ELYOS, 14); // an object the player does not target
	actor.player->setTarget(nullptr);
	actor.player->setCustomState(CustomPlayerState::WATCHING_CUTSCENE);
	playMovieEnd(0, 0, ASCENSION_ELYOS, 15); // no target at all
	actor.player->setCustomState(CustomPlayerState::WATCHING_CUTSCENE);
	playMovieEnd(0, 0, 2008, 14); // no handler for 2008 in this process: QuestEngine.onMovieEnd finds none

	EXPECT_EQ(RecordingQuestHandler::movieEnds, (std::vector<MovieEnd>{{ASCENSION_ELYOS, me, legionary->getObjectId(), 14},
													{ASCENSION_ELYOS, me, 0, 14}, {ASCENSION_ELYOS, me, 0, 15}}));
}

TEST_F(AscensionPacketsTest, AMovieEndTheServerDidNotStartIsAudited) {
	addRecordingQuestHandler();
	runtime::Ptr<world::WorldMapInstance> instance = enterMovieInstance();
	configs::main::LoggingConfig::LOG_AUDIT.store(true);
	network::test::LogCapture capture({"AUDIT_LOG"}, spdlog::level::info);
	ASSERT_FALSE(actor.player->isInCustomState(CustomPlayerState::WATCHING_CUTSCENE));

	playMovieEnd(0, 0, ASCENSION_ELYOS, 14);

	EXPECT_TRUE(capture.contains("for cutscene 14 that wasn't sent by the server")) << "CM_PLAY_MOVIE_END.java:47-48";
	// the `return` after the audit (CM_PLAY_MOVIE_END.java:49): a client cannot drive a quest or the instance script with a movie the server never
	// started, whatever quest id it writes
	EXPECT_TRUE(RecordingQuestHandler::movieEnds.empty()) << "no QuestEngine.onMovieEnd";
	EXPECT_TRUE(dynamic_cast<MovieInstanceHandler&>(*instance->getInstanceHandler()).movies.empty()) << "no InstanceHandler.onPlayMovieEnd";
}

TEST_F(AscensionPacketsTest, OnlyTheClientsOwnBookMoviesAreNotAudited) {
	configs::main::LoggingConfig::LOG_AUDIT.store(true);
	network::test::LogCapture capture({"AUDIT_LOG"}, spdlog::level::info);

	// type 1 (CutSceneMovies), no quest, movies 3, 4 and 5: the books 730079/730091, 730092 and 730085 (CM_PLAY_MOVIE_END.java:45-46)
	for (int32_t movie : {3, 4, 5})
		playMovieEnd(1, 0, 0, movie);
	EXPECT_EQ(capture.count("that wasn't sent by the server"), 0);

	playMovieEnd(0, 0, 0, 3); // a CutScene, not a CutSceneMovie
	EXPECT_EQ(capture.count("for cutscene 3 that wasn't sent by the server"), 1);
	playMovieEnd(1, 0, 0, 6); // not a book movie
	EXPECT_EQ(capture.count("for cutscene 6 that wasn't sent by the server"), 1);
	playMovieEnd(1, 0, 1006, 4); // a book movie, but with a quest id
	EXPECT_EQ(capture.count("for cutscene 4 that wasn't sent by the server"), 1);
}

} // namespace
} // namespace aion::gameserver::instance::test
