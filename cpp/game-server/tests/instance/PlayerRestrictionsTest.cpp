// P5-13 restrictions/PlayerRestrictions (m5b-plan.md C-01): the decision table of canAttack, the first statement of
// PlayerController::attackTarget and therefore of every melee swing a client sends. Until M5b-1 the whole restrictions package was one fwd.h
// and the controller called the AION_UNPORTED stand-in standins::playerRestrictionsCanAttack (ControllerStandIns.cpp:94).
//
// Expectations are read off PlayerRestrictions.java:42-49 (checkFly) and :206-238 (canAttack), in the order Java writes them; several cases
// below exist only to pin that ORDER, because each guard answers with a different packet (or with none) and a reordered port would answer the
// wrong one:
//
//   isInPrison                       -> STR_MSG_ACCUSE_TARGET_IS_NOT_VALID, false      (before the spawned/dead checks)
//   !isSpawned | aboutToDie | isDead -> false, silently
//   checkFly                         -> STR_SKILL_CANT_CAST(flying) + an audit line, false
//   target is a flying Player        -> false, silently                                (before !canAttack)
//   !canAttack()                     -> STR_SKILL_CAN_NOT_ATTACK_WHILE_IN_ABNORMAL_STATE + SM_ATTACK_RESPONSE.STOP_WITHOUT_MESSAGE, false
//   target not a live Creature       -> SM_ATTACK_RESPONSE.STOP_INVALID_TARGET, false
//   transformModel.cantAttack()      -> false, silently
//   otherwise                        -> player.isEnemy(creature)
//
// Not covered here: transformModel.cantAttack() (TransformModel::apply dereferences the owner's object template, which a Player fixture without
// PLAYER_INITIAL_DATA does not have). It is named so a reader does not mistake its absence for coverage.
//
// A note on how the three terms of the second guard are asserted, because the obvious form of those tests proves nothing. `!isSpawned ||
// isAboutToDie || isDead` answers false *silently*, and so does the body's last statement `return player.isEnemy(creature)` for two ordinary
// characters - so a test whose target is an ordinary character passes with the whole guard deleted. Every one of the three cases below
// therefore makes the target an ENEMY_OF_ALL_PLAYERS first, which is the one state in which `isEnemy` answers **true** (Player.cpp:681-687):
// with the guard in place the answer is still false, and without it the answer flips to true. The `sent().empty()` half of each case cannot
// fail that way (the fall-through sends nothing either) and is a shape assertion, not the guard's.

#include "../cm_ak/InWorldPacketRunSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/model/ActionState.h"
#include "aion/gameserver/model/ActionStateInfo.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/model/templates/flypath/FlightPath_Type.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::gameobjects::player::CustomPlayerState;
using model::gameobjects::state::CreatureState;
using network::test::LogCapture;
using restrictions::PlayerRestrictions;
using serverpackets::SM_ATTACK_RESPONSE;
using serverpackets::SM_SYSTEM_MESSAGE;

const char* AUDIT_LOGGER = "AUDIT_LOG"; // AuditLogger.cpp:22

/** VisibleObjectController with a public constructor (the base one is protected), as tests/world's RecordingController has it */
class PlainController final : public controllers::VisibleObjectController {
public:
	PlainController() = default;
};

/** A visible object that is not a Creature, so `target instanceof Creature` is false (the shape of CheckOutputTest's NotifyingObject) */
class PlainObject final : public model::gameobjects::VisibleObject {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<PlainObject> create(int32_t objectId) { return VisibleObject::create<PlainObject>(objectId); }

	PlainObject(CreateKey key, int32_t objectId)
		: VisibleObject(key, objectId, std::make_unique<PlainController>(), nullptr, nullptr, world::WorldPosition::create(210010000), false) {}

	std::string getName() override { return "PlainObject" + std::to_string(getObjectId()); }

protected:
	~PlainObject() override = default;

	void postConstruct() override {
		VisibleObject::postConstruct();
		getController().setOwner(*this);
		setKnownlist(std::make_unique<world::knownlist::KnownList>(*this));
	}
};

/** AuditLogger only writes its line with gameserver.log.audit on, and must not reach AutoBan (PunishmentConfig off) */
class AuditScope {
public:
	AuditScope() {
		configs::main::PunishmentConfig::PUNISHMENT_ENABLE.store(false);
		configs::main::LoggingConfig::LOG_AUDIT.store(true);
	}
	~AuditScope() { configs::main::LoggingConfig::LOG_AUDIT.store(false); }
	AuditScope(const AuditScope&) = delete;
	AuditScope& operator=(const AuditScope&) = delete;
};

class PlayerRestrictionsTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		actor = makePlayer(200001, 9101, "Actor");
		other = makePlayer(200002, 9102, "Other");
		// Java: a character that is in the world. The fixture builds the Player without the World, so the spawned flag is set by hand.
		actor.player->getPosition()->setIsSpawned(true);
		other.player->getPosition()->setIsSpawned(true);
		client = std::make_unique<TestClient>();
		client->enterWorld(actor);
		(*client)->clearSent();
	}

	void TearDown() override {
		if (actor.player) {
			actor.player->setTarget(nullptr);
			actor.player->setFlightPath(nullptr);
			actor.player->setClientConnection(nullptr);
		}
		client.reset();
		actor = {};
		other = {};
		InWorldPacketTest::TearDown();
	}

	/** Java: the player is on a flight transporter or a windstream (Player.cpp:647-649 reads both the path and the FLYING state) */
	void putOnFlightPath(PlayerFixture& fixture) {
		fixture.player->setFlightPath(
			model::templates::flypath::FlightPath::create(model::templates::flypath::FlightPath::Type::FLIGHT_TRANSPORTER, 7, 0));
		fixture.player->setState(CreatureState::FLYING);
		ASSERT_TRUE(fixture.player->isUsingFlightTransporterOrWindstream());
	}

	std::vector<std::vector<uint8_t>> sent() { return (*client)->sentBytes(); }

	PlayerFixture actor, other;
	std::unique_ptr<TestClient> client;
};

TEST_F(PlayerRestrictionsTest, AnUnspawnedPlayerIsRefusedWithoutAPacket) {
	// the target is one every later check permits, so that deleting the guard would answer true instead of false (see the file header)
	other.player->setCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS);
	ASSERT_TRUE(PlayerRestrictions::canAttack(*actor.player, *other.player)) << "the fixture must permit the attack before the guard is armed";
	(*client)->clearSent();
	actor.player->getPosition()->setIsSpawned(false);
	// both counters are process wide, and the other suites of this executable reach partials of their own (PvpMapService::init)
	runtime::resetUnportedHitsForTests();
	runtime::resetPartialHitsForTests();

	EXPECT_FALSE(PlayerRestrictions::canAttack(*actor.player, *other.player)) << "PlayerRestrictions.java:212, the !isSpawned term";
	EXPECT_TRUE(sent().empty()) << "PlayerRestrictions.java:212-213 answers false and says nothing";
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
	EXPECT_EQ(runtime::partialHitCount(), 0u) << "canAttack must not be the canUseSkill partial";
}

TEST_F(PlayerRestrictionsTest, APlayerWithAKillingBlowPendingIsRefusedWithoutAPacket) {
	other.player->setCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS);
	ASSERT_TRUE(PlayerRestrictions::canAttack(*actor.player, *other.player));
	(*client)->clearSent();
	// Java: getLifeStats().isAboutToDie(), i.e. `killingBlow != 0` (CreatureLifeStats.java, CreatureLifeStats.cpp:58-60). The server sets it
	// from reduceHp for a long-animation skill that will kill; setKillingBlow is public, so the state is arranged without the damage path.
	actor.player->getLifeStats()->setKillingBlow(7);
	ASSERT_TRUE(actor.player->getLifeStats()->isAboutToDie());
	ASSERT_FALSE(actor.player->isDead()) << "about to die is not dead: the two terms must be told apart";
	ASSERT_TRUE(actor.player->isSpawned());

	EXPECT_FALSE(PlayerRestrictions::canAttack(*actor.player, *other.player)) << "PlayerRestrictions.java:212, the isAboutToDie term";
	EXPECT_TRUE(sent().empty());
}

TEST_F(PlayerRestrictionsTest, APrisonerIsRefusedWithTheAccuseMessageBeforeEveryOtherCheck) {
	actor.player->setPrisonEndTimeMillis(commons::utils::currentTimeMillis() + 600000);
	ASSERT_TRUE(actor.player->isInPrison());
	// also unspawned: the prison message must still be the answer, which is what pins its position as the FIRST statement
	actor.player->getPosition()->setIsSpawned(false);

	EXPECT_FALSE(PlayerRestrictions::canAttack(*actor.player, *other.player));
	EXPECT_EQ(sent(), exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_ACCUSE_TARGET_IS_NOT_VALID(), client->con())}));
}

TEST_F(PlayerRestrictionsTest, ADeadPlayerIsRefusedWithoutAPacket) {
	other.player->setCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS);
	ASSERT_TRUE(PlayerRestrictions::canAttack(*actor.player, *other.player));
	actor.player->setLifeStats(std::make_unique<DeadPlayerLifeStats>(*actor.player));
	ASSERT_TRUE(actor.player->isDead());
	ASSERT_FALSE(actor.player->getLifeStats()->isAboutToDie()) << "dead is not about to die: the two terms must be told apart";
	(*client)->clearSent(); // the HP write broadcasts nothing to the empty known list, but the fixture's own packets must not leak in

	EXPECT_FALSE(PlayerRestrictions::canAttack(*actor.player, *other.player)) << "PlayerRestrictions.java:212, the isDead term";
	EXPECT_TRUE(sent().empty());
}

TEST_F(PlayerRestrictionsTest, APlayerOnAFlightPathIsRefusedWithTheCantCastMessageAndAnAuditLine) {
	putOnFlightPath(actor);
	actor.player->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*other.player)); // PlayerController::onTargetChanged sends its own
	(*client)->clearSent();
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});

	EXPECT_FALSE(PlayerRestrictions::canAttack(*actor.player, *other.player));
	// checkFly sends STR_SKILL_CANT_CAST(ActionState.PATH_FLYING.getL10n()); the l10n id is the one ActionStateInfo.h carries for PATH_FLYING
	EXPECT_EQ(sent(),
		exactly({serialized(SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST(utils::ChatUtil::l10n(getL10nId(model::ActionState::PATH_FLYING))),
			client->con())}));
	EXPECT_TRUE(audit.contains("tried to attack")) << audit.dump();
	EXPECT_TRUE(audit.contains(other.player->toString())) << "Java logs the current target, not the argument: " << audit.dump();
	EXPECT_TRUE(audit.contains("FLIGHT_TRANSPORTER")) << "the flight path's type ends the message: " << audit.dump();
}

TEST_F(PlayerRestrictionsTest, TheAuditLineOfAFlyingPlayerWithoutATargetSaysNull) {
	putOnFlightPath(actor);
	ASSERT_FALSE(actor.player->getTarget());
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});

	EXPECT_FALSE(PlayerRestrictions::canAttack(*actor.player, *other.player));
	// Java string concatenation of a null reference is "null" (String.valueOf), not the attack target
	EXPECT_TRUE(audit.contains("tried to attack null while using")) << audit.dump();
}

TEST_F(PlayerRestrictionsTest, AFlyingTargetIsRefusedWithoutAPacket) {
	// the target must be one canAttack would otherwise permit, or every later guard would refuse him anyway and this case would prove nothing
	other.player->setCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS);
	ASSERT_TRUE(PlayerRestrictions::canAttack(*actor.player, *other.player));
	putOnFlightPath(other);

	EXPECT_FALSE(PlayerRestrictions::canAttack(*actor.player, *other.player));
	EXPECT_TRUE(sent().empty()) << "PlayerRestrictions.java:218-219 answers false and says nothing";
}

TEST_F(PlayerRestrictionsTest, APlayerWhoCannotAttackGetsTheAbnormalMessageAndAStopResponse) {
	actor.player->setState(CreatureState::RESTING); // Creature::canAttack refuses while RESTING (Creature.cpp:178-181)
	ASSERT_FALSE(actor.player->canAttack());
	const int32_t attackCounter = actor.player->getGameStats()->getAttackCounter();

	EXPECT_FALSE(PlayerRestrictions::canAttack(*actor.player, *other.player));
	// both packets, in Java's order, and the response carries the attack counter of the caller
	EXPECT_EQ(sent(),
		exactly({serialized(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_ATTACK_WHILE_IN_ABNORMAL_STATE(), client->con()),
			serialized(SM_ATTACK_RESPONSE::STOP_WITHOUT_MESSAGE(attackCounter), client->con())}));
}

TEST_F(PlayerRestrictionsTest, ATargetThatIsNoCreatureIsAnInvalidTarget) {
	runtime::Ref<PlainObject> object = PlainObject::create(200003);

	EXPECT_FALSE(PlayerRestrictions::canAttack(*actor.player, *object));
	EXPECT_EQ(sent(),
		exactly({serialized(SM_ATTACK_RESPONSE::STOP_INVALID_TARGET(actor.player->getGameStats()->getAttackCounter()), client->con())}));
}

TEST_F(PlayerRestrictionsTest, ADeadTargetIsAnInvalidTarget) {
	other.player->setLifeStats(std::make_unique<DeadLifeStats>(*other.player));
	ASSERT_TRUE(other.player->isDead());

	EXPECT_FALSE(PlayerRestrictions::canAttack(*actor.player, *other.player));
	EXPECT_EQ(sent(),
		exactly({serialized(SM_ATTACK_RESPONSE::STOP_INVALID_TARGET(actor.player->getGameStats()->getAttackCounter()), client->con())}));
}

TEST_F(PlayerRestrictionsTest, TheLastAnswerIsIsEnemy) {
	// a live, spawned, unrestricted pair of players that are not enemies of each other: `return player.isEnemy(creature)` is false and, unlike
	// every refusal above, sends nothing
	EXPECT_FALSE(PlayerRestrictions::canAttack(*actor.player, *other.player));
	EXPECT_TRUE(sent().empty()) << "the isEnemy answer carries no packet of its own";

	// Player::isEnemyFrom(Player) answers true for an enemy of all players (Player.cpp:681-687), so the same call now permits the attack
	other.player->setCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS);
	ASSERT_TRUE(actor.player->isEnemy(*other.player));

	EXPECT_TRUE(PlayerRestrictions::canAttack(*actor.player, *other.player));
	EXPECT_TRUE(sent().empty());
}

TEST_F(PlayerRestrictionsTest, CanUseSkillIsThePartialOfM5b1) {
	xml::LoadContext context;
	dataholders::DataManager::SKILL_DATA.resetForTests(); // the fixture published an empty holder in SetUp
	dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(context, R"(<skill_data>)"
		R"(<skill_template skill_id="1" name="active" nameId="1" skilltype="PHYSICAL" skillsubtype="ATTACK" activation="ACTIVE" duration="1500" stack="A1"/>)"
		R"(</skill_data>)"));
	actor.player->setSkillList(model::skill::PlayerSkillList::create());
	runtime::Ref<skillengine::model::Skill> skill =
		skillengine::model::Skill::create(dataholders::DataManager::SKILL_DATA->getSkillTemplate(1), *actor.player, nullptr, 1);
	runtime::resetPartialHitsForTests();
	runtime::resetUnportedHitsForTests();

	// m5b-plan.md C-01: the body needs the skill engine (M5b-2). It must return, not throw, and it answers the conservative half; Java's own
	// answer for this state would be true, which is why the divergence carries a docs/deviations/P5-13.md row.
	EXPECT_FALSE(PlayerRestrictions::canUseSkill(*actor.player, *skill));
	EXPECT_EQ(runtime::partialHitCount(), 1u);
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "an AION_PARTIAL returns; it must not reach an AION_UNPORTED";
	EXPECT_TRUE(sent().empty()) << "the partial sends none of the Java messages";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
