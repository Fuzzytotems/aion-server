// The hotspot teleport of the world map (P5-08, m5f-plan.md §2.3, §5 T-04 and T-08): BindPointTeleportService.teleport with its two anonymous
// Runnables (fieldmap BindPointTeleportService$1, stored as the SKILL_USE task, and $2, a one-shot task), cancelTeleport,
// calculateTeleportationPrice and checkRequirements.
//
// Java: BindPointTeleportService.java:39-114.
// The fixture is TravelTestSupport.h (a connected Elyos WARRIOR in Poeta beside Daines, a second player beside him) on its DeterministicExecutor:
// the cases advance the ManualClock through the 10 s cast and the 1 s before the move. The hotspot rows are hotspot_template.xml:4, 6, 8
// and 10 (verbatim: Akarios Village, ... in Poeta, and one in Ishalgen), plus two synthetic rows in Poeta: a hotspot without a race attribute
// and one with price 0.
//
// The price (m5f-plan.md §2.3): max(1, base + (long) (base * distance / 1000d)), the distance in Java's float arithmetic
// (PositionUtil.getDistance: float dx, dy, dz and their float sum of squares, then Math.sqrt). From the actor's spot (805.5, 1243.6, 118.986):
// hotspot 15 (427, 1741, 120, price 44) is 625.03605 m away, 44 * 625.03605 / 1000 = 27.5016 -> 27, price 71; hotspot 14 (560, 1382, 119) is
// 281.82408 m, 12.4003 -> 12, price 56; hotspot 13 (807, 1242, 119) is 2.1932 m, 0.0965 -> 0, price 44. Computed with IEEE single-precision
// steps (Python struct round trips), the vectors G-01's oracle is to reproduce.
//
// The cooldowns are a static map of BindPointTeleportService keyed by the player's object id (BindPointTeleportService.java:30) that no Java
// method clears; every case runs in a process of its own under ctest, and a case that needs a cooldown makes it itself.

#include "TravelTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/HotspotData.bind.h"
#include "aion/gameserver/dataholders/HotspotData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/services/teleport/BindPointTeleportService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::teleport::test {
namespace {

using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using network::test::PacketWriter;

/** ServerPacketsOpcodes.java:314 (SM_BIND_POINT_TELEPORT), :47 (SM_INVENTORY_UPDATE_ITEM) */
constexpr int32_t SM_BIND_POINT_TELEPORT_OPCODE = 296;
constexpr int32_t SM_INVENTORY_UPDATE_ITEM_OPCODE = 29;
constexpr const char* AUDIT_LOGGER = "AUDIT_LOG"; // AuditLogger.cpp
constexpr const char* HOTSPOT_LOGGER = "com.aionemu.gameserver.services.teleport.BindPointTeleportService";

/** hotspot_template.xml:4, 6, 8, 10 and two synthetic rows in Poeta (900: no race attribute, 901: price 0) */
constexpr std::string_view HOTSPOT_XML = R"xml(<hotspot_template>
	<hotspot_location id="13" worldId="210010000" x="807.0" y="1242.0" z="119.0" race="ELYOS" price="44"/>
	<hotspot_location id="14" worldId="210010000" x="560.0" y="1382.0" z="119.0" race="ELYOS" price="44"/>
	<hotspot_location id="15" worldId="210010000" x="427.0" y="1741.0" z="120.0" race="ELYOS" price="44"/>
	<hotspot_location id="16" worldId="220010000" x="528.0" y="2449.0" z="282.0" race="ASMODIANS" price="44"/>
	<hotspot_location id="900" worldId="210010000" x="560.0" y="1382.0" z="119.0" price="44"/>
	<hotspot_location id="901" worldId="210010000" x="560.0" y="1382.0" z="119.0" race="ELYOS" price="0"/>
</hotspot_template>)xml";

class BindPointTeleportTest : public TravelTest {
protected:
	void SetUp() override {
		TravelTest::SetUp();
		if (!prepared)
			return;
		dataholders::DataManager::HOTSPOT_DATA.publish(xml::bindString<dataholders::HotspotData>(context, HOTSPOT_XML));
	}

	void TearDown() override {
		if (prepared)
			dataholders::DataManager::HOTSPOT_DATA.resetForTests();
		TravelTest::TearDown();
	}

	static runtime::DeterministicExecutor& executor() {
		return dynamic_cast<runtime::DeterministicExecutor&>(*utils::ThreadPoolManager::installedBackend());
	}

	static void advance(int64_t millis) { executor().advance(std::chrono::milliseconds(millis)); }

	/** SM_BIND_POINT_TELEPORT.writeImpl: C action, D player; action 1: D loc; action 3: D loc, D cooldown */
	std::vector<uint8_t> hotspotPacket(int32_t action, int32_t locId, int32_t cooldown) {
		PacketWriter body;
		body.C(action).D(player().getObjectId());
		if (action == 1)
			body.D(locId);
		else if (action == 3)
			body.D(locId).D(cooldown);
		return javaPacket(SM_BIND_POINT_TELEPORT_OPCODE, body);
	}

	/** The whole cast: the request, 10 s, then the 1 s before the move */
	void teleportAndWait(int32_t locId, int64_t clientPrice) {
		BindPointTeleportService::teleport(player(), locId, clientPrice);
		advance(11000);
	}
};

// ---- the price (calculateTeleportationPrice, BindPointTeleportService.java:81-90) ------------------------------------------------------------

TEST_F(BindPointTeleportTest, ThePriceAddsTheTruncatedDistanceCost) {
	spawnActor(1000);
	network::test::LogCapture capture({HOTSPOT_LOGGER}, spdlog::level::info);

	teleportAndWait(15, 71);

	EXPECT_EQ(kinah(), 1000 - 71) << "44 + (long) 27.50";
	EXPECT_FALSE(capture.contains("prices don't match")) << capture.dump();
}

TEST_F(BindPointTeleportTest, ANearerHotspotCostsLess) {
	spawnActor(1000);

	teleportAndWait(14, 56);

	EXPECT_EQ(kinah(), 1000 - 56) << "44 + (long) 12.40";
}

TEST_F(BindPointTeleportTest, AHotspotNextToThePlayerCostsItsBasePrice) {
	spawnActor(1000);

	teleportAndWait(13, 44);

	EXPECT_EQ(kinah(), 1000 - 44) << "44 + (long) 0.0965";
}

TEST_F(BindPointTeleportTest, AFreeHotspotStillCostsOneKinah) {
	spawnActor(1000);

	teleportAndWait(901, 0); // the client sends 0: only Math.max(1, ...) makes it one

	EXPECT_EQ(kinah(), 999) << "Math.max(1, 0 + (long) 0.0)";
}

TEST_F(BindPointTeleportTest, AHigherClientPriceIsChargedAndWarned) {
	spawnActor(1000);
	network::test::LogCapture capture({HOTSPOT_LOGGER}, spdlog::level::info);

	teleportAndWait(15, 80);

	EXPECT_EQ(kinah(), 1000 - 80) << "Math.max(price, priceSentByGameClient)";
	EXPECT_TRUE(capture.contains("warning|" + std::string(HOTSPOT_LOGGER) + "|Hotspot teleport 15 prices don't match: 71 vs. 80")) << capture.dump();
}

TEST_F(BindPointTeleportTest, ALowerClientPriceIsIgnoredAndWarned) {
	spawnActor(1000);
	network::test::LogCapture capture({HOTSPOT_LOGGER}, spdlog::level::info);

	teleportAndWait(15, 0);

	EXPECT_EQ(kinah(), 1000 - 71);
	EXPECT_TRUE(capture.contains("warning|" + std::string(HOTSPOT_LOGGER) + "|Hotspot teleport 15 prices don't match: 71 vs. 0")) << capture.dump();
}

TEST_F(BindPointTeleportTest, ADifferenceOfOneKinahIsNotWarned) {
	spawnActor(1000);
	network::test::LogCapture capture({HOTSPOT_LOGGER}, spdlog::level::info);

	teleportAndWait(15, 72);

	EXPECT_EQ(kinah(), 1000 - 72) << "the higher price is still the client's";
	EXPECT_FALSE(capture.contains("prices don't match")) << "priceDifference > 1 only (BindPointTeleportService.java:87)\n" << capture.dump();
}

// ---- the requirements (checkRequirements, BindPointTeleportService.java:92-114) -------------------------------------------------------------

TEST_F(BindPointTeleportTest, AnUnknownHotspotIsAuditedAndNoRoute) {
	spawnActor(1000);
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);

	BindPointTeleportService::teleport(player(), 999, 71);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE())}));
	EXPECT_TRUE(capture.contains("Tried to use invalid hotspot teleport to locId 999")) << capture.dump();
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::SKILL_USE));
}

TEST_F(BindPointTeleportTest, AHotspotOfAnotherWorldIsAuditedAndNoRoute) {
	spawnActor(1000);
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);

	BindPointTeleportService::teleport(player(), 16, 44); // Ishalgen's

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE())}));
	EXPECT_TRUE(capture.contains("tried to use hotspot teleport 16 from invalid start world 210010000, expected 220010000")) << capture.dump();
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::SKILL_USE));
}

TEST_F(BindPointTeleportTest, AHotspotOfTheOtherRaceIsAuditedWithoutAMessage) {
	spawnActor(1000, model::Race::ASMODIANS); // an Asmodian standing in Poeta
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);

	BindPointTeleportService::teleport(player(), 15, 71);

	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(capture.contains("tried to use hotspot teleport 15 for invalid race ASMODIANS, expected ELYOS")) << capture.dump();
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::SKILL_USE));
}

TEST_F(BindPointTeleportTest, AHotspotWithoutRaceRefusesEveryRace) {
	spawnActor(1000);
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);

	BindPointTeleportService::teleport(player(), 900, 56); // Java: player.getRace() != null

	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(capture.contains("tried to use hotspot teleport 900 for invalid race ELYOS, expected null")) << capture.dump();
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::SKILL_USE));
}

TEST_F(BindPointTeleportTest, NotEnoughKinahForTheComputedPriceIsRefused) {
	spawnActor(70);

	BindPointTeleportService::teleport(player(), 15, 71);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NOT_ENOUGH_FEE())}));
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::SKILL_USE));
}

TEST_F(BindPointTeleportTest, ExactlyThePriceIsEnough) {
	spawnActor(71);

	teleportAndWait(15, 71);

	EXPECT_EQ(kinah(), 0);
	EXPECT_FLOAT_EQ(player().getX(), 427.0f);
}

TEST_F(BindPointTeleportTest, TheCooldownRefusesTheNextTeleport) {
	spawnActor(1000);
	teleportAndWait(13, 44);
	clearSent();

	BindPointTeleportService::teleport(player(), 15, 71);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_FLYING_TIME_NOT_READY())}));
	advance(11000);
	EXPECT_EQ(kinah(), 1000 - 44) << "no second cast";
}

TEST_F(BindPointTeleportTest, OnLoginSendsTheCooldownLeft) {
	spawnActor(1000);
	teleportAndWait(13, 44);
	clearSent();

	BindPointTeleportService::onLogin(player());

	// the cooldown runs on the wall clock (System.currentTimeMillis): 600 s minus the few milliseconds of the case, truncated
	const std::vector<std::vector<uint8_t>> packets = ofOpcode(sent(), SM_BIND_POINT_TELEPORT_OPCODE);
	ASSERT_EQ(packets.size(), 1u);
	EXPECT_TRUE(packets[0] == hotspotPacket(3, 13, 599) || packets[0] == hotspotPacket(3, 13, 600));
}

// ---- the cast and the move (the two Runnables, BindPointTeleportService.java:51-71) ---------------------------------------------------------

TEST_F(BindPointTeleportTest, TheCastChargesAfterTenSecondsAndMovesOneSecondLater) {
	spawnActor(1000);
	const float x = player().getX(), y = player().getY();

	BindPointTeleportService::teleport(player(), 15, 71);

	// broadcast to the player and to who sees him: SM_BIND_POINT_TELEPORT(1, player, loc)
	EXPECT_EQ(ofOpcode(sent(), SM_BIND_POINT_TELEPORT_OPCODE), cptest::exactly({hotspotPacket(1, 15, 0)}));
	EXPECT_EQ(ofOpcode(watcherSent(), SM_BIND_POINT_TELEPORT_OPCODE), cptest::exactly({hotspotPacket(1, 15, 0)}));
	EXPECT_TRUE(player().getController().hasTask(model::TaskId::SKILL_USE)) << "the cast is the SKILL_USE task";
	clearSent();

	advance(9999);
	EXPECT_EQ(kinah(), 1000);
	EXPECT_TRUE(sent().empty());

	advance(1);
	EXPECT_EQ(kinah(), 1000 - 71);
	const std::vector<std::vector<uint8_t>> kinahUpdates = ofOpcode(sent(), SM_INVENTORY_UPDATE_ITEM_OPCODE);
	ASSERT_EQ(kinahUpdates.size(), 1u);
	// ItemUpdateType.DEC_KINAH_FLY (0x4B, ItemPacketService.java:48), the last H of SM_INVENTORY_UPDATE_ITEM
	EXPECT_EQ(std::vector<uint8_t>(kinahUpdates[0].end() - 2, kinahUpdates[0].end()), (std::vector<uint8_t>{0x4B, 0x00}));
	EXPECT_EQ(ofOpcode(sent(), SM_BIND_POINT_TELEPORT_OPCODE), cptest::exactly({hotspotPacket(3, 15, 600)}));
	EXPECT_EQ(ofOpcode(watcherSent(), SM_BIND_POINT_TELEPORT_OPCODE), cptest::exactly({hotspotPacket(3, 15, 600)}));
	EXPECT_FLOAT_EQ(player().getX(), x) << "not moved yet";
	EXPECT_FLOAT_EQ(player().getY(), y);

	advance(999);
	EXPECT_FLOAT_EQ(player().getX(), x);

	advance(1);
	// teleportTo(player, world, x, y, z): the same map, so moved and spawned at once (heading kept)
	EXPECT_EQ(player().getWorldId(), POETA);
	EXPECT_FLOAT_EQ(player().getX(), 427.0f);
	EXPECT_FLOAT_EQ(player().getY(), 1741.0f);
	EXPECT_FLOAT_EQ(player().getZ(), 120.0f);
	EXPECT_TRUE(player().isSpawned());
}

TEST_F(BindPointTeleportTest, KinahSpentDuringTheCastStopsItAtTheCharge) {
	spawnActor(100);
	BindPointTeleportService::teleport(player(), 15, 71);
	player().getInventory().decreaseKinah(50);
	clearSent();

	advance(11000);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NOT_ENOUGH_FEE())}));
	EXPECT_EQ(kinah(), 50);
	EXPECT_FLOAT_EQ(player().getX(), ACTOR_SPOT.x) << "no move";
	clearSent();
	BindPointTeleportService::teleport(player(), 13, 44);
	EXPECT_EQ(ofOpcode(sent(), SM_BIND_POINT_TELEPORT_OPCODE), cptest::exactly({hotspotPacket(1, 13, 0)})) << "no cooldown was added";
}

TEST_F(BindPointTeleportTest, ADeadPlayerIsNotMovedAfterTheCharge) {
	spawnActor(1000);
	BindPointTeleportService::teleport(player(), 15, 71);
	advance(10000);
	ASSERT_EQ(kinah(), 1000 - 71);

	player().setLifeStats(std::make_unique<cptest::DeadPlayerLifeStats>(player())); // dead (isDead) in the second before the move
	// a teleportTo of the dead player would revive him first (TeleportService.java:282-283), which this fixture cannot (HouseData): the
	// task's exception would be logged by ExecuteWrapper - the second check that the move was not tried
	network::test::LogCapture capture({"com.aionemu.commons.utils.concurrent.ExecuteWrapper"}, spdlog::level::info);

	ASSERT_TRUE(player().isDead());
	advance(1000);
	EXPECT_FALSE(capture.contains("Exception")) << capture.dump();
	EXPECT_FLOAT_EQ(player().getX(), ACTOR_SPOT.x);
	EXPECT_FLOAT_EQ(player().getY(), ACTOR_SPOT.y);
}

TEST_F(BindPointTeleportTest, APlayerAboutToDieIsNotMovedAfterTheCharge) {
	spawnActor(1000);
	BindPointTeleportService::teleport(player(), 15, 71);
	advance(10000);
	ASSERT_EQ(kinah(), 1000 - 71);

	player().getLifeStats()->setKillingBlow(1); // a killing blow is on its way (CreatureLifeStats.isAboutToDie: killingBlow != 0), not dead yet

	ASSERT_TRUE(player().getLifeStats()->isAboutToDie());
	ASSERT_FALSE(player().isDead());
	advance(1000);
	EXPECT_FLOAT_EQ(player().getX(), ACTOR_SPOT.x);
	EXPECT_FLOAT_EQ(player().getY(), ACTOR_SPOT.y);
}

TEST_F(BindPointTeleportTest, CancellingTheCastBroadcastsTheCancelAndChargesNothing) {
	spawnActor(1000);
	BindPointTeleportService::teleport(player(), 15, 71);
	clearSent();

	BindPointTeleportService::cancelTeleport(player(), 15);

	EXPECT_EQ(ofOpcode(sent(), SM_BIND_POINT_TELEPORT_OPCODE), cptest::exactly({hotspotPacket(2, 15, 0)}));
	EXPECT_EQ(ofOpcode(watcherSent(), SM_BIND_POINT_TELEPORT_OPCODE), cptest::exactly({hotspotPacket(2, 15, 0)}));
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::SKILL_USE));
	advance(11000);
	EXPECT_EQ(kinah(), 1000);
	EXPECT_FLOAT_EQ(player().getX(), ACTOR_SPOT.x);
}

TEST_F(BindPointTeleportTest, CancellingWithoutACastDoesNothing) {
	spawnActor(1000);

	BindPointTeleportService::cancelTeleport(player(), 15);

	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(watcherSent().empty());
}

TEST_F(BindPointTeleportTest, LeavingTheWorldCancelsTheCastAndReleasesThePlayer) {
	spawnActor(1000);
	BindPointTeleportService::teleport(player(), 15, 71);

	player().getController().cancelAllTasks(); // what the logout does (PlayerLeaveWorldService -> CreatureController.cancelAllTasks)

	EXPECT_FALSE(player().getController().hasTask(model::TaskId::SKILL_USE));
	advance(11000);
	EXPECT_EQ(kinah(), 1000) << "the cancelled $1 never ran";
}

} // namespace
} // namespace aion::gameserver::services::teleport::test
