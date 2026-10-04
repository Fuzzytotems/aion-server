// P5-08, M5e stage 3, M-01 (m5e-plan.md §2.5, §5): SummonsService - createSummon, the releases and their races (Summon.registerRelease,
// SummonRelease, ReleaseSummonTask), the modes and doMode - and TrapService, the per-owner trap queue.
//
// The summons are spawned by the real VisibleObjectSpawner.spawnSummon into the effect lane's Poeta map instance (EffectsMzTestSupport.h),
// for a level 16 Spiritmaster, with the earth spirit of 3644 "Summon: Earth Spirit" (npc 833287), both copied verbatim from the data
// (skill_templates.xml, npc_templates.xml) below. The delayed releases run on the fixture's DeterministicExecutor: advance() moves its clock.
// The expectations follow SummonsService.java:30-219, SummonRelease.java:31-41, Summon.java:213-240 and TrapService.java:16-37.

#include "EffectsMzTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/SummonController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/Trap.h"
#include "aion/gameserver/model/summons/SummonMode.h"
#include "aion/gameserver/model/summons/UnsummonType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_OWNER_REMOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_PANEL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_PANEL_REMOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/summons/SummonsService.h"
#include "aion/gameserver/services/summons/TrapService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/spawnengine/VisibleObjectSpawner.h"

namespace aion::gameserver::skillengine::effect::mztest {
namespace {

using gameserver::model::PlayerClass;
using gameserver::model::gameobjects::Summon;
using gameserver::model::gameobjects::Trap;
using gameserver::model::summons::SummonMode;
using gameserver::model::summons::UnsummonType;
using network::aion::serverpackets::SM_SUMMON_OWNER_REMOVE;
using network::aion::serverpackets::SM_SUMMON_PANEL;
using network::aion::serverpackets::SM_SUMMON_PANEL_REMOVE;
using network::aion::serverpackets::SM_SUMMON_UPDATE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::summons::SummonsService;
using services::summons::TrapService;

/** 3644 "Summon: Earth Spirit" of skill_templates.xml, verbatim but for its properties, conditions and motion (cooldownId 1045, cooldown 50) */
constexpr const char* SUMMON_SKILLS_XML =
	R"(<skill_template skill_id="3644" name="Summon: Earth Spirit" nameId="2287414" cooldownId="1045" group="EL_SUMMON_EARTHELEMENTAL")"
	R"( stack="EL_LIGHT_SUMMON_EARTHELEMENTAL" lvl="1" skilltype="MAGICAL" skillsubtype="SUMMON" tslot="NONE" activation="ACTIVE" cooldown="50")"
	R"( duration="4500" cancel_rate="30" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<summon npc_id="833287" e="1" noresist="true" element="EARTH" hoptype="SKILLLV" hopb="2295" /></effects></skill_template>)";

/** The earth spirit 833287 and the trap 749211 "small aether generator" of npc_templates.xml, verbatim */
constexpr std::string_view SUMMON_NPCS_XML =
	R"(<npc_templates><npc_template npc_id="833287" level="16" name="earth spirit" name_id="466282" height="1" group_drop="ELEMENTALEARTH1")"
	R"( rank="DISCIPLINED" rating="NORMAL" race="ELEMENTAL" tribe="PET" type="SUMMON_PET" srange="15" arange="2" attack_speed="2040" hpgauge="3")"
	R"( cancel_level="90"><stats maxHp="1575" attack="85" pdef="575" mresist="340" accuracy="503" macc="291" pcrit="50" mcrit="18" evasion="503")"
	R"( parry="0"><speeds walk="2" group_walk="1.2" run="8.4" run_fight="8.4" group_run_fight="8" /></stats>)"
	R"(<bound_radius front="0.25" side="0.25" upper="2.8" /></npc_template>)"
	R"(<npc_template npc_id="749211" level="54" name="small aether generator" name_id="392067" height="3" group_drop="NONE" rank="NOVICE")"
	R"( rating="JUNK" race="ELYOS" tribe="ATKDRAKAN" type="ABYSS_GUARD" ai="trap" srange="4" attack_speed="2000" hpgauge="1"><stats maxHp="1" />)"
	R"(<bound_radius front="0.6" side="1.728" upper="3" /></npc_template></npc_templates>)";

constexpr int32_t EARTH_SPIRIT = 833287;
constexpr int32_t SUMMON_EARTH_SPIRIT = 3644;
constexpr int32_t SUMMON_COOLDOWN_ID = 1045;
constexpr int32_t AETHER_GENERATOR = 749211;

class SummonsServiceTest : public EffectsMzTest {
protected:
	void SetUp() override {
		EffectsMzTest::SetUp();
		EFFECT_TEST_SCOPE;
	// C++ only (gameserver.dev.missing_ai_handlers=warn, docs/deviations/P4-01.md): the npcs keep their ai names; the ones not ported yet
	// (trap: M-04) get the NpcAI-derived DummyNpcAI from AIEngine.newAI
		missingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the base published the lane's templates; the holder is immortal, only forgotten
		publishSkillData(effectsMzSkills() + SUMMON_SKILLS_XML);
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(npcDataContext, std::string(SUMMON_NPCS_XML)));
		npcDataPublished = true;
		master = player(9201, PlayerClass::SPIRIT_MASTER, 16);
		addToRegion(*master);
	}

	void TearDown() override {
		{
			EFFECT_TEST_SCOPE;
			if (Ptr<Summon> left = master->getSummon()) // a case that keeps its summon: World.despawn and the master's reference
				left->getController().delete_();
			master->setSummon(nullptr);
		}
		trapSpawns.clear();
		master = nullptr;
		configs::main::AIConfig::MISSING_AI_HANDLERS.set(*missingAiHandlers);
		EffectsMzTest::TearDown();
	}

	Ptr<Summon> summon() { return SummonsService::createSummon(*master, EARTH_SPIRIT, SUMMON_EARTH_SPIRIT, 1, 0); }

	/** SummonTrapEffect's spawn: a single time spawn of the trap npc at the master's spot, brought into the world (VisibleObjectSpawner) */
	Ref<Trap> trap(Creature& owner) {
		Ref<gameserver::model::templates::spawns::SpawnTemplate> spawn = spawnengine::SpawnEngine::newSingleTimeSpawn(
			owner.getWorldId(), AETHER_GENERATOR, owner.getX(), owner.getY(), owner.getZ(), owner.getHeading());
		Ref<Trap> placed = spawnengine::VisibleObjectSpawner::spawnTrap(*spawn, owner.getInstanceId(), owner);
		trapSpawns.push_back(spawn);
		return placed;
	}

	int64_t countSent(const std::vector<uint8_t>& packet) {
		const std::vector<std::vector<uint8_t>> packets = connection(*master).sentBytes();
		return std::count(packets.begin(), packets.end(), packet);
	}

	int64_t countSent(SM_SYSTEM_MESSAGE&& message) { return countSent(cp::serialized(std::move(message))); }

	Ref<Player> master;
	std::vector<Ref<gameserver::model::templates::spawns::SpawnTemplate>> trapSpawns;
	std::shared_ptr<const std::string> missingAiHandlers;
};

// --------------------------------------------------------------------------------------------------------------------------- createSummon

TEST_F(SummonsServiceTest, CreateSummonSpawnsTheSpiritForItsMasterAndShowsThePanel) {
	EFFECT_TEST_SCOPE;
	clearSent(*master);
	Ptr<Summon> spirit = summon();

	ASSERT_TRUE(spirit);
	EXPECT_EQ(master->getSummon(), spirit);
	EXPECT_TRUE(spirit->isSpawned());
	EXPECT_EQ(spirit->getMaster(), Ptr<Creature>(*master));
	EXPECT_EQ(spirit->getSummonedBySkillId(), SUMMON_EARTH_SPIRIT);
	EXPECT_EQ(spirit->getMode(), SummonMode::GUARD) << "Summon's initial mode";
	EXPECT_EQ(sentTo<SM_SUMMON_PANEL>(*master).size(), 1u);
}

TEST_F(SummonsServiceTest, ASecondSummonIsRefusedWhileTheFirstIsOut) {
	EFFECT_TEST_SCOPE;
	Ptr<Summon> first = summon();
	ASSERT_TRUE(first);
	clearSent(*master);

	EXPECT_FALSE(summon());
	EXPECT_EQ(master->getSummon(), first);
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_ALREADY_HAVE_A_FOLLOWER()), 1);
	EXPECT_TRUE(sentTo<SM_SUMMON_PANEL>(*master).empty());
}

// --------------------------------------------------------------------------------------------------------------------------- the releases

TEST_F(SummonsServiceTest, AnInstantReleaseRemovesTheSummonAtOnceAndStartsTheSkillCooldown) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());
	const std::string l10n = spirit->getL10n();
	clearSent(*master);

	const int64_t before = commons::utils::currentTimeMillis();
	SummonsService::release(*spirit, UnsummonType::UNSPECIFIED);
	const int64_t after = commons::utils::currentTimeMillis();

	EXPECT_FALSE(spirit->isSpawned());
	EXPECT_FALSE(master->getSummon());
	EXPECT_EQ(spirit->getMode(), SummonMode::RELEASE);
	// cooldown 50 (tenths of a second) * 100 ms, from the wall clock's now
	EXPECT_GE(master->getSkillCoolDown(SUMMON_COOLDOWN_ID), before + 5000);
	EXPECT_LE(master->getSkillCoolDown(SUMMON_COOLDOWN_ID), after + 5000);
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_UNSUMMONED(l10n)), 1);
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_UNSUMMON_BY_TOO_DISTANCE()), 0);
	EXPECT_EQ(countSent(cp::serialized(SM_SUMMON_PANEL_REMOVE(SUMMON_EARTH_SPIRIT))), 1);
	EXPECT_EQ(countSent(cp::serialized(SM_SUMMON_OWNER_REMOVE(spirit->getObjectId()))), 1);
}

TEST_F(SummonsServiceTest, ADistanceReleaseSaysTheSummonWentTooFar) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());
	const std::string l10n = spirit->getL10n();
	clearSent(*master);

	SummonsService::release(*spirit, UnsummonType::DISTANCE);

	EXPECT_FALSE(spirit->isSpawned());
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_UNSUMMON_BY_TOO_DISTANCE()), 1);
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_UNSUMMONED(l10n)), 0);
}

TEST_F(SummonsServiceTest, ACommandReleaseRunsAfterThreeSeconds) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());
	const std::string l10n = spirit->getL10n();
	clearSent(*master);

	SummonsService::release(*spirit, UnsummonType::COMMAND);

	EXPECT_TRUE(spirit->isSpawned());
	EXPECT_EQ(master->getSummon(), Ptr<Summon>(spirit));
	EXPECT_EQ(spirit->getMode(), SummonMode::RELEASE);
	EXPECT_TRUE(spirit->isBeingReleased());
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_UNSUMMON_FOLLOWER(l10n)), 1);
	EXPECT_EQ(countSent(cp::serialized(SM_SUMMON_UPDATE(*spirit))), 1);

	advance(2999);
	EXPECT_TRUE(spirit->isSpawned());
	advance(1);
	EXPECT_FALSE(spirit->isSpawned());
	EXPECT_FALSE(master->getSummon());
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_UNSUMMONED(l10n)), 1);
}

TEST_F(SummonsServiceTest, TheMasterTakesACommandReleaseBackWithAnOrder) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());
	SummonsService::release(*spirit, UnsummonType::COMMAND);
	clearSent(*master);

	SummonsService::doMode(SummonMode::GUARD, *spirit, UnsummonType::COMMAND);

	EXPECT_FALSE(spirit->isBeingReleased());
	EXPECT_EQ(spirit->getMode(), SummonMode::GUARD);
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_GUARD_MODE(spirit->getL10n())), 1);
	advance(3000);
	EXPECT_TRUE(spirit->isSpawned()) << "the cancelled release task never runs";
	EXPECT_EQ(master->getSummon(), Ptr<Summon>(spirit));
}

TEST_F(SummonsServiceTest, AnUnkOrderDoesNotTakeACommandReleaseBack) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());
	SummonsService::release(*spirit, UnsummonType::COMMAND);

	SummonsService::doMode(SummonMode::UNK, *spirit, UnsummonType::COMMAND);

	EXPECT_TRUE(spirit->isBeingReleased());
	advance(3000);
	EXPECT_FALSE(spirit->isSpawned());
}

TEST_F(SummonsServiceTest, ASkillOrderReleaseCannotBeTakenBack) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());
	SummonsService::release(*spirit, UnsummonType::SKILL_ORDER);
	clearSent(*master);

	EXPECT_TRUE(spirit->isReleaseUncancelable());
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_UNSUMMON_FOLLOWER(spirit->getL10n())), 0) << "only a cancelable release says so";
	SummonsService::doMode(SummonMode::GUARD, *spirit, UnsummonType::COMMAND);

	EXPECT_EQ(spirit->getMode(), SummonMode::RELEASE) << "doMode returns before the mode change";
	advance(3000);
	EXPECT_FALSE(spirit->isSpawned());
}

TEST_F(SummonsServiceTest, AnInstantReleaseSupersedesAPendingDelayedOne) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());
	const std::string l10n = spirit->getL10n();
	SummonsService::release(*spirit, UnsummonType::COMMAND);
	clearSent(*master);

	SummonsService::release(*spirit, UnsummonType::SUMMON_DEATH);
	EXPECT_FALSE(spirit->isSpawned());
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_UNSUMMONED(l10n)), 1);

	advance(3000);
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_UNSUMMONED(l10n)), 1) << "the superseded task was cancelled";
}

TEST_F(SummonsServiceTest, ADelayedReleaseDoesNotReplaceAPendingOne) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());
	const std::string l10n = spirit->getL10n();
	SummonsService::release(*spirit, UnsummonType::COMMAND);
	clearSent(*master);

	SummonsService::release(*spirit, UnsummonType::SKILL_ORDER);

	EXPECT_FALSE(spirit->isReleaseUncancelable()) << "the pending COMMAND release stays";
	SummonsService::doMode(SummonMode::REST, *spirit, UnsummonType::COMMAND);
	EXPECT_FALSE(spirit->isBeingReleased());
	advance(3000);
	EXPECT_TRUE(spirit->isSpawned());
}

TEST_F(SummonsServiceTest, ASecondCommandReleaseIsRefusedWithoutAWord) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());
	SummonsService::release(*spirit, UnsummonType::COMMAND);
	clearSent(*master);

	SummonsService::release(*spirit, UnsummonType::COMMAND); // Summon.registerRelease refuses a delayed release while one is pending

	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_UNSUMMON_FOLLOWER(spirit->getL10n())), 0);
	EXPECT_TRUE(sentTo<SM_SUMMON_UPDATE>(*master).empty());
}

TEST_F(SummonsServiceTest, AnOrderAfterTheReleaseRanIsIgnored) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());
	SummonsService::release(*spirit, UnsummonType::COMMAND);
	advance(3000);
	ASSERT_FALSE(spirit->isSpawned());
	clearSent(*master);

	// the release started (SummonRelease.markStarted): it is no longer cancelable by the master, so doMode returns at isReleaseUncancelable
	EXPECT_TRUE(spirit->isReleaseUncancelable());
	SummonsService::doMode(SummonMode::GUARD, *spirit, UnsummonType::COMMAND);

	EXPECT_EQ(spirit->getMode(), SummonMode::RELEASE);
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_GUARD_MODE(spirit->getL10n())), 0);
}

TEST_F(SummonsServiceTest, AStartedReleaseIsNotSupersededByAnInstantOne) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());
	const std::string l10n = spirit->getL10n();
	clearSent(*master);

	// the delete of the running release makes the summon forget its master (SummonController.notKnow), whose DISTANCE release the started one
	// refuses (Summon.registerRelease): the master hears of one release only
	SummonsService::release(*spirit, UnsummonType::UNSPECIFIED);

	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_UNSUMMONED(l10n)), 1);
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_UNSUMMON_BY_TOO_DISTANCE()), 0);
	SummonsService::release(*spirit, UnsummonType::MASTER_DEATH); // an instant release after the started one
	EXPECT_EQ(countSent(cp::serialized(SM_SUMMON_OWNER_REMOVE(spirit->getObjectId()))), 1) << "registerRelease refused the second";
}

// ------------------------------------------------------------------------------------------------------------------------------ the modes

TEST_F(SummonsServiceTest, RestGuardAttackAndUnkTellTheMaster) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());
	const std::string l10n = spirit->getL10n();
	clearSent(*master);

	SummonsService::doMode(SummonMode::REST, *spirit);
	EXPECT_EQ(spirit->getMode(), SummonMode::REST);
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_REST_MODE(l10n)), 1);

	SummonsService::doMode(SummonMode::GUARD, *spirit);
	EXPECT_EQ(spirit->getMode(), SummonMode::GUARD);
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_GUARD_MODE(l10n)), 1);

	SummonsService::attackMode(*spirit);
	EXPECT_EQ(spirit->getMode(), SummonMode::ATTACK);
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_ATTACK_MODE(l10n)), 1);

	SummonsService::setUnkMode(*spirit);
	EXPECT_EQ(spirit->getMode(), SummonMode::UNK);
	EXPECT_EQ(sentTo<SM_SUMMON_UPDATE>(*master).size(), 4u) << "one SM_SUMMON_UPDATE per mode change";

	SummonsService::doMode(SummonMode::UNK, *spirit);
	EXPECT_EQ(spirit->getMode(), SummonMode::UNK);
	EXPECT_EQ(sentTo<SM_SUMMON_UPDATE>(*master).size(), 4u) << "doMode's UNK arm does nothing";
}

TEST_F(SummonsServiceTest, AnAttackOrderWithoutAnEnemyTargetChangesNothing) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());
	SummonsService::release(*spirit, UnsummonType::COMMAND);

	SummonsService::doMode(SummonMode::ATTACK, *spirit, 12345, UnsummonType::COMMAND);

	EXPECT_TRUE(spirit->isBeingReleased()) << "an order that won't be carried out keeps the pending release";
	EXPECT_EQ(spirit->getMode(), SummonMode::RELEASE);
}

TEST_F(SummonsServiceTest, AReleaseOrderNeedsAnUnsummonType) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());

	SummonsService::doMode(SummonMode::RELEASE, *spirit);
	EXPECT_FALSE(spirit->isBeingReleased());

	SummonsService::doMode(SummonMode::RELEASE, *spirit, UnsummonType::UNSPECIFIED);
	EXPECT_FALSE(spirit->isSpawned());
}

// ------------------------------------------------------------------------------------------------------------------------------ master hate

// The release task deletes the summon before it looks for haters (SummonsService.java:79-104): the delete makes every npc forget the summon
// (NpcController.notKnow -> AggroList.remove), which hands its entry to the master with hate 1 (AggroList.transferDamagesToMaster). The
// scheduled master hate is therefore what a release that does not delete the summon at once adds: one that is not cancelable by the master
// (SKILL_ORDER) schedules it when it is registered, while the summon still stands between the npcs and its master.

TEST_F(SummonsServiceTest, AnInstantReleaseHandsTheHateToTheMasterThroughTheDeleteAndSchedulesNoMore) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());
	Ref<Npc> npc = monster(503, 500, 100);
	pair(*npc, *master);
	EXPECT_TRUE(KnownListPairing::pair(*npc, *spirit));
	npc->getAggroList().addHate(*spirit, 10);
	ASSERT_FALSE(npc->getAggroList().isHating(*master));

	SummonsService::release(*spirit, UnsummonType::UNSPECIFIED);

	EXPECT_FALSE(npc->getAggroList().isHating(*spirit));
	EXPECT_EQ(npc->getAggroList().getHate(*master), 1) << "transferDamagesToMaster";
	advance(1000);
	EXPECT_EQ(npc->getAggroList().getHate(*master), 1) << "no npc hated only the summon any more when the haters were looked for";
}

TEST_F(SummonsServiceTest, ACommandReleaseHandsTheHateOverOnlyWhenItRuns) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());
	Ref<Npc> npc = monster(503, 500, 100);
	pair(*npc, *master);
	EXPECT_TRUE(KnownListPairing::pair(*npc, *spirit));
	npc->getAggroList().addHate(*spirit, 10);

	SummonsService::release(*spirit, UnsummonType::COMMAND); // the master may still take it back: no master hate yet
	advance(2999);
	EXPECT_FALSE(npc->getAggroList().isHating(*master));
	advance(1);
	EXPECT_EQ(npc->getAggroList().getHate(*master), 1);
	advance(1000);
	EXPECT_EQ(npc->getAggroList().getHate(*master), 1);
}

TEST_F(SummonsServiceTest, ASkillOrderReleaseTurnsTheNpcsOnlyTheSummonFoughtOnTheMasterASecondLater) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());
	Ref<Npc> summonOnly = monster(503, 500, 100);
	Ref<Npc> both = monster(504, 500, 100);
	Ref<Npc> neither = monster(506, 500, 100);
	for (Npc* npc : {summonOnly.get(), both.get(), neither.get()}) {
		pair(*npc, *master);
		EXPECT_TRUE(KnownListPairing::pair(*npc, *spirit));
	}
	summonOnly->getAggroList().addHate(*spirit, 10);
	both->getAggroList().addHate(*spirit, 10);
	both->getAggroList().addHate(*master, 7);

	SummonsService::release(*spirit, UnsummonType::SKILL_ORDER); // not cancelable: the hate does not wait for the release

	advance(999);
	EXPECT_FALSE(summonOnly->getAggroList().isHating(*master));
	advance(1);
	EXPECT_EQ(summonOnly->getAggroList().getHate(*master), 1);
	EXPECT_EQ(both->getAggroList().getHate(*master), 7) << "it already hated the master";
	EXPECT_FALSE(neither->getAggroList().isHating(*master)) << "it never hated the summon";
	advance(2000);
	EXPECT_FALSE(spirit->isSpawned());
	EXPECT_EQ(summonOnly->getAggroList().getHate(*master), 1) << "the release adds no hate a second time";
}

TEST_F(SummonsServiceTest, NoMasterHateIsScheduledForAMasterWhoIsOffline) {
	EFFECT_TEST_SCOPE;
	Ref<Summon> spirit(summon());
	Ref<Npc> npc = monster(503, 500, 100);
	pair(*npc, *master);
	EXPECT_TRUE(KnownListPairing::pair(*npc, *spirit));
	npc->getAggroList().addHate(*spirit, 10);
	master->setClientConnection(nullptr); // Player.isOnline
	ASSERT_FALSE(master->isOnline());

	SummonsService::release(*spirit, UnsummonType::SKILL_ORDER);
	advance(1000);
	EXPECT_FALSE(npc->getAggroList().isHating(*master));
}

// ------------------------------------------------------------------------------------------------------------------------------ TrapService

TEST_F(SummonsServiceTest, AThirdTrapOfAnOwnerDeletesItsFirst) {
	EFFECT_TEST_SCOPE;
	Ref<Trap> first = trap(*master);
	Ref<Trap> second = trap(*master);
	Ref<Trap> third = trap(*master);

	TrapService::registerTrap(master->getObjectId(), first, true);
	TrapService::registerTrap(master->getObjectId(), second, true);
	EXPECT_TRUE(first->isSpawned());
	TrapService::registerTrap(master->getObjectId(), third, true);

	EXPECT_FALSE(first->isSpawned()) << "TRAP_LIMIT_PER_OWNER is 2";
	EXPECT_TRUE(second->isSpawned());
	EXPECT_TRUE(third->isSpawned());
	TrapService::unregisterTrap(second->getObjectId());
	TrapService::unregisterTrap(third->getObjectId());
	second->getController().delete_();
	third->getController().delete_();
}

TEST_F(SummonsServiceTest, TrapsRegisteredWithoutTheLimitStay) {
	EFFECT_TEST_SCOPE;
	std::vector<Ref<Trap>> traps{trap(*master), trap(*master), trap(*master)};
	for (const Ref<Trap>& placed : traps)
		TrapService::registerTrap(master->getObjectId(), placed, false);
	for (const Ref<Trap>& placed : traps)
		EXPECT_TRUE(placed->isSpawned());

	// a later registration with the limit removes the excess, oldest first
	Ref<Trap> fourth = trap(*master);
	TrapService::registerTrap(master->getObjectId(), fourth, true);
	EXPECT_FALSE(traps[0]->isSpawned());
	EXPECT_FALSE(traps[1]->isSpawned());
	EXPECT_TRUE(traps[2]->isSpawned());
	EXPECT_TRUE(fourth->isSpawned());
	traps[2]->getController().delete_();
	fourth->getController().delete_();
}

TEST_F(SummonsServiceTest, ADeletedTrapLeavesTheQueueOfItsOwner) {
	EFFECT_TEST_SCOPE;
	Ref<Trap> first = trap(*master);
	Ref<Trap> second = trap(*master);
	TrapService::registerTrap(master->getObjectId(), first, true);
	TrapService::registerTrap(master->getObjectId(), second, true);

	second->getController().delete_(); // TrapController.onDelete -> TrapService.unregisterTrap
	Ref<Trap> third = trap(*master);
	TrapService::registerTrap(master->getObjectId(), third, true);

	EXPECT_TRUE(first->isSpawned()) << "two traps again, none in excess";
	EXPECT_TRUE(third->isSpawned());
	first->getController().delete_();
	third->getController().delete_();
}

TEST_F(SummonsServiceTest, EachOwnerHasItsOwnTrapLimit) {
	EFFECT_TEST_SCOPE;
	Ref<Player> other = player(9202, PlayerClass::RANGER, 16, 510, 500, 100);
	Ref<Trap> mine1 = trap(*master);
	Ref<Trap> mine2 = trap(*master);
	Ref<Trap> theirs = trap(*other);

	TrapService::registerTrap(master->getObjectId(), mine1, true);
	TrapService::registerTrap(master->getObjectId(), mine2, true);
	TrapService::registerTrap(other->getObjectId(), theirs, true);

	EXPECT_TRUE(mine1->isSpawned());
	EXPECT_TRUE(mine2->isSpawned());
	EXPECT_TRUE(theirs->isSpawned());
	for (Trap* placed : {mine1.get(), mine2.get(), theirs.get()})
		placed->getController().delete_();
}

TEST_F(SummonsServiceTest, ANullTrapIsNotRegistered) {
	EFFECT_TEST_SCOPE;
	Ref<Trap> first = trap(*master);
	Ref<Trap> second = trap(*master);
	TrapService::registerTrap(master->getObjectId(), first, true);
	TrapService::registerTrap(master->getObjectId(), nullptr, true);
	TrapService::registerTrap(master->getObjectId(), second, true);

	EXPECT_TRUE(first->isSpawned()) << "the null was not counted";
	first->getController().delete_();
	second->getController().delete_();
}

} // namespace
} // namespace aion::gameserver::skillengine::effect::mztest
