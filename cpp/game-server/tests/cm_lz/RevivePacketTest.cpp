// CM_REVIVE, CM_REJECT_REVIVE and CM_MOTION (P5-16, m5b-plan.md C-02/C-03): the L-Z client packets of a fight's end. CM_REVIVE is the answer
// to the death window; CM_REJECT_REVIVE is what a player sends who declines another player's resurrection and that Java answers with nothing;
// CM_MOTION is the custom-animation packet the real client sends while fighting.
//
// Java: CM_REVIVE.java:32-63, CM_REJECT_REVIVE.java:20-25, CM_MOTION.java:24-32.
//
// The switch of CM_REVIVE is asserted arm by arm through the service body each one calls: only bindRevive and revive are ported at M5b-1
// (m5b-plan.md C-03 leaves the kisk, rebirth, item and instance arms as AION_UNPORTED), so an arm that reaches the wrong service reports the
// wrong function name in the UnportedException. When those arms are ported, each EXPECT below becomes an assertion about what that revive does.

#include "../cm_ak/InWorldPacketRunSupport.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/ReviveType.h"
#include "aion/gameserver/model/gameobjects/player/ReviveTypeInfo.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/network/aion/clientpackets/CM_MOTION.h"
#include "aion/gameserver/network/aion/clientpackets/CM_REJECT_REVIVE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_REVIVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::gameobjects::player::CustomPlayerState;
using model::gameobjects::player::ReviveType;
using network::test::LogCapture;
using network::test::PacketWriter;
using serverpackets::SM_MOTION;

/** the decoded opcodes of ClientPacketInfo.gen.inc:23, :129 and :78 */
constexpr int32_t REVIVE_OPCODE = 5;
constexpr int32_t REJECT_REVIVE_OPCODE = 146;
constexpr int32_t MOTION_OPCODE = 71;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";

/** Reads `data` with a fresh packet of type P (no connection) and returns the unread byte count, -1 if read() threw */
template <class P>
int32_t unreadBytesAfterRead(const std::vector<uint8_t>& data, int32_t opcode) {
	std::vector<uint8_t> copy = data;
	auto packet = std::make_unique<P>(opcode, StateSet{AionConnection_State::IN_GAME});
	packet->setBuffer(commons::utils::ByteBuffer::wrap(copy));
	if (!packet->read())
		return -1;
	return packet->getRemainingBytes();
}

/** Asserts that P consumes `data` exactly and logs no missing field */
template <class P>
void expectExactRead(const std::vector<uint8_t>& data, int32_t opcode) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	EXPECT_EQ(unreadBytesAfterRead<P>(data, opcode), 0) << typeid(P).name();
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(RevivePacketReadTest, ReviveRejectReviveAndMotionBodies) {
	// CM_REVIVE: a single UC revive id
	expectExactRead<CM_REVIVE>(PacketWriter().C(0).data, REVIVE_OPCODE);
	expectExactRead<CM_REVIVE>(PacketWriter().C(8).data, REVIVE_OPCODE);
	{
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		EXPECT_EQ(unreadBytesAfterRead<CM_REVIVE>({}, REVIVE_OPCODE), 0);
		EXPECT_TRUE(capture.contains("Missing C")) << capture.dump();
	}

	// CM_REJECT_REVIVE: an empty body; anything in it is left unread
	expectExactRead<CM_REJECT_REVIVE>({}, REJECT_REVIVE_OPCODE);
	EXPECT_EQ(unreadBytesAfterRead<CM_REJECT_REVIVE>(PacketWriter().C(1).data, REJECT_REVIVE_OPCODE), 1) << "readImpl is empty";

	// CM_MOTION: C unk, UH motion id, UC motion type = 4 bytes
	expectExactRead<CM_MOTION>(PacketWriter().C(4).H(0x1234).C(2).data, MOTION_OPCODE);
	expectExactRead<CM_MOTION>(PacketWriter().C(0).H(0).C(0).data, MOTION_OPCODE);
	{
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		EXPECT_EQ(unreadBytesAfterRead<CM_MOTION>(PacketWriter().C(4).H(0x1234).data, MOTION_OPCODE), 0);
		EXPECT_TRUE(capture.contains("Missing C")) << "the trailing motion type must underflow: " << capture.dump();
	}
}

TEST(ReviveTypeIdTest, TheClientIdsAreTheOnesTheSwitchExpects) {
	// CM_REVIVE forwards the raw client id to ReviveType.getReviveTypeById (ReviveTypeInfo.h:17-27); these are the ids the run tests send
	using model::gameobjects::player::getReviveTypeById;
	EXPECT_EQ(getReviveTypeById(0), ReviveType::BIND_REVIVE);
	EXPECT_EQ(getReviveTypeById(1), ReviveType::REBIRTH_REVIVE);
	EXPECT_EQ(getReviveTypeById(2), ReviveType::ITEM_SELF_REVIVE);
	EXPECT_EQ(getReviveTypeById(3), ReviveType::SKILL_REVIVE);
	EXPECT_EQ(getReviveTypeById(4), ReviveType::KISK_REVIVE);
	EXPECT_EQ(getReviveTypeById(6), ReviveType::INSTANCE_REVIVE);
	EXPECT_EQ(getReviveTypeById(8), ReviveType::OBELISK_REVIVE);
	EXPECT_THROW(getReviveTypeById(5), runtime::IllegalArgumentException) << "5 and 7 are not revive types";
}

class ReviveRunTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		actor = makePlayer(500001, 9501, "Corpse");
		actor.player->getPosition()->setIsSpawned(true);
		client = std::make_unique<TestClient>();
		client->enterWorld(actor);
		(*client)->clearSent();
	}

	void TearDown() override {
		if (actor.player) {
			actor.player->setTarget(nullptr);
			actor.player->setClientConnection(nullptr);
		}
		client.reset();
		actor = {};
		InWorldPacketTest::TearDown();
	}

	/** Makes the character dead the way the runImpl guard sees it */
	void die() { actor.player->setLifeStats(std::make_unique<DeadPlayerLifeStats>(*actor.player)); }

	void revive(int32_t reviveId) {
		(*client)->clearSent();
		Driver<CM_REVIVE> packet(REVIVE_OPCODE);
		packet.readAndRun(PacketWriter().C(reviveId).data, client->get());
	}

	/**
	 * Runs one CM_REVIVE and returns the name of the service body it reached, from the UnportedException it threw - or, for the EVENT_MODE
	 * arms, the NullPointerException of the holder this fixture does not publish: TeleportService::teleportToEvent (ported in M5f) moves a
	 * player without an event position to his bind location, whose fallback reads PlayerInitialData
	 */
	std::string unportedArm(int32_t reviveId) {
		try {
			revive(reviveId);
		} catch (const runtime::UnportedException& unported) {
			return unported.what();
		} catch (const runtime::NullPointerException& missing) {
			return missing.what();
		}
		return "<did not throw>";
	}

	PlayerFixture actor;
	std::unique_ptr<TestClient> client;
};

TEST_F(ReviveRunTest, ALivingCharacterIsIgnored) {
	ASSERT_FALSE(actor.player->isDead());
	runtime::resetUnportedHitsForTests();

	revive(0);

	// Java: `if (!activePlayer.isDead()) return;` - before the revive type is even looked up
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
	EXPECT_TRUE((*client)->sentBytes().empty());
}

TEST_F(ReviveRunTest, BindAndObeliskReviveBothReachBindRevive) {
	die();
	actor.player->setCustomState(CustomPlayerState::EVENT_MODE); // the only bindRevive arm a unit test can follow (see PlayerReviveServiceTest)

	// BIND_REVIVE and OBELISK_REVIVE share the switch arm (CM_REVIVE.java:47-50); both end in TeleportService::teleportToEvent here, whose
	// bind fallback (no event position) reads the PlayerInitialData this fixture does not publish
	EXPECT_NE(unportedArm(0).find("PlayerInitialData"), std::string::npos) << unportedArm(0);
	die(); // bindRevive revived him, so the guard needs a fresh corpse
	actor.player->setCustomState(CustomPlayerState::EVENT_MODE);
	EXPECT_NE(unportedArm(8).find("PlayerInitialData"), std::string::npos) << unportedArm(8);
}

TEST_F(ReviveRunTest, EveryOtherArmReachesItsOwnService) {
	die();
	// gameserver.administration.auto_res: Java's default is 1 (AdminConfig.java:45), which this fixture's account (level 0) is below; the C++
	// atomic starts at 0 until a config is loaded, which would let every character take rebirthRevive's AUTO_RES arm
	const int8_t autoRes = configs::administration::AdminConfig::AUTO_RES.exchange(1);
	// REBIRTH_REVIVE reaches rebirthRevive, ported (M5j S-02): without a rebirth effect it audits and returns (PlayerReviveService.java:64-68)
	EXPECT_EQ(unportedArm(1), "<did not throw>");
	EXPECT_TRUE(actor.player->isDead());
	// ITEM_SELF_REVIVE reaches itemSelfRevive, ported (M5j S-02): without a self-res stone it audits and returns (:215-220)
	EXPECT_EQ(unportedArm(2), "<did not throw>");
	EXPECT_TRUE(actor.player->isDead());
	// SKILL_REVIVE reaches skillRevive, ported (P5-08, C-02): without a resurrection offer it audits and returns (PlayerReviveService.java:42-46)
	EXPECT_EQ(unportedArm(3), "<did not throw>");
	EXPECT_TRUE(actor.player->isDead());
	// KISK_REVIVE reaches kiskRevive, ported (P5-08): without a bound kisk it does nothing (PlayerReviveService.java:139-155)
	EXPECT_EQ(unportedArm(4), "<did not throw>");
	EXPECT_TRUE(actor.player->isDead());
	configs::administration::AdminConfig::AUTO_RES.store(autoRes);
}

TEST_F(ReviveRunTest, InstanceReviveTakesItsEventModeArm) {
	// INSTANCE_REVIVE (CM_REVIVE.java:59-61) reaches instanceRevive, ported by the ascension lane (m5f-plan.md §15, T-06); its EVENT_MODE arm
	// (PlayerReviveService.java:162-168) is the one this fixture can follow, to TeleportService::teleportToEvent's bind fallback. bindRevive's
	// EVENT_MODE arm ends there too, so the rebirth message tells them apart: instanceRevive sends it always, bindRevive only for a skill > 0.
	die();
	actor.player->setCustomState(CustomPlayerState::EVENT_MODE);
	EXPECT_NE(unportedArm(6).find("PlayerInitialData"), std::string::npos) << "teleportToEvent -> moveToBindLocation";
	std::vector<std::vector<uint8_t>> sent = (*client)->sentBytes();
	EXPECT_NE(std::find(sent.begin(), sent.end(), serialized(serverpackets::SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME(), client->con())), sent.end());
	EXPECT_FALSE(actor.player->isDead()) << "revived at 100 % before the teleport";
}

TEST_F(ReviveRunTest, AnUnknownReviveIdThrowsLikeJava) {
	die();

	// Java ReviveType.getReviveTypeById throws IllegalArgumentException("Unsupported revive type: " + id); the packet boundary logs it
	EXPECT_THROW(revive(5), runtime::IllegalArgumentException);
}

TEST(RejectReviveRunTest, RunImplDoesNothing) {
	// Java CM_REJECT_REVIVE.runImpl is empty and does not even read the connection
	Driver<CM_REJECT_REVIVE> packet(REJECT_REVIVE_OPCODE);
	EXPECT_NO_THROW(packet.runNow());
}

class MotionRunTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		actor = makePlayer(510001, 9601, "Dancer");
		actor.player->setMotions(std::make_unique<model::gameobjects::player::motion::MotionList>(*actor.player));
		client = std::make_unique<TestClient>();
		client->enterWorld(actor);
		(*client)->clearSent();
	}

	void TearDown() override {
		if (actor.player)
			actor.player->setClientConnection(nullptr);
		client.reset();
		actor = {};
		InWorldPacketTest::TearDown();
	}

	void motion(int32_t motionId, int32_t motionType) {
		(*client)->clearSent();
		Driver<CM_MOTION> packet(MOTION_OPCODE);
		packet.readAndRun(PacketWriter().C(4).H(motionId).C(motionType).data, client->get());
	}

	PlayerFixture actor;
	std::unique_ptr<TestClient> client;
};

TEST_F(MotionRunTest, MotionIdZeroDeactivatesAndAnswersWithSmMotion) {
	// MotionList.setActive(0, type) with no active motions falls through to the single-motion SM_MOTION and returns before the broadcast
	// (MotionList.cpp:85-101), so the packet the client gets is the proof that runImpl passed both fields on in the right order
	motion(0, 3);

	EXPECT_EQ((*client)->sentBytes(), exactly({serialized(SM_MOTION(int16_t{0}, int8_t{3}), client->con())}));
}

TEST_F(MotionRunTest, TheMotionTypeIsTheThirdFieldNotTheSecond) {
	// a readImpl that swapped motionId and motionType would send SM_MOTION(3, 0) here
	motion(0, 5);

	EXPECT_EQ((*client)->sentBytes(), exactly({serialized(SM_MOTION(int16_t{0}, int8_t{5}), client->con())}));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
