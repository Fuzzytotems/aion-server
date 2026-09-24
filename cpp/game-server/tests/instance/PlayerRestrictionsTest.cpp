// P5-13 restrictions/PlayerRestrictions (m5b-plan.md C-01): the decision table of canAttack, the first statement of
// PlayerController::attackTarget and therefore of every melee swing a client sends. Until M5b-1 the whole restrictions package was one fwd.h
// and the controller called the AION_UNPORTED stand-in standins::playerRestrictionsCanAttack (ControllerStandIns.cpp:94).
// The second half of the file is canUseSkill's table (m5b2-plan.md P-01), the restriction step of PlayerController::useSkill and so of every
// CM_CASTSPELL; its own header comment is above CanUseSkillTest.
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
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/PanelSkillsData.bind.h"
#include "aion/gameserver/dataholders/PanelSkillsData.h"
#include "aion/gameserver/model/ActionState.h"
#include "aion/gameserver/model/ActionStateInfo.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/PrivateStore.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/model/templates/flypath/FlightPath_Type.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/model/TransformType.h"
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
using skillengine::effect::AbnormalState;

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

// ------------------------------------------------------------------------------------------------ canUseSkill (m5b2-plan.md P-01, P-05)
//
// PlayerRestrictions.java:51-116 in the order Java writes it. Until M5b-2 the body was an AION_PARTIAL that refused every skill (M5b-1 C-01); the
// case that pinned that partial is gone with it. As for canAttack, several cases exist only to pin the ORDER, because the guards answer with
// different packets or with none:
//
//   isInPrison                               -> STR_MSG_ACCUSE_TARGET_IS_NOT_VALID, false
//   checkFly | aboutToDie | isDead           -> checkFly's STR_SKILL_CANT_CAST(flying) + audit line, or false silently
//   getStore() != null                       -> STR_SKILL_CANT_CAST(personal shop), false
//   casting a skill that is no item cast     -> false, silently
//   !canAttack() && !hasEvadeEffect          -> STR_SKILL_CAN_NOT_ATTACK_WHILE_IN_ABNORMAL_STATE, false
//   MAGICAL && SILENCE && !hasEvadeEffect    -> STR_SKILL_CANT_CAST_MAGIC_SKILL_WHILE_SILENCED, false
//   PHYSICAL && BIND                         -> STR_SKILL_CANT_CAST_PHYSICAL_SKILL_IN_FEAR, false             (no evade exemption)
//   isSkillDisabled                          -> STR_SKILL_NOT_READY (sent by Player::isSkillDisabled), false
//   transformed: cantUseSkills               -> STR_SKILL_CAN_NOT_CAST_IN_SHAPECHANGE, false
//   transformed FORM1: skill not on panel    -> audit line, false
//   hasResurrectEffect: target no dead Player -> STR_SKILL_TARGET_IS_NOT_VALID, false
//   otherwise                                -> true
//
// Unlike canAttack's, this body's last answer is `true`, so every refusal that says nothing is asserted by its return value flipping, not by a
// silence the fall-through would share. The single-guard cases pin what each guard answers; the ORDER of the whole table - which answer a
// player with several restrictions at once sees - is pinned by EveryGuardIsAskedBeforeEveryGuardBehindIt, which arms the guards from the last
// to the first.
//
// The SILENCE and BIND arms put the player into the state through EffectController::setAbnormal (EffectController.java:706-718), which
// M5b-2 part 2 ported (m5b2-plan.md K-02): the bit is what isAbnormalSet reads, with no Effect behind it.
//
// NOT COVERED, and named so that nobody mistakes the absence for coverage:
// - the multicast exception of Player::isSkillDisabled (a ChainCondition with allowedActivations > 1). It is the Player's body, not this one,
//   and it reads the caster's ChainSkills, whose cases are tests/skills/P5-02a's.

constexpr int32_t PHYSICAL_SKILL = 10;
constexpr int32_t MAGICAL_SKILL = 11;
constexpr int32_t EVADE_SKILL = 12;
constexpr int32_t RESURRECT_SKILL = 13;
constexpr int32_t PANEL_SKILL = 14;
constexpr int32_t PHYSICAL_EVADE_SKILL = 15;
constexpr int32_t PANEL_ID = 7;
constexpr int32_t TRANSFORM_MODEL_ID = 202650; // any model id other than the player's own template id (100000 + race * 2 + gender)

std::string skillTemplateXml(int32_t skillId, std::string_view skillType, std::string_view children = {}) {
	return "<skill_template skill_id=\"" + std::to_string(skillId) + "\" name=\"s" + std::to_string(skillId) + "\" nameId=\"1\" skilltype=\"" +
		std::string(skillType) + R"(" skillsubtype="ATTACK" activation="ACTIVE" duration="0" stack="S)" + std::to_string(skillId) + "\">" +
		std::string(children) + "</skill_template>";
}

/**
 * The six templates of these cases. EVADE_SKILL is MAGICAL, so it is the skill for which hasEvadeEffect decides the canAttack and SILENCE arms;
 * PHYSICAL_EVADE_SKILL is the same effect on a PHYSICAL skill, for the BIND arm, which has no evade exemption.
 */
std::string canUseSkillData() {
	return "<skill_data>" + skillTemplateXml(PHYSICAL_SKILL, "PHYSICAL") + skillTemplateXml(MAGICAL_SKILL, "MAGICAL") +
		skillTemplateXml(EVADE_SKILL, "MAGICAL", R"(<effects><evade duration2="1" e="1"/></effects>)") +
		skillTemplateXml(RESURRECT_SKILL, "MAGICAL", R"(<effects><resurrect duration2="1" e="1"/></effects>)") +
		skillTemplateXml(PANEL_SKILL, "PHYSICAL") +
		skillTemplateXml(PHYSICAL_EVADE_SKILL, "PHYSICAL", R"(<effects><evade duration2="1" e="1"/></effects>)") + "</skill_data>";
}

class CanUseSkillTest : public PlayerRestrictionsTest {
protected:
	void SetUp() override {
		PlayerRestrictionsTest::SetUp();
		xml::LoadContext context;
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the fixture published an empty holder
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(context, canUseSkillData()));
		// panel 7 holds PANEL_SKILL at level 1: SkillPanel stores `skillId << 8 | level` (SkillPanel.cpp)
		dataholders::DataManager::PANEL_SKILL_DATA.publish(xml::bindString<dataholders::PanelSkillsData>(context,
			"<polymorph_panels><panel panel_id=\"" + std::to_string(PANEL_ID) + "\" panel_skills=\"" + std::to_string(PANEL_SKILL << 8 | 1) +
				"\"/></polymorph_panels>"));
		actor.player->setSkillList(model::skill::PlayerSkillList::create());
		runtime::resetUnportedHitsForTests();
		runtime::resetPartialHitsForTests();
	}

	void TearDown() override {
		if (actor.player) {
			actor.player->setCasting(nullptr);
			actor.player->setStore(nullptr);
		}
		dataholders::DataManager::PANEL_SKILL_DATA.resetForTests();
		PlayerRestrictionsTest::TearDown();
	}

	const skillengine::model::SkillTemplate* skillTemplate(int32_t skillId) const {
		return dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId);
	}

	runtime::Ref<skillengine::model::Skill> skill(int32_t skillId) {
		return skillengine::model::Skill::create(skillTemplate(skillId), *actor.player, nullptr, 1);
	}

	/** PlayerRestrictions.canUseSkill for a fresh skill of the actor, with everything the arrangement sent dropped first */
	bool canUse(int32_t skillId) {
		runtime::Ref<skillengine::model::Skill> s = skill(skillId);
		(*client)->clearSent();
		return PlayerRestrictions::canUseSkill(*actor.player, *s);
	}

	/** Java TransformModel.apply(model, type, panelId, cantUseSkills, false, ...): the transform the polymorph and shapechange effects make */
	void transform(skillengine::model::TransformType type, int32_t panelId, bool cantUseSkills) {
		actor.player->getTransformModel().apply(TRANSFORM_MODEL_ID, type, panelId, cantUseSkills, false, false, false, false, false, false);
		ASSERT_TRUE(actor.player->getTransformModel().isActive());
	}

	void makeTarget(model::gameobjects::VisibleObject& target) {
		actor.player->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(target)); // PlayerController::onTargetChanged sends its own
	}

	/** Java EffectController.setAbnormal: the state bit the SILENCE and BIND guards read (isAbnormalSet), without an effect behind it */
	void setAbnormal(skillengine::effect::AbnormalState state) {
		actor.player->getEffectController()->setAbnormal(state);
		ASSERT_TRUE(actor.player->getEffectController()->isAbnormalSet(state));
	}

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return serialized(std::move(packet), client->con()); }

	std::vector<uint8_t> cantCast(model::ActionState state) {
		return message(SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST(utils::ChatUtil::l10n(getL10nId(state))));
	}
};

TEST_F(CanUseSkillTest, AnUnrestrictedCasterMayUseAnySkill) {
	EXPECT_TRUE(canUse(PHYSICAL_SKILL));
	EXPECT_TRUE(canUse(MAGICAL_SKILL));
	EXPECT_TRUE(canUse(EVADE_SKILL));
	EXPECT_TRUE(sent().empty()) << "PlayerRestrictions.java:115 answers true and says nothing";
	EXPECT_EQ(runtime::partialHitCount(), 0u) << "the AION_PARTIAL of M5b-1 C-01 must be gone";
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "every callee of the body is ported";
}

TEST_F(CanUseSkillTest, APrisonerIsRefusedWithTheAccuseMessageBeforeEveryOtherCheck) {
	actor.player->setPrisonEndTimeMillis(commons::utils::currentTimeMillis() + 600000);
	putOnFlightPath(actor); // checkFly would answer with its own message and an audit line
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});

	EXPECT_FALSE(canUse(PHYSICAL_SKILL));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_ACCUSE_TARGET_IS_NOT_VALID())})) << "PlayerRestrictions.java:52-55";
	EXPECT_FALSE(audit.contains("tried to attack")) << "checkFly comes after the prison check: " << audit.dump();
}

TEST_F(CanUseSkillTest, APlayerOnAFlightPathIsRefusedByCheckFly) {
	putOnFlightPath(actor);
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});

	EXPECT_FALSE(canUse(PHYSICAL_SKILL));
	EXPECT_EQ(sent(), exactly({cantCast(model::ActionState::PATH_FLYING)}));
	// Java's checkFly is shared with canAttack and says "attack" for a skill too (PlayerRestrictions.java:45)
	EXPECT_TRUE(audit.contains("tried to attack null while using FLIGHT_TRANSPORTER")) << audit.dump();
}

TEST_F(CanUseSkillTest, CheckFlyIsAskedBeforeTheDeathTerms) {
	// `!checkFly(player) || isAboutToDie() || isDead()`: a dead player on a flight path still gets checkFly's message, because it is the first
	// operand of the short circuit (PlayerRestrictions.java:60)
	actor.player->setLifeStats(std::make_unique<DeadPlayerLifeStats>(*actor.player));
	putOnFlightPath(actor);

	EXPECT_FALSE(canUse(PHYSICAL_SKILL));
	EXPECT_EQ(sent(), exactly({cantCast(model::ActionState::PATH_FLYING)}));
}

TEST_F(CanUseSkillTest, APlayerWithAKillingBlowPendingIsRefusedSilently) {
	actor.player->getLifeStats()->setKillingBlow(7);
	ASSERT_TRUE(actor.player->getLifeStats()->isAboutToDie());
	ASSERT_FALSE(actor.player->isDead()) << "about to die is not dead: the two terms must be told apart";

	EXPECT_FALSE(canUse(PHYSICAL_SKILL)) << "PlayerRestrictions.java:60, the isAboutToDie term";
	EXPECT_TRUE(sent().empty());
}

TEST_F(CanUseSkillTest, ADeadPlayerIsRefusedSilently) {
	actor.player->setLifeStats(std::make_unique<DeadPlayerLifeStats>(*actor.player));
	ASSERT_TRUE(actor.player->isDead());
	ASSERT_FALSE(actor.player->getLifeStats()->isAboutToDie()) << "dead is not about to die: the two terms must be told apart";

	EXPECT_FALSE(canUse(PHYSICAL_SKILL)) << "PlayerRestrictions.java:60, the isDead term";
	EXPECT_TRUE(sent().empty());
}

TEST_F(CanUseSkillTest, APrivateStoreOwnerIsRefusedWithThePersonalShopMessage) {
	actor.player->setStore(std::make_unique<model::gameobjects::player::PrivateStore>(*actor.player));
	// and casting: the store is asked first, so its message is the answer (a cast alone refuses silently, see below)
	actor.player->setCasting(skill(MAGICAL_SKILL));

	EXPECT_FALSE(canUse(PHYSICAL_SKILL));
	EXPECT_EQ(sent(), exactly({cantCast(model::ActionState::PERSONAL_SHOP)})) << "PlayerRestrictions.java:64-67";
}

TEST_F(CanUseSkillTest, ASkillCastInProgressRefusesEveryOtherSkillSilently) {
	actor.player->setCasting(skill(MAGICAL_SKILL));
	ASSERT_FALSE(actor.player->canAttack()) << "a caster cannot attack (Creature.java canAttack), which the next guard would answer";

	// Without the cast guard a PHYSICAL skill would reach `!canAttack()` and be answered with the abnormal-state message, and the evade skill
	// would pass that guard and be allowed: the guard is what makes both a silent no (PlayerRestrictions.java:69-70)
	EXPECT_FALSE(canUse(PHYSICAL_SKILL));
	EXPECT_FALSE(canUse(EVADE_SKILL));
	EXPECT_TRUE(sent().empty());
}

TEST_F(CanUseSkillTest, AnItemCastInProgressIsNoReasonToRefuse) {
	// Java: "item casts are interruptible (PlayerController cancels them), skill casts are not" - an item cast passes the cast guard ...
	xml::LoadContext context;
	std::unique_ptr<model::templates::item::ItemTemplate> scroll =
		xml::bindString<model::templates::item::ItemTemplate>(context, R"(<item_template id="164000001"/>)");
	actor.player->setCasting(skillengine::model::Skill::create(skillTemplate(MAGICAL_SKILL), *actor.player, 1, nullptr, scroll.get()));
	ASSERT_TRUE(actor.player->isCastingItemSkill());

	EXPECT_TRUE(canUse(EVADE_SKILL)) << "... and an evade skill is not stopped by the casting player's canAttack() either";
	EXPECT_TRUE(sent().empty());
	// ... while any other skill still meets `!canAttack()`, because casting (even an item) means cannot attack
	EXPECT_FALSE(canUse(PHYSICAL_SKILL));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_ATTACK_WHILE_IN_ABNORMAL_STATE())}));
	actor.player->setCasting(nullptr); // before the item template goes
}

TEST_F(CanUseSkillTest, APlayerWhoCannotAttackIsRefusedWithTheAbnormalStateMessage) {
	actor.player->setState(CreatureState::RESTING); // Creature::canAttack refuses while RESTING
	ASSERT_FALSE(actor.player->canAttack());

	EXPECT_FALSE(canUse(PHYSICAL_SKILL));
	EXPECT_FALSE(canUse(MAGICAL_SKILL));
	// one message per call, and unlike canAttack's arm no SM_ATTACK_RESPONSE (PlayerRestrictions.java:72-75)
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_ATTACK_WHILE_IN_ABNORMAL_STATE())}));
}

TEST_F(CanUseSkillTest, AnEvadeSkillMayBeUsedByAPlayerWhoCannotAttack) {
	actor.player->setState(CreatureState::RESTING);
	ASSERT_FALSE(actor.player->canAttack());
	ASSERT_TRUE(skillTemplate(EVADE_SKILL)->hasEvadeEffect());

	EXPECT_TRUE(canUse(EVADE_SKILL)) << "`!player.canAttack() && !template.hasEvadeEffect()`: remove shock is the way out of a stun";
	EXPECT_TRUE(sent().empty());
}

TEST_F(CanUseSkillTest, ASilencedPlayerIsRefusedAMagicalSkillUnlessItHasAnEvadeEffect) {
	setAbnormal(AbnormalState::SILENCE);
	ASSERT_TRUE(actor.player->canAttack()) << "SILENCE is no CANT_ATTACK_STATE bit: the canAttack guard lets the player through";

	// PlayerRestrictions.java:76-80, "in 3.0 players can use remove shock even when silenced"
	EXPECT_FALSE(canUse(MAGICAL_SKILL));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST_MAGIC_SKILL_WHILE_SILENCED())}));
	EXPECT_TRUE(canUse(EVADE_SKILL)) << "a MAGICAL skill with an evade effect";
	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(canUse(PHYSICAL_SKILL)) << "SILENCE stops MAGICAL skills only";
	EXPECT_TRUE(sent().empty());
}

TEST_F(CanUseSkillTest, ABoundPlayerIsRefusedEveryPhysicalSkill) {
	setAbnormal(AbnormalState::BIND);
	ASSERT_TRUE(actor.player->canAttack()) << "BIND is no CANT_ATTACK_STATE bit either";

	// PlayerRestrictions.java:82-85: the message speaks of fear, the state is BIND - and unlike SILENCE's, the guard has no evade exemption
	EXPECT_FALSE(canUse(PHYSICAL_SKILL));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST_PHYSICAL_SKILL_IN_FEAR())}));
	EXPECT_FALSE(canUse(PHYSICAL_EVADE_SKILL)) << "an evade effect does not free a PHYSICAL skill from BIND";
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST_PHYSICAL_SKILL_IN_FEAR())}));
	EXPECT_TRUE(canUse(MAGICAL_SKILL)) << "BIND stops PHYSICAL skills only";
	EXPECT_TRUE(sent().empty());
}

TEST_F(CanUseSkillTest, ASkillOnCooldownIsRefusedWithNotReady) {
	const skillengine::model::SkillTemplate* physical = skillTemplate(PHYSICAL_SKILL);
	actor.player->setSkillCoolDown(physical->getCooldownId(), commons::utils::currentTimeMillis() + 60000);

	EXPECT_FALSE(canUse(PHYSICAL_SKILL)) << "PlayerRestrictions.java:87-88";
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_READY())})) << "the message is Player.isSkillDisabled's own";
	EXPECT_TRUE(canUse(MAGICAL_SKILL)) << "a cooldown is per cooldown id";

	actor.player->setSkillCoolDown(physical->getCooldownId(), commons::utils::currentTimeMillis() - 1);
	EXPECT_TRUE(canUse(PHYSICAL_SKILL)) << "an expired cooldown no longer disables the skill";
}

TEST_F(CanUseSkillTest, ACooldownIsAskedBeforeTheTransform) {
	actor.player->setSkillCoolDown(skillTemplate(PHYSICAL_SKILL)->getCooldownId(), commons::utils::currentTimeMillis() + 60000);
	transform(skillengine::model::TransformType::AVATAR, 0, true);

	EXPECT_FALSE(canUse(PHYSICAL_SKILL));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_READY())}));
}

TEST_F(CanUseSkillTest, ATransformThatForbidsSkillsRefusesWithTheShapechangeMessage) {
	transform(skillengine::model::TransformType::AVATAR, 0, true);

	EXPECT_FALSE(canUse(PHYSICAL_SKILL));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_CAST_IN_SHAPECHANGE())})) << "PlayerRestrictions.java:92-95";
}

TEST_F(CanUseSkillTest, ATransformThatAllowsSkillsChecksNoPanelUnlessItIsForm1) {
	transform(skillengine::model::TransformType::AVATAR, 99, false); // panel 99 does not exist; only FORM1 would look it up

	EXPECT_TRUE(canUse(PHYSICAL_SKILL));
	EXPECT_TRUE(sent().empty());
}

TEST_F(CanUseSkillTest, AForm1TransformAllowsOnlyTheSkillsOfItsPanel) {
	transform(skillengine::model::TransformType::FORM1, PANEL_ID, false);
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});

	EXPECT_TRUE(canUse(PANEL_SKILL)) << "SkillPanel.isSkillPresent compares `skill >> 8` with the skill id";
	EXPECT_FALSE(audit.contains("non panel skill")) << audit.dump();

	EXPECT_FALSE(canUse(PHYSICAL_SKILL)) << "PlayerRestrictions.java:97-103";
	EXPECT_TRUE(sent().empty()) << "the refusal is an audit line, not a message to the client";
	EXPECT_EQ(audit.count("tried to use non panel skill while transformed in TransformType.FORM1"), 1) << audit.dump();
}

TEST_F(CanUseSkillTest, AForm1TransformWithoutAPanelRefusesEverySkill) {
	transform(skillengine::model::TransformType::FORM1, 99, false); // no panel 99: getSkillPanel answers null
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});

	EXPECT_FALSE(canUse(PANEL_SKILL));
	EXPECT_TRUE(audit.contains("tried to use non panel skill while transformed in TransformType.FORM1")) << audit.dump();
}

TEST_F(CanUseSkillTest, AResurrectSkillWithoutATargetIsRefused) {
	ASSERT_TRUE(skillTemplate(RESURRECT_SKILL)->hasResurrectEffect());
	ASSERT_FALSE(actor.player->getTarget());

	EXPECT_FALSE(canUse(RESURRECT_SKILL)) << "`target instanceof Player` is false for null";
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID())}));
}

TEST_F(CanUseSkillTest, AResurrectSkillOnATargetThatIsNoPlayerIsRefused) {
	runtime::Ref<PlainObject> object = PlainObject::create(200003);
	makeTarget(*object);

	EXPECT_FALSE(canUse(RESURRECT_SKILL));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID())}));
	actor.player->setTarget(nullptr);
}

TEST_F(CanUseSkillTest, AResurrectSkillNeedsADeadPlayerAsItsTarget) {
	makeTarget(*other.player);
	ASSERT_FALSE(other.player->isDead());

	EXPECT_FALSE(canUse(RESURRECT_SKILL)) << "PlayerRestrictions.java:109-112: a living player cannot be resurrected";
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID())}));
	EXPECT_TRUE(canUse(PHYSICAL_SKILL)) << "a skill without a resurrect effect does not look at the target";

	other.player->setLifeStats(std::make_unique<DeadLifeStats>(*other.player));
	ASSERT_TRUE(other.player->isDead());
	EXPECT_TRUE(canUse(RESURRECT_SKILL));
	EXPECT_TRUE(sent().empty());
}

TEST_F(CanUseSkillTest, TheTransformIsAskedBeforeTheResurrectTarget) {
	transform(skillengine::model::TransformType::AVATAR, 0, true);

	EXPECT_FALSE(canUse(RESURRECT_SKILL)); // no target either, which the resurrect arm would answer with STR_SKILL_TARGET_IS_NOT_VALID
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_CAST_IN_SHAPECHANGE())}));
}

TEST_F(CanUseSkillTest, AForm1TransformThatForbidsSkillsAnswersBeforeItsPanelIsAsked) {
	// PlayerRestrictions.java:90-104: cantUseSkills is the first statement of the transform block, the FORM1 panel lookup the second - so a
	// FORM1 transform that forbids skills refuses with the shapechange message, even a skill of its own panel, and never reaches the audit line
	transform(skillengine::model::TransformType::FORM1, PANEL_ID, true);
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});

	EXPECT_FALSE(canUse(PHYSICAL_SKILL)) << "not on the panel";
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_CAST_IN_SHAPECHANGE())}));
	EXPECT_FALSE(canUse(PANEL_SKILL)) << "on the panel, which only the FORM1 arm would have allowed";
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_CAST_IN_SHAPECHANGE())}));
	EXPECT_FALSE(audit.contains("non panel skill")) << audit.dump();
}

TEST_F(CanUseSkillTest, EveryGuardIsAskedBeforeEveryGuardBehindIt) {
	// Java's check ORDER decides which message a refused player sees, and a port that asks the same questions in another order passes every
	// single-guard case above. So the guards are armed here from the LAST to the FIRST, one per step, and never disarmed: at each step every
	// guard behind the new one is armed too, and the new one answers only if it is asked before all of them (PlayerRestrictions.java:51-116).
	// A guard moved up past another one answers in that one's place at the step that arms the one it passed - moving isSkillDisabled in front
	// of the canAttack guard, or the private store in front of the death terms or the prison, fails here. The only pairs this cannot order are
	// the silent guards with no answering guard between them: the aboutToDie and isDead terms of one expression, which Java cannot tell apart
	// either; and SILENCE and BIND, of which a skill meets at most one (MAGICAL, PHYSICAL). The skill is RESURRECT_SKILL: MAGICAL, without an
	// evade effect, with the resurrect effect of the last guard and its own cooldown id. BIND answers only a PHYSICAL skill, so from the cooldown
	// step on PHYSICAL_SKILL is asked as well, with its own cooldown armed: it is the probe that pins BIND's place (:82-85) - behind the
	// canAttack guard and every guard before it, in front of isSkillDisabled - as RESURRECT_SKILL pins SILENCE's (:76-80).
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});
	other.player->setLifeStats(std::make_unique<DeadLifeStats>(*other.player));
	makeTarget(*other.player);
	ASSERT_TRUE(canUse(RESURRECT_SKILL)) << "nothing armed: a dead Player target is what the resurrect arm asks for";
	EXPECT_TRUE(sent().empty());

	// :106-113, the last guard: a resurrect skill without a target
	actor.player->setTarget(nullptr);
	EXPECT_FALSE(canUse(RESURRECT_SKILL));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID())})) << "the resurrect target";

	// :90-95, a transform that forbids skills
	transform(skillengine::model::TransformType::AVATAR, 0, true);
	EXPECT_FALSE(canUse(RESURRECT_SKILL));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_CAST_IN_SHAPECHANGE())})) << "the transform";

	// :87-88, isSkillDisabled: the skill's cooldown; Player.isSkillDisabled sends STR_SKILL_NOT_READY itself
	actor.player->setSkillCoolDown(skillTemplate(RESURRECT_SKILL)->getCooldownId(), commons::utils::currentTimeMillis() + 600000);
	actor.player->setSkillCoolDown(skillTemplate(PHYSICAL_SKILL)->getCooldownId(), commons::utils::currentTimeMillis() + 600000);
	EXPECT_FALSE(canUse(RESURRECT_SKILL));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_READY())})) << "the cooldown";
	EXPECT_FALSE(canUse(PHYSICAL_SKILL));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_READY())})) << "the probe's cooldown";

	// :82-85, BIND: the PHYSICAL probe's answer; the MAGICAL skill passes it and still meets its cooldown
	setAbnormal(AbnormalState::BIND);
	EXPECT_FALSE(canUse(PHYSICAL_SKILL));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST_PHYSICAL_SKILL_IN_FEAR())}))
		<< "BIND, and no STR_SKILL_NOT_READY: isSkillDisabled comes after";
	EXPECT_FALSE(canUse(RESURRECT_SKILL));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_READY())})) << "BIND does not stop a MAGICAL skill";

	// :76-80, SILENCE: the MAGICAL skill's answer; the probe still meets BIND
	setAbnormal(AbnormalState::SILENCE);
	EXPECT_FALSE(canUse(RESURRECT_SKILL));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST_MAGIC_SKILL_WHILE_SILENCED())})) << "SILENCE, before the cooldown";
	EXPECT_FALSE(canUse(PHYSICAL_SKILL));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST_PHYSICAL_SKILL_IN_FEAR())})) << "SILENCE does not stop a PHYSICAL skill";

	// :72-75, a player who cannot attack
	actor.player->setState(CreatureState::RESTING);
	ASSERT_FALSE(actor.player->canAttack());
	for (int32_t skillId : {RESURRECT_SKILL, PHYSICAL_SKILL}) {
		EXPECT_FALSE(canUse(skillId));
		EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_ATTACK_WHILE_IN_ABNORMAL_STATE())}))
			<< "skill " << skillId << ": the abnormal state message, and no SILENCE, BIND or STR_SKILL_NOT_READY: they come after";
	}

	// :69-70, a skill cast in progress: silent
	actor.player->setCasting(skill(MAGICAL_SKILL));
	for (int32_t skillId : {RESURRECT_SKILL, PHYSICAL_SKILL}) {
		EXPECT_FALSE(canUse(skillId));
		EXPECT_TRUE(sent().empty()) << "skill " << skillId << ": the cast in progress";
	}

	// :64-67, a private store
	actor.player->setStore(std::make_unique<model::gameobjects::player::PrivateStore>(*actor.player));
	for (int32_t skillId : {RESURRECT_SKILL, PHYSICAL_SKILL}) {
		EXPECT_FALSE(canUse(skillId));
		EXPECT_EQ(sent(), exactly({cantCast(model::ActionState::PERSONAL_SHOP)})) << "skill " << skillId << ": the store";
	}

	// :60, the death terms: silent
	actor.player->setLifeStats(std::make_unique<DeadPlayerLifeStats>(*actor.player));
	ASSERT_TRUE(actor.player->isDead());
	for (int32_t skillId : {RESURRECT_SKILL, PHYSICAL_SKILL}) {
		EXPECT_FALSE(canUse(skillId));
		EXPECT_TRUE(sent().empty()) << "skill " << skillId << ": isDead, before the store message";
	}
	actor.player->getLifeStats()->setKillingBlow(7);
	ASSERT_TRUE(actor.player->getLifeStats()->isAboutToDie());
	for (int32_t skillId : {RESURRECT_SKILL, PHYSICAL_SKILL}) {
		EXPECT_FALSE(canUse(skillId));
		EXPECT_TRUE(sent().empty()) << "skill " << skillId << ": isAboutToDie, before the store message";
	}
	EXPECT_EQ(audit.count("tried to attack"), 0) << audit.dump();

	// :60, checkFly, the first operand of the same expression
	putOnFlightPath(actor);
	for (int32_t skillId : {RESURRECT_SKILL, PHYSICAL_SKILL}) {
		EXPECT_FALSE(canUse(skillId));
		EXPECT_EQ(sent(), exactly({cantCast(model::ActionState::PATH_FLYING)})) << "skill " << skillId << ": checkFly, before isAboutToDie and isDead";
	}
	EXPECT_EQ(audit.count("tried to attack"), 2) << "one checkFly audit line per call: " << audit.dump();

	// :52-55, the prison, the first guard of all
	actor.player->setPrisonEndTimeMillis(commons::utils::currentTimeMillis() + 600000);
	ASSERT_TRUE(actor.player->isInPrison());
	for (int32_t skillId : {RESURRECT_SKILL, PHYSICAL_SKILL}) {
		EXPECT_FALSE(canUse(skillId));
		EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_ACCUSE_TARGET_IS_NOT_VALID())})) << "skill " << skillId << ": the prison";
	}
	EXPECT_EQ(audit.count("tried to attack"), 2) << "no further checkFly audit line: " << audit.dump();
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
