// P5-01 (M5b-1, work items B-01, B-03, B-04 and B-08): the arithmetic a kill is measured against.
//
// - StatFunctions.calculateExperienceReward / calculateBaseExp (StatFunctions.java:49-91) per NpcRating x NpcRank and through the XPRewardEnum
//   table, and Rates.XP_HUNTING's `Math.min(xp * rate, expNeed * 0.2f)` cap (Rates.java:13-18), which is the m5b-plan.md D7 worked example.
// - AttackUtil.calculatePhysAttackResult (AttackUtil.java:45-53) as golden vectors over a seeded Rnd.
// - AggroList.addDamage's clamp to the remaining HP and its damage * 10 hate factor (AggroList.java:37-52), and the DamageList /
//   TeamDamageList aggregation that folds a summon's damage into its master (DamageList.java:22-27).
//
// m5b-plan.md §6.3 A4 moves the AggroList clamp assertion here: the clamp is applied to a local copy, so no packet and no database column can
// see it, and only getFinalDamageList() can.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/configuration/ConfigValue.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AggroTarget.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/attack/AttackStatusInfo.h"
#include "aion/gameserver/controllers/attack/AttackUtil.h"
#include "aion/gameserver/controllers/attack/DamageInfo.h"
#include "aion/gameserver/controllers/attack/DamageList.h"
#include "aion/gameserver/controllers/attack/PlayerAggroList.h"
#include "aion/gameserver/controllers/attack/TeamDamageList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/SkillElement.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/SummonedObject.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/Rates.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/stats/container/SummonedObjectGameStats.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/model/templates/npc/NpcRank.h"
#include "aion/gameserver/model/templates/npc/NpcRating.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/skillengine/model/HopType.h"
#include "aion/gameserver/utils/stats/StatFunctions.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "StatsTestSupport.h"

namespace aion::gameserver::model::stats::test {
namespace {

using controllers::attack::AttackResult;
using controllers::attack::AttackStatus;
using controllers::attack::AttackUtil;
using runtime::Ptr;
using runtime::Ref;
using templates::npc::NpcRank;
using templates::npc::NpcRating;
using utils::stats::StatFunctions;

constexpr int32_t POETA = 210010000;

/** world_maps.xml of Poeta (the real attribute set) - an open world map, so WorldMap.isInstanceType() is false */
const char* const WORLD_MAPS_XML = R"(<world_maps>)"
								   R"(<map id="210010000" cName="LF1" name="Poeta" name_id="1" water_level="16" death_level="0")"
								   R"( world_type="ELYSEA" world_size="1024" flags="FLY GLIDE RECALL"/>)"
								   R"(</world_maps>)";

/**
 * The first eleven rows of player_experience_table.xml, verbatim. PlayerCommonData.setExp reads getStartExpForLevel(10) for the non-daeva
 * cap, so a shorter table throws.
 * <p>
 * PlayerExperienceTable.getStartExpForLevel(level) returns `experience[level - 1]`, so the row at index 0 is the start of level **1**, not of
 * level 0 as the XML comments label it: level 1 starts at 0, level 2 at 400, level 3 at 1433. getExpNeed() at level 1 is therefore
 * 400 - 0 = 400, and at level 2 it is 1433 - 400 = 1033.
 */
const char* const EXPERIENCE_TABLE_XML = R"(<player_experience_table>)"
										 R"(<exp>0</exp><exp>400</exp><exp>1433</exp><exp>3820</exp><exp>9054</exp><exp>17655</exp>)"
										 R"(<exp>30978</exp><exp>52010</exp><exp>82982</exp><exp>126069</exp><exp>182252</exp>)"
										 R"(</player_experience_table>)";

/** MONSTER is hostile to the two player tribes, so AggroList.isAware's tribe arm answers true for an npc owner and a player attacker */
const char* const TRIBE_RELATIONS_XML = R"(<tribe_relations>)"
										R"(<tribe name="PC"/><tribe name="PC_DARK"/>)"
										R"(<tribe name="MONSTER"><hostile>PC</hostile><hostile>PC_DARK</hostile></tribe>)"
										R"(</tribe_relations>)";

constexpr int32_t SPARKIE_NPC_ID = 210663;  // level 2, maxHp 199, NORMAL/DISCIPLINED - the gate's monster (m5b-plan.md D11)
constexpr int32_t RATING_NPC_ID = 700200;   // level 1, maxHp 1000 - the rating x rank table, one template per pair is not needed
constexpr int32_t SUMMONED_NPC_ID = 700201; // the summon of the DamageList grouping case

/** one template per NpcRating x NpcRank pair would be 30 templates; the table test rebinds this one with the pair in its attributes */
std::string ratingTemplate(NpcRating rating, NpcRank rank) {
	static const char* const RATINGS[] = {"JUNK", "NORMAL", "ELITE", "HERO", "LEGENDARY"};
	static const char* const RANKS[] = {"NOVICE", "DISCIPLINED", "SEASONED", "EXPERT", "VETERAN", "MASTER"};
	return std::string(R"(<npc_template npc_id="700200" name_id="1" level="1" name="rated" attack_speed="2000" tribe="MONSTER" rating=")")
		+ RATINGS[static_cast<size_t>(rating)] + R"(" rank=")" + RANKS[static_cast<size_t>(rank)] + R"(">)"
		+ R"(<stats maxHp="1000" maxMp="0" attack="16"><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)";
}

std::string npcTemplatesXml(const std::string& ratedTemplate) {
	return std::string(R"(<npc_templates>)") + ratedTemplate
		+ R"(<npc_template npc_id="210663" name_id="1" level="2" name="juvenile sparkie" attack_speed="2142" tribe="MONSTER" race="BEAST")"
		  R"( rating="NORMAL" rank="DISCIPLINED"><stats maxHp="199" maxMp="0" attack="16" pdef="50" evasion="20" accuracy="60" pcrit="10")"
		  R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)"
		  R"(<npc_template npc_id="700201" name_id="1" level="1" name="spirit" attack_speed="2000" tribe="MONSTER" rating="NORMAL" rank="NOVICE")"
		  R"(><stats maxHp="100" maxMp="0" attack="10"><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)"
		+ R"(</npc_templates>)";
}

/**
 * GeneralInstanceHandler::getExpMultiplier is AION_UNPORTED (P5-13, aion_gs_instance), and it is the one value
 * StatFunctions.calculateExperienceReward reads from outside this chunk. The test supplies Java's open-world answer
 * (GeneralInstanceHandler.java:248-251: `instance.getParent().isInstanceType() ? 1.5f : 1.25f`) so the rest of the chain can be asserted.
 */
class OpenWorldInstanceHandler final : public ::aion::gameserver::instance::handlers::GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
public:
	explicit OpenWorldInstanceHandler(world::WorldMapInstance& instance)
		: ::aion::gameserver::instance::handlers::GeneralInstanceHandler(instance) {}

	static Ref<OpenWorldInstanceHandler> create(world::WorldMapInstance& instance) {
		return runtime::makeRef<OpenWorldInstanceHandler>(instance);
	}

	float getExpMultiplier() override { return 1.25f; }

protected:
	~OpenWorldInstanceHandler() override = default;
};

/**
 * Exposes the protected static KnownList::addPair (KnownList.h:71), so two objects can be made to know each other without the World singleton
 * and its real map instances (whose GeneralInstanceHandler::getExpMultiplier is unported).
 */
struct KnownListPairing : world::knownlist::KnownList {
	static bool pair(gameobjects::VisibleObject& a, gameobjects::VisibleObject& b) { return addPair(a, b); }

	/** the two-sided removal KnownList::delPair does when the objects move apart (protected, like addPair) */
	static void unpair(gameobjects::VisibleObject& a, gameobjects::VisibleObject& b) { delPair(a, b); }
};

/** Binds XML text as T and keeps the object for the process (static data is immortal), like tests/stats/StatListenersTest.cpp */
template <class T>
const T* bindStatic(std::string_view xml) {
	xml::LoadContext context;
	return xml::bindString<T>(context, xml).release();
}

/**
 * The weapon-mastery skills Equipment.checkAvailableEquipSkills asks for before it accepts an equipped weapon: every ItemGroup carries its own
 * `requiredSkill` array (ItemGroupInfo.h, e.g. SWORD {37, 44}, DAGGER {66, 45}, BOW {53}), and a character without one puts the weapon back into
 * its inventory. The three-int PlayerSkillEntry constructor is the one that reads no SKILL_DATA.
 */
Ref<skill::PlayerSkillList> masterySkillList() {
	std::vector<Ref<skill::PlayerSkillEntry>> owned;
	std::vector<Ptr<skill::PlayerSkillEntry>> entries;
	for (int32_t skillId : {37, 39, 44, 45, 46, 51, 52, 53, 66, 89, 111}) {
		owned.push_back(skill::PlayerSkillEntry::create(skillId, 1, 0, gameobjects::Persistable::PersistentState::NOACTION));
		entries.emplace_back(*owned.back());
	}
	return skill::PlayerSkillList::create(entries);
}

/**
 * An NpcAI leaf whose two damage hooks scale what AttackUtil::modifyDamageByNpcAi hands them (AttackUtil.java:180-190). AbstractAI's own
 * modifyDamage / modifyOwnerDamage return the damage unchanged, so a port that never calls them is indistinguishable from one that does
 * until an AI answers something else - which is what every world AI handler of chunk A1 will do.
 */
class ScalingNpcAI final : public ::aion::gameserver::ai::NpcAI {
public:
	ScalingNpcAI(gameobjects::Npc& owner, float ownerFactorValue, float attackedFactorValue)
		: NpcAI(owner), ownerFactor(ownerFactorValue), attackedFactor(attackedFactorValue) {}

	/** Java NpcAI.modifyOwnerDamage: the damage this npc deals */
	float modifyOwnerDamage(float damage, gameobjects::Creature& effected, runtime::Ptr<skillengine::model::Effect> effect) override {
		return damage * ownerFactor;
	}

	/** Java NpcAI.modifyDamage: the damage this npc takes */
	float modifyDamage(gameobjects::Creature& attacker, float damage, runtime::Ptr<skillengine::model::Effect> effect) override {
		return damage * attackedFactor;
	}

private:
	const float ownerFactor;
	const float attackedFactor;
};

/** A spawn template of the group, like the spawn data of a map */
class CombatSpawnTemplate final : public templates::spawns::SpawnTemplate {
public:
	CombatSpawnTemplate(templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** Adds nothing but Java's SummonedObject.setupStatContainers (the pattern of SummonedObjectStatsTest) */
class TestSummonedObject final : public gameobjects::SummonedObject {
	AION_MAKE_REF_FRIEND
public:
	TestSummonedObject(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
		int8_t level, Ptr<gameobjects::VisibleObject> creator)
		: SummonedObject(key, std::move(controller), spawnTemplate, level, creator) {}

protected:
	~TestSummonedObject() override = default;

	void setupStatContainers() override {
		setGameStats(std::make_unique<container::SummonedObjectGameStats>(*this));
		setLifeStats(std::make_unique<container::NpcLifeStats>(*this));
	}
};

/**
 * The holders the map regions and the reward chain read. ZoneService and WorldMapInstance::regionSize() read their data once per process, so
 * these four are published once and never reset (the pattern of tests/world/WorldTestSupport.h, which this chunk may not include).
 */
void publishMapStaticDataOnce() {
	static const bool published = [] {
		configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
		// the unit tests never load geo data; with gameserver.geodata.cansee.enable off GeoService::canSee answers true (GeoService.cpp:117-119),
		// which AggroList::streamValidTargetInfo asks for every candidate target
		configs::main::GeoDataConfig::CANSEE_ENABLE.store(false);
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		static std::deque<xml::LoadContext> contexts;
		dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(contexts.emplace_back(), WORLD_MAPS_XML));
		dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(contexts.emplace_back(), "<zones/>"));
		dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(contexts.emplace_back(), "<shields/>"));
		dataholders::DataManager::MATERIAL_DATA.publish(
			xml::bindString<dataholders::MaterialData>(contexts.emplace_back(), "<material_templates/>"));
		return true;
	}();
	static_cast<void>(published);
}

class CombatDamageTest : public StatsPlayerTest {
protected:
	void SetUp() override {
		StatsPlayerTest::SetUp();
		publishMapStaticDataOnce();
		configs::main::RatesConfig::XP_SOLO_RATES.set({1.0f, 2.0f}); // the M5b profile (m5b-plan.md D1), membership 0 -> rate 1.0
		configs::main::RatesConfig::DP_PVE_RATES.set({1.0f, 2.0f}); // the server default (RatesConfig.cpp:28), membership 0 -> rate 1.0
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		publishNpcData(ratingTemplate(NpcRating::NORMAL, NpcRank::NOVICE));
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(
			xml::bindString<dataholders::TribeRelationsData>(contexts.emplace_back(), TRIBE_RELATIONS_XML));
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(contexts.emplace_back(), EXPERIENCE_TABLE_XML));

		// a world map and one instance of it, so the npcs have a map region and calculateExperienceReward finds an instance handler
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		map = world::WorldMap::create(dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(POETA));
		mapInstance = world::WorldMap2DInstance::create(*map, 1, 0, 0,
			[](world::WorldMapInstance& instance) {
				return Ref<::aion::gameserver::instance::handlers::InstanceHandler>(OpenWorldInstanceHandler::create(instance));
			});
	}

	void TearDown() override {
		mapInstance = nullptr;
		map = nullptr;
		groups.clear();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
		StatsPlayerTest::TearDown();
	}

	void publishNpcData(const std::string& ratedTemplate) {
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(
			xml::bindString<dataholders::NpcData>(contexts.emplace_back(), npcTemplatesXml(ratedTemplate)));
	}

	templates::spawns::SpawnTemplate& makeSpawn(int32_t npcId, float x, float y, float z) {
		Ref<templates::spawns::SpawnGroup> group = templates::spawns::SpawnGroup::create(POETA, npcId, 0, nullptr);
		templates::spawns::SpawnTemplate& spawnTemplate = group->addSpawnTemplate(std::make_unique<CombatSpawnTemplate>(*group, x, y, z));
		groups.push_back(group);
		return spawnTemplate;
	}

	/** places the object in the test map instance, so getPosition()->getWorldMapInstance() answers (Java: World.setPosition) */
	void place(gameobjects::VisibleObject& object, float x, float y, float z) {
		object.setPosition(world::WorldPosition::create(POETA, x, y, z, int8_t{0}, mapInstance->getRegion(x, y, z)));
		// Java: World.spawn sets the flag; addPair rolls its inserts back for an object that is not spawned
		object.getPosition()->setIsSpawned(true);
	}

	Ref<gameobjects::Npc> makeNpc(int32_t npcId, float x = 500, float y = 500, float z = 10) {
		Ref<gameobjects::Npc> npc = gameobjects::VisibleObject::create<gameobjects::Npc>(std::make_unique<controllers::NpcController>(),
			makeSpawn(npcId, x, y, z), dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		// Java: VisibleObjectSpawner gives every spawned npc an EffectController (AggroList.isAware reads it)
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		place(*npc, x, y, z);
		return npc;
	}

	Ref<TestSummonedObject> makeSummoned(Ptr<gameobjects::VisibleObject> creator, float x = 502, float y = 500, float z = 10) {
		Ref<TestSummonedObject> summoned = gameobjects::VisibleObject::create<TestSummonedObject>(std::make_unique<controllers::NpcController>(),
			makeSpawn(SUMMONED_NPC_ID, x, y, z), int8_t{1}, creator);
		summoned->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*summoned));
		summoned->setEffectController(std::make_unique<controllers::effect::EffectController>(*summoned));
		place(*summoned, x, y, z);
		return summoned;
	}

	PlayerFixture makeLevelOnePlayer(int32_t objectId, float x = 501, float y = 500, float z = 10) {
		PlayerFixture fixture = makePlayer(objectId, PlayerClass::WARRIOR);
		fixture.commonData->setLevel(1);
		place(*fixture.player, x, y, z);
		return fixture;
	}

	std::deque<xml::LoadContext> contexts;
	std::vector<Ref<templates::spawns::SpawnGroup>> groups;
	Ref<world::WorldMap> map;
	Ref<world::WorldMap2DInstance> mapInstance;
};

// ------------------------------------------------------------------------------------------------ the experience a kill awards (B-03)

TEST_F(CombatDamageTest, ExperienceRewardOfTheGateMonsterAgainstALevelOneCharacter) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// m5b-plan.md D7, worked for npc 210663 (level 2, maxHp 199, NORMAL -> 2.2, DISCIPLINED -> ordinal 1 -> +0.2) against a level-1 character:
	//   baseExp = round(199 * 2.4) = 478; reward = round(478 * 1.25 * 105/100) = 627
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID);
	ASSERT_EQ(sparkie->getGameStats()->getMaxHp()->getCurrent(), 199);
	ASSERT_EQ(sparkie->getLevel(), 2);

	EXPECT_EQ(StatFunctions::calculateExperienceReward(1, *sparkie), 627)
		<< "round(round(199 * 2.4f) * 1.25f * (105 / 100f)) - the npc is one level above the character";

	// the Rates.XP_HUNTING cap. m5b-plan.md D7 says expNeed at level 1 is 1433 - 400 = 1033 and the character therefore gains 206; that is one
	// table row off. PlayerExperienceTable.getStartExpForLevel(level) reads `experience[level - 1]`, so level 1 starts at the row the XML
	// comments label "Level 0" (0 exp) and level 2 at the row labelled "Level 1" (400 exp). getExpNeed() at level 1 is 400, the cap is
	// 400 * 0.2f = 80, and the kill awards 80. 1033 is the exp need of level **2**.
	PlayerFixture fixture = makeLevelOnePlayer(9001);
	ASSERT_EQ(fixture.commonData->getLevel(), 1);
	ASSERT_EQ(fixture.commonData->getExpNeed(), 400) << "getStartExpForLevel(2) - getStartExpForLevel(1) = 400 - 0";
	EXPECT_EQ(gameobjects::player::calcResult(gameobjects::player::Rates::XP_HUNTING, *fixture.player, int64_t{627}), 80)
		<< "Math.min(627 * 1.0f, 400 * 0.2f) = 80, so a port that drops the cap awards 627 instead";

	// a reward below the cap is not capped
	EXPECT_EQ(gameobjects::player::calcResult(gameobjects::player::Rates::XP_HUNTING, *fixture.player, int64_t{40}), 40);
	EXPECT_EQ(gameobjects::player::calcResult(gameobjects::player::Rates::XP_HUNTING, *fixture.player, int64_t{80}), 80);
	EXPECT_EQ(gameobjects::player::calcResult(gameobjects::player::Rates::XP_HUNTING, *fixture.player, int64_t{81}), 80) << "the cap bites at 81";

	// the plan's 1033 and 206 are the level-2 row
	fixture.commonData->setLevel(2);
	ASSERT_EQ(fixture.commonData->getExpNeed(), 1033) << "getStartExpForLevel(3) - getStartExpForLevel(2) = 1433 - 400";
	EXPECT_EQ(gameobjects::player::calcResult(gameobjects::player::Rates::XP_HUNTING, *fixture.player, int64_t{627}), 206)
		<< "Math.min(627 * 1.0f, 1033 * 0.2f) = 206.6, and the (long) cast truncates";
}

TEST_F(CombatDamageTest, BaseExperiencePerRatingAndRank) {
	// StatFunctions.calculateBaseExp: round(maxHp * (ratingMultiplier + rank.ordinal() * 0.2f)), here maxHp = 1000 and the npc and the character
	// are both level 1, so xpRewardFrom(0) is 100 % and the reward is round(baseExp * 1.25f).
	struct Row {
		NpcRating rating;
		NpcRank rank;
		int64_t reward;
	};
	const Row rows[] = {
		{NpcRating::JUNK, NpcRank::NOVICE, 2500},          // 1000 * 2.0 = 2000
		{NpcRating::JUNK, NpcRank::DISCIPLINED, 2750},     // 1000 * 2.2 = 2200
		{NpcRating::JUNK, NpcRank::SEASONED, 3000},        // 1000 * 2.4 = 2400
		{NpcRating::JUNK, NpcRank::EXPERT, 3250},          // 1000 * 2.6 = 2600
		{NpcRating::JUNK, NpcRank::VETERAN, 3500},         // 1000 * 2.8 = 2800
		{NpcRating::JUNK, NpcRank::MASTER, 3750},          // 1000 * 3.0 = 3000
		{NpcRating::NORMAL, NpcRank::NOVICE, 2750},        // 1000 * 2.2 = 2200
		{NpcRating::NORMAL, NpcRank::DISCIPLINED, 3000},   // 1000 * 2.4 = 2400
		{NpcRating::NORMAL, NpcRank::SEASONED, 3250},      // 1000 * 2.6 = 2600
		{NpcRating::NORMAL, NpcRank::EXPERT, 3500},        // 1000 * 2.8 = 2800
		{NpcRating::NORMAL, NpcRank::VETERAN, 3750},       // 1000 * 3.0 = 3000
		{NpcRating::NORMAL, NpcRank::MASTER, 4000},        // 1000 * 3.2 = 3200
		{NpcRating::ELITE, NpcRank::NOVICE, 5000},         // 1000 * 4.0 = 4000
		{NpcRating::ELITE, NpcRank::DISCIPLINED, 5250},    // 1000 * 4.2 = 4200
		{NpcRating::ELITE, NpcRank::SEASONED, 5500},       // 1000 * 4.4 = 4400
		{NpcRating::ELITE, NpcRank::EXPERT, 5750},         // 1000 * 4.6 = 4600
		{NpcRating::ELITE, NpcRank::VETERAN, 6000},        // 1000 * 4.8 = 4800
		{NpcRating::ELITE, NpcRank::MASTER, 6250},         // 1000 * 5.0 = 5000
		{NpcRating::HERO, NpcRank::NOVICE, 6750},          // 1000 * 5.4 = 5400
		{NpcRating::HERO, NpcRank::DISCIPLINED, 7000},     // 1000 * 5.6 = 5600
		{NpcRating::HERO, NpcRank::SEASONED, 7250},        // 1000 * 5.8 = 5800
		{NpcRating::HERO, NpcRank::EXPERT, 7500},          // 1000 * 6.0 = 6000
		{NpcRating::HERO, NpcRank::VETERAN, 7750},         // 1000 * 6.2 = 6200
		{NpcRating::HERO, NpcRank::MASTER, 8000},          // 1000 * 6.4 = 6400
		{NpcRating::LEGENDARY, NpcRank::NOVICE, 8000},     // 1000 * 6.4 = 6400
		{NpcRating::LEGENDARY, NpcRank::DISCIPLINED, 8250},// 1000 * 6.6 = 6600
		{NpcRating::LEGENDARY, NpcRank::SEASONED, 8500},   // 1000 * 6.8 = 6800
		{NpcRating::LEGENDARY, NpcRank::EXPERT, 8750},     // 1000 * 7.0 = 7000
		{NpcRating::LEGENDARY, NpcRank::VETERAN, 9000},    // 1000 * 7.2 = 7200
		{NpcRating::LEGENDARY, NpcRank::MASTER, 9250},     // 1000 * 7.4 = 7400
	};
	for (const Row& row : rows) {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		publishNpcData(ratingTemplate(row.rating, row.rank));
		Ref<gameobjects::Npc> npc = makeNpc(RATING_NPC_ID);
		ASSERT_EQ(npc->getGameStats()->getMaxHp()->getCurrent(), 1000);
		EXPECT_EQ(StatFunctions::calculateExperienceReward(1, *npc), row.reward)
			<< "rating " << static_cast<int32_t>(row.rating) << " rank " << static_cast<int32_t>(row.rank);
	}
}

TEST_F(CombatDamageTest, ExperienceRewardFollowsTheLevelDifferenceTable) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// baseExp of the level-1 NORMAL/NOVICE template is round(1000 * 2.2f) = 2200, so the reward is round(2200 * 1.25f * percent / 100f)
	Ref<gameobjects::Npc> npc = makeNpc(RATING_NPC_ID);
	EXPECT_EQ(StatFunctions::calculateExperienceReward(1, *npc), 2750) << "same level: 100 %";
	EXPECT_EQ(StatFunctions::calculateExperienceReward(2, *npc), 2750) << "one level below: xpRewardFrom(-1) is 100 %";
	EXPECT_EQ(StatFunctions::calculateExperienceReward(4, *npc), 2475) << "xpRewardFrom(-3) is 90 %";
	EXPECT_EQ(StatFunctions::calculateExperienceReward(11, *npc), 28) << "xpRewardFrom(-10) is 1 %";
	EXPECT_EQ(StatFunctions::calculateExperienceReward(12, *npc), 0) << "xpRewardFrom(-11) is 0 %";
	EXPECT_EQ(StatFunctions::calculateExperienceReward(100, *npc), 0) << "below MINUS_11 the clamp keeps 0 %";
}

TEST_F(CombatDamageTest, TheZeroHitPointArmOfCalculateBaseExpIsUnreachableThroughATemplate) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// StatFunctions.calculateBaseExp opens with `if (maxHp <= 0) return 0;` (StatFunctions.java:66-68). An npc cannot get there from its
	// template: StatCapUtil's lower cap for MAXHP floors the stat at 1, so a template with maxHp="0" still awards the rating's share of 1 HP.
	// The arm stays ported for an npc whose max HP an effect drove to 0, which M5b-1 has no way to produce.
	publishNpcData(R"(<npc_template npc_id="700200" name_id="1" level="1" name="bodiless" attack_speed="2000" tribe="MONSTER")"
				   R"( rating="LEGENDARY" rank="MASTER"><stats maxHp="0" maxMp="0" attack="16")"
				   R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)");
	Ref<gameobjects::Npc> npc = makeNpc(RATING_NPC_ID);
	EXPECT_EQ(npc->getGameStats()->getMaxHp()->getCurrent(), 1) << "the stat cap floors MAXHP at 1, so maxHp is never <= 0 here";
	// LEGENDARY 6.4f + MASTER's ordinal 5 * 0.2f = 7.4f: baseExp = round(1 * 7.4f) = 7, reward = round(7 * 1.25f * 1.0f) = 9
	EXPECT_EQ(StatFunctions::calculateExperienceReward(1, *npc), 9);
}

// ------------------------------------------------------------------------------------- AttackUtil.calculatePhysAttackResult (B-01)

TEST_F(CombatDamageTest, PhysicalAttackGoldenVectorsOfAnNpcAttacker) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// The gate's direction: the monster hits the character. StatFunctions.calculateAttackDamage takes the non-player arm
	// (`mainHandAttack.getCurrent() * Rnd.get(80, 120) / 100f`), then adjustDamageByStatModifiers subtracts pdef/10 and applies the PvE ratios.
	//
	// The numbers are golden vectors of THIS port, not of the Java server: Rnd is a xoshiro256++ whose sequence deliberately differs from
	// java.util.random (CONVENTIONS.md "Random numbers"), so only the arithmetic between the draws can be pinned this way. Any change to the
	// formula, to the order of the Rnd draws or to the number of draws per attack moves every one of them.
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID);
	PlayerFixture fixture = makeLevelOnePlayer(9002);
	KnownListPairing::pair(*sparkie, *fixture.player);

	const std::vector<int32_t> expected{18, 18, 18, 15, 14, 16, 14, 17};
	commons::utils::Rnd::seedCurrentThreadForTests(4711);
	for (size_t i = 0; i < expected.size(); i++) {
		std::vector<Ref<AttackResult>> results = AttackUtil::calculatePhysAttackResult(*sparkie, *fixture.player, {});
		ASSERT_EQ(results.size(), 1u) << "an npc has no off hand, so calculateAttackDamage returns one result";
		EXPECT_EQ(results[0]->getDamage(), expected[i]) << "hit " << i;
		EXPECT_EQ(results[0]->getAttackStatus(), AttackStatus::NORMALHIT) << "hit " << i << ": this seed rolls no counter and no critical";
	}

	// the same seed replays the same sequence
	commons::utils::Rnd::seedCurrentThreadForTests(4711);
	std::vector<Ref<AttackResult>> replay = AttackUtil::calculatePhysAttackResult(*sparkie, *fixture.player, {});
	EXPECT_EQ(replay[0]->getDamage(), expected[0]);
}

TEST_F(CombatDamageTest, PhysicalAttackGoldenVectorsOfAnUnarmedPlayerAttacker) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// A character without a main hand weapon takes StatFunctions.calculateAttackDamage's "Attack without weapon" arm:
	//   Rnd.get(16, 20) * (1 + ((power - 100) / 100f * 70f) / 100f) + mainHandAttack.getBonus()
	// Golden vectors of this port, for the reason the npc case gives above.
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID);
	PlayerFixture fixture = makeLevelOnePlayer(9003);
	KnownListPairing::pair(*sparkie, *fixture.player);
	ASSERT_FALSE(fixture.player->getEquipment().getMainHandWeapon()) << "the fixture equips nothing";

	const std::vector<int32_t> expected{16, 16, 14, 12, 12, 13, 12, 12};
	commons::utils::Rnd::seedCurrentThreadForTests(20260922);
	for (size_t i = 0; i < expected.size(); i++) {
		std::vector<Ref<AttackResult>> results = AttackUtil::calculatePhysAttackResult(*fixture.player, *sparkie, {});
		ASSERT_EQ(results.size(), 1u) << "no weapon means one result and no off hand";
		EXPECT_EQ(results[0]->getDamage(), expected[i]) << "hit " << i;
		EXPECT_GE(results[0]->getDamage(), 1) << "adjustDamageByStatModifiers floors a landed hit at 1";
	}
}

TEST_F(CombatDamageTest, TheAttackStatusDecidesWhetherDamageLandsAndHowHardItHits) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// The decision table of AttackUtil.calculatePhysicalStatus and the two arms of adjustDamageByStatModifiers it selects, over a seeded run:
	// DODGE returns before any multiplier (the AttackResult of calculateAttackDamage keeps its 0 damage), and a critical hit of an npc without
	// equipment keeps the 1.5f default multiplier, so its damage is half again a normal hit's.
	//
	// The caps of StatCapUtil make both statuses reachable but neither certain: limit(EVASION, ...) is 300 and limit(PHYSICAL_CRITICAL, ...)
	// is 500 out of the 1000 Rnd.nextInt draws, so the seeded run below sees both.
	publishNpcData(R"(<npc_template npc_id="700200" name_id="1" level="1" name="slippery" attack_speed="2000" tribe="MONSTER" rating="NORMAL")"
				   R"( rank="NOVICE"><stats maxHp="1000" maxMp="0" attack="16" evasion="100000" pcrit="100000")"
				   R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)");
	Ref<gameobjects::Npc> npc = makeNpc(RATING_NPC_ID);
	PlayerFixture fixture = makeLevelOnePlayer(9008);
	KnownListPairing::pair(*npc, *fixture.player);

	// the character attacks the evasive npc: every dodged hit carries exactly 0
	int32_t dodges = 0;
	int32_t hits = 0;
	commons::utils::Rnd::seedCurrentThreadForTests(31337);
	for (int i = 0; i < 200; i++) {
		std::vector<Ref<AttackResult>> results = AttackUtil::calculatePhysAttackResult(*fixture.player, *npc, {});
		ASSERT_EQ(results.size(), 1u);
		if (getBaseStatus(results[0]->getAttackStatus()) == AttackStatus::DODGE) {
			dodges++;
			EXPECT_EQ(results[0]->getDamage(), 0) << "adjustDamageByStatModifiers returns before the multipliers for DODGE";
		} else {
			hits++;
			EXPECT_GE(results[0]->getDamage(), 1);
		}
	}
	EXPECT_GT(dodges, 0) << "limit(EVASION, dodgeRate) is 300 of 1000, so 200 attacks must contain dodges";
	EXPECT_GT(hits, 0) << "and it is not 1000 of 1000, so they must contain landed hits too";

	// the npc attacks back: a critical hit of an attacker without a weapon group keeps the 1.5f coefficient
	int32_t maxNormal = 0;
	int32_t maxCritical = 0;
	commons::utils::Rnd::seedCurrentThreadForTests(31337);
	for (int i = 0; i < 200; i++) {
		std::vector<Ref<AttackResult>> results = AttackUtil::calculatePhysAttackResult(*npc, *fixture.player, {});
		ASSERT_EQ(results.size(), 1u);
		AttackStatus status = results[0]->getAttackStatus();
		if (isCritical(status))
			maxCritical = std::max(maxCritical, results[0]->getDamage());
		else if (status == AttackStatus::NORMALHIT)
			maxNormal = std::max(maxNormal, results[0]->getDamage());
	}
	EXPECT_GT(maxNormal, 0) << "limit(PHYSICAL_CRITICAL, ...) is 500 of 1000, so both statuses appear";
	EXPECT_GT(maxCritical, 0);
	EXPECT_GT(maxCritical, maxNormal) << "AttackUtil.adjustDamageByStatModifiers multiplies a critical hit by 1.5f (no weapon group)";
}

// ------------------------------------------------------------------------- AggroList, DamageList and TeamDamageList (B-04)

TEST_F(CombatDamageTest, AggroListClampsTheDamageToTheRemainingHitPointsAndMultipliesTheHate) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// m5b-plan.md §6.3 A4: the clamp of AggroList.addDamage (AggroList.java:41-42) is applied to a local copy, so it is only visible in
	// getFinalDamageList(). Overkilling the npc must leave the final damage list at exactly its maximum HP.
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID);
	PlayerFixture fixture = makeLevelOnePlayer(9004);
	KnownListPairing::pair(*sparkie, *fixture.player);
	ASSERT_TRUE(sparkie->getKnownList().knows(*fixture.player));
	ASSERT_EQ(sparkie->getLifeStats()->getCurrentHp(), 199);

	controllers::attack::AggroList& aggroList = sparkie->getAggroList();
	aggroList.addDamage(*fixture.player, 50, true, skillengine::model::HopType::DAMAGE);
	EXPECT_EQ(aggroList.getFinalDamageList().getTotalDamage(), 50);
	// hate = calculateHate(attacker, damage * 10) = 500 * 1100 / 1000
	EXPECT_EQ(aggroList.getHate(*fixture.player), 550) << "AggroList multiplies the damage by 10 before StatFunctions.calculateHate";

	// Java CreatureController.onAttack calls lifeStats.reduceHp one line after aggroList.addDamage, so the aggro list sees the HP of the
	// previous hit; the fixture applies the 50 damage the same way before the overkill
	sparkie->getLifeStats()->setCurrentHp(149);
	ASSERT_EQ(sparkie->getLifeStats()->getCurrentHp(), 149);

	// the overkill: 10000 damage against 149 remaining HP is stored as 149
	aggroList.addDamage(*fixture.player, 10000, true, skillengine::model::HopType::DAMAGE);
	EXPECT_EQ(aggroList.getFinalDamageList().getTotalDamage(), 199)
		<< "50 + clamp(10000, remaining 149) - a port without the clamp stores 10050";
	EXPECT_EQ(aggroList.getFinalDamageList().getMostDamage()->getDamage(), 199);
	EXPECT_EQ(aggroList.getFinalDamageList().getMostDamage()->getAttacker().rawPointer(),
		static_cast<gameobjects::AionObject*>(fixture.player.get()));
	EXPECT_EQ(aggroList.getMostPlayerDamage().rawPointer(), fixture.player.get());

	// HopType.DAMAGE and notifyAttack are both required for the hate (AggroList.java:47)
	int32_t hateBefore = aggroList.getHate(*fixture.player);
	aggroList.addDamage(*fixture.player, 10, false, skillengine::model::HopType::DAMAGE);
	EXPECT_EQ(aggroList.getHate(*fixture.player), hateBefore) << "notifyAttack false adds damage but no hate";
	aggroList.addDamage(*fixture.player, 10, true, skillengine::model::HopType::SKILLLV);
	EXPECT_EQ(aggroList.getHate(*fixture.player), hateBefore) << "only HopType.DAMAGE adds hate";
	aggroList.addDamage(*fixture.player, 10, true, std::nullopt);
	EXPECT_EQ(aggroList.getHate(*fixture.player), hateBefore) << "a null HopType (reflected damage) adds no hate";
}

TEST_F(CombatDamageTest, AggroListIgnoresACreatureItIsNotAwareOf) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// AggroList.isAware: without the known list relation nothing is recorded (AggroList.java:198-201)
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID);
	PlayerFixture fixture = makeLevelOnePlayer(9005);
	ASSERT_FALSE(sparkie->getKnownList().knows(*fixture.player));

	sparkie->getAggroList().addDamage(*fixture.player, 50, true, skillengine::model::HopType::DAMAGE);
	EXPECT_EQ(sparkie->getAggroList().getHate(*fixture.player), 0);
	EXPECT_EQ(sparkie->getAggroList().getFinalDamageList().getTotalDamage(), 0);
	EXPECT_FALSE(sparkie->getAggroList().getMostPlayerDamage());
}

TEST_F(CombatDamageTest, TheFinalDamageListGroupsASummonsDamageUnderItsMaster) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// DamageList.java:22-27: every aggro entry is keyed by `aggroInfo.getAttacker().getMaster()`, so a summon's damage lands on its owner.
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID);
	PlayerFixture owner = makeLevelOnePlayer(9006);
	Ref<TestSummonedObject> summon = makeSummoned(Ptr<gameobjects::VisibleObject>(*owner.player));
	ASSERT_EQ(summon->getMaster().rawPointer(), static_cast<gameobjects::Creature*>(owner.player.get()));
	KnownListPairing::pair(*sparkie, *owner.player);
	KnownListPairing::pair(*sparkie, *summon);

	controllers::attack::AggroList& aggroList = sparkie->getAggroList();
	aggroList.addDamage(*owner.player, 30, true, skillengine::model::HopType::DAMAGE);
	aggroList.addDamage(*summon, 20, true, skillengine::model::HopType::DAMAGE);
	EXPECT_EQ(aggroList.stream().size(), 2u) << "the aggro list itself keeps the summon apart from its master";

	controllers::attack::DamageList damageList = aggroList.getFinalDamageList();
	EXPECT_EQ(damageList.getTotalDamage(), 50);
	ASSERT_EQ(damageList.getCreatureDamages().size(), 1u) << "the summon's 20 damage is folded into the master's entry";
	EXPECT_EQ(damageList.getCreatureDamages()[0].getDamage(), 50);
	EXPECT_EQ(damageList.getCreatureDamages()[0].getAttacker().rawPointer(), static_cast<gameobjects::AionObject*>(owner.player.get()));
	EXPECT_EQ(damageList.getMostDamage()->getDamage(), 50);

	// TeamDamageList regroups by team; a character without a team stays its own key
	controllers::attack::TeamDamageList teamDamages = damageList.toTeamDamages();
	EXPECT_EQ(teamDamages.getTotalDamage(), 50);
	ASSERT_EQ(teamDamages.getCreatureOrTeamDamages().size(), 1u);
	EXPECT_EQ(teamDamages.getMostDamage()->getDamage(), 50);
	EXPECT_EQ(teamDamages.getMostDamage()->getAttacker().rawPointer(), static_cast<gameobjects::AionObject*>(owner.player.get()));
}

TEST_F(CombatDamageTest, TheFinalDamageListSkipsEntriesWithoutDamageAndOutsideTheKnownList) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// DamageList.java:23-26: `if (aggroInfo.getDamage() <= 0) continue;` and the known list check
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID);
	PlayerFixture fixture = makeLevelOnePlayer(9007);
	KnownListPairing::pair(*sparkie, *fixture.player);

	// addHate puts the character into the aggro list without damage
	sparkie->getAggroList().addHate(*fixture.player, 5);
	EXPECT_EQ(sparkie->getAggroList().getHate(*fixture.player), 5);
	EXPECT_EQ(sparkie->getAggroList().stream().size(), 1u);
	EXPECT_EQ(sparkie->getAggroList().getFinalDamageList().getCreatureDamages().size(), 0u) << "hate alone is no damage";
	EXPECT_EQ(sparkie->getAggroList().getFinalDamageList().getTotalDamage(), 0);
	EXPECT_FALSE(sparkie->getAggroList().getFinalDamageList().getMostDamage());
}

// ------------------------------------------------------------------- AttackUtil.adjustDamageByStatModifiers, arm by arm (B-01)
//
// calculatePhysAttackResult rolls its status, so the golden-vector cases above can only ever see the arms that seed happens to produce.
// adjustDamageByStatModifiers is public in the port (header request m5b-1), so each arm can be driven with the status it belongs to, and the
// damage it returns compared with the same input through the NORMALHIT arm - which cancels every factor the two share and leaves exactly the
// constant under test.

TEST_F(CombatDamageTest, AdjustDamageByStatModifiersFloorsALandedHitAtOneDamage) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// AttackUtil.java:167-169 (`if (damage < 1) damage = 1;`), the last statement before setDamage. Without it a character hitting a wall of
	// physical defense stores a *negative* number, which SM_ATTACK sends and which CreatureLifeStats.reduceHp would add to the target's HP.
	publishNpcData(R"(<npc_template npc_id="700200" name_id="1" level="1" name="wall" attack_speed="2000" tribe="MONSTER" rating="NORMAL")"
				   R"( rank="NOVICE"><stats maxHp="1000" maxMp="0" attack="16" pdef="100000")"
				   R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)");
	Ref<gameobjects::Npc> wall = makeNpc(RATING_NPC_ID);
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID, 505, 500, 10);
	PlayerFixture fixture = makeLevelOnePlayer(9010);
	ASSERT_GT(wall->getGameStats()->getPDef()->getCurrent(), 200) << "pdef/10 alone must exceed the 20 damage below";

	std::vector<Ref<AttackResult>> againstTheWall{AttackResult::create(20.0f, AttackStatus::NORMALHIT)};
	AttackUtil::adjustDamageByStatModifiers(*fixture.player, *wall, AttackStatus::NORMALHIT, againstTheWall, model::SkillElement::NONE);
	EXPECT_FLOAT_EQ(againstTheWall[0]->getExactDamage(), 1.0f) << "20 - pdef/10 is far below 1, and the floor is the whole answer";
	EXPECT_EQ(againstTheWall[0]->getDamage(), 1);

	// the floor is a floor and not a constant: the same hit against the gate's monster keeps its own number
	std::vector<Ref<AttackResult>> againstTheMonster{AttackResult::create(20.0f, AttackStatus::NORMALHIT)};
	AttackUtil::adjustDamageByStatModifiers(*fixture.player, *sparkie, AttackStatus::NORMALHIT, againstTheMonster, model::SkillElement::NONE);
	EXPECT_GT(againstTheMonster[0]->getExactDamage(), 1.0f) << "pdef 50 leaves 20 - 5 to pass through";
}

TEST_F(CombatDamageTest, TheParryArmCutsTheDamageToSixTenths) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// AttackUtil.java:134-137: `case PARRY: mainMultiplier *= 0.6f; offMultiplier *= 0.6f;`. Only a Player can parry
	// (calculatePhysicalStatus asks attackedPlayer), so the direction is the gate's: the monster hits the character.
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID);
	PlayerFixture fixture = makeLevelOnePlayer(9011);

	auto damageOf = [&](AttackStatus status) {
		std::vector<Ref<AttackResult>> list{AttackResult::create(100.0f, status)};
		AttackUtil::adjustDamageByStatModifiers(*sparkie, *fixture.player, status, list, model::SkillElement::NONE);
		return list[0]->getExactDamage();
	};

	const float normal = damageOf(AttackStatus::NORMALHIT);
	ASSERT_GT(normal, 1.0f) << "the floor must not be the answer, or the ratio below proves nothing";
	EXPECT_FLOAT_EQ(damageOf(AttackStatus::PARRY), normal * 0.6f) << "a parried hit lands for six tenths";
	EXPECT_FLOAT_EQ(damageOf(AttackStatus::OFFHAND_PARRY), normal * 0.6f) << "getBaseStatus folds the off-hand constant into PARRY";

	// the neighbouring arms of the same switch, so a port that put 0.6f on the wrong one fails here
	EXPECT_FLOAT_EQ(damageOf(AttackStatus::BLOCK), normal) << "a Player without a shield takes the BLOCK arm with reduceRatio 0";
	std::vector<Ref<AttackResult>> dodged{AttackResult::create(100.0f, AttackStatus::DODGE)};
	AttackUtil::adjustDamageByStatModifiers(*sparkie, *fixture.player, AttackStatus::DODGE, dodged, model::SkillElement::NONE);
	EXPECT_FLOAT_EQ(dodged[0]->getExactDamage(), 100.0f) << "DODGE returns before every multiplier and before setDamage";
}

TEST_F(CombatDamageTest, TheCriticalMultiplierOfAPhysicalHitIsTheWeaponGroupsMultiplier) {
	// AttackUtil.getWeaponMultiplier (AttackUtil.java:253-269), reached from adjustDamageByStatModifiers' `if (isCritical(status))` arm for a
	// physical hit of an attacker that holds a weapon. These are the numbers a critical hit multiplies a player's damage by, so a port that
	// answered one constant for every group would change every critical hit in the game and still pass the seeded golden vectors.
	struct Row {
		const char* group;
		float multiplier;
	};
	const Row rows[] = {{"DAGGER", 2.3f}, {"SWORD", 2.2f}, {"MACE", 2.0f}, {"GREATSWORD", 1.8f}, {"POLEARM", 1.8f}, {"STAFF", 1.7f},
		{"BOW", 1.7f}, {"ORB", 1.5f}};

	int32_t nextId = 9100;
	for (const Row& row : rows) {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		Ref<gameobjects::Npc> target = makeNpc(RATING_NPC_ID);
		PlayerFixture fixture = makeLevelOnePlayer(nextId);
		fixture.player->setSkillList(masterySkillList());
		const std::string xml = std::string(R"(<item_template id="100000132" level="1" item_group=")") + row.group
			+ R"(" attack_type="PHYSICAL"><weapon_stats hit_count="1" attack_range="1500" attack_speed="1400" max_damage="20" min_damage="16"/>)"
			  R"(</item_template>)";
		Ref<gameobjects::Item> weapon = gameobjects::Item::create(nextId++, bindStatic<templates::item::ItemTemplate>(xml), 1, true,
			items::getSlotIdMask(items::ItemSlot::MAIN_HAND));
		fixture.player->getEquipment().onLoadHandler(*weapon);
		ASSERT_TRUE(fixture.player->getEquipment().getMainHandWeapon()) << row.group << " was not equipped";

		auto damageOf = [&](AttackStatus status) {
			std::vector<Ref<AttackResult>> list{AttackResult::create(1000.0f, status)};
			AttackUtil::adjustDamageByStatModifiers(*fixture.player, *target, status, list, model::SkillElement::NONE);
			return list[0]->getExactDamage();
		};
		const float normal = damageOf(AttackStatus::NORMALHIT);
		ASSERT_GT(normal, 1.0f) << row.group;
		const float expected = normal * row.multiplier;
		EXPECT_NEAR(damageOf(AttackStatus::CRITICAL), expected, expected * 1e-4f) << row.group;
	}

	// and without a weapon the 1.5f default of the `if (mainHandGroup != null)` guard stands (AttackUtil.java:142-152)
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<gameobjects::Npc> target = makeNpc(RATING_NPC_ID);
	PlayerFixture unarmed = makeLevelOnePlayer(nextId++);
	ASSERT_FALSE(unarmed.player->getEquipment().getMainHandWeapon());
	auto damageOf = [&](AttackStatus status) {
		std::vector<Ref<AttackResult>> list{AttackResult::create(1000.0f, status)};
		AttackUtil::adjustDamageByStatModifiers(*unarmed.player, *target, status, list, model::SkillElement::NONE);
		return list[0]->getExactDamage();
	};
	const float normal = damageOf(AttackStatus::NORMALHIT);
	EXPECT_NEAR(damageOf(AttackStatus::CRITICAL), normal * 1.5f, normal * 1.5f * 1e-4f);
}

TEST_F(CombatDamageTest, AMultiHitWeaponAppendsItsExtraHitsAtATenthOfTheDamage) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// AttackUtil.calculateAdditionalHitCount / amplifyDamageByAdditionalHitCount (AttackUtil.java:172-206). Both are private, so the only way in
	// is a whole attack of a character holding a weapon with hit_count > 1 - which is what every dagger and sword in the game has.
	// `Rnd.get(0, hitCount) - 1` is -1..hitCount-1, so some swings amplify and some do not; each appended hit carries `(int) (damage * 0.1)`.
	Ref<gameobjects::Npc> target = makeNpc(RATING_NPC_ID);
	PlayerFixture fixture = makeLevelOnePlayer(9200);
	fixture.player->setSkillList(masterySkillList());
	KnownListPairing::pair(*target, *fixture.player);
	const templates::item::ItemTemplate* dagger = bindStatic<templates::item::ItemTemplate>(
		R"(<item_template id="100000133" level="1" item_group="DAGGER" attack_type="PHYSICAL">)"
		R"(<weapon_stats hit_count="3" attack_range="1500" attack_speed="1400" max_damage="220" min_damage="200"/></item_template>)");
	Ref<gameobjects::Item> weapon =
		gameobjects::Item::create(9201, dagger, 1, true, items::getSlotIdMask(items::ItemSlot::MAIN_HAND));
	fixture.player->getEquipment().onLoadHandler(*weapon);
	ASSERT_TRUE(fixture.player->getEquipment().getMainHandWeapon());

	int32_t amplified = 0;
	int32_t plain = 0;
	commons::utils::Rnd::seedCurrentThreadForTests(20260922);
	for (int i = 0; i < 200; i++) {
		std::vector<Ref<AttackResult>> results = AttackUtil::calculatePhysAttackResult(*fixture.player, *target, {});
		ASSERT_GE(results.size(), 1u);
		ASSERT_LE(results.size(), 3u) << "hit_count 3 means at most two extra main-hand hits";
		if (results.size() == 1u) {
			plain++;
			continue;
		}
		amplified++;
		for (size_t hit = 1; hit < results.size(); hit++) {
			EXPECT_EQ(results[hit]->getAttackStatus(), AttackStatus::NORMALHIT) << "the main-hand amplification";
			EXPECT_EQ(results[hit]->getDamage(), static_cast<int32_t>(static_cast<double>(results[0]->getDamage()) * 0.1))
				<< "each extra hit is a tenth of the first one";
		}
	}
	EXPECT_GT(amplified, 0) << "a hit_count of 3 must produce extra hits - a port that skips the amplification never appends one";
	EXPECT_GT(plain, 0) << "and `Rnd.get(0, 3) - 1` is -1 often enough that not every swing amplifies";

	// the same character without the weapon never amplifies, which is calculateAdditionalHitCount's `if (mainHandWeapon != null)` guard
	PlayerFixture unarmed = makeLevelOnePlayer(9202);
	KnownListPairing::pair(*target, *unarmed.player);
	for (int i = 0; i < 50; i++)
		EXPECT_EQ(AttackUtil::calculatePhysAttackResult(*unarmed.player, *target, {}).size(), 1u);
}

TEST_F(CombatDamageTest, TheNpcAiScalesTheDamageItDealsAndTheDamageItTakes) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// AttackUtil.modifyDamageByNpcAi (AttackUtil.java:208-218): the last hook before the shield check, and the one every world AI handler of
	// chunk A1 uses to make a boss hit harder or take less. AbstractAI answers the damage unchanged, so only an AI that answers something else
	// can tell a port that calls the hooks from one that does not.
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID);
	PlayerFixture fixture = makeLevelOnePlayer(9210);
	KnownListPairing::pair(*sparkie, *fixture.player);

	// the npc as the attacker: modifyOwnerDamage
	commons::utils::Rnd::seedCurrentThreadForTests(4711);
	const int32_t dealt = AttackUtil::calculatePhysAttackResult(*sparkie, *fixture.player, {})[0]->getDamage();
	ASSERT_GT(dealt, 1) << "the floor must not be the answer";
	sparkie->replaceAi(std::make_unique<ScalingNpcAI>(*sparkie, 2.0f, 1.0f));
	commons::utils::Rnd::seedCurrentThreadForTests(4711);
	EXPECT_EQ(AttackUtil::calculatePhysAttackResult(*sparkie, *fixture.player, {})[0]->getDamage(), dealt * 2)
		<< "the attacking npc's AI doubled its own damage";

	// the npc as the attacked: modifyDamage
	sparkie->replaceAi(std::make_unique<ScalingNpcAI>(*sparkie, 1.0f, 1.0f));
	commons::utils::Rnd::seedCurrentThreadForTests(31337);
	const int32_t taken = AttackUtil::calculatePhysAttackResult(*fixture.player, *sparkie, {})[0]->getDamage();
	ASSERT_GT(taken, 3) << "halving must stay above the floor";
	sparkie->replaceAi(std::make_unique<ScalingNpcAI>(*sparkie, 1.0f, 0.5f));
	commons::utils::Rnd::seedCurrentThreadForTests(31337);
	EXPECT_EQ(AttackUtil::calculatePhysAttackResult(*fixture.player, *sparkie, {})[0]->getDamage(), taken / 2)
		<< "the attacked npc's AI halved the damage it took";
}

// ------------------------------------------------------------- AggroList::getTarget, the DamageList known-list skip, PlayerAggroList (B-04)

TEST_F(CombatDamageTest, TheFinalDamageListDropsAnAttackerThatLeftTheKnownList) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// DamageList.java:23-26, the `// Don't include damage from creatures outside the known list` skip. It is the rule that decides who is paid
	// for a kill: a character who ran out of range before the monster died keeps its aggro entry and loses its share of the reward.
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID);
	PlayerFixture stayed = makeLevelOnePlayer(9220, 501, 500, 10);
	PlayerFixture left = makeLevelOnePlayer(9221, 502, 500, 10);
	KnownListPairing::pair(*sparkie, *stayed.player);
	KnownListPairing::pair(*sparkie, *left.player);

	controllers::attack::AggroList& aggroList = sparkie->getAggroList();
	aggroList.addDamage(*stayed.player, 30, true, skillengine::model::HopType::DAMAGE);
	aggroList.addDamage(*left.player, 20, true, skillengine::model::HopType::DAMAGE);
	ASSERT_EQ(aggroList.getFinalDamageList().getTotalDamage(), 50);
	ASSERT_EQ(aggroList.getFinalDamageList().getCreatureDamages().size(), 2u);
	ASSERT_EQ(aggroList.getFinalDamageList().getMostDamage()->getDamage(), 30);

	KnownListPairing::unpair(*sparkie, *left.player); // Java: the knownlist update when the two move apart
	ASSERT_FALSE(sparkie->getKnownList().knows(*left.player));
	EXPECT_EQ(aggroList.stream().size(), 2u) << "the aggro list itself keeps the entry; only the damage list filters";

	controllers::attack::DamageList damageList = aggroList.getFinalDamageList();
	EXPECT_EQ(damageList.getTotalDamage(), 30) << "a port without the known-list skip still counts the 20";
	ASSERT_EQ(damageList.getCreatureDamages().size(), 1u);
	EXPECT_EQ(damageList.getCreatureDamages()[0].getAttacker().rawPointer(), static_cast<gameobjects::AionObject*>(stayed.player.get()));
	EXPECT_EQ(aggroList.getMostPlayerDamage().rawPointer(), stayed.player.get()) << "and the kill is credited to the one who stayed";
}

TEST_F(CombatDamageTest, GetTargetRanksTheAggroListByHateAndFiltersByRange) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// AggroList.getTarget (AggroList.java:230-260) is what GeneralNpcAI.chooseAttackIntention and SimpleAttackManager.attackAction ask for the
	// creature to swing at, so a port that answers null ends every fight on its first tick - and one that answers the wrong element makes an
	// npc chase the character who hit it least.
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID);
	PlayerFixture near = makeLevelOnePlayer(9230, 510, 500, 10);  // 10 m
	PlayerFixture mid = makeLevelOnePlayer(9231, 520, 500, 10);   // 20 m
	PlayerFixture far = makeLevelOnePlayer(9232, 530, 500, 10);   // 30 m
	for (const PlayerFixture* f : {&near, &mid, &far}) {
		KnownListPairing::pair(*sparkie, *f->player);
		ASSERT_TRUE(sparkie->getKnownList().sees(*f->player)) << "streamValidTargetInfo asks the known list for every candidate";
	}
	controllers::attack::AggroList& aggroList = sparkie->getAggroList();
	using controllers::attack::AggroTarget;

	EXPECT_FALSE(aggroList.getTarget(AggroTarget::MOST_HATED)) << "an empty aggro list has no target at all";

	aggroList.addHate(*near.player, 10);
	aggroList.addHate(*far.player, 30);
	aggroList.addHate(*mid.player, 20);
	EXPECT_EQ(aggroList.getTarget(AggroTarget::MOST_HATED).rawPointer(), static_cast<gameobjects::Creature*>(far.player.get()));
	EXPECT_EQ(aggroList.getTarget(AggroTarget::SECOND_MOST_HATED).rawPointer(), static_cast<gameobjects::Creature*>(mid.player.get()));
	EXPECT_EQ(aggroList.getTarget(AggroTarget::THIRD_MOST_HATED).rawPointer(), static_cast<gameobjects::Creature*>(near.player.get()));

	// RANDOM_EXCEPT_CURRENT_TARGET reads owner.getTarget() for every element (AggroList.java:238-245)
	sparkie->setTarget(runtime::Ptr<gameobjects::VisibleObject>(*far.player));
	for (int i = 0; i < 40; i++) {
		Ptr<gameobjects::Creature> chosen = aggroList.getTarget(AggroTarget::RANDOM_EXCEPT_CURRENT_TARGET);
		ASSERT_TRUE(chosen);
		EXPECT_NE(chosen.rawPointer(), static_cast<gameobjects::Creature*>(far.player.get()));
	}
	EXPECT_TRUE(aggroList.getTarget(AggroTarget::RANDOM)) << "RANDOM may pick the current target too";

	// stopHating drops the hate to 0, and streamValidTargetInfo's `getHate() > 0` drops the entry
	aggroList.stopHating(*far.player);
	EXPECT_EQ(aggroList.getTarget(AggroTarget::MOST_HATED).rawPointer(), static_cast<gameobjects::Creature*>(mid.player.get()));

	// the range overload: 15 m leaves only the character 10 m away, 5 m leaves nobody
	EXPECT_EQ(aggroList.getTarget(AggroTarget::MOST_HATED, 15.0f).rawPointer(), static_cast<gameobjects::Creature*>(near.player.get()));
	EXPECT_FALSE(aggroList.getTarget(AggroTarget::MOST_HATED, 5.0f));
}

TEST_F(CombatDamageTest, ACharactersAggroListOnlyAsksItsKnownList) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// PlayerAggroList.java:16-19 overrides AggroList.isAware with the known-list check alone: no SANCTUARY arm and no tribe relation
	// (AggroList.java:198-208). The monster's own aggro list is the control - it refuses a creature of a tribe it is not hostile to.
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID);
	Ref<gameobjects::Npc> otherMonster = makeNpc(SPARKIE_NPC_ID, 505, 500, 10);
	PlayerFixture fixture = makeLevelOnePlayer(9240, 501, 500, 10);
	PlayerFixture ally = makeLevelOnePlayer(9241, 502, 500, 10); // the same race: no hostility, not an enemy

	// an unknown creature is refused, whatever the owner is
	fixture.player->getAggroList().addDamage(*sparkie, 40, true, skillengine::model::HopType::DAMAGE);
	EXPECT_EQ(fixture.player->getAggroList().getHate(*sparkie), 0) << "isAware answers false without the known-list relation";
	EXPECT_EQ(fixture.player->getAggroList().getFinalDamageList().getTotalDamage(), 0);

	KnownListPairing::pair(*sparkie, *fixture.player);
	KnownListPairing::pair(*ally.player, *fixture.player);
	KnownListPairing::pair(*sparkie, *otherMonster);

	fixture.player->getAggroList().addDamage(*sparkie, 40, true, skillengine::model::HopType::DAMAGE);
	EXPECT_GT(fixture.player->getAggroList().getHate(*sparkie), 0) << "known: the character records the monster";

	fixture.player->getAggroList().addDamage(*ally.player, 15, true, skillengine::model::HopType::DAMAGE);
	EXPECT_GT(fixture.player->getAggroList().getHate(*ally.player), 0)
		<< "a character of the same race is neither hostile nor an enemy: only the override lets this one through";

	// the control: the base AggroList refuses exactly that creature relation
	sparkie->getAggroList().addDamage(*otherMonster, 15, true, skillengine::model::HopType::DAMAGE);
	EXPECT_EQ(sparkie->getAggroList().getHate(*otherMonster), 0) << "MONSTER is not hostile to MONSTER in this fixture's tribe relations";
	EXPECT_EQ(sparkie->getAggroList().stream().size(), 0u);
}

// ------------------------------------------------------------------------------- StatFunctions.calculateDPReward (B-03)

TEST_F(CombatDamageTest, DivinePowerRewardIsTheTargetLevelTimesItsRatingMultiplier) {
	// StatFunctions.java:96-107: `targetLevel * calculateRatingMultiplier(rating)`, scaled by the XPRewardEnum percentage of the level
	// difference and by Rates.DP_PVE. The rating multiplier has its own table case in CombatMathTest, but nothing read it *through* this body,
	// so a port that dropped the factor kept every rating table green and still paid the wrong DP for every kill in the game.
	struct Row {
		NpcRating rating;
		int32_t reward;
	};
	const Row rows[] = {{NpcRating::JUNK, 2}, {NpcRating::NORMAL, 2}, {NpcRating::ELITE, 3}, {NpcRating::HERO, 4}, {NpcRating::LEGENDARY, 5}};
	for (const Row& row : rows) {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		publishNpcData(ratingTemplate(row.rating, NpcRank::NOVICE));
		Ref<gameobjects::Npc> npc = makeNpc(RATING_NPC_ID); // level 1
		PlayerFixture fixture = makeLevelOnePlayer(9250);
		ASSERT_EQ(npc->getLevel(), 1);
		// baseDP = 1 * multiplier, xpRewardFrom(0) is 100 %, so floor(baseDP * 100 / 100f) is the multiplier itself
		EXPECT_EQ(StatFunctions::calculateDPReward(*fixture.player, *npc), row.reward)
			<< "rating " << static_cast<int32_t>(row.rating) << ": the rank plays no part in DP";
	}

	// the level of the target and the level difference are the other two factors (the gate's monster: level 2, NORMAL -> 2 * 2 = 4)
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	publishNpcData(ratingTemplate(NpcRating::NORMAL, NpcRank::NOVICE));
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID);
	PlayerFixture fixture = makeLevelOnePlayer(9251);
	ASSERT_EQ(sparkie->getLevel(), 2);
	EXPECT_EQ(StatFunctions::calculateDPReward(*fixture.player, *sparkie), 4) << "floor(2 * 2 * 105 / 100f) = floor(4.2)";
	fixture.commonData->setLevel(2);
	ASSERT_EQ(fixture.commonData->getLevel(), 2);
	EXPECT_EQ(StatFunctions::calculateDPReward(*fixture.player, *sparkie), 4) << "same level: floor(4 * 100 / 100f)";
	// 9 is the highest level a starting class reaches (PlayerCommonData.setExp caps a non-daeva at getStartExpForLevel(10))
	fixture.commonData->setLevel(9);
	ASSERT_EQ(fixture.commonData->getLevel(), 9);
	EXPECT_EQ(StatFunctions::calculateDPReward(*fixture.player, *sparkie), 1) << "xpRewardFrom(-7) is 30 %: floor(4 * 30 / 100f) = floor(1.2)";
}

} // namespace
} // namespace aion::gameserver::model::stats::test
