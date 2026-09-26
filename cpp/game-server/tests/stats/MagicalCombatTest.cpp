// P5-01 (M5b-2 stage 0, work items M-01, M-02 and M-04): the magical half of the auto-attack arithmetic - the four bodies that decide whether a
// character whose main-hand weapon is magical can swing at all (m5b-client-session.md S-3, m5b2-plan.md §2.5).
//
// - StatFunctions.calculateMagicalResistRate (StatFunctions.java:609-626): the always-resist 1000 arm, the level-difference term behind its
//   `mResi > 0` guard, the PvP `Math.min(500, ...)` clamp and the PvE `limit(MAGICAL_RESIST, ...)` cap. **The summon 1000 arm has no case; see
//   MagicalResistRateAnswersAThousandForAnAlwaysResistTarget for why and for who owns it.**
// - StatFunctions.calculateMagicalCriticalRate (StatFunctions.java:420-433): the Servant/Homing false arm, the MCritical - MCR difference, the
//   limit(MAGICAL_CRITICAL, ...) cap at 500 and the criticalProb arm - which differs from the physical body and is the easiest thing to get wrong.
// - AttackUtil.calculateMagicalStatus (AttackUtil.java:494-506): the decision table, and that `isSkill` skips the resist roll entirely.
// - AttackUtil.calculateMagAttackResult (AttackUtil.java:417-425): the whole swing, its damage derived from the fixture's own numbers.
//
// The stat values are the ones a level-1 character actually has: PlayerClass.createStatsTemplate sets macc = (int) (14.26f * level), mcrit = 50
// and spell_resist = PlayerClass.getMagicalCriticalResist (0 for MAGE) and leaves mresist at the StatsTemplate default 0
// (model/PlayerClassInfo.cpp, model/stats/calc/PlayerStatCalculator.cpp), so a level-1 Mage has MAccuracy 14, MCritical 50, MCR 0, MResist 0
// before equipment.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/attack/AttackStatusInfo.h"
#include "aion/gameserver/controllers/attack/AttackUtil.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/observer/AttackStatusObserver.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/SkillElement.h"
#include "aion/gameserver/model/gameobjects/Homing.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Servant.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/StatCapUtil.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/stats/container/SummonedObjectGameStats.h"
#include "aion/gameserver/model/templates/item/ItemAttackType.h"
#include "aion/gameserver/model/templates/item/ItemAttackTypeInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/skillengine/model/HitType.h"
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
using utils::stats::StatFunctions;

constexpr int32_t POETA = 210010000;

/** The first eleven rows of player_experience_table.xml, which PlayerCommonData.setLevel/setExp read */
const char* const EXPERIENCE_TABLE_XML = R"(<player_experience_table>)"
										 R"(<exp>0</exp><exp>400</exp><exp>1433</exp><exp>3820</exp><exp>9054</exp><exp>17655</exp>)"
										 R"(<exp>30978</exp><exp>52010</exp><exp>82982</exp><exp>126069</exp><exp>182252</exp>)"
										 R"(</player_experience_table>)";

constexpr int32_t SPARKIE_NPC_ID = 210663;  // level 2, the gate's monster (m5b-plan.md D11)
constexpr int32_t WARDED_NPC_ID = 700300;   // the mresist/macc/mcrit/spell_resist rows; its attributes are rebound per case
constexpr int32_t SUMMONED_NPC_ID = 700302; // the Servant / Homing template

/**
 * One template per case would be a dozen; the mresist/macc/mcrit/spell_resist rows rebind this one.
 * <p>
 * **Every magical stat is written out, and that is not decoration.** `NpcData::loadData` fills a stat the XML leaves at 0 from
 * `NpcStatCalculation` per rating, rank and level (`dataholders/NpcData.cpp:64-90`): `mresist`, `macc`, `mdef` and `pdef` become level-scaled
 * values and `mcrit` becomes 50. A case that wants a *known* number has to write one, and a case that wants `mResi == 0` cannot use an npc at
 * all - which is why the `mResi > 0` guard below is asserted against a character, whose `createStatsTemplate` never sets mresist.
 * `spell_resist` is the exception: it is only computed for level >= 50, so 0 stays 0 here.
 */
std::string wardedTemplate(int32_t level, int32_t mresist, int32_t macc, int32_t mcrit, int32_t spellResist) {
	return std::string(R"(<npc_template npc_id="700300" name_id="1" name="warded" attack_speed="2000" rating="NORMAL" rank="NOVICE" tribe="MONSTER")")
		+ R"( level=")" + std::to_string(level) + R"(">)" + R"(<stats maxHp="1000" maxMp="0" attack="16" pdef="50" mdef="40")" + R"( mresist=")"
		+ std::to_string(mresist) + R"(" macc=")" + std::to_string(macc) + R"(" mcrit=")" + std::to_string(mcrit) + R"(" spell_resist=")"
		+ std::to_string(spellResist) + R"(")" + R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)";
}

std::string npcTemplatesXml(const std::string& warded) {
	return std::string(R"(<npc_templates>)") + warded
		+ R"(<npc_template npc_id="210663" name_id="1" level="2" name="juvenile sparkie" attack_speed="2142" tribe="MONSTER" race="BEAST")"
		  R"( rating="NORMAL" rank="DISCIPLINED"><stats maxHp="199" maxMp="0" attack="16" pdef="50" mdef="40" evasion="20" accuracy="60" pcrit="10")"
		  R"( mresist="30" macc="20" mcrit="40"><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)"
		  R"(<npc_template npc_id="700302" name_id="1" level="1" name="totem" attack_speed="2000" tribe="MONSTER" rating="NORMAL" rank="NOVICE")"
		  R"(><stats maxHp="100" maxMp="0" attack="10" mcrit="5000" mresist="30" macc="20" mdef="40" pdef="50")"
		  R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats>)"
		  R"(</npc_template>)"
		+ R"(</npc_templates>)";
}

/** GeneralInstanceHandler::getExpMultiplier is AION_UNPORTED (P5-13); nothing here reads it, but WorldMap2DInstance needs a handler */
class MagicalInstanceHandler final : public ::aion::gameserver::instance::handlers::GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
public:
	explicit MagicalInstanceHandler(world::WorldMapInstance& instance)
		: ::aion::gameserver::instance::handlers::GeneralInstanceHandler(instance) {}

	static Ref<MagicalInstanceHandler> create(world::WorldMapInstance& instance) { return runtime::makeRef<MagicalInstanceHandler>(instance); }

protected:
	~MagicalInstanceHandler() override = default;
};

/** Java: the anonymous AttackStatusObserver of AlwaysResistEffect (checkStatus answers RESIST), the first 1000 arm of calculateMagicalResistRate */
class ResistObserver final : public controllers::observer::AttackStatusObserver {
	AION_MAKE_REF_FRIEND
public:
	static Ref<ResistObserver> create() { return runtime::makeRef<ResistObserver>(); }

	bool checkStatus(AttackStatus attackStatus) override { return attackStatus == status.get() && value.get() > 0; }

protected:
	ResistObserver() : AttackStatusObserver(1, AttackStatus::RESIST) {}
	~ResistObserver() override = default;
};

/** A spawn template of the group, like the spawn data of a map */
class MagicalSpawnTemplate final : public templates::spawns::SpawnTemplate {
public:
	MagicalSpawnTemplate(templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/**
 * Servant.setupStatContainers needs ServantGameStats, which has no declaration header yet (P5-01, Servant.cpp), so the test subclass installs the
 * SummonedObject containers instead - exactly what tests/stats/SummonedObjectStatsTest.cpp does for SummonedObject. Nothing below reads the
 * container; the class only has to *be* a Servant, because that is what calculateMagicalCriticalRate's first line tests.
 */
class TestServant final : public gameobjects::Servant {
	AION_MAKE_REF_FRIEND
public:
	TestServant(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate, int8_t level,
		gameobjects::Creature& creator)
		: Servant(key, std::move(controller), spawnTemplate, level, creator) {}

protected:
	~TestServant() override = default;

	void setupStatContainers() override {
		setGameStats(std::make_unique<container::SummonedObjectGameStats>(*this));
		setLifeStats(std::make_unique<container::NpcLifeStats>(*this));
	}
};

/** The same for Homing (HomingGameStats has no header either, Homing.cpp) */
class TestHoming final : public gameobjects::Homing {
	AION_MAKE_REF_FRIEND
public:
	TestHoming(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate, int8_t level,
		gameobjects::Creature& creator, int32_t skillId)
		: Homing(key, std::move(controller), spawnTemplate, level, creator, skillId) {}

protected:
	~TestHoming() override = default;

	void setupStatContainers() override {
		setGameStats(std::make_unique<container::SummonedObjectGameStats>(*this));
		setLifeStats(std::make_unique<container::NpcLifeStats>(*this));
	}
};

/** Binds XML text as T and keeps the object for the process (static data is immortal) */
template <class T>
const T* bindStatic(std::string_view xml) {
	xml::LoadContext context;
	return xml::bindString<T>(context, xml).release();
}

/**
 * The weapon-mastery skills Equipment.checkAvailableEquipSkills asks for before it accepts an equipped weapon (ItemGroupInfo.h: ORB {111},
 * SPELLBOOK {100}); a character without one puts the weapon back into its inventory.
 */
Ref<skill::PlayerSkillList> masterySkillList() {
	std::vector<Ref<skill::PlayerSkillEntry>> owned;
	std::vector<Ptr<skill::PlayerSkillEntry>> entries;
	for (int32_t skillId : {37, 39, 44, 45, 46, 51, 52, 53, 66, 89, 100, 111}) {
		owned.push_back(skill::PlayerSkillEntry::create(skillId, 1, 0, gameobjects::Persistable::PersistentState::NOACTION));
		entries.emplace_back(*owned.back());
	}
	return skill::PlayerSkillList::create(entries);
}

class MagicalCombatTest : public StatsPlayerTest {
protected:
	void SetUp() override {
		StatsPlayerTest::SetUp();
		publishMapStaticDataOnce();
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		publishNpcData(wardedTemplate(2, 50, 20, 40, 0));
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(contexts.emplace_back(), EXPERIENCE_TABLE_XML));

		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		map = world::WorldMap::create(dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(POETA));
		mapInstance = world::WorldMap2DInstance::create(*map, 1, 0, 0, [](world::WorldMapInstance& instance) {
			return Ref<::aion::gameserver::instance::handlers::InstanceHandler>(MagicalInstanceHandler::create(instance));
		});
	}

	void TearDown() override {
		mapInstance = nullptr;
		map = nullptr;
		groups.clear();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
		StatsPlayerTest::TearDown();
	}

	void publishNpcData(const std::string& warded) {
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), npcTemplatesXml(warded)));
	}

	templates::spawns::SpawnTemplate& makeSpawn(int32_t npcId, float x, float y, float z) {
		Ref<templates::spawns::SpawnGroup> group = templates::spawns::SpawnGroup::create(POETA, npcId, 0, nullptr);
		templates::spawns::SpawnTemplate& spawnTemplate = group->addSpawnTemplate(std::make_unique<MagicalSpawnTemplate>(*group, x, y, z));
		groups.push_back(group);
		return spawnTemplate;
	}

	/** places the object in the test map instance, so getPosition()->getWorldMapInstance() answers (Java: World.setPosition) */
	void place(gameobjects::VisibleObject& object, float x, float y, float z) {
		object.setPosition(world::WorldPosition::create(POETA, x, y, z, int8_t{0}, mapInstance->getRegion(x, y, z)));
		object.getPosition()->setIsSpawned(true);
	}

	Ref<gameobjects::Npc> makeNpc(int32_t npcId, float x = 500, float y = 500, float z = 10) {
		Ref<gameobjects::Npc> npc = gameobjects::VisibleObject::create<gameobjects::Npc>(std::make_unique<controllers::NpcController>(),
			makeSpawn(npcId, x, y, z), dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		place(*npc, x, y, z);
		return npc;
	}

	/**
	 * PlayerGameStats interns the stats template of the class and level once, in the constructor
	 * (`PlayerGameStats.cpp:108`), and `PlayerController::onLevelChange` is what re-reads it after a level change
	 * (`PlayerController.cpp:685`). StatsTestSupport's makePlayer creates the character before the level is known, so the level-up call has to be
	 * made here too - without it every character would carry the level-0 template, whose magical accuracy is `(int) (14.26f * 0)` = 0.
	 */
	PlayerFixture makeMage(int32_t objectId, int32_t level = 1, float x = 501, float y = 500, float z = 10) {
		PlayerFixture fixture = makePlayer(objectId, PlayerClass::MAGE);
		fixture.commonData->setLevel(level);
		fixture.player->getGameStats()->updateStatsTemplate();
		place(*fixture.player, x, y, z);
		return fixture;
	}

	PlayerFixture makeLevelOneMage(int32_t objectId, float x = 501, float y = 500, float z = 10) {
		return makeMage(objectId, 1, x, y, z);
	}

	/** An orb: two-handed, MAGICAL_FIRE, the attack type Player.getAttackType() answers with (Player.java:937-941) */
	Ref<gameobjects::Item> equipOrb(gameobjects::player::Player& player, int32_t itemObjectId, int32_t magicalAccuracy, int32_t critical) {
		player.setSkillList(masterySkillList());
		const std::string xml = std::string(R"(<item_template id="100900001" level="1" item_group="ORB" attack_type="MAGICAL_FIRE">)")
			+ R"(<weapon_stats hit_count="1" attack_range="1500" attack_speed="2000" max_damage="60" min_damage="40" magical_accuracy=")"
			+ std::to_string(magicalAccuracy) + R"(" critical=")" + std::to_string(critical) + R"("/></item_template>)";
		Ref<gameobjects::Item> orb = gameobjects::Item::create(itemObjectId, bindStatic<templates::item::ItemTemplate>(xml), 1, true,
			items::getSlotIdMask(items::ItemSlot::MAIN_HAND));
		player.getEquipment().onLoadHandler(*orb);
		return orb;
	}

	std::deque<xml::LoadContext> contexts;
	std::vector<Ref<templates::spawns::SpawnGroup>> groups;
	Ref<world::WorldMap> map;
	Ref<world::WorldMapInstance> mapInstance;
};

// -------------------------------------------------------------------------- StatFunctions.calculateMagicalResistRate (M-01)

TEST_F(MagicalCombatTest, MagicalResistRateIsTheTargetsResistMinusTheAttackersAccuracy) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// The stat values a level-1 Mage and the gate's monster actually have. The answer is a per-mille chance, because calculateMagicalStatus rolls
	// `Rnd.get(1, 1000) <= rate` against it - so a negative rate is "never resists" and 186 is "18.6 % of swings resist".
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID);
	PlayerFixture mage = makeLevelOneMage(9301);
	ASSERT_EQ(mage.player->getGameStats()->getMAccuracy()->getCurrent(), 14) << "(int) (14.26f * 1), PlayerStatCalculator.calculateMagicalAccuracy";
	ASSERT_EQ(sparkie->getGameStats()->getMResist()->getCurrent(), 30) << R"(the fixture's <stats mresist="30">)";

	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*mage.player, *sparkie, 0, SkillElement::NONE), 16)
		<< "mResist(30) - mAccuracy(14) - accMod(0): 1.6 % of a level-1 Mage's swings are resisted";
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*mage.player, *sparkie, 5, SkillElement::NONE), 11) << "accMod is subtracted, not added";
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*mage.player, *sparkie, -5, SkillElement::NONE), 21);

	// a better warded monster of the same level: 200 - 14 = 186, i.e. 18.6 % of the swings
	publishNpcData(wardedTemplate(2, 200, 20, 40, 0));
	Ref<gameobjects::Npc> warded = makeNpc(WARDED_NPC_ID, 505, 500, 10);
	ASSERT_EQ(warded->getGameStats()->getMResist()->getCurrent(), 200);
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*mage.player, *warded, 0, SkillElement::NONE), 186);

	// the other direction: the two creatures are not interchangeable. The monster's macc is 20 and a character's MResist is 0 at every level.
	ASSERT_EQ(warded->getGameStats()->getMAccuracy()->getCurrent(), 20);
	ASSERT_EQ(mage.player->getGameStats()->getMResist()->getCurrent(), 0) << "createStatsTemplate never sets mresist";
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*warded, *mage.player, 0, SkillElement::NONE), -20)
		<< "0 - 20 - 0; a body that read the attacker's MResist would answer 180 here";

	// and the weapon's magical accuracy reaches the stat (PlayerGameStats.getMAccuracy adds the main hand's)
	equipOrb(*mage.player, 9302, 40, 0);
	ASSERT_TRUE(mage.player->getEquipment().getMainHandWeapon()) << "the orb was not equipped";
	ASSERT_EQ(mage.player->getGameStats()->getMAccuracy()->getCurrent(), 54);
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*mage.player, *warded, 0, SkillElement::NONE), 146);
}

TEST_F(MagicalCombatTest, MagicalResistRateAddsAHundredPerLevelAboveFourOnlyForAWardedTarget) {
	// StatFunctions.java:619-620: `if (mResi > 0 && levelDiff > 4) resistRate += (levelDiff - 4) * 100`. Both halves matter: the guard keeps
	// AI#modifyOwnerStat's zeroed resist at zero, and the threshold is strictly greater than 4.
	struct Row {
		int32_t level;
		int32_t mresist;
		int32_t expected;
		const char* why;
	};
	const Row rows[] = {
		{2, 50, 36, "levelDiff 1: no term"},
		{5, 50, 36, "levelDiff 4 is not > 4"},
		{6, 50, 136, "levelDiff 5: + (5 - 4) * 100"},
		{7, 50, 236, "levelDiff 6: + (6 - 4) * 100"},
		{11, 50, 636, "levelDiff 10: + 600"},
		{1, 50, 36, "a target below the attacker's level: levelDiff 0"},
	};
	int32_t nextId = 9310;
	for (const Row& row : rows) {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		publishNpcData(wardedTemplate(row.level, row.mresist, 20, 40, 0));
		Ref<gameobjects::Npc> warded = makeNpc(WARDED_NPC_ID);
		PlayerFixture mage = makeLevelOneMage(nextId++);
		ASSERT_EQ(mage.player->getLevel(), 1);
		ASSERT_EQ(warded->getLevel(), row.level);
		EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*mage.player, *warded, 0, SkillElement::NONE), row.expected) << row.why;
	}

	// The `mResi > 0` half of the guard needs a target whose MResist really is 0, and no npc can be one: NpcData fills a zero `mresist` from
	// NpcStatCalculation. A character can - createStatsTemplate sets every other magical stat and never touches mresist - so the guard is
	// asserted with a level-7 character attacked by a level-1 npc, six levels below it.
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	publishNpcData(wardedTemplate(1, 50, 20, 40, 0));
	Ref<gameobjects::Npc> lowNpc = makeNpc(WARDED_NPC_ID);
	PlayerFixture highMage = makeMage(9319, 7);
	ASSERT_EQ(highMage.player->getLevel(), 7);
	ASSERT_EQ(highMage.player->getGameStats()->getMResist()->getCurrent(), 0) << "a character has no magical resist at any level";
	ASSERT_EQ(lowNpc->getGameStats()->getMAccuracy()->getCurrent(), 20);
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*lowNpc, *highMage.player, 0, SkillElement::NONE), -20)
		<< "levelDiff is 6, but mResi is 0, so the term is skipped: without the guard the answer would be 180";
}

TEST_F(MagicalCombatTest, MagicalResistRateAnswersAThousandForAnAlwaysResistTarget) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// Arm 1 (StatFunctions.java:610-611): an AttackStatusObserver that answers RESIST - what AlwaysResistEffect attaches - short-circuits to 1000
	// before any stat is read. 1000 is the only value that makes `Rnd.get(1, 1000) <= rate` certain.
	PlayerFixture mage = makeLevelOneMage(9320);
	publishNpcData(wardedTemplate(2, 50, 20, 40, 0));
	Ref<gameobjects::Npc> warded = makeNpc(WARDED_NPC_ID);
	ASSERT_EQ(StatFunctions::calculateMagicalResistRate(*mage.player, *warded, 0, SkillElement::NONE), 36) << "before the observer";

	Ref<controllers::observer::AttackStatusObserver> alwaysResist = ResistObserver::create();
	warded->getObserveController()->addAttackCalcObserver(*alwaysResist);
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*mage.player, *warded, 0, SkillElement::NONE), 1000);
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*mage.player, *warded, 5000, SkillElement::FIRE), 1000)
		<< "the arm returns before accMod and before the element";
	warded->getObserveController()->removeAttackCalcObserver(*alwaysResist);
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*mage.player, *warded, 0, SkillElement::NONE), 36) << "and it is the observer, not the npc";

	// Arm 2 (StatFunctions.java:612-613), `element != SkillElement.NONE && attacked instanceof Summon summon && element ==
	// summon.getAlwaysResistElement()`, **has no unit coverage in this wave and that is a real gap, not an oversight**. A live `Summon` cannot be
	// built in a test today: `Summon::postConstruct` installs `SummonLifeStats`, whose constructor reads `getMaxHp()` through
	// `SummonGameStats::getStatsTemplate`, which is `AION_UNPORTED` (`model/stats/container/SummonGameStats.cpp:37`, P5-01 but not one of stage
	// 0's four bodies), so `VisibleObject::create<Summon>` throws before the object exists. The case below is what *is* reachable: the arm must
	// not fire for a creature that is no Summon, whatever element is passed.
	//
	// The lane that ports SummonGameStats owns the missing case; it is recorded in docs/deviations/P5-01.md.
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*mage.player, *warded, 0, SkillElement::FIRE), 36)
		<< "an ordinary npc is not a Summon, so an element changes nothing";
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*mage.player, *warded, 0, SkillElement::WATER), 36);
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*mage.player, *mage.player, 0, SkillElement::FIRE), -14)
		<< "and neither is a character (0 - 14, through the PvP exit)";
}

TEST_F(MagicalCombatTest, MagicalResistRateClampsAtFiveHundredInPvpAndAtNineHundredOtherwise) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// StatFunctions.java:622-626. The two exits are different functions, not two caps: PvP is `Math.min(500, resistRate)` with no lower bound and
	// no StatCapUtil rule, PvE is `(int) limit(MAGICAL_RESIST, resistRate)` whose difference limit is 900 (StatCapUtil.cpp:78).
	ASSERT_EQ(calc::StatCapUtil::getDifferenceLimit(container::StatEnum::MAGICAL_RESIST), 900);
	PlayerFixture attacker = makeLevelOneMage(9330);
	PlayerFixture defender = makeLevelOneMage(9331, 502, 500, 10);
	publishNpcData(wardedTemplate(2, 100, 20, 40, 0));
	Ref<gameobjects::Npc> warded = makeNpc(WARDED_NPC_ID);
	ASSERT_EQ(attacker.player->getGameStats()->getMAccuracy()->getCurrent(), 14);
	ASSERT_EQ(defender.player->getGameStats()->getMResist()->getCurrent(), 0);
	ASSERT_EQ(warded->getGameStats()->getMResist()->getCurrent(), 100);

	// a negative accMod is the only way to drive the rate up without a stat effect: 0 - 14 - accMod
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*attacker.player, *defender.player, -600, SkillElement::NONE), 500)
		<< "586 clamped by Math.min(500, ...)";
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*attacker.player, *defender.player, -1000, SkillElement::NONE), 500)
		<< "986 too - the PvP arm never reaches limit(MAGICAL_RESIST, ...), which would answer 900";
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*attacker.player, *defender.player, -400, SkillElement::NONE), 386)
		<< "below 500 the clamp does not bite";
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*attacker.player, *defender.player, 0, SkillElement::NONE), -14)
		<< "Math.min has no lower bound, so a negative rate stays negative";

	// the same shape against an npc takes the other exit (its mresist is 100, so the rate is 100 - 14 - accMod)
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*attacker.player, *warded, -600, SkillElement::NONE), 686) << "no PvP clamp";
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*attacker.player, *warded, -1000, SkillElement::NONE), 900) << "limit(MAGICAL_RESIST, 1086)";
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*attacker.player, *warded, -100000, SkillElement::NONE), 900);
	// and so does an npc attacking a character: the clamp needs BOTH to be players (0 - 20 + 1000)
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*warded, *defender.player, -1000, SkillElement::NONE), 900);
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*warded, *defender.player, -400, SkillElement::NONE), 380)
		<< "and below the 900 limit it passes through, where a PvP clamp would have answered 380 as well - so the row above is the one that parts them";
}

// --------------------------------------------------------------------- StatFunctions.calculateMagicalCriticalRate (M-01)

TEST_F(MagicalCombatTest, MagicalCriticalRateIsTheAttackersMCriticalMinusTheTargetsMCR) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// A level-1 Mage has MCritical 50 and the gate's monster MCR 0, so `Rnd.nextInt(1000) < 50` - one magical critical in twenty swings.
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID);
	PlayerFixture mage = makeLevelOneMage(9340);
	ASSERT_EQ(mage.player->getGameStats()->getMCritical()->getCurrent(), 50) << "createStatsTemplate sets mcrit = 50 at every level";
	ASSERT_EQ(sparkie->getGameStats()->getMCR()->getCurrent(), 0)
		<< "spell_resist is the one magical stat NpcData does not fill below level 50 (NpcData.cpp:87)";

	auto criticals = [&](gameobjects::Creature& attacker, gameobjects::Creature& attacked, int32_t criticalProb, int32_t draws) {
		commons::utils::Rnd::seedCurrentThreadForTests(20260923);
		int32_t hits = 0;
		for (int32_t i = 0; i < draws; i++)
			hits += StatFunctions::calculateMagicalCriticalRate(attacker, attacked, criticalProb) ? 1 : 0;
		return hits;
	};

	const int32_t atFifty = criticals(*mage.player, *sparkie, 100, 10000);
	EXPECT_GE(atFifty, 400) << "50 of 1000 draws, over 10,000 draws";
	EXPECT_LE(atFifty, 600);

	// a target whose MCR cancels the attacker's MCritical never takes a magical critical
	publishNpcData(wardedTemplate(2, 50, 20, 40, 50));
	Ref<gameobjects::Npc> warded = makeNpc(WARDED_NPC_ID, 505, 500, 10);
	ASSERT_EQ(warded->getGameStats()->getMCR()->getCurrent(), 50);
	EXPECT_EQ(criticals(*mage.player, *warded, 100, 2000), 0) << "50 - 50 = 0, and `Rnd.nextInt(1000) < 0` is never true";

	// and a magical weapon's `critical` reaches MCritical, so the same target starts taking criticals again
	equipOrb(*mage.player, 9341, 0, 120);
	ASSERT_EQ(mage.player->getGameStats()->getMCritical()->getCurrent(), 170) << "PlayerGameStats.getMCritical adds a MAGICAL weapon's critical";
	const int32_t withOrb = criticals(*mage.player, *warded, 100, 10000);
	EXPECT_GE(withOrb, 1000) << "170 - 50 = 120 of 1000";
	EXPECT_LE(withOrb, 1400);
}

TEST_F(MagicalCombatTest, MagicalCriticalRateIsCappedAtFiveHundredOfAThousand) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// limit(MAGICAL_CRITICAL, critical) has difference limit 500 (StatCapUtil.cpp:77). The assertion is exact rather than statistical: the Rnd
	// sequence is the same for both npcs, so two rates that the cap folds together must produce the *same* outcome for every draw, and a rate
	// below the cap must not.
	ASSERT_EQ(calc::StatCapUtil::getDifferenceLimit(container::StatEnum::MAGICAL_CRITICAL), 500);
	PlayerFixture mage = makeLevelOneMage(9350);

	auto outcomes = [&](int32_t mcrit) {
		publishNpcData(wardedTemplate(2, 50, 20, mcrit, 0));
		Ref<gameobjects::Npc> attacker = makeNpc(WARDED_NPC_ID);
		EXPECT_EQ(attacker->getGameStats()->getMCritical()->getCurrent(), mcrit);
		commons::utils::Rnd::seedCurrentThreadForTests(31337);
		std::vector<bool> result;
		for (int32_t i = 0; i < 2000; i++)
			result.push_back(StatFunctions::calculateMagicalCriticalRate(*attacker, *mage.player, 100));
		return result;
	};

	const std::vector<bool> atFiveHundred = outcomes(500);
	const std::vector<bool> atSixHundred = outcomes(600);
	const std::vector<bool> atTenThousand = outcomes(10000);
	const std::vector<bool> atFourHundred = outcomes(400);
	EXPECT_EQ(atSixHundred, atFiveHundred) << "600 and 500 are the same rate once limit(MAGICAL_CRITICAL, ...) has folded them";
	EXPECT_EQ(atTenThousand, atFiveHundred) << "and so is 10,000 - a port without the cap would crit on every draw";
	EXPECT_NE(atFourHundred, atFiveHundred) << "400 is below the cap, so it must differ";

	const int32_t capped = static_cast<int32_t>(std::count(atFiveHundred.begin(), atFiveHundred.end(), true));
	EXPECT_GE(capped, 900) << "500 of 1000 over 2,000 draws";
	EXPECT_LE(capped, 1100);
	EXPECT_LT(std::count(atFourHundred.begin(), atFourHundred.end(), true), capped);
}

TEST_F(MagicalCombatTest, MagicalCriticalRateRaisesANonPositiveRateToOneBeforeScalingByCriticalProb) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// StatFunctions.java:424-430. This is where the magical body and the physical one part: checkIsPhysicalCriticalHit scales only
	// `criticalRate > 0`, calculateMagicalCriticalRate first raises a rate of 0 or below to **1** and then scales it - so a skill with a
	// criticalProb above 100 can crit even against a target that cancels the whole stat, and one below 100 keeps a fractional chance.
	PlayerFixture mage = makeLevelOneMage(9360);
	publishNpcData(wardedTemplate(2, 50, 20, 40, 700));
	Ref<gameobjects::Npc> immune = makeNpc(WARDED_NPC_ID);
	ASSERT_EQ(immune->getGameStats()->getMCR()->getCurrent(), 700) << "700 is also MAGICAL_CRITICAL_RESIST's upper cap (StatCapUtil.cpp:80-82)";
	ASSERT_EQ(mage.player->getGameStats()->getMCritical()->getCurrent(), 50) << "so critical = 50 - 700 = -650";

	auto criticals = [&](int32_t criticalProb, int32_t draws) {
		commons::utils::Rnd::seedCurrentThreadForTests(4711);
		int32_t hits = 0;
		for (int32_t i = 0; i < draws; i++)
			hits += StatFunctions::calculateMagicalCriticalRate(*mage.player, *immune, criticalProb) ? 1 : 0;
		return hits;
	};

	EXPECT_EQ(criticals(100, 20000), 0) << "criticalProb 100 skips the whole arm, so -650 stands and nothing ever crits";
	const int32_t doubled = criticals(200, 20000);
	EXPECT_GT(doubled, 0) << "criticalProb 200: -650 becomes 1, then 1 * 2 = 2 of 1000";
	EXPECT_GE(doubled, 15) << "about 40 of 20,000";
	EXPECT_LE(doubled, 90);

	// criticalProb 50 gives 1 * 0.5 = 0.5, and `Rnd.nextInt(1000) < 0.5f` is true only on a draw of 0 - which is a float comparison. A port that
	// truncated the rate to an int would answer 0 here and the case would be the only thing that notices.
	const int32_t halved = criticals(50, 20000);
	EXPECT_GT(halved, 0) << "0.5f of 1000 is still a chance, because the comparison is `int < float`";
	EXPECT_LT(halved, doubled) << "and it is a smaller one than 2 of 1000";

	// EXACTLY zero, which is the boundary Java's `critical <= 0` owns and `critical < 0` would not: a target whose MCR cancels the
	// attacker's MCritical. Without this row, `<= 0` can be weakened to `< 0` and every case above still passes - an attacker who exactly
	// cancels would then never crit however large the skill's criticalProb, because `Rnd::get(1000) < 0` is never true.
	publishNpcData(wardedTemplate(2, 50, 20, 40, 50)); // MCR 50 against the mage's MCritical 50
	Ref<gameobjects::Npc> cancelled = makeNpc(WARDED_NPC_ID, 515, 500, 10);
	ASSERT_EQ(cancelled->getGameStats()->getMCR()->getCurrent(), 50);
	ASSERT_EQ(mage.player->getGameStats()->getMCritical()->getCurrent(), 50) << "so critical is exactly 0, not negative";
	commons::utils::Rnd::seedCurrentThreadForTests(4711);
	int32_t raisedFromZero = 0;
	for (int32_t i = 0; i < 20000; i++)
		raisedFromZero += StatFunctions::calculateMagicalCriticalRate(*mage.player, *cancelled, 200) ? 1 : 0;
	EXPECT_GT(raisedFromZero, 0) << "a rate of exactly 0 is raised to 1 and then doubled: 2 of 1000";
	EXPECT_GE(raisedFromZero, 15);
	EXPECT_LE(raisedFromZero, 90);

	// a positive rate is scaled without the raise, which is the ordinary path
	publishNpcData(wardedTemplate(2, 50, 20, 40, 0));
	Ref<gameobjects::Npc> plain = makeNpc(WARDED_NPC_ID, 505, 500, 10);
	commons::utils::Rnd::seedCurrentThreadForTests(4711);
	int32_t quartered = 0;
	for (int32_t i = 0; i < 20000; i++)
		quartered += StatFunctions::calculateMagicalCriticalRate(*mage.player, *plain, 25) ? 1 : 0;
	EXPECT_GE(quartered, 180) << "50 * 0.25 = 12.5 of 1000 over 20,000 draws";
	EXPECT_LE(quartered, 320);
}

TEST_F(MagicalCombatTest, AServantAndAHomingNeverCritMagically) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// StatFunctions.java:421-422, the first statement: `attacker instanceof Servant || attacker instanceof Homing` -> false, before any stat is
	// read. Npc 700302 carries mcrit 5000, which limit(MAGICAL_CRITICAL, ...) folds to the maximum rate of 500 of 1000 - so an ordinary npc of
	// that template crits about half its draws and a Servant of the same template must crit none of them.
	PlayerFixture master = makeLevelOneMage(9370);
	Ref<gameobjects::Npc> plainNpc = makeNpc(SUMMONED_NPC_ID);
	ASSERT_EQ(plainNpc->getGameStats()->getMCritical()->getCurrent(), 5000);

	commons::utils::Rnd::seedCurrentThreadForTests(20260923);
	int32_t npcCriticals = 0;
	for (int32_t i = 0; i < 500; i++)
		npcCriticals += StatFunctions::calculateMagicalCriticalRate(*plainNpc, *master.player, 100) ? 1 : 0;
	ASSERT_GE(npcCriticals, 200) << "an ordinary npc of the same template crits about half its draws, so the case is not vacuous";
	ASSERT_LE(npcCriticals, 300);

	Ref<TestServant> servant = gameobjects::VisibleObject::create<TestServant>(std::make_unique<controllers::NpcController>(),
		makeSpawn(SUMMONED_NPC_ID, 503, 500, 10), int8_t{1}, *master.player);
	place(*servant, 503, 500, 10);
	ASSERT_EQ(servant->getGameStats()->getMCritical()->getCurrent(), 5000) << "the same template";

	Ref<TestHoming> homing = gameobjects::VisibleObject::create<TestHoming>(std::make_unique<controllers::NpcController>(),
		makeSpawn(SUMMONED_NPC_ID, 504, 500, 10), int8_t{1}, *master.player, 0);
	place(*homing, 504, 500, 10);

	for (int32_t i = 0; i < 500; i++) {
		EXPECT_FALSE(StatFunctions::calculateMagicalCriticalRate(*servant, *master.player, 100)) << "draw " << i;
		EXPECT_FALSE(StatFunctions::calculateMagicalCriticalRate(*homing, *master.player, 100)) << "draw " << i;
		EXPECT_FALSE(StatFunctions::calculateMagicalCriticalRate(*servant, *master.player, 1000)) << "not even with a criticalProb, draw " << i;
	}
	// the arm tests the ATTACKER, so a Servant on the receiving end changes nothing
	commons::utils::Rnd::seedCurrentThreadForTests(20260923);
	int32_t againstServant = 0;
	for (int32_t i = 0; i < 500; i++)
		againstServant += StatFunctions::calculateMagicalCriticalRate(*plainNpc, *servant, 100) ? 1 : 0;
	EXPECT_EQ(againstServant, npcCriticals) << "the same rate and the same seed: the guard reads neither the attacked nor its type";
}

// ------------------------------------------------------------------------- AttackUtil.calculateMagicalStatus (M-02)

TEST_F(MagicalCombatTest, MagicalStatusRollsResistOnlyForANonSkillAndCriticalAfterIt) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// AttackUtil.java:495-506. The `if (!isSkill)` guard is the whole difference between an auto attack and a skill hit: a skill never rolls the
	// resist at all (EffectTemplate does its own resist roll with the skill's accuracy modifier, AttackUtil.java is not asked).
	//
	// The attacker is an npc with mcrit 2000 so that the critical arm is certain once the resist arm has not fired; the target carries mresist
	// 2000, which limit(MAGICAL_RESIST, 1986) folds to 900, i.e. 90 % of the auto-attack rolls.
	publishNpcData(wardedTemplate(2, 2000, 20, 2000, 0));
	Ref<gameobjects::Npc> both = makeNpc(WARDED_NPC_ID);
	PlayerFixture mage = makeLevelOneMage(9380);
	ASSERT_EQ(StatFunctions::calculateMagicalResistRate(*both, *both, 0, SkillElement::NONE), 900) << "limit(MAGICAL_RESIST, 2000 - 20)";

	auto tally = [&](bool isSkill, int32_t criticalProb) {
		commons::utils::Rnd::seedCurrentThreadForTests(20260923);
		int32_t resists = 0;
		int32_t criticals = 0;
		int32_t normals = 0;
		for (int32_t i = 0; i < 2000; i++) {
			switch (AttackUtil::calculateMagicalStatus(*both, *both, criticalProb, isSkill)) {
				case AttackStatus::RESIST:
					resists++;
					break;
				case AttackStatus::CRITICAL:
					criticals++;
					break;
				case AttackStatus::NORMALHIT:
					normals++;
					break;
				default:
					ADD_FAILURE() << "calculateMagicalStatus answers only RESIST, CRITICAL and NORMALHIT";
			}
		}
		return std::vector<int32_t>{resists, criticals, normals};
	};

	const std::vector<int32_t> autoAttack = tally(false, 100);
	EXPECT_GE(autoAttack[0], 1700) << "900 of 1000 rolls resist";
	EXPECT_LE(autoAttack[0], 1900);
	EXPECT_GT(autoAttack[1], 0) << "the swings that got through roll the critical (500 of 1000 after the cap)";
	EXPECT_GT(autoAttack[2], 0) << "and about half of those are a plain hit";
	EXPECT_EQ(autoAttack[0] + autoAttack[1] + autoAttack[2], 2000);

	const std::vector<int32_t> skill = tally(true, 100);
	EXPECT_EQ(skill[0], 0) << "isSkill skips the resist roll entirely (AttackUtil.java:496)";
	EXPECT_EQ(skill[1] + skill[2], 2000);
	EXPECT_GT(skill[1], autoAttack[1]) << "with the resist arm gone, ten times as many swings reach the critical roll";

	// criticalProb is forwarded to calculateMagicalCriticalRate: 0 scales a positive rate to nothing
	const std::vector<int32_t> noCritical = tally(true, 0);
	EXPECT_EQ(noCritical[1], 0) << "criticalProb 0 on a positive rate: 1980 * 0 / 100 = 0";
	EXPECT_EQ(noCritical[2], 2000) << "so every skill hit is a NORMALHIT";
}

TEST_F(MagicalCombatTest, MagicalStatusNeverResistsWhenTheRateIsZeroAndAlwaysWhenTheObserverSaysSo) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// The two ends of `Rnd.get(1, 1000) <= rate`: the draw is inclusive on both ends, so a rate of 0 never resists and the always-resist arm's
	// 1000 always does. A port that wrote `<` instead of `<=`, or Rnd.get(0, 1000), moves exactly one of these.
	PlayerFixture mage = makeLevelOneMage(9390);
	// mresist 14 - the Mage's mAccuracy 14 = a rate of exactly 0; spell_resist 700 puts the critical rate at 50 - 700 and out of the way
	publishNpcData(wardedTemplate(2, 14, 20, 40, 700));
	Ref<gameobjects::Npc> warded = makeNpc(WARDED_NPC_ID);
	ASSERT_EQ(StatFunctions::calculateMagicalResistRate(*mage.player, *warded, 0, SkillElement::NONE), 0);

	commons::utils::Rnd::seedCurrentThreadForTests(4711);
	for (int32_t i = 0; i < 3000; i++)
		ASSERT_EQ(AttackUtil::calculateMagicalStatus(*mage.player, *warded, 100, false), AttackStatus::NORMALHIT)
			<< "draw " << i << ": a rate of 0 must never resist";

	Ref<controllers::observer::AttackStatusObserver> alwaysResist = ResistObserver::create();
	warded->getObserveController()->addAttackCalcObserver(*alwaysResist);
	for (int32_t i = 0; i < 3000; i++)
		ASSERT_EQ(AttackUtil::calculateMagicalStatus(*mage.player, *warded, 100, false), AttackStatus::RESIST) << "draw " << i;
	EXPECT_EQ(AttackUtil::calculateMagicalStatus(*mage.player, *warded, 100, true), AttackStatus::NORMALHIT)
		<< "and even an always-resist target does not resist a skill through this body";
}

// -------------------------------------------------------------------- AttackUtil.calculateMagAttackResult (M-02)

TEST_F(MagicalCombatTest, MagicalAttackGoldenVectorsOfALevelOneMageWithAnOrb) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// The swing m5b-client-session.md S-3 says a Mage could not make. The numbers are golden vectors of THIS port, not of the Java server: Rnd is
	// a xoshiro256++ whose sequence deliberately differs from java.util.random (CONVENTIONS.md "Random numbers"), so only the arithmetic between
	// the draws can be pinned this way. Any change to the formula, to the order of the Rnd draws or to their number moves every one of them.
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID);
	PlayerFixture mage = makeLevelOneMage(9400);
	equipOrb(*mage.player, 9401, 40, 0);
	ASSERT_TRUE(mage.player->getEquipment().getMainHandWeapon());
	ASSERT_EQ(mage.player->getAttackType(), templates::item::ItemAttackType::MAGICAL_FIRE)
		<< "Player.getAttackType() is the main hand template's, which is what routes CreatureController.attackTarget into the magical arm";
	ASSERT_EQ(templates::item::getMagicalElement(mage.player->getAttackType()), SkillElement::FIRE);

	// The whole number, derived from the fixture and not read back from the port:
	//   getMainHandMAttack  = WeaponStats.getMeanDamage() = (40 + 60) / 2            = 50   (PlayerGameStats.cpp:234-241; unlike the physical
	//                                                                                       arm this draws no random number, which is why every
	//                                                                                       unresisted, uncritical swing below is the same)
	//   adjustDamageByStatModifiers: damage - MDef/10                = 50 - 40/10    = 46
	//                                * 1 (NORMALHIT), no movement modifier, PvE ratios 0
	//   adjustDamageByPvpOrPveModifiers: 1 - getNpcLevelDiffMod      = 1 for a one-level difference
	constexpr int32_t NORMAL_HIT = 46;
	ASSERT_EQ(sparkie->getGameStats()->getMDef()->getCurrent(), 40);

	commons::utils::Rnd::seedCurrentThreadForTests(20260923);
	for (int32_t i = 0; i < 8; i++) {
		std::vector<Ref<AttackResult>> results = AttackUtil::calculateMagAttackResult(*mage.player, *sparkie, SkillElement::FIRE, {});
		ASSERT_EQ(results.size(), 1u) << "hit " << i << ": a two-handed orb has no off hand and hit_count 1";
		ASSERT_EQ(results[0]->getAttackStatus(), AttackStatus::NORMALHIT) << "hit " << i << ": this seed rolls no resist and no critical";
		EXPECT_EQ(results[0]->getDamage(), NORMAL_HIT) << "hit " << i;
		EXPECT_EQ(results[0]->getHitType(), skillengine::model::HitType::MAHIT)
			<< "hit " << i << ": StatFunctions.calculateAttackDamage sets MAHIT for every element but NONE";
	}

	// the same seed replays the same sequence
	commons::utils::Rnd::seedCurrentThreadForTests(20260923);
	EXPECT_EQ(AttackUtil::calculateMagAttackResult(*mage.player, *sparkie, SkillElement::FIRE, {})[0]->getDamage(), NORMAL_HIT);

	// the element parameter is forwarded, not dropped: with NONE the chain reads PHYSICAL_ATTACK - which PlayerGameStats.getMainHandPAttack
	// answers 0 for a magical weapon (PlayerGameStats.cpp:188-189) - and PHYSICAL_DEFENSE instead of MAGICAL_ATTACK and MAGICAL_DEFEND
	commons::utils::Rnd::seedCurrentThreadForTests(20260923);
	std::vector<Ref<AttackResult>> asPhysical = AttackUtil::calculateMagAttackResult(*mage.player, *sparkie, SkillElement::NONE, {});
	EXPECT_EQ(asPhysical[0]->getHitType(), skillengine::model::HitType::PHHIT);
	EXPECT_EQ(asPhysical[0]->getDamage(), 1) << "a magical weapon has no physical attack, so the 1-damage floor is all that is left";
	EXPECT_NE(asPhysical[0]->getDamage(), NORMAL_HIT) << "a body that ignored the element would answer the same number";
}

TEST_F(MagicalCombatTest, TheNpcAiScalesTheMagicalDamageItDealsAndTheDamageItTakes) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// AttackUtil.calculateMagAttackResult calls modifyDamageByNpcAi (AttackUtil.java:423) exactly as the physical body does, and the review of
	// this lane proved the statement could be DELETED with every other case of this file still green: the golden vectors fix the damage of a
	// player hitting an npc whose AbstractAI answers the damage unchanged, and the seam cases compare the controller against a replay of this
	// same function, so a step dropped from the function is dropped from both sides of their equality. Only an AI that answers something else
	// can tell the two ports apart - which is what CombatDamageTest.TheNpcAiScalesTheDamageItDealsAndTheDamageItTakes does for the physical half.
	Ref<gameobjects::Npc> sparkie = makeNpc(SPARKIE_NPC_ID);
	PlayerFixture mage = makeLevelOneMage(9420);
	equipOrb(*mage.player, 9421, 40, 0);

	// the npc as the attacked: modifyDamage
	commons::utils::Rnd::seedCurrentThreadForTests(4711);
	const int32_t taken = AttackUtil::calculateMagAttackResult(*mage.player, *sparkie, SkillElement::FIRE, {})[0]->getDamage();
	ASSERT_GT(taken, 3) << "halving must stay above the 1-damage floor";
	sparkie->replaceAi(std::make_unique<ScalingNpcAI>(*sparkie, 1.0f, 0.5f));
	commons::utils::Rnd::seedCurrentThreadForTests(4711);
	EXPECT_EQ(AttackUtil::calculateMagAttackResult(*mage.player, *sparkie, SkillElement::FIRE, {})[0]->getDamage(), taken / 2)
		<< "the attacked npc's AI halved the magical damage it took";

	// the npc as the attacker: modifyOwnerDamage. The npc's own magical attack is what an npc skill will use in M5b-2 stage 1.
	sparkie->replaceAi(std::make_unique<ScalingNpcAI>(*sparkie, 1.0f, 1.0f));
	commons::utils::Rnd::seedCurrentThreadForTests(31337);
	const int32_t dealt = AttackUtil::calculateMagAttackResult(*sparkie, *mage.player, SkillElement::FIRE, {})[0]->getDamage();
	ASSERT_GT(dealt, 1) << "the floor must not be the answer";
	sparkie->replaceAi(std::make_unique<ScalingNpcAI>(*sparkie, 2.0f, 1.0f));
	commons::utils::Rnd::seedCurrentThreadForTests(31337);
	EXPECT_EQ(AttackUtil::calculateMagAttackResult(*sparkie, *mage.player, SkillElement::FIRE, {})[0]->getDamage(), dealt * 2)
		<< "the attacking npc's AI doubled its own magical damage";
}

TEST_F(MagicalCombatTest, AMagicalAttackResistsAndCritsBecauseItAsksCalculateMagicalStatusForAnAutoAttack) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// calculateMagAttackResult passes `criticalProb = 100, isSkill = false` (AttackUtil.java:418). Both arguments are observable: with isSkill
	// true no swing would ever resist, and a resisted swing carries 0 damage because StatFunctions.calculateAttackDamage returns before the
	// weapon arm and adjustDamageByStatModifiers returns before the multipliers.
	publishNpcData(wardedTemplate(2, 400, 20, 40, 0));
	Ref<gameobjects::Npc> warded = makeNpc(WARDED_NPC_ID);
	PlayerFixture mage = makeLevelOneMage(9410);
	equipOrb(*mage.player, 9411, 0, 300);
	ASSERT_EQ(StatFunctions::calculateMagicalResistRate(*mage.player, *warded, 0, SkillElement::NONE), 386)
		<< "the resist roll of an auto attack passes accMod 0 and SkillElement.NONE whatever the weapon's element is";
	ASSERT_EQ(mage.player->getGameStats()->getMCritical()->getCurrent(), 350);

	int32_t resisted = 0;
	int32_t critical = 0;
	int32_t normal = 0;
	commons::utils::Rnd::seedCurrentThreadForTests(31337);
	for (int32_t i = 0; i < 2000; i++) {
		std::vector<Ref<AttackResult>> results = AttackUtil::calculateMagAttackResult(*mage.player, *warded, SkillElement::FIRE, {});
		ASSERT_EQ(results.size(), 1u);
		const AttackStatus status = results[0]->getAttackStatus();
		if (getBaseStatus(status) == AttackStatus::RESIST) {
			resisted++;
			EXPECT_EQ(results[0]->getDamage(), 0) << "a resisted swing carries no damage";
		} else if (isCritical(status)) {
			critical++;
			EXPECT_GE(results[0]->getDamage(), 1);
		} else {
			normal++;
			EXPECT_GE(results[0]->getDamage(), 1) << "adjustDamageByStatModifiers floors a landed hit at 1";
		}
	}
	EXPECT_GE(resisted, 650) << "386 of 1000 over 2,000 swings - isSkill is false, so the resist roll runs";
	EXPECT_LE(resisted, 900);
	EXPECT_GT(critical, 0) << "350 - 0 = 350 of 1000 of what is left, i.e. criticalProb 100 does not scale it away";
	EXPECT_GT(normal, 0);
}

} // namespace
} // namespace aion::gameserver::model::stats::test
