// P5-08 TeleportService: the statements of sendLoc -> abortPlayerActions -> SpawnTask::run -> spawnOnSameMap that no test of the M5b-1
// player-side lane watched. TeleportOnSameMapTest (PlayerReviveServiceTest.cpp) proves where the character lands; this file proves what else
// the same call does on the way, because every one of those statements can be deleted without moving the character one metre.
//
// Java: TeleportService.java:179-193 (sendLoc), 195-206 (abortPlayerActions), 208-219 (spawnOnSameMap), 500-536 (SpawnTask.run).
//
// The fixture differs from TeleportOnSameMapTest in one way that does all the work: it has a client connection, which spawnOnSameMap's
// SM_PLAYER_INFO would otherwise refuse to serialize (it reads the player's houses out of a database). The packet lookups seam of P4-17
// (serverpackets/detail/PacketLookups.h, the same one tests/sm_lz uses) answers that read with "no house", so the whole burst can be recorded.
//
// What this file does NOT cover, said in place so a reader does not mistake the absence for coverage:
// - abortPlayerActions' `RecallService.getInstance().cancel(player, CancelReason.CANCELLED)` (TeleportService.java:198). cancel() begins with
//   `remove(summoned)` and returns at once when the player has no pending request (RecallService.cpp:54-56), and nothing in the tree can give
//   him one: RecallService::requestSummon is AION_UNPORTED, the `requests` map is private, and there is no other writer. The statement is
//   therefore a no-op in every in-process arrangement, and deleting it changes nothing a test can see. It needs RecallService::requestSummon
//   (P5-08, the summon skill) before it can be asserted - recorded as a request in the wave report.
// - SpawnTask::run's first `setPortAnimation(getDefaultArrivalAnimation(animation))` (TeleportService.java:522). It is observable only when
//   spawnOnSameMap does NOT run - i.e. on the cross-map arm - and that arm reaches
//   `InstanceService::onLeaveInstance`, which is AION_UNPORTED (InstanceService.cpp:170-172) and throws before the assignment. The second
//   setPortAnimation, in spawnOnSameMap, is covered below.

#include "../cm_ak/InWorldPacketRunSupport.h"
#include "../world/WorldTestSupport.h"

#include <algorithm>
#include <any>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/dataholders/PlayerInitialData.bind.h"
#include "aion/gameserver/dataholders/PlayerInitialData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/animations/ArrivalAnimation.h"
#include "aion/gameserver/model/animations/ArrivalAnimationInfo.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/ride/RideInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHANNEL_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_SPAWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATS_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlotInfo.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/zone/ZoneUpdateService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::animations::ArrivalAnimation;
using model::gameobjects::state::CreatureState;
using services::teleport::TeleportService;

/**
 * The world chunk's test Ishalgen, 256 x 256, `water_level="16" death_level="0" flags="RIDE"`, and the flags are why this fixture is not on
 * the test Poeta the other playersvc tests use. Every map has a map-wide zone, and ZoneInstance::canRide falls back to the map's own flags
 * for it (ZoneInstance.cpp:130-137); the test Poeta's flags are `FLY GLIDE RECALL`, so every spawn there runs
 * PlayerController::onEnterZone -> unsetPlayerMode(RIDE) (PlayerController.cpp:288-289) and dismounts the character a second time, which
 * would hide the dismount AbortPlayerActionsDismountsTheRider is about. On a map that allows riding only abortPlayerActions can do it.
 */
constexpr int32_t ISHALGEN = 220010000;
constexpr int32_t INSTANCE = 1;
constexpr float START_X = 200.0f;
constexpr float START_Y = 200.0f;
constexpr float START_Z = 10.0f;
/** the bind point on the same map and the same instance, so SpawnTask takes the spawnOnSameMap arm */
constexpr float BIND_X = 100.0f;
constexpr float BIND_Y = 100.0f;
/** above death_level=0 (so ZoneLevelService does not kill), under water_level=16 (so it starts the drown task): see the zone-update case */
constexpr float BIND_Z = 1.0f;

inline const char* const PLAYER_INITIAL_DATA_XML = R"(<player_initial_data>
	<asmodian_spawn_location map_id="220010000" heading="17" x="71.0388" y="87.3420" z="299.8750"/>
	<elyos_spawn_location map_id="210010000" heading="32" x="812.5" y="644.25" z="140.75568"/>
	<player_data class="WARRIOR"><items/></player_data>
	<player_data class="MAGE"><items/></player_data>
</player_initial_data>)";

runtime::Ptr<model::house::House> noHouse(model::gameobjects::player::Player&) {
	return nullptr;
}

runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo> noCpInfo(model::gameobjects::player::Player&) {
	return nullptr;
}

class TeleportStatementsTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		ASSERT_TRUE(world::test::publishTestStaticData());
		xml::LoadContext context;
		dataholders::DataManager::PLAYER_INITIAL_DATA.publish(
			xml::bindString<dataholders::PlayerInitialData>(context, PLAYER_INITIAL_DATA_XML));
		// the two reads of SM_PLAYER_INFO that need services this test has not got (HousingService loads from the database)
		lookups.activeHouseOfPlayer = &noHouse;
		lookups.cpInfoForCurrentMap = &noCpInfo;
		serverpackets::detail::setPacketLookupsForTests(&lookups);

		actor = makePlayer(420001, 9411, "Porter");
		actor.player->setMotions(std::make_unique<model::gameobjects::player::motion::MotionList>(*actor.player));
		world::World& world = world::World::getInstance();
		world.storeObject(*actor.player);
		ASSERT_TRUE(world.setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player), ISHALGEN, INSTANCE, START_X, START_Y,
			START_Z, int8_t{0}));
		world.spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
		ASSERT_TRUE(actor.player->isSpawned());
		ASSERT_EQ(actor.player->getInstanceId(), INSTANCE);
		actor.player->setBindPoint(model::gameobjects::player::BindPointPosition::create(ISHALGEN, BIND_X, BIND_Y, BIND_Z, int8_t{7}));

		client = std::make_unique<TestClient>();
		client->enterWorld(actor);
		(*client)->clearSent();
	}

	void TearDown() override {
		if (actor.player) {
			if (actor.player->isSpawned())
				world::World::getInstance().despawn(*actor.player);
			world::World::getInstance().removeObject(*actor.player);
			actor.player->setTarget(nullptr);
			actor.player->setClientConnection(nullptr);
		}
		client.reset();
		actor = {};
		serverpackets::detail::setPacketLookupsForTests(nullptr);
		dataholders::DataManager::PLAYER_INITIAL_DATA.resetForTests();
		InWorldPacketTest::TearDown();
	}

	std::vector<std::vector<uint8_t>> sent() { return (*client)->sentBytes(); }

	bool wasSent(const std::vector<uint8_t>& packet) {
		std::vector<std::vector<uint8_t>> bytes = sent();
		return std::find(bytes.begin(), bytes.end(), packet) != bytes.end();
	}

	/**
	 * The 5 byte opcode header of a serialized packet (AionServerPacket::writeOP, see bodyOf in InWorldPacketRunSupport.h): it identifies the
	 * packet class and nothing else. The burst below is compared class by class rather than byte by byte, because three of its packets read
	 * state that the rest of the burst then changes - spawnOnSameMap sends SM_CHANNEL_INFO while the character is still despawned, so a
	 * SM_CHANNEL_INFO built after the call carries the spawned answer (1, 1 against 0, 0) and could never be equal.
	 */
	static std::vector<uint8_t> headerOf(const std::vector<uint8_t>& packet) {
		EXPECT_GE(packet.size(), 5u);
		return std::vector<uint8_t>(packet.begin(), packet.begin() + std::min<size_t>(5, packet.size()));
	}

	/** Java TeleportService.moveToBindLocation: the bind point this fixture set, on the same map and the same instance */
	void teleportToBindPoint() { TeleportService::moveToBindLocation(*actor.player); }

	PlayerFixture actor;
	std::unique_ptr<TestClient> client;
	serverpackets::detail::PacketLookupsForTests lookups{};
};

TEST_F(TeleportStatementsTest, TheSameMapBurstIsChannelInfoPlayerInfoStatsInfoMotion) {
	teleportToBindPoint();

	// Java spawnOnSameMap (TeleportService.java:208-219), in order. This is the burst the M5b gate's P2 sees after CM_REVIVE(BIND_REVIVE) on
	// Poeta: the bind point is on the same map, so SpawnTask takes the same-map arm and never sends SM_PLAYER_SPAWN.
	std::vector<std::vector<uint8_t>> bytes = sent();
	ASSERT_GE(bytes.size(), 4u) << "spawnOnSameMap sends at least four packets to the character";
	EXPECT_EQ(headerOf(bytes[0]), headerOf(serialized(serverpackets::SM_CHANNEL_INFO(actor.player->getPosition()), client->con())));
	EXPECT_EQ(headerOf(bytes[1]), headerOf(serialized(serverpackets::SM_PLAYER_INFO(*actor.player), client->con())));
	EXPECT_EQ(headerOf(bytes[2]), headerOf(serialized(serverpackets::SM_STATS_INFO(*actor.player), client->con())));
	EXPECT_EQ(headerOf(bytes[3]),
		headerOf(serialized(serverpackets::SM_MOTION(actor.player->getObjectId(),
						 std::unordered_map<int32_t, runtime::Ptr<model::gameobjects::player::motion::Motion>>{}),
			client->con())));
	const std::vector<uint8_t> playerSpawnHeader = headerOf(serialized(serverpackets::SM_PLAYER_SPAWN(*actor.player), client->con()));
	for (const std::vector<uint8_t>& packet : bytes)
		EXPECT_NE(headerOf(packet), playerSpawnHeader)
			<< "SM_PLAYER_SPAWN belongs to the map-reloading arm, which a same-map teleport never takes";
}

TEST_F(TeleportStatementsTest, SpawnOnSameMapSendsTheAbnormalStateIcons) {
	teleportToBindPoint();

	// Java: player.getEffectController().updatePlayerEffectIcons(null) (TeleportService.java:216), i.e.
	// SM_ABNORMAL_STATE(getAbnormalEffectsToShow(), getAbnormals(), SKILL_TARGET_SLOT_FULLSLOTS) for a null effect
	// (PlayerEffectController.cpp:84-88). The character carries no effect, so the packet is the empty one - and its absence is what a deleted
	// statement looks like.
	runtime::Ptr<controllers::effect::PlayerEffectController> effects = actor.player->getEffectController();
	EXPECT_TRUE(wasSent(serialized(serverpackets::SM_ABNORMAL_STATE(effects->getAbnormalEffectsToShow(), effects->getAbnormals(),
							   skillengine::model::SKILL_TARGET_SLOT_FULLSLOTS),
		client->con())));
}

TEST_F(TeleportStatementsTest, SpawnOnSameMapStartsTheSpawnProtection) {
	ASSERT_FALSE(actor.player->isProtectionActive());
	ASSERT_FALSE(actor.player->getController().hasTask(model::TaskId::PROTECTION_ACTIVE));

	teleportToBindPoint();

	// Java: player.getController().startProtectionActiveTask() (TeleportService.java:215) -> BLINKING + a 60 s PROTECTION_ACTIVE task
	// (PlayerController.cpp:726-736). isProtectionActive() IS the BLINKING state (Player.cpp:606-608).
	EXPECT_TRUE(actor.player->isProtectionActive());
	EXPECT_TRUE(actor.player->getController().hasTask(model::TaskId::PROTECTION_ACTIVE));
	EXPECT_TRUE(wasSent(serialized(serverpackets::SM_PLAYER_STATE(*actor.player), client->con())))
		<< "the visual state change is broadcast to the character himself (toSelf = true)";
}

TEST_F(TeleportStatementsTest, SpawnOnSameMapQueuesTheZoneUpdateAndTheCharacterDrowns) {
	teleportToBindPoint();

	// Java: player.getController().updateZone() (TeleportService.java:217) -> ZoneUpdateService.getInstance().add(creature)
	// (CreatureController.cpp:201-203). The queue is drained by the service's own 500 ms task; nothing has run it yet, so the bind point's
	// z of 1.0 (under the test map's water_level of 16) has had no effect at all.
	ASSERT_FLOAT_EQ(actor.player->getZ(), BIND_Z);
	ASSERT_FALSE(actor.player->getController().hasTask(model::TaskId::DROWN)) << "nothing may have run the queue yet";

	world::zone::ZoneUpdateService::getInstance().run();

	// callTask -> revalidateZones + ZoneLevelService::checkZoneLevels -> startDrowning, because z + (boundHeight - 0.1) < water_level
	// (ZoneLevelService.cpp:16-32). A character that was never queued is not in the batch and does not drown.
	EXPECT_TRUE(actor.player->getController().hasTask(model::TaskId::DROWN))
		<< "the character must have been in the zone-update batch";
	actor.player->getController().cancelTask(model::TaskId::DROWN); // the task holds a pin on the player
}

TEST_F(TeleportStatementsTest, SpawnOnSameMapClearsThePortAnimation) {
	// something other than the value the same-map arm writes, and other than LANDING, which SpawnTask::run writes one line earlier
	actor.player->setPortAnimation(ArrivalAnimation::LANDING_GLOW);
	ASSERT_EQ(actor.player->getPortAnimationId(), model::animations::getId(ArrivalAnimation::LANDING_GLOW));

	teleportToBindPoint();

	// Java: player.setPortAnimation(ArrivalAnimation.NONE) as the last statement of spawnOnSameMap (TeleportService.java:219). Without it the
	// character keeps the LANDING that SpawnTask::run assigned from TeleportAnimation.NONE
	// (TeleportAnimationInfo.h getDefaultArrivalAnimation: the default arm).
	EXPECT_EQ(actor.player->getPortAnimationId(), model::animations::getId(ArrivalAnimation::NONE));
	EXPECT_NE(actor.player->getPortAnimationId(), model::animations::getId(ArrivalAnimation::LANDING));
}

TEST_F(TeleportStatementsTest, AbortPlayerActionsDismountsTheRider) {
	static const model::templates::ride::RideInfo RIDE{};
	actor.player->setPlayerMode(model::actions::PlayerMode::RIDE, std::any(static_cast<const model::templates::ride::RideInfo*>(&RIDE)));
	// Java dereferences rideObservers without a null check (`synchronized (rideObservers)`, PlayerActions.java:62), and the list is null until
	// the first addRideObserver (Player.java:1578-1585). A real mount adds one; the fixture adds one for the same reason.
	actor.player->addRideObserver(*controllers::observer::ActionObserver::create(controllers::observer::ObserverType::MOVE));
	actor.player->setState(CreatureState::RESTING);
	ASSERT_TRUE(actor.player->isInPlayerMode(model::actions::PlayerMode::RIDE));
	ASSERT_EQ(actor.player->getRideObservers()->size(), 1);
	(*client)->clearSent();

	teleportToBindPoint();

	// Java: player.unsetPlayerMode(PlayerMode.RIDE) (TeleportService.java:200) -> PlayerActions.unsetPlayerMode's RIDE arm clears the ride and
	// forces the character back to ACTIVE (PlayerActions.cpp:44-60)
	EXPECT_FALSE(actor.player->isInPlayerMode(model::actions::PlayerMode::RIDE));
	EXPECT_FALSE(actor.player->isInState(CreatureState::RESTING));
	EXPECT_TRUE(actor.player->isInState(CreatureState::ACTIVE));
	EXPECT_TRUE(actor.player->getRideObservers()->isEmpty()) << "the ride observers are removed with the ride";
}

TEST_F(TeleportStatementsTest, AbortPlayerActionsCancelsTheCastingSkill) {
	dataholders::DataManager::SKILL_DATA.resetForTests(); // the fixture published an empty holder in SetUp
	xml::LoadContext context;
	dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(context, R"(<skill_data>)"
		R"(<skill_template skill_id="1" name="active" nameId="1" skilltype="PHYSICAL" skillsubtype="ATTACK" activation="ACTIVE" duration="1500" stack="A1"/>)"
		R"(</skill_data>)"));
	actor.player->setSkillList(model::skill::PlayerSkillList::create());
	runtime::Ref<skillengine::model::Skill> skill =
		skillengine::model::Skill::create(dataholders::DataManager::SKILL_DATA->getSkillTemplate(1), *actor.player, nullptr, 1);
	actor.player->setCasting(runtime::Ptr<skillengine::model::Skill>(skill));
	ASSERT_TRUE(actor.player->getCastingSkill());

	// Java: player.getController().cancelCurrentSkill(null) (TeleportService.java:199). The body's first statement on a casting character is
	// castingSkill.cancelCast() (PlayerController.cpp:621), and Skill::cancelCast is AION_UNPORTED until M5b-2 (Skill.cpp:168-170). The throw
	// is what proves the statement is there: a teleport that skips it finishes silently. Nothing on the M5b-1 gate path reaches it - a dying
	// character's cast is already cleared by CreatureController::onDie (CreatureController.cpp:208) before bindRevive teleports him.
	try {
		teleportToBindPoint();
		FAIL() << "Skill::cancelCast is unported";
	} catch (const runtime::UnportedException& unported) {
		EXPECT_NE(std::string(unported.what()).find("cancelCast"), std::string::npos) << unported.what();
	}
	actor.player->setCasting(nullptr);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
