// CM_TELEPORT_SELECT (P5-16, m5f-plan.md §5 P-01 and P-07, the early travel slice of §16): the client's choice on a teleporter's or a flight
// master's map. Java: CM_TELEPORT_SELECT.java:38-69.
//
// The fixture is tests/playersvc/TravelTestSupport.h (P5-08's, by relative path as this directory already includes tests/instance): the real
// map rows of Poeta and Verteron, the real teleporter, location and npc rows, a connected Elyos WARRIOR beside Daines and a second player beside
// him. The npc of a selection is put into the actor's known list with TestKnownList (a real client selects on a map the npc's dialog opened).
// A good selection is followed through CM_TELEPORT_ANIMATION_DONE, the packet a real client sends when the jump animation ends.
//
// m5f-plan.md D7, kept and pinned below: the packet does not repeat the dialog's Daeva check (DialogService.java:188-195), so a level-1
// character that sends it for Daines' Verteron route goes to Verteron if he can pay.

#include "../playersvc/TravelTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/network/aion/AionConnection_State.h"
#include "aion/gameserver/network/aion/StateSet.h"
#include "aion/gameserver/network/aion/clientpackets/CM_TELEPORT_ANIMATION_DONE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_TELEPORT_SELECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHANNEL_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_SPAWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

/** The fields CM_TELEPORT_SELECT::readImpl decoded (Java keeps them private without a getter) */
struct CM_TELEPORT_SELECTTestAccess {
	static int32_t targetObjId(const CM_TELEPORT_SELECT& p) { return p.targetObjId; }
	static int32_t locId(const CM_TELEPORT_SELECT& p) { return p.locId; }
};

} // namespace aion::gameserver::network::aion::clientpackets

namespace aion::gameserver::services::teleport::test {
namespace {

using model::gameobjects::Npc;
using network::aion::clientpackets::CM_TELEPORT_ANIMATION_DONE;
using network::aion::clientpackets::CM_TELEPORT_SELECT;
using network::aion::clientpackets::CM_TELEPORT_SELECTTestAccess;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using network::test::PacketWriter;

// ClientPacketInfo.gen.inc:131, :31 (AionClientPacketFactory packets[148], [15])
constexpr int32_t TELEPORT_SELECT_OPCODE = 148;
constexpr int32_t TELEPORT_ANIMATION_DONE_OPCODE = 15;
/** ServerPacketsOpcodes.java:38, 55 */
constexpr int32_t SM_TELEPORT_LOC_OPCODE = 20;
constexpr int32_t SM_EMOTION_OPCODE = 37;
constexpr const char* AUDIT_LOGGER = "AUDIT_LOG"; // AuditLogger.cpp

/** CM_TELEPORT_SELECT with its readImpl and runImpl reachable, and the bytes it left unread */
class SelectDriver final : public CM_TELEPORT_SELECT {
public:
	SelectDriver() : CM_TELEPORT_SELECT(TELEPORT_SELECT_OPCODE, network::aion::StateSet{network::aion::AionConnection_State::IN_GAME}) {}

	/** Reads the body as AionConnection::processData does (the body is kept: the buffer wraps it) */
	bool readBody(std::vector<uint8_t> bytes, const std::shared_ptr<network::aion::AionConnection>& connection) {
		body = std::move(bytes);
		setBuffer(commons::utils::ByteBuffer::wrap(body));
		setConnection(connection);
		return read();
	}

	int32_t remaining() const { return getRemainingBytes(); }

	void runNow() { runImpl(); }

private:
	std::vector<uint8_t> body;
};

class TeleportSelectPacketTest : public TravelTest {
protected:
	/** CM_TELEPORT_SELECT.readImpl's body: D targetObjId, D locId, H (read and dropped) */
	static std::vector<uint8_t> selectBody(int32_t targetObjId, int32_t locId) { return PacketWriter().D(targetObjId).D(locId).H(0).data; }

	void select(int32_t targetObjId, int32_t locId) {
		SelectDriver packet;
		ASSERT_TRUE(packet.readBody(selectBody(targetObjId, locId), actorClient->get()));
		packet.runNow();
	}

	void animationDone() {
		cptest::Driver<CM_TELEPORT_ANIMATION_DONE> packet(TELEPORT_ANIMATION_DONE_OPCODE);
		packet.readAndRun({}, actorClient->get());
	}

	/** The npc in the actor's known list, the way the map dialog of a real client leaves it; the packets of the see are cleared */
	Npc& known(Npc& npc) {
		actor.knownList().addForTest(npc);
		EXPECT_TRUE(player().getKnownList().getObject(npc.getObjectId()));
		clearSent();
		return npc;
	}
};

// ---- readImpl ------------------------------------------------------------------------------------------------------------------------------

TEST_F(TeleportSelectPacketTest, ReadsItsTenBytes) {
	spawnActor(0);
	SelectDriver packet;
	std::vector<uint8_t> body = PacketWriter().D(0x01020304).D(4).H(0x7F7F).data;
	ASSERT_EQ(body.size(), 10u);

	ASSERT_TRUE(packet.readBody(body, actorClient->get()));

	EXPECT_EQ(CM_TELEPORT_SELECTTestAccess::targetObjId(packet), 0x01020304);
	EXPECT_EQ(CM_TELEPORT_SELECTTestAccess::locId(packet), 4);
	EXPECT_EQ(packet.remaining(), 0) << "the trailing readH is consumed";
}

TEST_F(TeleportSelectPacketTest, ANineByteBodyReadsTheTwoIdsAndLogsTheMissingShort) {
	spawnActor(0);
	network::test::LogCapture capture({"com.aionemu.commons.network.packet.BaseClientPacket"}, spdlog::level::info);
	SelectDriver packet;

	// BaseClientPacket.readH catches the underflow, logs it and answers 0 (the Java commons do the same)
	EXPECT_TRUE(packet.readBody(PacketWriter().D(7).D(4).C(0).data, actorClient->get()));

	EXPECT_EQ(CM_TELEPORT_SELECTTestAccess::targetObjId(packet), 7);
	EXPECT_EQ(CM_TELEPORT_SELECTTestAccess::locId(packet), 4);
	EXPECT_TRUE(capture.contains("Missing H for")) << capture.dump();
}

// ---- runImpl -------------------------------------------------------------------------------------------------------------------------------

TEST_F(TeleportSelectPacketTest, ADeadPlayerSelectsNothing) {
	spawnActor(5000);
	Npc& daines = known(npc(DAINES, POETA, DAINES_SPOT));
	player().setLifeStats(std::make_unique<cptest::DeadPlayerLifeStats>(player()));
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);

	select(daines.getObjectId(), 4);

	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(kinah(), 5000);
	EXPECT_TRUE(player().isSpawned());
	EXPECT_FALSE(capture.contains("tried to")) << capture.dump();
}

TEST_F(TeleportSelectPacketTest, AnObjectIdNobodyKnowsIsAuditedAsAnUnknownNpc) {
	spawnActor(5000);
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);

	select(987654, 4);

	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(capture.contains("tried to teleport to locId 4 via unknown npc (objId 987654) at " + player().getPosition()->toString()))
		<< capture.dump();
	EXPECT_EQ(kinah(), 5000);
}

TEST_F(TeleportSelectPacketTest, AnNpcOutsideTheKnownListIsAuditedByItsWorldObject) {
	spawnActor(5000);
	Npc& daines = npc(DAINES, POETA, DAINES_SPOT); // not in the known list
	world::World::getInstance().storeObject(daines);
	const std::string dainesText = daines.toString();
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);

	select(daines.getObjectId(), 4);
	world::World::getInstance().removeObject(daines);

	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(capture.contains("tried to teleport to locId 4 via " + dainesText + " at ")) << "World.findVisibleObject: " << dainesText << " / "
																							 << capture.dump();
	EXPECT_EQ(kinah(), 5000);
	EXPECT_TRUE(player().isSpawned());
}

TEST_F(TeleportSelectPacketTest, AKnownObjectThatIsNoNpcIsAudited) {
	spawnActor(5000); // the watcher is known to him: a Player, not an Npc
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);

	select(watcher.player->getObjectId(), 4);

	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(capture.contains("tried to teleport to locId 4 via " + watcher.player->toString() + " at ")) << capture.dump();
	EXPECT_EQ(kinah(), 5000);
}

TEST_F(TeleportSelectPacketTest, ALocIdTheTeleporterDoesNotHaveIsNoRoute) {
	spawnActor(5000);
	Npc& daines = known(npc(DAINES, POETA, DAINES_SPOT));
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);

	select(daines.getObjectId(), 13); // Kustanon's loc, not Daines'

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE())}));
	EXPECT_TRUE(capture.contains("tried to teleport to invalid locId 13 via " + daines.toString() + " at " + player().getPosition()->toString()))
		<< capture.dump();
	EXPECT_EQ(kinah(), 5000);
}

TEST_F(TeleportSelectPacketTest, ATeleporterWithoutRoutesIsJavasNullPointerException) {
	// <locations> without a <telelocation>: JAXB leaves TeleLocIdData.locids null, and getTeleportLocation's for-each over it throws
	// NullPointerException (CM_TELEPORT_SELECT.java:62, TeleLocIdData.java:27-34) - no audit line, no NO_ROUTE
	dataholders::DataManager::TELEPORTER_DATA.resetForTests();
	dataholders::DataManager::TELEPORTER_DATA.publish(xml::bindString<dataholders::TeleporterData>(context,
		R"xml(<npc_teleporter><teleporter_template npc_ids="203194" teleportId="2"><locations/></teleporter_template></npc_teleporter>)xml"));
	spawnActor(5000);
	Npc& daines = known(npc(DAINES, POETA, DAINES_SPOT));
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);

	EXPECT_THROW(select(daines.getObjectId(), 4), runtime::NullPointerException);

	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(capture.contains("tried to")) << capture.dump();
	EXPECT_EQ(kinah(), 5000);
}

TEST_F(TeleportSelectPacketTest, ARefusedTeleporterEndsTheSelection) {
	spawnActor(5000);
	Npc& aero = known(npc(AERO, POETA, AERO_SPOT)); // 630 m away

	select(aero.getObjectId(), 12);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_FAR_FROM_NPC())}));
	EXPECT_EQ(kinah(), 5000);
	EXPECT_FALSE(player().isInState(model::gameobjects::state::CreatureState::FLYING));
}

TEST_F(TeleportSelectPacketTest, AGoodSelectionTeleportsWithTheJumpAndArrivesAfterTheAnimation) {
	spawnActor(5000);
	Npc& daines = known(npc(DAINES, POETA, DAINES_SPOT));

	select(daines.getObjectId(), 4);

	EXPECT_EQ(kinah(), 5000 - 1130);
	// JUMP_IN (3): Daines has no static id (CM_TELEPORT_SELECT.java:68)
	EXPECT_EQ(ofOpcode(sent(), SM_TELEPORT_LOC_OPCODE), cptest::exactly({javaPacket(SM_TELEPORT_LOC_OPCODE,
		PacketWriter().C(3).D(VERTERON).D(VERTERON).F(VERTERON_X).F(VERTERON_Y).F(VERTERON_Z).C(0))}));
	EXPECT_FALSE(player().isSpawned());
	clearSent();

	animationDone();

	EXPECT_EQ(sent(), cptest::exactly({forActor(network::aion::serverpackets::SM_CHANNEL_INFO(player().getPosition())),
						  forActor(network::aion::serverpackets::SM_PLAYER_SPAWN(player()))}));
	EXPECT_EQ(player().getWorldId(), VERTERON);
	EXPECT_FLOAT_EQ(player().getX(), VERTERON_X);
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::TELEPORT));
}

TEST_F(TeleportSelectPacketTest, ATeleporterWithAStaticIdJumpsWithTheStatueAnimation) {
	spawnActor(5000);
	Npc& statue = known(npc(DAINES, POETA, DAINES_SPOT, 1234));

	select(statue.getObjectId(), 4);

	EXPECT_EQ(ofOpcode(sent(), SM_TELEPORT_LOC_OPCODE), cptest::exactly({javaPacket(SM_TELEPORT_LOC_OPCODE,
		PacketWriter().C(4).D(VERTERON).D(VERTERON).F(VERTERON_X).F(VERTERON_Y).F(VERTERON_Z).C(0))}))
		<< "npc.hasStatic() ? JUMP_IN_STATUE (4)";
}

TEST_F(TeleportSelectPacketTest, TheDialogsDaevaCheckIsNotRepeated) {
	spawnActor(5000);
	ASSERT_FALSE(player().getCommonData()->isDaeva());
	Npc& daines = known(npc(DAINES, POETA, DAINES_SPOT));

	select(daines.getObjectId(), 4);

	EXPECT_TRUE(player().getController().hasTask(model::TaskId::TELEPORT)) << "m5f-plan.md D7: a non-Daeva goes to Verteron by packet";
	EXPECT_EQ(kinah(), 5000 - 1130);
}

TEST_F(TeleportSelectPacketTest, AFlightMastersSelectionStartsTheFlight) {
	spawnActor(1000);
	Npc& kustanon = known(npc(KUSTANON, POETA, KUSTANON_SPOT));

	select(kustanon.getObjectId(), 13);

	EXPECT_TRUE(player().isInState(model::gameobjects::state::CreatureState::FLYING));
	ASSERT_TRUE(player().getFlightPath());
	EXPECT_EQ(player().getFlightPath()->getId(), 5001);
	EXPECT_EQ(ofOpcode(sent(), SM_EMOTION_OPCODE),
		cptest::exactly({forActor(network::aion::serverpackets::SM_EMOTION(player(), model::EmotionType::START_FLYTELEPORT, 5001, 0))}));
	EXPECT_TRUE(ofOpcode(sent(), SM_TELEPORT_LOC_OPCODE).empty());
	EXPECT_EQ(kinah(), 1000 - 226);
}

} // namespace
} // namespace aion::gameserver::services::teleport::test
