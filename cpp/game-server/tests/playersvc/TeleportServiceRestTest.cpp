// The rest of TeleportService (P5-08, m5f-plan.md §5 T-02 and T-08, §16.4): teleportToPrison, moveToTargetWithDistance, useTeleportScroll,
// changeChannel, setEventPos, teleportToEvent and sendTeleportRequest with its anonymous RequestResponseHandler (fieldmap TeleportService$1).
//
// Java: TeleportService.java:297-302, 385-392, 405-479.
// The fixture is TravelTestSupport.h: the ascension lane's World of real map rows (Poeta, Verteron, Ishalgen, Altgard and five instance maps,
// no prison map), a connected Elyos WARRIOR beside Daines in Poeta. Data published by the cases that need it: portal_template2.xml's scroll
// LF1A_RETURN_AREA_1 and portal_loc.xml's loc 2100300 (verbatim, :2012-2014 and :41), two synthetic scroll rows (one without a portal path, one
// whose loc has no row), and player_initial_data.xml's spawn locations (tests/instance/AscensionTestData.h) for the bind fallback.
//
// Said in place:
// - the prison maps (510010000, 520010000) are not in the fixture's World, so teleportToPrison is followed to World.createPosition, which names
//   the map id it was asked for in its NullPointerException (World.cpp:338); the coordinates (275, 239, 49) are not observable there.
// - setEventPos keeps the event positions in two static fields of TeleportService that no Java method resets; ctest runs every case in a
//   process of its own, and the one case that needs them unset is the first of this file.

#include "TravelTestSupport.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/PlayerInitialData.bind.h"
#include "aion/gameserver/dataholders/PlayerInitialData.h"
#include "aion/gameserver/dataholders/Portal2Data.bind.h"
#include "aion/gameserver/dataholders/Portal2Data.h"
#include "aion/gameserver/dataholders/PortalLocData.bind.h"
#include "aion/gameserver/dataholders/PortalLocData.h"
#include "aion/gameserver/dataholders/SpawnsData.bind.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHANNEL_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_SPAWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::teleport::test {
namespace {

using model::animations::TeleportAnimation;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using network::test::PacketWriter;

/** ServerPacketsOpcodes.java:38, 15 */
constexpr int32_t SM_TELEPORT_LOC_OPCODE = 20;
constexpr int32_t SM_PLAYER_SPAWN_OPCODE = 15;
constexpr const char* TELEPORT_LOGGER = "com.aionemu.gameserver.services.teleport.TeleportService";

/** WorldMapType.java:28-29 */
constexpr int32_t LF_PRISON = 510010000;
constexpr int32_t DF_PRISON = 520010000;
/** TeleportService.java:466 */
constexpr int32_t TELEPORT_REQUEST_QUESTION = 905097;

/**
 * portal_template2.xml:2012-2014 (LF1A_RETURN_AREA_1, Verteron's return scroll) and two synthetic scrolls: one without a portal path (JAXB
 * leaves portalPath null), one whose loc id has no portal_loc row
 */
constexpr std::string_view SCROLL_PORTALS_XML = R"xml(<portal_templates2>
	<portal_scroll name="LF1A_RETURN_AREA_1">
		<portal_path loc_id="2100300" race="ELYOS" />
	</portal_scroll>
	<portal_scroll name="NO_PATH_SCROLL">
	</portal_scroll>
	<portal_scroll name="NO_LOC_SCROLL">
		<portal_path loc_id="2100399" race="ELYOS" />
	</portal_scroll>
</portal_templates2>)xml";

/** portal_loc.xml:41 (loc 2100300, the scroll's destination in Verteron, no heading) */
constexpr std::string_view SCROLL_PORTAL_LOCS_XML = R"xml(<portal_locs>
	<portal_loc world_id="210030000" loc_id="2100300" x="1690.4" y="1481.4" z="121.4"/>
</portal_locs>)xml";

class TeleportServiceRestTest : public TravelTest {
protected:
	void TearDown() override {
		if (prepared) {
			dataholders::DataManager::PORTAL2_DATA.resetForTests();
			dataholders::DataManager::PORTAL_LOC_DATA.resetForTests();
			dataholders::DataManager::PLAYER_INITIAL_DATA.resetForTests();
			dataholders::DataManager::SPAWNS_DATA.resetForTests();
		}
		TravelTest::TearDown();
	}

	void publishScrolls() {
		dataholders::DataManager::PORTAL2_DATA.publish(xml::bindString<dataholders::Portal2Data>(context, SCROLL_PORTALS_XML));
		dataholders::DataManager::PORTAL_LOC_DATA.publish(xml::bindString<dataholders::PortalLocData>(context, SCROLL_PORTAL_LOCS_XML));
	}

	/** spawns/Npcs/210010000_Poeta.xml:134-136, the spawn of Daines (teleportToNpc reads SpawnsData.getFirstSpawnByNpcId) */
	void publishDainesSpawn() {
		dataholders::DataManager::SPAWNS_DATA.publish(xml::bindString<dataholders::SpawnsData>(context,
			R"xml(<spawns><spawn_map map_id="210010000"><spawn npc_id="203194" respawn_time="295">)xml"
			R"xml(<spot x="804.924" y="1244.6" z="118.986" h="105"/></spawn></spawn_map></spawns>)xml"));
	}

	void publishInitialData() {
		dataholders::DataManager::PLAYER_INITIAL_DATA.publish(
			xml::bindString<dataholders::PlayerInitialData>(context, ::aion::gameserver::instance::test::playerInitialDataXml()));
	}

	/** Runs the TELEPORT task the way CM_TELEPORT_ANIMATION_DONE does */
	void animationDone() {
		runtime::Ptr<runtime::Future> task = player().getController().getAndRemoveTask(model::TaskId::TELEPORT);
		ASSERT_TRUE(task) << "no TELEPORT task: sendLoc did not wait for the animation";
		task->run();
		task->get();
	}

	/** A position with its region in instance 1 of the map (what a player's position is) */
	static runtime::Ref<world::WorldPosition> positionIn(int32_t mapId, float x, float y, float z, int8_t h) {
		runtime::Ptr<world::WorldMapInstance> mapInstance = world::World::getInstance().getWorldMap(mapId)->getWorldMapInstance(1);
		return world::WorldPosition::create(mapId, x, y, z, h, mapInstance->getRegion(x, y, z));
	}

	/** The NullPointerException message of `call`, empty if it did not throw one */
	template <class Call>
	static std::string nullPointerMessage(Call&& call) {
		try {
			call();
		} catch (const runtime::NullPointerException& e) {
			return e.what();
		}
		return {};
	}
};

// ---- teleportToEvent without an event position (first: the event positions are static, TeleportService.java:62-63) ----------------------------

TEST_F(TeleportServiceRestTest, TeleportToEventWithoutAnEventPositionMovesToTheBindLocation) {
	publishInitialData();
	spawnActor(0);
	namespace inst = ::aion::gameserver::instance::test;

	TeleportService::teleportToEvent(player()); // TeleportService.java:454-455: pos == null -> moveToBindLocation

	// no bind point: player_initial_data.xml's Elyos spawn location, on Poeta - the same map, so the instant arm (no SM_TELEPORT_LOC)
	EXPECT_TRUE(ofOpcode(sent(), SM_TELEPORT_LOC_OPCODE).empty());
	EXPECT_EQ(player().getWorldId(), POETA);
	EXPECT_FLOAT_EQ(player().getX(), inst::ELYOS_SPAWN_X);
	EXPECT_FLOAT_EQ(player().getY(), inst::ELYOS_SPAWN_Y);
	EXPECT_FLOAT_EQ(player().getZ(), inst::ELYOS_SPAWN_Z);
	EXPECT_EQ(player().getHeading(), inst::ELYOS_SPAWN_HEADING);
	EXPECT_TRUE(player().isSpawned());
}

// ---- teleportToPrison (TeleportService.java:297-302) ----------------------------------------------------------------------------------------

TEST_F(TeleportServiceRestTest, AnElyosGoesToTheElyosPrison) {
	spawnActor(0);

	const std::string message = nullPointerMessage([&] { TeleportService::teleportToPrison(player()); });

	EXPECT_NE(message.find("invalid mapId: " + std::to_string(LF_PRISON)), std::string::npos) << message;
}

TEST_F(TeleportServiceRestTest, AnAsmodianGoesToTheAsmodianPrison) {
	spawnActor(0, model::Race::ASMODIANS, ISHALGEN, Spot{OSMAR_SPOT.x + 1.0f, OSMAR_SPOT.y, OSMAR_SPOT.z, int8_t{0}});

	const std::string message = nullPointerMessage([&] { TeleportService::teleportToPrison(player()); });

	EXPECT_NE(message.find("invalid mapId: " + std::to_string(DF_PRISON)), std::string::npos) << message;
}

// ---- moveToTargetWithDistance (TeleportService.java:385-392) --------------------------------------------------------------------------------

/** Java's arithmetic of the expected spot: Math.toRadians(heading * 3), then cos/sin(PI * direction + radian) * distance as float */
struct Expected {
	float x, y;
};

Expected targetWithDistance(const Spot& object, int32_t direction, int32_t distance) {
	double radian = utils::PositionUtil::convertHeadingToAngle(object.h) * 0.017453292519943295;
	float x1 = static_cast<float>(std::cos(3.141592653589793 * direction + radian) * distance);
	float y1 = static_cast<float>(std::sin(3.141592653589793 * direction + radian) * distance);
	return {object.x + x1, object.y + y1};
}

TEST_F(TeleportServiceRestTest, MoveToTargetWithDistanceInFrontOfTheTargetKeepsItsHeight) {
	spawnActor(0);
	// Daines' spot raised 0.5 m: the player takes the target's z, not his own
	const Spot target{DAINES_SPOT.x, DAINES_SPOT.y, DAINES_SPOT.z + 0.5f, DAINES_SPOT.h};
	Npc& daines = npc(DAINES, POETA, target);
	const Expected expected = targetWithDistance(target, 0, 3);
	ASSERT_GT(std::abs(expected.x - target.x), 1.0f) << "heading 105 (315 degrees) moves both coordinates";

	TeleportService::moveToTargetWithDistance(daines, player(), 0, 3);

	EXPECT_EQ(player().getWorldId(), POETA);
	EXPECT_FLOAT_EQ(player().getX(), expected.x);
	EXPECT_FLOAT_EQ(player().getY(), expected.y);
	EXPECT_FLOAT_EQ(player().getZ(), target.z);
	EXPECT_TRUE(player().isSpawned()) << "the same map: spawned at once";
}

TEST_F(TeleportServiceRestTest, MoveToTargetWithDistanceInDirectionOneGoesBehindTheTarget) {
	spawnActor(0);
	Npc& daines = npc(DAINES, POETA, DAINES_SPOT);
	const Expected front = targetWithDistance(DAINES_SPOT, 0, 2);
	const Expected behind = targetWithDistance(DAINES_SPOT, 1, 2);
	// PI * 1 turns the offset around: behind = spot - (front - spot)
	ASSERT_NEAR(behind.x - DAINES_SPOT.x, DAINES_SPOT.x - front.x, 1e-3);

	TeleportService::moveToTargetWithDistance(daines, player(), 1, 2);

	EXPECT_FLOAT_EQ(player().getX(), behind.x);
	EXPECT_FLOAT_EQ(player().getY(), behind.y);
}

// ---- useTeleportScroll (TeleportService.java:405-424) ---------------------------------------------------------------------------------------

TEST_F(TeleportServiceRestTest, AScrollMovesToItsPortalLocInTheGivenWorld) {
	publishScrolls();
	spawnActor(0);

	TeleportService::useTeleportScroll(player(), "LF1A_RETURN_AREA_1", VERTERON);

	// teleportTo(player, worldId, x, y, z): NONE, another map - SpawnTask's map-reloading arm at once
	EXPECT_EQ(player().getWorldId(), VERTERON);
	EXPECT_FLOAT_EQ(player().getX(), 1690.4f);
	EXPECT_FLOAT_EQ(player().getY(), 1481.4f);
	EXPECT_FLOAT_EQ(player().getZ(), 121.4f);
	EXPECT_EQ(ofOpcode(sent(), SM_PLAYER_SPAWN_OPCODE).size(), 1u);
}

TEST_F(TeleportServiceRestTest, AScrollUsesTheWorldItIsGivenNotTheWorldOfItsLoc) {
	publishScrolls();
	spawnActor(0);

	TeleportService::useTeleportScroll(player(), "LF1A_RETURN_AREA_1", POETA); // loc 2100300 is Verteron's: Java takes the argument

	EXPECT_EQ(player().getWorldId(), POETA);
	EXPECT_FLOAT_EQ(player().getX(), 1690.4f);
	EXPECT_FLOAT_EQ(player().getY(), 1481.4f);
}

TEST_F(TeleportServiceRestTest, AnUnknownScrollOnlyWarns) {
	publishScrolls();
	spawnActor(0);
	network::test::LogCapture capture({TELEPORT_LOGGER}, spdlog::level::info);

	TeleportService::useTeleportScroll(player(), "NO_SUCH_SCROLL", VERTERON);

	EXPECT_TRUE(capture.contains("warning|" + std::string(TELEPORT_LOGGER) + "|No portal template found for: NO_SUCH_SCROLL 210030000"))
		<< capture.dump();
	EXPECT_EQ(player().getWorldId(), POETA);
	EXPECT_TRUE(player().isSpawned());
}

TEST_F(TeleportServiceRestTest, AScrollWithoutPortalPathWarnsWithTheRace) {
	publishScrolls();
	spawnActor(0);
	network::test::LogCapture capture({TELEPORT_LOGGER}, spdlog::level::info);

	TeleportService::useTeleportScroll(player(), "NO_PATH_SCROLL", VERTERON);

	EXPECT_TRUE(capture.contains("warning|" + std::string(TELEPORT_LOGGER) + "|No portal scroll for ELYOS on: NO_PATH_SCROLL 210030000"))
		<< capture.dump();
	EXPECT_EQ(player().getWorldId(), POETA);
}

TEST_F(TeleportServiceRestTest, AScrollWhoseLocHasNoRowWarns) {
	publishScrolls();
	spawnActor(0);
	network::test::LogCapture capture({TELEPORT_LOGGER}, spdlog::level::info);

	TeleportService::useTeleportScroll(player(), "NO_LOC_SCROLL", VERTERON);

	EXPECT_TRUE(capture.contains("warning|" + std::string(TELEPORT_LOGGER) + "|No portal loc for locId 2100399")) << capture.dump();
	EXPECT_EQ(player().getWorldId(), POETA);
}

// ---- changeChannel (TeleportService.java:426-432) -------------------------------------------------------------------------------------------

TEST_F(TeleportServiceRestTest, ChangeChannelMovesToInstanceChannelPlusOneInPlace) {
	// the fixture's open maps have one instance each (an open map refuses a second one), so the channels are two instances of Karamatis
	const int32_t from = newInstance(KARAMATIS_B);
	const int32_t to = newInstance(KARAMATIS_B);
	ASSERT_EQ(to, from + 1);
	spawnActor(0, model::Race::ELYOS, KARAMATIS_B, Spot{512.0f, 512.0f, 100.0f, int8_t{0}}, false, from);
	const float x = player().getX(), y = player().getY(), z = player().getZ();

	TeleportService::changeChannel(player(), to - 1);

	EXPECT_EQ(player().getWorldId(), KARAMATIS_B);
	EXPECT_EQ(player().getInstanceId(), to) << "channel + 1";
	EXPECT_FLOAT_EQ(player().getX(), x);
	EXPECT_FLOAT_EQ(player().getY(), y);
	EXPECT_FLOAT_EQ(player().getZ(), z);
	// startProtectionActiveTask's SM_PLAYER_STATE (broadcast to self, PlayerController.java) comes first; then the three of TeleportService
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_GE(packets.size(), 3u);
	EXPECT_EQ(std::vector<std::vector<uint8_t>>(packets.end() - 3, packets.end()),
		cptest::exactly({forActor(network::aion::serverpackets::SM_CHANNEL_INFO(player().getPosition())),
			forActor(network::aion::serverpackets::SM_PLAYER_SPAWN(player())), forActor(SM_SYSTEM_MESSAGE::STR_MSG_TELEPORT_ZONECHANNEL(to - 1))}));
	EXPECT_TRUE(player().getController().hasTask(model::TaskId::PROTECTION_ACTIVE)) << "startProtectionActiveTask";
}

// ---- setEventPos / teleportToEvent (TeleportService.java:434-458) ---------------------------------------------------------------------------

TEST_F(TeleportServiceRestTest, AnElyosEventPositionIsLoggedAndTakesElyosThereWithTheBeam) {
	spawnActor(0);
	network::test::LogCapture capture({TELEPORT_LOGGER}, spdlog::level::info);

	TeleportService::setEventPos(*positionIn(VERTERON, VERTERON_X, VERTERON_Y, VERTERON_Z, int8_t{12}), model::Race::ELYOS);

	EXPECT_TRUE(capture.contains("info|" + std::string(TELEPORT_LOGGER)
								 + "|elyos: mapId: 210030000, instanceId: 1, X: 1640.760009765625, Y: 1500.3199462890625, Z: 119.70999145507812, H: 12"))
		<< capture.dump();

	TeleportService::teleportToEvent(player());

	// TeleportService.java:457: FADE_OUT_BEAM (animation 1) to the stored map, instance and place
	EXPECT_EQ(ofOpcode(sent(), SM_TELEPORT_LOC_OPCODE), cptest::exactly({javaPacket(SM_TELEPORT_LOC_OPCODE,
		PacketWriter().C(1).D(VERTERON).D(VERTERON).F(VERTERON_X).F(VERTERON_Y).F(VERTERON_Z).C(12))}));
	animationDone();
	EXPECT_EQ(player().getWorldId(), VERTERON);
	EXPECT_EQ(player().getInstanceId(), 1);
	EXPECT_FLOAT_EQ(player().getX(), VERTERON_X);
	EXPECT_EQ(player().getHeading(), 12);
}

TEST_F(TeleportServiceRestTest, TheElyosEventPositionIsNotTheAsmodians) {
	spawnActor(0, model::Race::ASMODIANS, ISHALGEN, Spot{OSMAR_SPOT.x + 1.0f, OSMAR_SPOT.y, OSMAR_SPOT.z, int8_t{0}});
	TeleportService::setEventPos(*positionIn(VERTERON, VERTERON_X, VERTERON_Y, VERTERON_Z, int8_t{12}), model::Race::ELYOS);
	TeleportService::setEventPos(*positionIn(ALTGARD, 1752.5322f, 1806.6096f, 254.66133f, int8_t{60}), model::Race::ASMODIANS);

	TeleportService::teleportToEvent(player());

	EXPECT_EQ(ofOpcode(sent(), SM_TELEPORT_LOC_OPCODE), cptest::exactly({javaPacket(SM_TELEPORT_LOC_OPCODE,
		PacketWriter().C(1).D(ALTGARD).D(ALTGARD).F(1752.5322f).F(1806.6096f).F(254.66133f).C(60))}));
}

TEST_F(TeleportServiceRestTest, AnAsmodianEventPositionIsLoggedAsAsmo) {
	spawnActor(0);
	network::test::LogCapture capture({TELEPORT_LOGGER}, spdlog::level::info);

	TeleportService::setEventPos(*positionIn(ALTGARD, 1752.5322f, 1806.6096f, 254.66133f, int8_t{-60}), model::Race::ASMODIANS);

	EXPECT_TRUE(capture.contains("info|" + std::string(TELEPORT_LOGGER)
								 + "|asmo: mapId: 220030000, instanceId: 1, X: 1752.5322265625, Y: 1806.609619140625, Z: 254.6613311767578, H: -60"))
		<< capture.dump();
}

TEST_F(TeleportServiceRestTest, AnAsmodianEventPositionNeedsItsMapInstanceWhereTheElyosOneDoesNot) {
	spawnActor(0);
	// a position without a region has no map instance: the Asmodian arm reads pos.getWorldMapInstance().getMapId() (TeleportService.java:440)
	runtime::Ref<world::WorldPosition> regionless = world::WorldPosition::create(ALTGARD, 100.0f, 100.0f, 100.0f, int8_t{0});

	EXPECT_NO_THROW(TeleportService::setEventPos(*regionless, model::Race::ELYOS));
	EXPECT_THROW(TeleportService::setEventPos(*regionless, model::Race::ASMODIANS), runtime::NullPointerException);
}

// ---- sendTeleportRequest and its handler (TeleportService.java:465-479) ---------------------------------------------------------------------

TEST_F(TeleportServiceRestTest, ATeleportRequestAsksWithTheNpcsNameOnce) {
	spawnActor(0);
	const model::templates::npc::NpcTemplate* daines = dataholders::DataManager::NPC_DATA->getNpcTemplate(DAINES);
	ASSERT_NE(daines, nullptr);

	EXPECT_TRUE(TeleportService::sendTeleportRequest(player(), DAINES));
	EXPECT_EQ(sent(), cptest::exactly({forActor(network::aion::serverpackets::SM_QUESTION_WINDOW(TELEPORT_REQUEST_QUESTION, 0, 0, daines->getL10n()))}));
	clearSent();

	EXPECT_FALSE(TeleportService::sendTeleportRequest(player(), DAINES)) << "ResponseRequester.putRequest: one request per question";
	EXPECT_TRUE(sent().empty());
}

TEST_F(TeleportServiceRestTest, AcceptingTheRequestTeleportsInFrontOfTheNpc) {
	publishDainesSpawn();
	spawnActor(0);
	ASSERT_TRUE(TeleportService::sendTeleportRequest(player(), DAINES));

	// the handler's acceptRequest: teleportToNpc(responder, npcId) (TeleportService.java:304-333)
	EXPECT_TRUE(player().getResponseRequester().respond(TELEPORT_REQUEST_QUESTION, 1));

	// 1 m + the npc's bound radius (front 0.25, npc_templates.xml) in front of the spawn spot along heading 105 (315 degrees); geo data is
	// off, so GeoService.getZ answers NaN and z is the spot's + 0.5; the heading turns 60 towards the npc (105 >= 60: 45); the same map
	double radian = utils::PositionUtil::convertHeadingToAngle(DAINES_SPOT.h) * 0.017453292519943295;
	EXPECT_EQ(player().getWorldId(), POETA);
	EXPECT_FLOAT_EQ(player().getX(), DAINES_SPOT.x + static_cast<float>(std::cos(radian)) * (1.0f + 0.25f));
	EXPECT_FLOAT_EQ(player().getY(), DAINES_SPOT.y + static_cast<float>(std::sin(radian)) * (1.0f + 0.25f));
	EXPECT_FLOAT_EQ(player().getZ(), DAINES_SPOT.z + 0.5f);
	EXPECT_EQ(player().getHeading(), 45);
	EXPECT_TRUE(player().isSpawned());
}

TEST_F(TeleportServiceRestTest, AcceptingARequestForAnNpcWithoutSpawnOnlyWarns) {
	publishDainesSpawn();
	spawnActor(0);
	network::test::LogCapture capture({TELEPORT_LOGGER}, spdlog::level::info);
	ASSERT_TRUE(TeleportService::sendTeleportRequest(player(), KUSTANON)); // a template, but no spawn row

	EXPECT_TRUE(player().getResponseRequester().respond(TELEPORT_REQUEST_QUESTION, 1));

	EXPECT_TRUE(capture.contains("warning|" + std::string(TELEPORT_LOGGER) + "|No npc spawn found for : 203070")) << capture.dump();
	EXPECT_FLOAT_EQ(player().getX(), ACTOR_SPOT.x);
}

TEST_F(TeleportServiceRestTest, DenyingTheRequestDoesNothing) {
	publishDainesSpawn();
	spawnActor(0);
	network::test::LogCapture capture({TELEPORT_LOGGER}, spdlog::level::info);
	ASSERT_TRUE(TeleportService::sendTeleportRequest(player(), DAINES));

	EXPECT_TRUE(player().getResponseRequester().respond(TELEPORT_REQUEST_QUESTION, 0));

	EXPECT_FALSE(capture.contains("No npc spawn found")) << capture.dump();
	EXPECT_TRUE(TeleportService::sendTeleportRequest(player(), DAINES)) << "the answered request is gone";
}

TEST_F(TeleportServiceRestTest, AnUnknownNpcIsJavasNullPointerExceptionAfterTheRequestWasPut) {
	spawnActor(0);

	EXPECT_THROW(TeleportService::sendTeleportRequest(player(), 299999), runtime::NullPointerException);

	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(TeleportService::sendTeleportRequest(player(), DAINES)) << "the request of the unknown npc stays put (TeleportService.java:475-477)";
}

} // namespace
} // namespace aion::gameserver::services::teleport::test
