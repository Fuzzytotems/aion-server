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
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/PanelSkillsData.bind.h"
#include "aion/gameserver/dataholders/PanelSkillsData.h"
#include "aion/gameserver/model/ActionState.h"
#include "aion/gameserver/model/ActionStateInfo.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
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
#include "aion/gameserver/questEngine/QuestEngine.h"
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

// ============================================================================================================================ canUseItem
//
// PlayerRestrictions.canUseItem (m5b3-plan.md P-01), the restriction step of CM_USE_ITEM (CM_USE_ITEM.java:82), read off
// PlayerRestrictions.java:277-369 in Java order:
//
//   player null | !isOnline                -> false, silently
//   isInPrison                             -> STR_MSG_ACCUSE_TARGET_IS_NOT_VALID
//   isAboutToDie | isDead                  -> false, silently
//   CANT_ATTACK_STATE                      -> STR_SKILL_CAN_NOT_USE_ITEM_WHILE_IN_ABNORMAL_STATE
//   transform cantUseItems                 -> false, silently ("client sends message by itself")
//   private store                          -> STR_MSG_CANNOT_USE_ITEM_DURING_PATH_FLYING(PERSONAL_SHOP)
//   hasCooldown(item)                      -> STR_ITEM_CANT_USE_UNTIL_DELAY_TIME
//   item race                              -> STR_CANNOT_USE_ITEM_INVALID_RACE
//   no actions, not a quest item           -> STR_ITEM_IS_NOT_USABLE
//   gender                                 -> STR_CANNOT_USE_ITEM_INVALID_GENDER
//   class                                  -> STR_CANNOT_USE_ITEM_INVALID_CLASS
//   required level                         -> STR_CANNOT_USE_ITEM_TOO_LOW_LEVEL_MUST_BE_THIS_LEVEL(l10n, level)
//   max level                              -> STR_CANNOT_USE_ITEM_TOO_HIGH_LEVEL(level, l10n)
//   use area                               -> STR_SKILL_CAN_NOT_USE_ITEM_IN_CURRENT_POSITION
//   activation race: no Creature target    -> STR_ITEM_CANT_FIND_VALID_TARGET
//                    another race          -> STR_SKILL_CANT_CAST_TO_CURRENT_TARGET
//
// Every item is a row of item_templates.xml copied verbatim (line in the comment above it). The order of the guards is pinned in the last case
// the way CanUseSkillTest pins canUseSkill's, as far as one row can arm several guards; the pairs no shipped row can arm together are named
// there. The actions' place before every guard behind them is pinned by TheActionsAreAskedBeforeEveryGuardBehindThem, with rows without
// actions.
//
// NOT COVERED, and named so that nobody mistakes the absence for coverage:
// - the use area answering true: an item-use zone needs a zone template of the map (MapRegion.isInsideItemUseZone, ZoneData); the case below
//   arms the guard with an unspawned player, for whom Creature.isInsideItemUseZone answers false before any zone is asked.

/** item_templates.xml rows */
constexpr std::string_view MINOR_LIFE_POTION_XML = // :830724
	R"(<item_template id="162000002" name="Minor Life Potion" level="10" cName="remedy_hp_10a" mask="12414" max_stack_count="1000" quality="COMMON" price="250" desc="702583" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="9889"/>
		</actions>
		<uselimits usedelay="30000" usedelayid="11"/>
	</item_template>)";
constexpr std::string_view GRAVEKNIGHT_CANDY_XML = // :825798, ASMODIANS
	R"(<item_template id="160002284" name="Graveknight Candy" level="50" cName="food_d_GraveknightD" mask="12414" max_stack_count="1000" quality="RARE" price="3650" race="ASMODIANS" desc="743960" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10220"/>
		</actions>
		<uselimits usedelay="5000" usedelayid="24"/>
	</item_template>)";
constexpr std::string_view RAW_BRAX_MEAT_XML = // :744848, ASMODIANS and no actions
	R"(<item_template id="152015501" name="Raw Brax Meat" level="30" cName="food_d_material_30a" mask="12414" max_stack_count="1000" quality="COMMON" price="100" race="ASMODIANS" desc="728788"/>)";
constexpr std::string_view SPARKIE_CARAPACE_FRAGMENT_XML = // :874138, no actions
	R"(<item_template id="182004793" name="Sparkie Carapace Fragment" level="5" cName="junk_spaky_05" mask="12414" max_stack_count="1000" quality="JUNK" price="300" desc="718718"/>)";
constexpr std::string_view LESSER_ANCIENT_KINAH_XML = // :876350, no actions (registered as a quest item by one case)
	R"(<item_template id="182006985" name="Lesser Ancient Kinah" level="1" cName="junk_OwnerTree_housing_gold_01" mask="28684" max_stack_count="1000" quality="JUNK" price="42857" desc="801258"/>)";
constexpr std::string_view HOT_DENIM_DRESS_XML = // :273513, FEMALE
	R"(<item_template id="110900085" name="Hot Denim Dress" level="1" cName="cash_ms_torso_wondergirls_01" mask="37448" item_group="TORSO" quality="COMMON" price="5" desc="768562">
		<actions>
			<remodel type="2"/>
		</actions>
		<uselimits gender="FEMALE"/>
	</item_template>)";
constexpr std::string_view WORG_SOUL_XML = // :832502, assassins and rangers only
	R"(<item_template id="164000026" name="Worg Soul Level 1" level="20" cName="soulstone_shapechange_zaif_20" mask="12414" max_stack_count="10000" item_group="SOULSTONE" quality="COMMON" price="900" restrict="0 0 0 0 1 1 0 0 0 0 0 0 0 0 0 0 0" desc="701854" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="2" skillid="9935"/>
		</actions>
		<uselimits usedelay="15000" usedelayid="33"/>
	</item_template>)";
constexpr std::string_view XP_EXTRACTING_ITEM_14_XML = // :930498, level 10 exactly
	R"(<item_template id="188920013" name="PC XP Extracting Item 14" level="10" cName="world_cash_item_exp_extraction_test14" mask="12410" max_stack_count="1000" quality="RARE" price="5" restrict="10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10" restrict_max="10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10" desc="821732" activate_target="STANDALONE" activate_count="1">
		<actions>
			<expextract item_id="188052060" percent="true" cost="45"/>
		</actions>
		<uselimits usedelay="2000"/>
	</item_template>)";
constexpr std::string_view TALOC_FRUIT_XML = // :823434, usearea
	R"(<item_template id="160001286" name="Taloc Fruit" level="50" cName="food_kaspa_shapechange_light" casting_delay="1000" mask="4161" quality="LEGEND" price="5" desc="751618" activate_target="STANDALONE" activate_count="1000">
		<actions>
			<skilluse level="1" skillid="10251"/>
		</actions>
		<uselimits usedelay="1200000" usedelayid="33" usearea="IDELIM_ITEMUSE" ownership_worlds="300190000"/>
	</item_template>)";
constexpr std::string_view GREEN_CLEANSE_XML = // :833219, activation race LIVINGWATER
	R"(<item_template id="164000150" name="Green Cleanse" level="1" cName="sp_idelemental_prime_dispel_green" mask="12617" max_stack_count="1000" quality="COMMON" price="0" desc="773702" activate_target="LIVINGWATER" activate_count="1">
		<actions>
			<skilluse level="65" skillid="10352"/>
		</actions>
		<uselimits usedelay="1000"/>
	</item_template>)";
constexpr std::string_view BLESSING_OF_CONCENTRATION_XML = // :832823, ELYOS, level 50, usedelayid 17, activation race GHENCHMAN_LIGHT
	R"(<item_template id="164000081" name="Blessing of Concentration" level="50" cName="Item_test_Q_Wonkiock_skill" mask="12352" max_stack_count="10" quality="RARE" price="5" race="ELYOS" restrict="50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50" desc="753616" activate_target="GHENCHMAN_LIGHT" activate_combat="true" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10253"/>
		</actions>
		<uselimits usedelayid="17"/>
		<inventory id="2"/>
	</item_template>)";
// Rows WITHOUT actions that arm one guard behind the actions each (TheActionsAreAskedBeforeEveryGuardBehindThem)
constexpr std::string_view APPEARANCE_TEST_FEMALE_XML = // :273408, FEMALE
	R"(<item_template id="110900054" name="Test_AppearanceModificationTest_Female" level="1" cName="test_ms_torso_changeskin_04a" mask="37502" item_group="TORSO" quality="COMMON" price="5" desc="751891">
		<uselimits gender="FEMALE"/>
	</item_template>)";
constexpr std::string_view ADVANCED_DUAL_WIELDING_I_XML = // :739018, gladiators only
	R"(<item_template id="140000005" name="Advanced Dual-Wielding I" level="20" cName="stigma_fi_p_equip_dual_g1" mask="4684" item_group="STIGMA" quality="RARE" price="1000000" restrict="0 20 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" desc="727966"/>)";
constexpr std::string_view MERCENARY_SWORD_XML = // :379, restrict 3
	R"(<item_template id="100000095" name="Mercenary Sword" level="3" cName="sword_n_c_03a" mask="138366" item_group="SWORD" quality="COMMON" price="400" restrict="3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3" desc="700776" attack_type="PHYSICAL" max_enchant="10" m_slots="1">
		<weapon_stats hit_count="2" attack_range="1500" parry="191" physical_accuracy="72" critical="50" attack_speed="1400" max_damage="26" min_damage="20"/>
		<idian burn_attack="29" burn_defend="12"/>
	</item_template>)";
constexpr std::string_view LESSER_SECRET_REMEDY_OF_GROWTH_I_XML = // :930295, restrict 10 and restrict_max 19
	R"(<item_template id="188900002" name="Lesser Secret Remedy of Growth I" level="1" cName="world_cash_item_addexp_10_10" mask="12376" max_stack_count="1000" quality="RARE" price="5" restrict="10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10" restrict_max="19 19 19 19 19 19 19 19 19 19 19 19 19 19 19 19 19" desc="801728" activate_target="STANDALONE" activate_count="1">
		<uselimits usedelay="21600000" usedelayid="145"/>
	</item_template>)";
constexpr std::string_view RUSTED_VAULT_KEY_XML = // :896194, usearea
	R"(<item_template id="185000222" name="Rusted Vault Key" level="40" cName="key_IDSweep_world_event_box01" mask="28736" max_stack_count="1000" item_group="KEY" quality="RARE" price="5" desc="842381">
		<uselimits usearea="IDSWEEP_ITEMAREA" ownership_worlds="301400000"/>
	</item_template>)";
constexpr std::string_view GUARDIAN_GENERAL_BUFF_TEST_ITEM_08_XML = // :832421, activation race GCHIEF_LIGHT
	R"(<item_template id="164000008" name="Guardian General Buff Test Item 08" level="1" cName="item_test_avatar_buff_08" mask="12414" max_stack_count="1000" quality="COMMON" price="1" desc="752630" activate_target="GCHIEF_LIGHT" activate_count="1">
		<uselimits usedelay="5000" usedelayid="31"/>
	</item_template>)";

/** player_experience_table.xml:3-24, the start experience of levels 1 to 22: enough for a level-20 character (setLevel caps at size - 1) */
constexpr std::string_view PLAYER_EXPERIENCE_TABLE_TO_LEVEL_21_XML =
	"<player_experience_table><exp>0</exp><exp>400</exp><exp>1433</exp><exp>3820</exp><exp>9054</exp><exp>17655</exp><exp>30978</exp>"
	"<exp>52010</exp><exp>82982</exp><exp>126069</exp><exp>182252</exp><exp>260622</exp><exp>360825</exp><exp>490331</exp><exp>649169</exp>"
	"<exp>844378</exp><exp>1083018</exp><exp>1401356</exp><exp>1808613</exp><exp>2314771</exp><exp>2941893</exp><exp>3769257</exp>"
	"</player_experience_table>";

class CanUseItemTest : public PlayerRestrictionsTest {
protected:
	void SetUp() override {
		PlayerRestrictionsTest::SetUp();
		actor.commonData->setGender(model::Gender::MALE);
		runtime::resetUnportedHitsForTests();
		runtime::resetPartialHitsForTests();
	}

	void TearDown() override {
		if (actor.player) {
			actor.player->setStore(nullptr);
			actor.player->setTarget(nullptr);
		}
		items.clear();
		PlayerRestrictionsTest::TearDown();
		templates.clear(); // after the items that point at them
	}

	/** An item of the row (static data: the template stays with the fixture) */
	model::gameobjects::Item& item(std::string_view row) {
		xml::LoadContext context;
		templates.push_back(xml::bindString<model::templates::item::ItemTemplate>(context, row));
		items.push_back(model::gameobjects::Item::create(nextObjectId++, templates.back().get(), 1, false, 0));
		return *items.back();
	}

	/** PlayerRestrictions.canUseItem for the actor, with everything the arrangement sent dropped first */
	bool canUse(model::gameobjects::Item& usedItem) {
		(*client)->clearSent();
		return PlayerRestrictions::canUseItem(runtime::Ptr<model::gameobjects::player::Player>(*actor.player), usedItem);
	}

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return serialized(std::move(packet), client->con()); }

	/** Java TransformModel.apply with cantUseItems: the item-refusing transform of a shapechange */
	void transformCantUseItems() {
		actor.player->getTransformModel().apply(TRANSFORM_MODEL_ID, skillengine::model::TransformType::AVATAR, 0, false, false, false, false, false,
			true, false);
		ASSERT_TRUE(actor.player->getTransformModel().cantUseItems());
	}

	void setAbnormal(AbnormalState state) {
		actor.player->getEffectController()->setAbnormal(state);
		ASSERT_TRUE(actor.player->getEffectController()->isInAnyAbnormalState(AbnormalState::CANT_ATTACK_STATE));
	}

	/**
	 * Java PlayerCommonData.setLevel on a character that is not online (no level-up packets). Level 10 and above needs the ascension: a
	 * character that is no daeva stops at 9 (PlayerCommonData.java setExp: maxLevel 10, "9 for non daeva ... shown with full XP bar")
	 */
	void setLevel(int32_t level) {
		actor.commonData->setDaeva(level >= 10);
		actor.commonData->setLevel(level);
		ASSERT_EQ(actor.player->getLevel(), level);
	}

	std::vector<std::unique_ptr<model::templates::item::ItemTemplate>> templates;
	std::vector<runtime::Ref<model::gameobjects::Item>> items;
	int32_t nextObjectId = 780001;
};

TEST_F(CanUseItemTest, AnUnrestrictedPlayerMayUseAPotion) {
	EXPECT_TRUE(canUse(item(MINOR_LIFE_POTION_XML)));
	EXPECT_TRUE(sent().empty()) << "PlayerRestrictions.java:368 answers true and says nothing";
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "every callee of the body is ported";
	EXPECT_EQ(runtime::partialHitCount(), 0u);
}

TEST_F(CanUseItemTest, NoPlayerOrAnOfflineOneIsRefusedSilently) {
	model::gameobjects::Item& potion = item(MINOR_LIFE_POTION_XML);
	EXPECT_FALSE(PlayerRestrictions::canUseItem(nullptr, potion)) << "PlayerRestrictions.java:278, `player == null`";
	actor.player->setClientConnection(nullptr);
	ASSERT_FALSE(actor.player->isOnline());
	EXPECT_FALSE(canUse(potion)) << ":278, `!player.isOnline()`";
	EXPECT_TRUE(sent().empty());
}

TEST_F(CanUseItemTest, APrisonerIsRefusedWithTheAccuseMessage) {
	actor.player->setPrisonEndTimeMillis(commons::utils::currentTimeMillis() + 600000);
	EXPECT_FALSE(canUse(item(MINOR_LIFE_POTION_XML)));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_ACCUSE_TARGET_IS_NOT_VALID())})) << ":281-284";
}

TEST_F(CanUseItemTest, APlayerAboutToDieOrDeadIsRefusedSilently) {
	model::gameobjects::Item& potion = item(MINOR_LIFE_POTION_XML);
	actor.player->getLifeStats()->setKillingBlow(7);
	ASSERT_FALSE(actor.player->isDead());
	EXPECT_FALSE(canUse(potion)) << ":286, the isAboutToDie term";
	EXPECT_TRUE(sent().empty());
	actor.player->setLifeStats(std::make_unique<DeadPlayerLifeStats>(*actor.player));
	ASSERT_FALSE(actor.player->getLifeStats()->isAboutToDie());
	EXPECT_FALSE(canUse(potion)) << ":286, the isDead term";
	EXPECT_TRUE(sent().empty());
}

TEST_F(CanUseItemTest, AStunnedPlayerIsRefusedWithTheAbnormalStateMessage) {
	setAbnormal(AbnormalState::STUN);
	EXPECT_FALSE(canUse(item(MINOR_LIFE_POTION_XML)));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_USE_ITEM_WHILE_IN_ABNORMAL_STATE())})) << ":289-292";
}

TEST_F(CanUseItemTest, ATransformThatForbidsItemsRefusesSilently) {
	transformCantUseItems();
	EXPECT_FALSE(canUse(item(MINOR_LIFE_POTION_XML)));
	EXPECT_TRUE(sent().empty()) << ":295-298, the client sends the message itself";
}

TEST_F(CanUseItemTest, APrivateStoreOwnerIsRefusedWithThePersonalShopMessage) {
	actor.player->setStore(std::make_unique<model::gameobjects::player::PrivateStore>(*actor.player));
	EXPECT_FALSE(canUse(item(MINOR_LIFE_POTION_XML)));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_CANNOT_USE_ITEM_DURING_PATH_FLYING(
						  utils::ChatUtil::l10n(getL10nId(model::ActionState::PERSONAL_SHOP))))}))
		<< ":300-303";
}

TEST_F(CanUseItemTest, AnItemOnCooldownIsRefusedUntilTheDelayEnds) {
	model::gameobjects::Item& potion = item(MINOR_LIFE_POTION_XML);
	actor.player->addItemCoolDown(11, commons::utils::currentTimeMillis() + 30000, 30); // usedelayid 11 (:830728)

	EXPECT_FALSE(canUse(potion)) << ":306-309, `player.hasCooldown(item)`";
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_ITEM_CANT_USE_UNTIL_DELAY_TIME())}));

	actor.player->addItemCoolDown(11, commons::utils::currentTimeMillis() - 1, 30);
	EXPECT_TRUE(canUse(potion)) << "an expired delay no longer refuses";
}

TEST_F(CanUseItemTest, AnItemOfTheOtherRaceIsRefused) {
	EXPECT_FALSE(canUse(item(GRAVEKNIGHT_CANDY_XML)));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_RACE())})) << ":312-315";
}

TEST_F(CanUseItemTest, TheRaceIsAskedBeforeTheActions) {
	// ":311 Checked before the 'no actions' fallback below so a race mismatch reports correctly even without one"
	EXPECT_FALSE(canUse(item(RAW_BRAX_MEAT_XML)));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_RACE())}));
}

TEST_F(CanUseItemTest, AnItemWithoutActionsIsUsableOnlyAsAQuestItem) {
	EXPECT_FALSE(canUse(item(SPARKIE_CARAPACE_FRAGMENT_XML)));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_ITEM_IS_NOT_USABLE())})) << ":317-323";

	// a quest item passes (QuestEngine.isRegisteredQuestItem); the registration stays in the engine for the rest of this process (ctest runs
	// each case in its own), and no other case of this executable uses the item
	questEngine::QuestEngine::getInstance().registerQuestItem(182006985, 99999);
	EXPECT_TRUE(canUse(item(LESSER_ANCIENT_KINAH_XML)));
	EXPECT_TRUE(sent().empty());
}

TEST_F(CanUseItemTest, TheActionsAreAskedBeforeEveryGuardBehindThem) {
	// :317-323 before each of :325-366. Every row below has no actions and arms one guard behind them: Java answers STR_ITEM_IS_NOT_USABLE
	// for each (what a client sees for a weapon or armour used from the cube: 32,700 shipped rows without actions and not of the ASMODIANS
	// ask a warrior for a level above 1).
	// Registering the item as a quest item then lets it past :317-323 (QuestEngine.isRegisteredQuestItem) and the guard behind answers - so
	// each row shows that it arms both guards, and a port that asked that guard first would answer its message instead. The registrations stay
	// in the engine for the rest of the process (ctest runs each case in its own); no other case uses these items.
	questEngine::QuestEngine& quests = questEngine::QuestEngine::getInstance();
	const std::vector<uint8_t> notUsable = message(SM_SYSTEM_MESSAGE::STR_ITEM_IS_NOT_USABLE());

	// :325-329, the gender: a FEMALE-only torso, the actor is MALE
	model::gameobjects::Item& femaleTorso = item(APPEARANCE_TEST_FEMALE_XML);
	EXPECT_FALSE(canUse(femaleTorso));
	EXPECT_EQ(sent(), exactly({notUsable})) << "the actions before the gender";
	quests.registerQuestItem(110900054, 99999);
	EXPECT_FALSE(canUse(femaleTorso));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_GENDER())})) << "the row arms the gender";

	// :331-334, the class: a gladiator stigma, restrict 0 for the WARRIOR
	model::gameobjects::Item& stigma = item(ADVANCED_DUAL_WIELDING_I_XML);
	EXPECT_FALSE(canUse(stigma));
	EXPECT_EQ(sent(), exactly({notUsable})) << "the actions before the class";
	quests.registerQuestItem(140000005, 99999);
	EXPECT_FALSE(canUse(stigma));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_CLASS())})) << "the row arms the class";

	// :336-340, the required level: the Mercenary Sword (restrict 3) of the level-1 warrior
	model::gameobjects::Item& sword = item(MERCENARY_SWORD_XML);
	EXPECT_FALSE(canUse(sword));
	EXPECT_EQ(sent(), exactly({notUsable})) << "the actions before the required level";
	quests.registerQuestItem(100000095, 99999);
	EXPECT_FALSE(canUse(sword));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_TOO_LOW_LEVEL_MUST_BE_THIS_LEVEL(sword.getL10n(), 3))}))
		<< "the row arms the required level";

	// :348-354, the use area: an unspawned player is inside no item-use zone (Creature.isInsideItemUseZone)
	model::gameobjects::Item& key = item(RUSTED_VAULT_KEY_XML);
	actor.player->getPosition()->setIsSpawned(false);
	EXPECT_FALSE(canUse(key));
	EXPECT_EQ(sent(), exactly({notUsable})) << "the actions before the use area";
	quests.registerQuestItem(185000222, 99999);
	EXPECT_FALSE(canUse(key));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_USE_ITEM_IN_CURRENT_POSITION())})) << "the row arms the use area";
	actor.player->getPosition()->setIsSpawned(true);

	// :356-366, the activation race: GCHIEF_LIGHT, and the actor has no target
	ASSERT_FALSE(actor.player->getTarget());
	model::gameobjects::Item& buff = item(GUARDIAN_GENERAL_BUFF_TEST_ITEM_08_XML);
	EXPECT_FALSE(canUse(buff));
	EXPECT_EQ(sent(), exactly({notUsable})) << "the actions before the activation race";
	quests.registerQuestItem(164000008, 99999);
	EXPECT_FALSE(canUse(buff));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_ITEM_CANT_FIND_VALID_TARGET())})) << "the row arms the activation race";

	// :342-346, the max level: restrict_max 19 (the lowest of any shipped row without actions) for a level-20 character, which needs the
	// experience table beyond the fixture's 16 levels; last, because the level stays
	dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
	xml::LoadContext context;
	dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
		xml::bindString<dataholders::PlayerExperienceTable>(context, PLAYER_EXPERIENCE_TABLE_TO_LEVEL_21_XML));
	setLevel(20);
	model::gameobjects::Item& remedy = item(LESSER_SECRET_REMEDY_OF_GROWTH_I_XML);
	EXPECT_FALSE(canUse(remedy));
	EXPECT_EQ(sent(), exactly({notUsable})) << "the actions before the max level";
	quests.registerQuestItem(188900002, 99999);
	EXPECT_FALSE(canUse(remedy));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_TOO_HIGH_LEVEL(19, remedy.getL10n()))})) << "the row arms the max level";
}

TEST_F(CanUseItemTest, AnItemOfTheOtherGenderIsRefused) {
	model::gameobjects::Item& dress = item(HOT_DENIM_DRESS_XML);
	EXPECT_FALSE(canUse(dress));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_GENDER())})) << ":325-329";
	actor.commonData->setGender(model::Gender::FEMALE);
	EXPECT_TRUE(canUse(dress));
}

TEST_F(CanUseItemTest, AnItemOfAnotherClassIsRefused) {
	EXPECT_FALSE(canUse(item(WORG_SOUL_XML))) << "restrict 0 for the WARRIOR";
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_CLASS())})) << ":331-334";
}

TEST_F(CanUseItemTest, TheLevelMustBeInsideTheItemsRange) {
	model::gameobjects::Item& extractor = item(XP_EXTRACTING_ITEM_14_XML);
	EXPECT_FALSE(canUse(extractor));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_TOO_LOW_LEVEL_MUST_BE_THIS_LEVEL(extractor.getL10n(), 10))})) << ":336-340";

	setLevel(10);
	EXPECT_TRUE(canUse(extractor)) << "restrict 10 and restrict_max 10: level 10 is inside";

	setLevel(11);
	EXPECT_FALSE(canUse(extractor));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_TOO_HIGH_LEVEL(10, extractor.getL10n()))})) << ":342-346";
}

TEST_F(CanUseItemTest, AnItemBoundToAnAreaIsRefusedOutsideIt) {
	actor.player->getPosition()->setIsSpawned(false); // Creature.isInsideItemUseZone answers false for an unspawned creature
	EXPECT_FALSE(canUse(item(TALOC_FRUIT_XML)));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_USE_ITEM_IN_CURRENT_POSITION())})) << ":348-354";
}

TEST_F(CanUseItemTest, AnActivationRaceNeedsACreatureTargetOfThatRace) {
	model::gameobjects::Item& cleanse = item(GREEN_CLEANSE_XML);
	EXPECT_FALSE(canUse(cleanse)) << "no target";
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_ITEM_CANT_FIND_VALID_TARGET())})) << ":358-361";

	runtime::Ref<PlainObject> object = PlainObject::create(200003);
	actor.player->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*object));
	EXPECT_FALSE(canUse(cleanse)) << "a target that is no Creature";
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_ITEM_CANT_FIND_VALID_TARGET())}));

	actor.player->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*other.player));
	EXPECT_FALSE(canUse(cleanse)) << "an ELYOS target, not LIVINGWATER";
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST_TO_CURRENT_TARGET())})) << ":362-365";

	PlayerFixture livingWater = makePlayer(200004, 9104, "Water", model::Race::LIVINGWATER);
	actor.player->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*livingWater.player));
	EXPECT_TRUE(canUse(cleanse)) << "a Creature of the activation race";
	EXPECT_TRUE(sent().empty());
	actor.player->setTarget(nullptr);
}

TEST_F(CanUseItemTest, EveryGuardIsAskedBeforeEveryGuardBehindIt) {
	// As CanUseSkillTest pins canUseSkill's order: guards armed from the LAST to the FIRST and never disarmed, so each new one answers only if
	// Java asks it before all the armed ones. The Blessing of Concentration (ELYOS, restrict 50, usedelayid 17, activation race
	// GHENCHMAN_LIGHT) arms the activation race, the level, the race and the cooldown at once (the actions against every guard behind them:
	// TheActionsAreAskedBeforeEveryGuardBehindThem). The pairs no row arms together are not ordered here: gender/class/area against their
	// neighbours (no shipped row with actions has two of them), and class against level or max level -
	// a class refusal means a restriction of 0, i.e. no required level (ItemTemplate.getRequiredLevel answers -1), and a too-high level needs
	// restrict_max below the level, which no item without restrict_max >= restrict has - so neither order is observable
	model::gameobjects::Item& blessing = item(BLESSING_OF_CONCENTRATION_XML);
	ASSERT_FALSE(actor.player->getTarget());

	// :336-340 before :356-366: the level answers, not the missing target
	EXPECT_FALSE(canUse(blessing));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_TOO_LOW_LEVEL_MUST_BE_THIS_LEVEL(blessing.getL10n(), 50))}));

	// :312-315: the race, before the level
	actor.commonData->setRace(model::Race::ASMODIANS);
	EXPECT_FALSE(canUse(blessing));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_RACE())}));

	// :306-309: the cooldown, before the race
	actor.player->addItemCoolDown(17, commons::utils::currentTimeMillis() + 60000, 60);
	EXPECT_FALSE(canUse(blessing));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_ITEM_CANT_USE_UNTIL_DELAY_TIME())}));

	// :300-303: the store, before the cooldown
	actor.player->setStore(std::make_unique<model::gameobjects::player::PrivateStore>(*actor.player));
	EXPECT_FALSE(canUse(blessing));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_CANNOT_USE_ITEM_DURING_PATH_FLYING(
						  utils::ChatUtil::l10n(getL10nId(model::ActionState::PERSONAL_SHOP))))}));

	// :295-298: the transform, silently, before the store
	transformCantUseItems();
	EXPECT_FALSE(canUse(blessing));
	EXPECT_TRUE(sent().empty()) << "the transform";

	// :289-292: the abnormal state, before the transform
	setAbnormal(AbnormalState::STUN);
	EXPECT_FALSE(canUse(blessing));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_USE_ITEM_WHILE_IN_ABNORMAL_STATE())}));

	// :286: death, silently, before the abnormal state
	actor.player->setLifeStats(std::make_unique<DeadPlayerLifeStats>(*actor.player));
	EXPECT_FALSE(canUse(blessing));
	EXPECT_TRUE(sent().empty()) << "isDead";

	// :281-284: the prison, before death
	actor.player->setPrisonEndTimeMillis(commons::utils::currentTimeMillis() + 600000);
	EXPECT_FALSE(canUse(blessing));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_ACCUSE_TARGET_IS_NOT_VALID())}));
	// :278, offline, is not ordered here: without a connection no guard's message can reach the client, so every order answers the same
	actor.commonData->setRace(model::Race::ELYOS);
}

// ======================================================================================================================== canChangeEquip
//
// PlayerRestrictions.canChangeEquip (m5b3-plan.md P-01), the first check of CM_EQUIP_ITEM after cancelUseItem (CM_EQUIP_ITEM.java:41),
// PlayerRestrictions.java:371-389: the prison, a stance, CANT_ATTACK_STATE, an ITEM_USE task - each with its own message.

/** a stance skill for PlayerController.startStance / stopStance (stopStance ends the effect of the skill, which SKILL_DATA must know) */
constexpr int32_t STANCE_SKILL = 16;

class CanChangeEquipTest : public PlayerRestrictionsTest {
protected:
	void SetUp() override {
		PlayerRestrictionsTest::SetUp();
		xml::LoadContext context;
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the fixture published an empty holder; the base TearDown resets this one
		dataholders::DataManager::SKILL_DATA.publish(
			xml::bindString<dataholders::SkillData>(context, "<skill_data>" + skillTemplateXml(STANCE_SKILL, "PHYSICAL") + "</skill_data>"));
		runtime::resetUnportedHitsForTests();
	}

	void TearDown() override {
		if (actor.player) {
			actor.player->getController().cancelTask(model::TaskId::ITEM_USE);
			actor.player->getController().stopStance();
		}
		PlayerRestrictionsTest::TearDown();
	}

	bool canChange() {
		(*client)->clearSent();
		return PlayerRestrictions::canChangeEquip(*actor.player);
	}

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return serialized(std::move(packet), client->con()); }

	void startItemUseTask() {
		actor.player->getController().addTask(model::TaskId::ITEM_USE, utils::ThreadPoolManager::getInstance().schedule([] {}, 60000));
	}
};

TEST_F(CanChangeEquipTest, AnUnrestrictedPlayerMayChangeEquipment) {
	EXPECT_TRUE(canChange());
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(CanChangeEquipTest, EachGuardAnswersWithItsOwnMessage) {
	actor.player->setPrisonEndTimeMillis(commons::utils::currentTimeMillis() + 600000);
	EXPECT_FALSE(canChange());
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_ACCUSE_TARGET_IS_NOT_VALID())})) << ":372-375";
	actor.player->setPrisonEndTimeMillis(0);

	actor.player->getController().startStance(STANCE_SKILL);
	EXPECT_FALSE(canChange());
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_EQUIP_ITEM_WHILE_IN_CURRENT_STANCE())})) << ":376-379";
	actor.player->getController().stopStance();

	actor.player->getEffectController()->setAbnormal(AbnormalState::STUN);
	EXPECT_FALSE(canChange());
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_EQUIP_ITEM_WHILE_IN_ABNORMAL_STATE())})) << ":380-383";
	actor.player->getEffectController()->unsetAbnormal(AbnormalState::STUN);

	startItemUseTask();
	EXPECT_FALSE(canChange());
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_CANT_EQUIP_ITEM_IN_ACTION())})) << ":384-387";
	actor.player->getController().cancelTask(model::TaskId::ITEM_USE);

	EXPECT_TRUE(canChange()) << "every guard disarmed again";
}

TEST_F(CanChangeEquipTest, EveryGuardIsAskedBeforeEveryGuardBehindIt) {
	startItemUseTask();
	EXPECT_FALSE(canChange());
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_CANT_EQUIP_ITEM_IN_ACTION())}));

	actor.player->getEffectController()->setAbnormal(AbnormalState::STUN);
	EXPECT_FALSE(canChange());
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_EQUIP_ITEM_WHILE_IN_ABNORMAL_STATE())}));

	actor.player->getController().startStance(STANCE_SKILL);
	EXPECT_FALSE(canChange());
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_EQUIP_ITEM_WHILE_IN_CURRENT_STANCE())}));

	actor.player->setPrisonEndTimeMillis(commons::utils::currentTimeMillis() + 600000);
	EXPECT_FALSE(canChange());
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_ACCUSE_TARGET_IS_NOT_VALID())}));
}

// ============================================================================================================================== canTrade
//
// PlayerRestrictions.canTrade (m5b3-plan.md P-01, optional: its callers, the exchange and private-store packets, are M5c's; every callee is
// ported), PlayerRestrictions.java:240-252. NOT COVERED: the shutdown arm (it needs a running ShutdownHook within 30 s of its end) and the
// trading arm (ExchangeService.registerExchange, the only writer of the exchange map, is AION_UNPORTED).

TEST_F(PlayerRestrictionsTest, CanTradeRefusesNoPlayerADeadOneAndAnOfflineOneSilently) {
	EXPECT_TRUE(PlayerRestrictions::canTrade(runtime::Ptr<model::gameobjects::player::Player>(*actor.player)));
	EXPECT_FALSE(PlayerRestrictions::canTrade(nullptr));

	other.player->setLifeStats(std::make_unique<DeadLifeStats>(*other.player));
	ASSERT_TRUE(other.player->isDead());
	auto otherClient = std::make_unique<TestClient>();
	otherClient->enterWorld(other);
	EXPECT_FALSE(PlayerRestrictions::canTrade(runtime::Ptr<model::gameobjects::player::Player>(*other.player))) << "dead, online";
	other.player->setClientConnection(nullptr);
	otherClient.reset();

	actor.player->setClientConnection(nullptr);
	EXPECT_FALSE(PlayerRestrictions::canTrade(runtime::Ptr<model::gameobjects::player::Player>(*actor.player))) << "alive, offline";
	EXPECT_TRUE(sent().empty());
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
