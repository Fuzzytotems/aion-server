// CM_TARGET_SELECT (stage 3 wave B, m5a-plan.md §11 / m5a-client-session.md F-2): the packet the real client sends on every tab target and
// mob click, which the server ignored because the class was not ported. Every branch of the Java runImpl is driven here against real Players, a
// real KnownList and a real AionConnection, and each branch is pinned by the exact packet it queues (the serialized SM_SYSTEM_MESSAGE or
// SM_TARGET_SELECTED bytes, not just an opcode) and by the resulting player.getTarget().
//
// Java: game-server/src/com/aionemu/gameserver/network/aion/clientpackets/CM_TARGET_SELECT.java:30-68.

#include "../cm_ak/InWorldPacketRunSupport.h"

#include <cstdint>
#include <vector>

#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/network/aion/clientpackets/CM_TARGET_SELECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TARGET_SELECTED.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::gameobjects::state::CreatureVisualState;
using network::test::LogCapture;
using network::test::PacketWriter;
using serverpackets::SM_SYSTEM_MESSAGE;
using serverpackets::SM_TARGET_SELECTED;

/** the decoded opcode of CM_TARGET_SELECT (ClientPacketInfo.gen.inc:43) */
constexpr int32_t OPCODE = 31;

/** Java readImpl: D target object id, C select-target-of-target */
std::vector<uint8_t> body(int32_t targetObjectId, bool selectTargetOfTarget) {
	return PacketWriter().D(targetObjectId).C(selectTargetOfTarget ? 1 : 0).data;
}

/** A VisibleObject Ptr of a fixture's player, as player.getTarget() returns it */
runtime::Ptr<model::gameobjects::VisibleObject> asObject(const PlayerFixture& fixture) {
	return runtime::Ptr<model::gameobjects::VisibleObject>(*fixture.player);
}

class TargetSelectTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		actor = makePlayer(100001, 9001, "Actor");
		other = makePlayer(100002, 9002, "Other");
		third = makePlayer(100003, 9003, "Third");
		client = std::make_unique<TestClient>();
		client->enterWorld(actor);
	}

	void TearDown() override {
		if (actor.player)
			actor.player->setTarget(nullptr);
		if (other.player)
			other.player->setTarget(nullptr);
		if (actor.player)
			actor.player->setClientConnection(nullptr);
		client.reset();
		actor = {};
		other = {};
		third = {};
		InWorldPacketTest::TearDown();
	}

	/** Reads and runs one CM_TARGET_SELECT, after dropping everything the setup queued */
	void select(int32_t targetObjectId, bool selectTargetOfTarget) {
		(*client)->clearSent();
		Driver<CM_TARGET_SELECT> packet(OPCODE);
		packet.readAndRun(body(targetObjectId, selectTargetOfTarget), client->get());
	}

	/** Makes the object known to the actor; hidden objects end up known but not seen (Creature.canSee: visual state above the see state) */
	void makeKnown(model::gameobjects::VisibleObject& object, bool seen = true) {
		if (!seen)
			static_cast<model::gameobjects::Creature&>(object).setVisualState(CreatureVisualState::HIDE1);
		const uint64_t notifiesFailedBefore = knownSeeNotifiesFailed();
		ASSERT_TRUE(actor.knownList().addForTest(object));
		ASSERT_EQ(actor.knownList().sees(object), seen);
		ASSERT_TRUE(actor.knownList().knows(object));
		// what this fixture does and does not do, pinned rather than left to a log line: the know and see sets are real, the see notification
		// (SM_PLAYER_INFO) needs a database and is swallowed (see InWorldPacketRunSupport.h). CM_TARGET_SELECT reads only the two sets.
		EXPECT_EQ(knownSeeNotifiesFailed(), notifiesFailedBefore + (seen ? 1 : 0))
			<< "a seen object's see notification is expected to fail here, and an unseen object's must not be notified at all";
	}

	PlayerFixture actor, other, third;
	std::unique_ptr<TestClient> client;
};

TEST_F(TargetSelectTest, TargetObjectIdZeroUnselects) {
	makeKnown(*other.player);
	actor.player->setTarget(asObject(other));
	ASSERT_EQ(actor.player->getTarget(), asObject(other));

	select(0, false);

	EXPECT_FALSE(actor.player->getTarget());
	// PlayerController.onTargetChanged: SM_TARGET_SELECTED(null) to the owner, then SM_TARGET_UPDATE to the sighted players (not to self)
	ASSERT_FALSE((*client)->sentBytes().empty());
	EXPECT_EQ((*client)->sentBytes().at(0), serialized(SM_TARGET_SELECTED(nullptr)));
}

TEST_F(TargetSelectTest, TargetObjectIdOfTheSelfSelectsTheSelfWithoutTheKnownList) {
	// Java: `else if (targetObjectId == player.getObjectId()) newTarget = player;` - a player is never in his own known list
	EXPECT_FALSE(actor.knownList().knows(*actor.player));

	select(actor.player->getObjectId(), false);

	EXPECT_EQ(actor.player->getTarget(), asObject(actor));
	ASSERT_FALSE((*client)->sentBytes().empty());
	EXPECT_EQ((*client)->sentBytes().at(0), serialized(SM_TARGET_SELECTED(asObject(actor))));
}

TEST_F(TargetSelectTest, AVisibleKnownObjectBecomesTheTarget) {
	makeKnown(*other.player);

	select(other.player->getObjectId(), false);

	EXPECT_EQ(actor.player->getTarget(), asObject(other));
	ASSERT_FALSE((*client)->sentBytes().empty());
	// the packet carries the target's level and HP/MP, so the client's target frame is filled from the server's state
	EXPECT_EQ((*client)->sentBytes().at(0), serialized(SM_TARGET_SELECTED(asObject(other))));
}

TEST_F(TargetSelectTest, AnUnknownObjectIdSelectsNothingAndIsNotAudited) {
	configs::main::PunishmentConfig::PUNISHMENT_ENABLE.store(false);
	configs::main::LoggingConfig::LOG_AUDIT.store(true);
	LogCapture audit({"AUDIT_LOG"});
	// Java: getObject returns null, the team arm needs isInTeam() and the radar hack arm needs newTarget != null, so newTarget stays null
	EXPECT_FALSE(actor.knownList().knows(*third.player));
	EXPECT_FALSE(actor.player->isInTeam());

	select(third.player->getObjectId(), false);

	EXPECT_FALSE(actor.player->getTarget());
	EXPECT_FALSE(audit.contains("radar hack")) << audit.dump();
	configs::main::LoggingConfig::LOG_AUDIT.store(false);
}

TEST_F(TargetSelectTest, AKnownButInvisibleObjectIsRefusedAndAudited) {
	configs::main::PunishmentConfig::PUNISHMENT_ENABLE.store(false);
	configs::main::LoggingConfig::LOG_AUDIT.store(true);
	LogCapture audit({"AUDIT_LOG"});
	makeKnown(*other.player, false);

	select(other.player->getObjectId(), false);

	EXPECT_FALSE(actor.player->getTarget());
	// Java: AuditLogger.log(player, "possibly used radar hack: trying to target invisible " + newTarget), with VisibleObject.toString()
	EXPECT_TRUE(audit.contains("possibly used radar hack: trying to target invisible ")) << audit.dump();
	EXPECT_TRUE(audit.contains("invisible Player [id=100002, name=Other]")) << audit.dump() << "the refused object names itself in the audit line";
	configs::main::LoggingConfig::LOG_AUDIT.store(false);
}

TEST_F(TargetSelectTest, SelectTargetOfTargetWithoutATargetAnswersThisIsAssistkey) {
	ASSERT_FALSE(actor.player->getTarget());

	select(0, true);

	EXPECT_FALSE(actor.player->getTarget());
	EXPECT_EQ((*client)->sentBytes(), exactly({serialized(SM_SYSTEM_MESSAGE::STR_ASSISTKEY_THIS_IS_ASSISTKEY())}));
}

TEST_F(TargetSelectTest, SelectTargetOfTargetWhoseTargetIsNullAnswersNoUserAndKeepsTheTarget) {
	makeKnown(*other.player);
	actor.player->setTarget(asObject(other));
	ASSERT_FALSE(other.player->getTarget());

	select(0, true);

	EXPECT_EQ(actor.player->getTarget(), asObject(other)) << "Java returns before setTarget";
	EXPECT_EQ((*client)->sentBytes(), exactly({serialized(SM_SYSTEM_MESSAGE::STR_ASSISTKEY_NO_USER())}));
}

TEST_F(TargetSelectTest, SelectTargetOfTargetTakesTheVisibleTargetOfTheTarget) {
	makeKnown(*other.player);
	makeKnown(*third.player);
	actor.player->setTarget(asObject(other));
	other.player->setTarget(asObject(third));

	select(0, true);

	EXPECT_EQ(actor.player->getTarget(), asObject(third));
	ASSERT_FALSE((*client)->sentBytes().empty());
	EXPECT_EQ((*client)->sentBytes().at(0), serialized(SM_TARGET_SELECTED(asObject(third))));
}

TEST_F(TargetSelectTest, SelectTargetOfTargetTakesTheSelfWithoutTheSeesCheck) {
	makeKnown(*other.player);
	actor.player->setTarget(asObject(other));
	other.player->setTarget(asObject(actor));
	// Java: `!newTarget.equals(player) && !sees(newTarget)` - the actor is not in his own known list, so only equals() saves this case
	ASSERT_FALSE(actor.knownList().sees(*actor.player));

	select(0, true);

	EXPECT_EQ(actor.player->getTarget(), asObject(actor));
}

TEST_F(TargetSelectTest, SelectTargetOfTargetOfAKnownInvisibleObjectAnswersNoUser) {
	makeKnown(*other.player);
	makeKnown(*third.player, false); // known, not seen
	actor.player->setTarget(asObject(other));
	other.player->setTarget(asObject(third));

	select(0, true);

	EXPECT_EQ(actor.player->getTarget(), asObject(other));
	// Java: knows(newTarget) ? STR_ASSISTKEY_NO_USER : STR_ASSISTKEY_TOO_FAR
	EXPECT_EQ((*client)->sentBytes(), exactly({serialized(SM_SYSTEM_MESSAGE::STR_ASSISTKEY_NO_USER())}));
}

TEST_F(TargetSelectTest, SelectTargetOfTargetOfAnUnknownObjectAnswersTooFar) {
	makeKnown(*other.player);
	actor.player->setTarget(asObject(other));
	other.player->setTarget(asObject(third));
	ASSERT_FALSE(actor.knownList().knows(*third.player));

	select(0, true);

	EXPECT_EQ(actor.player->getTarget(), asObject(other));
	EXPECT_EQ((*client)->sentBytes(), exactly({serialized(SM_SYSTEM_MESSAGE::STR_ASSISTKEY_TOO_FAR())}));
}

TEST_F(TargetSelectTest, ReadImplConsumesTheBodyExactly) {
	// the select-target-of-target flag is `readC() == 1`, so any other byte is false
	for (int32_t flag : {0, 1, 2, 255}) {
		Driver<CM_TARGET_SELECT> packet(OPCODE);
		std::vector<uint8_t> data = PacketWriter().D(0x01020304).C(flag).data;
		packet.setBuffer(commons::utils::ByteBuffer::wrap(data));
		LogCapture capture({"com.aionemu.commons.network.packet.BaseClientPacket"});
		ASSERT_TRUE(packet.read());
		EXPECT_EQ(packet.getRemainingBytes(), 0);
		EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
	}
	{ // one byte short: the flag is missing and logged
		Driver<CM_TARGET_SELECT> packet(OPCODE);
		std::vector<uint8_t> data = PacketWriter().D(7).data;
		packet.setBuffer(commons::utils::ByteBuffer::wrap(data));
		LogCapture capture({"com.aionemu.commons.network.packet.BaseClientPacket"});
		ASSERT_TRUE(packet.read());
		EXPECT_TRUE(capture.contains("Missing C")) << capture.dump();
	}
}

/** The flag byte decides the branch: 2 is not 1, so the packet selects object id 0 instead of the target of the target */
TEST_F(TargetSelectTest, AFlagByteOtherThanOneIsAPlainUnselect) {
	makeKnown(*other.player);
	actor.player->setTarget(asObject(other));
	other.player->setTarget(asObject(third));

	(*client)->clearSent();
	Driver<CM_TARGET_SELECT> packet(OPCODE);
	packet.readAndRun(PacketWriter().D(0).C(2).data, client->get());

	EXPECT_FALSE(actor.player->getTarget());
}

/** Selecting the same object twice is a no-op in VisibleObject.setTarget, so the second packet answers nothing at all */
TEST_F(TargetSelectTest, ReselectingTheSameTargetSendsNoPacket) {
	makeKnown(*other.player);

	select(other.player->getObjectId(), false);
	ASSERT_FALSE((*client)->sentBytes().empty());

	select(other.player->getObjectId(), false);

	EXPECT_EQ(actor.player->getTarget(), asObject(other));
	EXPECT_TRUE((*client)->sentBytes().empty()) << "VisibleObject.setTarget returns early when the target does not change";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
