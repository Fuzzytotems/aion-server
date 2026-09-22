// P5-08 PvpService::doReward and AbyssService::announceHighRankedDeath (m5b-plan.md C-04's PvE half): what happens when a character dies and the
// creature that dealt the most damage is not a player.
//
// Java: PvpService.java:99-109 (the early return of doReward) and AbyssService.java:16-31.
//
// Why this file exists. PlayerController::onDie calls doReward() unconditionally (PlayerController.cpp:415 = PlayerController.java:323), and
// PlayerController::doReward is nothing but `PvpService.getInstance().doReward(getOwner())` (PlayerController.cpp:469-471). Until M5b-1 no
// monster could land damage on a character, so that call was unreachable and the body was AION_UNPORTED; registering the AI and porting the
// damage path made a monster able to kill a character, and the UnportedException then travelled out of reduceHp through the npc's attackTarget
// with no catch on the way.
//
// What this file reaches and what it cannot:
// - the PvE arm of doReward - no most-damage attacker, or one that is not a Player - runs end to end, driven through the real death path
//   (CreatureController::die -> reduceHp -> onHpChanged -> PlayerController::onDie -> doReward).
// - the PvP arm below the early return stays AION_UNPORTED (m5b-plan.md C-04 keeps it W). `TheDeathOfAPlayerKilledByAPlayerIsStillUnported`
//   pins that by the function name in the UnportedException, so the day somebody ports it the test says where to look.
// - the team arm inside the early return is an AION_UNPORTED behind `if (team)` (the B-07 pattern). A solo character never enters it; a test
//   would need P5-10's PlayerGroup, which is not ported.
// - one term of the broadcast predicate has no case here and is stated rather than left to the reader: `!p.isInInstance()`
//   (AbyssService.java:29). Isolating it needs a bystander who is inside an instance AND on a map of the victim's world type; the world
//   chunk's only instance map is the Dredgion, whose world_maps.xml row carries no world_type, so the world-type term filters that bystander
//   out first and an assertion on the instance term would pass with the term deleted. The gate is the place for it.

#include "../cm_ak/InWorldPacketRunSupport.h"
#include "../world/WorldTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/KillBountyData.bind.h"
#include "aion/gameserver/dataholders/KillBountyData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/PvpService.h"
#include "aion/gameserver/services/abyss/AbyssService.h"
#include "aion/gameserver/skillengine/model/HopType.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using serverpackets::SM_SYSTEM_MESSAGE;
using services::PvpService;
using services::abyss::AbyssService;
using utils::stats::AbyssRankEnum;

// The npc of tests/controllers/ControllersTestSupport.h, repeated here instead of included: that header's fixture builds its npc template in a
// `static inline` data member, i.e. during this executable's dynamic initialization, where it hangs. The four declarations below are the same
// shape - a real Npc through VisibleObject::create with a doubled pair of stat containers (NpcLifeStats reads the P5-01 stat calculation) - and
// the template is built lazily on first use instead.

/** Life stats with fixed HP (ControllersTestSupport.h's FixedLifeStats) */
class MonsterLifeStats final : public model::stats::container::CreatureLifeStats {
public:
	explicit MonsterLifeStats(model::gameobjects::Creature& owner) : CreatureLifeStats(owner, 1000, 100) {}
};

/** NpcController whose move notifications do not reach the movement task managers of P4-10 (this test runs no periodic task) */
class MonsterController final : public controllers::NpcController {
public:
	void onStartMove() override {}
	void onStopMove() override {}
};

class MonsterNpc final : public model::gameobjects::Npc {
	AION_MAKE_REF_FRIEND
public:
	MonsterNpc(CreateKey key, std::unique_ptr<controllers::NpcController> controller, model::templates::spawns::SpawnTemplate& spawnTemplate,
		const model::templates::npc::NpcTemplate* objectTemplate)
		: Npc(key, std::move(controller), spawnTemplate, objectTemplate) {}

protected:
	~MonsterNpc() override = default;

	void setupStatContainers() override {
		setGameStats(std::make_unique<model::stats::container::NpcGameStats>(*this));
		setLifeStats(std::make_unique<MonsterLifeStats>(*this));
	}
};

class MonsterSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	explicit MonsterSpawnTemplate(model::templates::spawns::SpawnGroup& group)
		: SpawnTemplate(group, 10.0f, 20.0f, 30.0f, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** The gate's Poeta monster. Npc templates are immortal static data, built on first use and kept for the process like the holder's own. */
const model::templates::npc::NpcTemplate* monsterTemplate() {
	static const model::templates::npc::NpcTemplate* const monster = [] {
		xml::LoadContext context;
		return xml::bindString<model::templates::npc::NpcTemplate>(context,
			R"(<npc_template npc_id="210663" name_id="1" level="2" name="mosbear" rating="NORMAL" rank="NOVICE" srange="8" tribe="MONSTER"/>)")
			.release();
	}();
	return monster;
}

/** tribe_relations.xml reduced to what AggroList::isAware asks: a MONSTER npc is hostile to the two player tribes (AttackSeamTest.cpp:150-153) */
const char* const TRIBE_RELATIONS_XML = R"(<tribe_relations>)"
										R"(<tribe name="PC"/><tribe name="PC_DARK"/>)"
										R"(<tribe name="MONSTER"><hostile>PC</hostile><hostile>PC_DARK</hostile></tribe>)"
										R"(</tribe_relations>)";

/** Java AbyssRankEnum.getRankById: the DAO's rank column, 1 = GRADE9_SOLDIER .. 18 = SUPREME_COMMANDER */
constexpr int32_t GRADE9_SOLDIER_ID = 1;
constexpr int32_t GRADE1_SOLDIER_ID = 9;

/** One of AbyssService.killAnnounceMaps (AbyssService.java:13-14); the world chunk's test maps have it as the 512 x 512 ABYSS map */
constexpr int32_t ANNOUNCE_MAP = world::test::RESHANTA;

class PvpDeathRewardTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		ASSERT_TRUE(world::test::publishTestStaticData());
		xml::LoadContext context;
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(
			xml::bindString<dataholders::TribeRelationsData>(context, TRIBE_RELATIONS_XML));
		// PvpService's constructor reads both (PvpService.cpp:21-25). An XML file without entries is an empty template list; the headhunter
		// query has no database here, and DB::select logs and answers false, so the map stays empty.
		dataholders::DataManager::KILL_BOUNTY_DATA.publish(xml::bindString<dataholders::KillBountyData>(context, "<kill_bounties/>"));

		group = model::templates::spawns::SpawnGroup::create(world::test::POETA, 210663, 0, nullptr);
		spawnTemplate = runtime::Ref<MonsterSpawnTemplate>(
			static_cast<MonsterSpawnTemplate&>(group->addSpawnTemplate(std::make_unique<MonsterSpawnTemplate>(*group))));

		victim = makePlayer(430001, 9601, "Victim");
		victim.player->setMotions(std::make_unique<model::gameobjects::player::motion::MotionList>(*victim.player));
		placeInWorld(*victim.player, world::test::POETA, 300.0f, 300.0f, 10.0f);
		client = std::make_unique<TestClient>();
		client->enterWorld(victim);
		(*client)->clearSent();
	}

	void TearDown() override {
		client.reset();
		removeFromWorld(watcher);
		removeFromWorld(victim);
		monster.reset();
		spawnTemplate.reset();
		group.reset();
		watcher = {};
		victim = {};
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		dataholders::DataManager::KILL_BOUNTY_DATA.resetForTests();
		InWorldPacketTest::TearDown();
	}

	void placeInWorld(model::gameobjects::player::Player& player, int32_t mapId, float x, float y, float z) {
		world::World& world = world::World::getInstance();
		world.storeObject(player);
		ASSERT_TRUE(world.setPosition(player, mapId, x, y, z, int8_t{0}));
		world.spawn(runtime::Ptr<model::gameobjects::VisibleObject>(player));
		ASSERT_TRUE(player.isSpawned());
	}

	void removeFromWorld(PlayerFixture& fixture) {
		if (!fixture.player)
			return;
		fixture.player->setTarget(nullptr);
		fixture.player->setClientConnection(nullptr);
		if (fixture.player->isSpawned())
			world::World::getInstance().despawn(*fixture.player);
		world::World::getInstance().removeObject(*fixture.player);
	}

	/** The npc of the controller tests plus the two parts VisibleObjectSpawner gives a spawned one (AttackSeamTest::createFighter) */
	MonsterNpc& createMonster() {
		monster = model::gameobjects::VisibleObject::create<MonsterNpc>(std::make_unique<MonsterController>(), *spawnTemplate, monsterTemplate());
		monster->setEffectController(std::make_unique<controllers::effect::EffectController>(*monster));
		monster->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*monster));
		monster->getPosition()->setIsSpawned(true);
		return *monster;
	}

	/**
	 * The shape of the aggro list after a monster has beaten a character down: the victim knows his attacker (AggroList::isAware reads the
	 * known list, AggroList.java:199-216, and DamageList's constructor reads it a second time, DamageList.java:29-31) and the attacker is the
	 * only entry, so getMostDamage() answers it.
	 */
	void beatenBy(model::gameobjects::Creature& attacker, int32_t damage) {
		if (!victim.knownList().knows(attacker)) // a player placed in the world nearby is already known through the spawn
			ASSERT_TRUE(victim.knownList().addForTest(attacker));
		ASSERT_TRUE(victim.knownList().knows(attacker));
		victim.player->getAggroList().addDamage(attacker, damage, true, skillengine::model::HopType::DAMAGE);
		ASSERT_TRUE(victim.player->getAggroList().isHating(attacker));
	}

	bool sent(const std::vector<uint8_t>& packet) {
		const std::vector<std::vector<uint8_t>> bytes = (*client)->sentBytes();
		return std::find(bytes.begin(), bytes.end(), packet) != bytes.end();
	}

	std::vector<uint8_t> myDeath() { return serialized(SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_MY_DEATH(), client->con()); }

	PlayerFixture victim, watcher;
	std::unique_ptr<TestClient> client;
	runtime::Ref<model::templates::spawns::SpawnGroup> group;
	runtime::Ref<MonsterSpawnTemplate> spawnTemplate;
	runtime::Ref<MonsterNpc> monster;
};

/**
 * The blocker itself: a character killed by a monster. PlayerController::onDie -> doReward() -> PvpService::doReward, whose most-damage
 * attacker is an Npc and not a Player, so Java takes the early return of PvpService.java:100-109 and the victim is told he died.
 */
TEST_F(PvpDeathRewardTest, AMonsterKillIsRewardedWithTheVictimsOwnDeathMessage) {
	MonsterNpc& npc = createMonster();
	beatenBy(npc, 150);
	(*client)->clearSent();
	runtime::resetUnportedHitsForTests();

	// Java: Creature.getController().die(lastAttacker) - reduceHp(Integer.MAX_VALUE) and then the whole onDie chain
	EXPECT_TRUE(victim.player->getController().die(npc)) << "the character must really be dead";

	EXPECT_TRUE(victim.player->isDead());
	EXPECT_TRUE(sent(myDeath())) << "PvpService.java:102 - SM_SYSTEM_MESSAGE.STR_MSG_COMBAT_MY_DEATH()";
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "a monster kill must reach no unported body";
}

/**
 * The other half of Java's `mostDamage == null || !(... instanceof Player winner)`: an empty aggro list. It is the shape of a character who
 * dies without an attacker at all (fall damage, drowning, a GM kill), and it must take the same early return.
 */
TEST_F(PvpDeathRewardTest, ADeathWithoutAnyDamageDealerStillSendsTheDeathMessage) {
	MonsterNpc& npc = createMonster();
	ASSERT_TRUE(victim.knownList().addForTest(npc));
	ASSERT_FALSE(victim.player->getAggroList().isHating(npc)) << "nothing dealt damage, so getMostDamage() answers null";
	(*client)->clearSent();
	runtime::resetUnportedHitsForTests();

	EXPECT_TRUE(victim.player->getController().die(npc));

	EXPECT_TRUE(sent(myDeath())) << "PvpService.java:100 - `mostDamage == null` takes the same arm";
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

/**
 * doReward(victim) is doReward(victim, 1) (PvpService.java:86-88). Called on its own, without the death path, it must answer the same - this is
 * the statement PlayerController::doReward reaches (PlayerController.cpp:469-471).
 */
TEST_F(PvpDeathRewardTest, PlayerControllerDoRewardReachesThePvEArm) {
	MonsterNpc& npc = createMonster();
	beatenBy(npc, 150);
	(*client)->clearSent();
	runtime::resetUnportedHitsForTests();

	victim.player->getController().doReward();

	EXPECT_TRUE(sent(myDeath()));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

/**
 * The PvP half below the early return is deliberately not ported (m5b-plan.md C-04, need W). This case pins that decision: the day a player can
 * kill a player, doReward must be finished, and the failure names the body.
 */
TEST_F(PvpDeathRewardTest, ADeathWhoseMostDamageAttackerIsAPlayerIsStillUnported) {
	watcher = makePlayer(430002, 9602, "Killer");
	placeInWorld(*watcher.player, world::test::POETA, 305.0f, 300.0f, 10.0f);
	beatenBy(*watcher.player, 150);
	runtime::resetUnportedHitsForTests();

	try {
		PvpService::getInstance().doReward(*victim.player);
		FAIL() << "the PvP arm of PvpService::doReward is not ported yet";
	} catch (const runtime::UnportedException& unported) {
		EXPECT_NE(std::string(unported.what()).find("doReward"), std::string::npos) << unported.what();
	}
	EXPECT_FALSE(sent(myDeath())) << "a player killer must not take the early return";
}

/** AbyssService.java:17 - the rank gate. A GRADE9_SOLDIER on a kill-announce map is announced to nobody. */
TEST_F(PvpDeathRewardTest, ALowRankedDeathIsNotAnnounced) {
	watcher = makePlayer(430003, 9603, "Bystander");
	placeInWorld(*watcher.player, ANNOUNCE_MAP, 200.0f, 200.0f, 10.0f);
	auto watcherClient = std::make_unique<TestClient>();
	watcherClient->enterWorld(watcher);
	world::World::getInstance().despawn(*victim.player);
	ASSERT_TRUE(world::World::getInstance().setPosition(*victim.player, ANNOUNCE_MAP, 210.0f, 200.0f, 10.0f, int8_t{0}));
	world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*victim.player));
	ASSERT_EQ(static_cast<int32_t>(victim.player->getAbyssRank()->getRank()), static_cast<int32_t>(AbyssRankEnum::GRADE9_SOLDIER));
	(*watcherClient)->clearSent();

	AbyssService::announceHighRankedDeath(*victim.player);

	EXPECT_TRUE((*watcherClient)->sentBytes().empty()) << "AbyssService.java:17 - below GRADE1_SOLDIER nothing is announced";
	watcherClient.reset();
}

/** AbyssService.java:18-21 - the map table. A GRADE1_SOLDIER who dies on Poeta, which is not in killAnnounceMaps, is announced to nobody. */
TEST_F(PvpDeathRewardTest, AHighRankedDeathOutsideTheAnnounceMapsIsNotAnnounced) {
	victim.player->setAbyssRank(model::gameobjects::player::AbyssRank::create(0, 0, 0, GRADE1_SOLDIER_ID, 0, 0, 0, GRADE1_SOLDIER_ID, 0, 0, 0, 0,
		0, 0, 0));
	watcher = makePlayer(430004, 9604, "Bystander");
	placeInWorld(*watcher.player, world::test::POETA, 320.0f, 300.0f, 10.0f);
	auto watcherClient = std::make_unique<TestClient>();
	watcherClient->enterWorld(watcher);
	ASSERT_EQ(victim.player->getWorldId(), world::test::POETA);
	(*watcherClient)->clearSent();

	AbyssService::announceHighRankedDeath(*victim.player);

	EXPECT_TRUE((*watcherClient)->sentBytes().empty()) << "Poeta (210010000) is not one of AbyssService.killAnnounceMaps";
	watcherClient.reset();
}

/**
 * AbyssService.java:22-31 - what a GRADE1_SOLDIER's death on an announce map does: every player of the same world type who is not in an
 * instance is told, and the victim himself is not (Java `p != victim`).
 */
TEST_F(PvpDeathRewardTest, AHighRankedDeathOnAnAnnounceMapIsBroadcastToEverybodyElse) {
	victim.player->setAbyssRank(model::gameobjects::player::AbyssRank::create(0, 0, 0, GRADE1_SOLDIER_ID, 0, 0, 0, GRADE1_SOLDIER_ID, 0, 0, 0, 0,
		0, 0, 0));
	world::World::getInstance().despawn(*victim.player);
	ASSERT_TRUE(world::World::getInstance().setPosition(*victim.player, ANNOUNCE_MAP, 210.0f, 200.0f, 10.0f, int8_t{0}));
	world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*victim.player));
	watcher = makePlayer(430005, 9605, "Bystander");
	placeInWorld(*watcher.player, ANNOUNCE_MAP, 200.0f, 200.0f, 10.0f);
	auto watcherClient = std::make_unique<TestClient>();
	watcherClient->enterWorld(watcher);
	(*watcherClient)->clearSent();
	(*client)->clearSent();

	AbyssService::announceHighRankedDeath(*victim.player);

	const std::vector<uint8_t> announcement =
		serialized(SM_SYSTEM_MESSAGE::STR_ABYSS_ORDER_RANKER_DIE(*victim.player), watcherClient->con());
	const std::vector<std::vector<uint8_t>> watcherBytes = (*watcherClient)->sentBytes();
	EXPECT_NE(std::find(watcherBytes.begin(), watcherBytes.end(), announcement), watcherBytes.end())
		<< "AbyssService.java:28 - STR_ABYSS_ORDER_RANKER_DIE(victim)";
	EXPECT_TRUE((*client)->sentBytes().empty()) << "AbyssService.java:29 - `p != victim`, the dead man is not told of his own rank";
	watcherClient.reset();
}

/** AbyssService.java:29 - the world-type term: a bystander standing on an ELYSEA map hears nothing about an ABYSS death. */
TEST_F(PvpDeathRewardTest, AHighRankedDeathIsNotBroadcastAcrossWorldTypes) {
	victim.player->setAbyssRank(model::gameobjects::player::AbyssRank::create(0, 0, 0, GRADE1_SOLDIER_ID, 0, 0, 0, GRADE1_SOLDIER_ID, 0, 0, 0, 0,
		0, 0, 0));
	world::World::getInstance().despawn(*victim.player);
	ASSERT_TRUE(world::World::getInstance().setPosition(*victim.player, ANNOUNCE_MAP, 210.0f, 200.0f, 10.0f, int8_t{0}));
	world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*victim.player));
	watcher = makePlayer(430006, 9606, "Bystander");
	placeInWorld(*watcher.player, world::test::POETA, 320.0f, 300.0f, 10.0f);
	auto watcherClient = std::make_unique<TestClient>();
	watcherClient->enterWorld(watcher);
	ASSERT_NE(static_cast<int32_t>(victim.player->getWorldType()), static_cast<int32_t>(watcher.player->getWorldType()));
	(*watcherClient)->clearSent();

	AbyssService::announceHighRankedDeath(*victim.player);

	EXPECT_TRUE((*watcherClient)->sentBytes().empty()) << "victim.getWorldType() == p.getWorldType()";
	watcherClient.reset();
}

/**
 * The wiring of the two ported bodies: doReward's last statement before the early return is AbyssService.announceHighRankedDeath(victim)
 * (PvpService.java:106). Without it a high-ranked character could die on an abyss map and nobody would hear it, while every other assertion of
 * this file would still pass.
 */
TEST_F(PvpDeathRewardTest, TheMonsterKillOfAHighRankedCharacterIsAnnounced) {
	victim.player->setAbyssRank(model::gameobjects::player::AbyssRank::create(0, 0, 0, GRADE1_SOLDIER_ID, 0, 0, 0, GRADE1_SOLDIER_ID, 0, 0, 0, 0,
		0, 0, 0));
	world::World::getInstance().despawn(*victim.player);
	ASSERT_TRUE(world::World::getInstance().setPosition(*victim.player, ANNOUNCE_MAP, 210.0f, 200.0f, 10.0f, int8_t{0}));
	world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*victim.player));
	watcher = makePlayer(430007, 9607, "Bystander");
	placeInWorld(*watcher.player, ANNOUNCE_MAP, 200.0f, 200.0f, 10.0f);
	auto watcherClient = std::make_unique<TestClient>();
	watcherClient->enterWorld(watcher);
	MonsterNpc& npc = createMonster();
	beatenBy(npc, 150);
	const std::vector<uint8_t> announcement =
		serialized(SM_SYSTEM_MESSAGE::STR_ABYSS_ORDER_RANKER_DIE(*victim.player), watcherClient->con());
	(*watcherClient)->clearSent();
	(*client)->clearSent();

	PvpService::getInstance().doReward(*victim.player);

	EXPECT_TRUE(sent(myDeath()));
	const std::vector<std::vector<uint8_t>> watcherBytes = (*watcherClient)->sentBytes();
	EXPECT_NE(std::find(watcherBytes.begin(), watcherBytes.end(), announcement), watcherBytes.end())
		<< "PvpService.java:106 - doReward announces the high ranked death before it returns";
	watcherClient.reset();
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
