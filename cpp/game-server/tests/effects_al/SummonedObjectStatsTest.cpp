// P5-01's stat containers of the summoned creatures (M5b-2's stats group, ported 2026-10-02 on the owner's request): SummonGameStats and
// SummonLifeStats (a Spiritmaster's spirit), ServantGameStats, TrapGameStats and HomingGameStats (the three classes Servant, Trap and Homing
// install in setupStatContainers, which were AION_UNPORTED until now). The objects are built for real on the DaevaEffectTest fixture with a
// Daeva master: each test binds the npc templates it needs (the name decides several stats) into NPC_DATA, which the summoned objects' base
// initializer reads. A case that reaches an AION_UNPORTED site fails in TearDown.

#include "DaevaEffectsTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/SummonController.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/SkillElement.h"
#include "aion/gameserver/model/gameobjects/Homing.h"
#include "aion/gameserver/model/gameobjects/Servant.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/Trap.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/FlyState.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/HomingGameStats.h"
#include "aion/gameserver/model/stats/container/ServantGameStats.h"
#include "aion/gameserver/model/stats/container/SummonGameStats.h"
#include "aion/gameserver/model/stats/container/SummonLifeStats.h"
#include "aion/gameserver/model/stats/container/TrapGameStats.h"
#include "aion/gameserver/model/summons/SummonMode.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/utils/stats/StatFunctions.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::effecttest {
namespace {

using gameserver::model::PlayerClass;
using gameserver::model::gameobjects::Homing;
using gameserver::model::gameobjects::Servant;
using gameserver::model::gameobjects::Summon;
using gameserver::model::gameobjects::Trap;
using gameserver::model::gameobjects::VisibleObject;
using gameserver::model::stats::container::StatEnum;
namespace container = gameserver::model::stats::container;

constexpr int32_t LAVA_SPIRIT = 700501;
constexpr int32_t DESTRUCTION_TRAP = 700502;
constexpr int32_t FIRE_ENERGY = 700503;
constexpr int32_t SERVANT = 700504;
constexpr int32_t FIRE_SPIRIT = 700505;
constexpr int32_t HOMING_SKILL = 64501;

std::string npcXml(int32_t npcId, std::string_view name, std::string_view stats) {
	return R"(<npc_template name_id="1" npc_id=")" + std::to_string(npcId) + R"(" level="20" name=")" + std::string(name) +
		R"(" attack_speed="2000" arange="2" rank="NOVICE" rating="NORMAL" tribe="GENERAL">)" + std::string(stats) + "</npc_template>";
}

const std::string NPC_TEMPLATES_XML = "<npc_templates>" +
	npcXml(LAVA_SPIRIT, "lava spirit", R"(<stats maxHp="2000" maxMp="100" macc="100" matk="500"><speeds walk="0.8" run="2.0"/></stats>)") +
	npcXml(DESTRUCTION_TRAP, "destruction trap", R"(<stats maxHp="100" maxMp="100" macc="100"><speeds walk="0.8" run="2.0"/></stats>)") +
	npcXml(FIRE_ENERGY, "fire energy", R"(<stats maxHp="100" maxMp="100" macc="100" matk="200"><speeds walk="0.8" run="2.0"/></stats>)") +
	npcXml(SERVANT, "healing servant", R"(<stats maxHp="100" maxMp="100" macc="100"><speeds walk="0.8" run="2.0"/></stats>)") +
	npcXml(FIRE_SPIRIT, "fire spirit", R"(<stats maxHp="2000" maxMp="100" macc="100" matk="500"><speeds walk="0.8" run="2.0"/></stats>)") +
	"</npc_templates>";

/** the skill of the homing, at level 4 (HomingGameStats.getMainHandMAttack's switch on skill.getLvl()) */
const std::string HOMING_SKILL_XML = templateXml(
	R"(skill_id="64501" name="homing test" nameId="1" stack="TEST_HOMING" lvl="4" skilltype="MAGICAL" skillsubtype="ATTACK" tslot="NONE")"
	R"( activation="ACTIVE" cooldown="0" duration="0")",
	R"(<dummy e="1" />)");

class SummonedObjectStatsTest : public DaevaEffectTest {
protected:
	void SetUp() override {
		DaevaEffectTest::SetUp();
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(npcContext, NPC_TEMPLATES_XML));
	}

	void TearDown() override {
		objects.clear();
		DaevaEffectTest::TearDown();
		dataholders::DataManager::NPC_DATA.resetForTests();
	}

	gameserver::model::templates::spawns::SpawnTemplate& spawnOf(int32_t npcId) {
		Ref<gameserver::model::templates::spawns::SpawnGroup> group = gameserver::model::templates::spawns::SpawnGroup::create(POETA, npcId, 0, nullptr);
		gameserver::model::templates::spawns::SpawnTemplate& spawnTemplate =
			group->addSpawnTemplate(std::make_unique<EffectTestSpawnTemplate>(*group, 505.0f, 500.0f, 100.0f));
		groups.push_back(group);
		return spawnTemplate;
	}

	Ref<Summon> summonOf(Player& master, int32_t npcId = LAVA_SPIRIT) {
		Ref<Summon> summon = VisibleObject::create<Summon>(utils::idfactory::IDFactory::getInstance().nextId(),
			std::make_unique<controllers::SummonController>(), spawnOf(npcId), dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId), master,
			0);
		summon->setKnownlist(std::make_unique<world::knownlist::KnownList>(*summon)); // SummonsService.createSummon gives it one at spawn
		objects.push_back(Ptr<Creature>(*summon));
		return summon;
	}

	xml::LoadContext npcContext;
	std::vector<Ref<gameserver::model::templates::spawns::SpawnGroup>> groups;
	std::vector<Ref<Creature>> objects;
};

// ---- SummonGameStats / SummonLifeStats -------------------------------------------------------------------------------------------------------

TEST_F(SummonedObjectStatsTest, ALavaSpiritsResistancesAndSpeedsFollowItsNameAndMaster) {
	EFFECT_TEST_SCOPE;
	Ref<Player> master = daeva(8801, PlayerClass::SPIRIT_MASTER);
	Ref<Summon> spirit = summonOf(*master);
	container::SummonGameStats& stats = *spirit->getGameStats();

	// SummonGameStats.getStat: the lava spirit's +100 to paralyze/sleep/poison, +200 earth and fire resistance
	EXPECT_EQ(stats.getStat(StatEnum::PARALYZE_RESISTANCE, 0)->getBase(), 100);
	EXPECT_EQ(stats.getStat(StatEnum::FIRE_RESISTANCE, 0)->getBase(), 200);
	EXPECT_EQ(stats.getStat(StatEnum::EARTH_RESISTANCE, 0)->getBase(), 200);
	EXPECT_EQ(stats.getStat(StatEnum::WATER_RESISTANCE, 0)->getBase(), 0) << "only the tempest spirit and the fire spirit change it";

	// getMovementSpeed: Math.round(runSpeed * 1000), and 3000 more while the master flies; getAttackRange: arange * 1000
	EXPECT_EQ(stats.getMovementSpeed()->getBase(), 2000);
	master->setFlyState(gameserver::model::gameobjects::state::FlyState::FLYING);
	master->setState(gameserver::model::gameobjects::state::CreatureState::FLYING);
	EXPECT_EQ(stats.getMovementSpeed()->getBase(), 5000);
	EXPECT_EQ(stats.getAttackRange()->getBase(), 2000);
	EXPECT_EQ(stats.getBaseAttackSpeed(), 2000);

	// getHpRegenRate: (int) (maxHp * 0.025f), or 0.05f at rest; getMpRegenRate throws
	const int32_t maxHp = spirit->getLifeStats()->getMaxHp();
	EXPECT_EQ(stats.getHpRegenRate()->getBase(), static_cast<int32_t>(static_cast<float>(maxHp) * 0.025f));
	spirit->setMode(gameserver::model::summons::SummonMode::REST);
	EXPECT_EQ(stats.getHpRegenRate()->getBase(), static_cast<int32_t>(static_cast<float>(maxHp) * 0.05f));
	EXPECT_THROW(stats.getMpRegenRate(), runtime::IllegalStateException);
}

TEST_F(SummonedObjectStatsTest, ASpiritsStatInfoGoesToItsMasterAndItsRestoreTaskIsScheduledOnce) {
	EFFECT_TEST_SCOPE;
	Ref<Player> master = daeva(8802, PlayerClass::SPIRIT_MASTER);
	cp::RecordingAionConnection& client = connectLast(*master);
	Ref<Summon> spirit = summonOf(*master);
	client.clearSent();

	spirit->getGameStats()->updateStatsAndSpeedVisually(); // updateStatsVisually -> updateStatInfo: SM_SUMMON_UPDATE to the master
	EXPECT_EQ(packetsOf<network::aion::serverpackets::SM_SUMMON_UPDATE>(client).size(), 1u);

	// SummonLifeStats.triggerRestoreTask schedules LifeStatsRestoreService's HP restore task once (lifeRestoreTask == null && !isDead). Its run
	// is not observed here: HpRestoreTask asks the creature's AI, and a summon has no AI yet (SummonAI is not ported, M5b-2's summon group)
	const size_t before = executor->pendingTaskCount();
	spirit->getLifeStats()->triggerRestoreTask();
	EXPECT_EQ(executor->pendingTaskCount(), before + 1) << "the restore task is scheduled";
	spirit->getLifeStats()->triggerRestoreTask();
	EXPECT_EQ(executor->pendingTaskCount(), before + 1) << "a second trigger finds the task and schedules none";
}

TEST_F(SummonedObjectStatsTest, AFireSpiritAlwaysResistsFireMagic) {
	EFFECT_TEST_SCOPE;
	// StatFunctions.calculateMagicalResistRate's second 1000 arm (StatFunctions.java:612-613): `element != SkillElement.NONE && attacked instanceof
	// Summon summon && element == summon.getAlwaysResistElement()` - the case MagicalCombatTest could not build before SummonGameStats was ported
	Ref<Player> attacker = daeva(8804, PlayerClass::SORCERER);
	Ref<Player> master = daeva(8805, PlayerClass::SPIRIT_MASTER);
	Ref<Summon> spirit = summonOf(*master, FIRE_SPIRIT);
	using gameserver::model::SkillElement;
	using gameserver::utils::stats::StatFunctions;
	ASSERT_EQ(spirit->getAlwaysResistElement(), SkillElement::FIRE);
	EXPECT_EQ(StatFunctions::calculateMagicalResistRate(*attacker, *spirit, 5000, SkillElement::FIRE), 1000)
		<< "the arm returns before accMod";
	EXPECT_NE(StatFunctions::calculateMagicalResistRate(*attacker, *spirit, 5000, SkillElement::WATER), 1000) << "another element";
	EXPECT_NE(StatFunctions::calculateMagicalResistRate(*attacker, *spirit, 5000, SkillElement::NONE), 1000) << "no element";
}

// ---- TrapGameStats / HomingGameStats / ServantGameStats -----------------------------------------------------------------------------------------

TEST_F(SummonedObjectStatsTest, AServantATrapAndAHomingGetTheirOwnStatContainers) {
	EFFECT_TEST_SCOPE;
	bindSkill(HOMING_SKILL_XML);
	Ref<Player> master = daeva(8803, PlayerClass::SPIRIT_MASTER);
	Ref<Trap> trap = VisibleObject::create<Trap>(std::make_unique<controllers::NpcController>(), spawnOf(DESTRUCTION_TRAP), *master);
	Ref<Homing> homing = VisibleObject::create<Homing>(std::make_unique<controllers::NpcController>(), spawnOf(FIRE_ENERGY), int8_t{20}, *master,
		HOMING_SKILL);
	Ref<Servant> servant = VisibleObject::create<Servant>(std::make_unique<controllers::NpcController>(), spawnOf(SERVANT), int8_t{20}, *master);
	objects.push_back(Ptr<Creature>(*trap));
	objects.push_back(Ptr<Creature>(*homing));
	objects.push_back(Ptr<Creature>(*servant));

	// Trap.java:25, Homing.java:35, Servant.java:26: setGameStats(new TrapGameStats / HomingGameStats / ServantGameStats(this))
	ASSERT_NE(dynamic_cast<container::TrapGameStats*>(&*trap->getGameStats()), nullptr);
	ASSERT_NE(dynamic_cast<container::HomingGameStats*>(&*homing->getGameStats()), nullptr);
	ASSERT_NE(dynamic_cast<container::ServantGameStats*>(&*servant->getGameStats()), nullptr);

	// TrapGameStats: a destruction trap's attack range 10 and magical accuracy 1876 (getAttackRange and getMAccuracy by name)
	EXPECT_EQ(trap->getGameStats()->getAttackRange()->getBase(), 10);
	EXPECT_EQ(trap->getGameStats()->getMAccuracy()->getBase(), 1876);

	// HomingGameStats.getMainHandMAttack: a fire energy of a level-4 skill has the power 313
	EXPECT_EQ(homing->getGameStats()->getMainHandMAttack({})->getBase(), 313);

	// ServantGameStats.setUpStats (Servant.setUpStats): the magical accuracy fixed at spawn, the template's macc 100 at base rate 1.2
	servant->setUpStats();
	EXPECT_EQ(servant->getGameStats()->getMAccuracy()->getBase(), 120);
}

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
