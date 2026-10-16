// P4-11a construction path of SummonedObject and its subclasses, the one the startup run reaches through HousingService.spawnHouses ->
// HouseController.updateSpawns -> VisibleObjectSpawner.spawnHouseNpc (m5a-plan.md W-06): the base initializer looks the NPC template up in
// DataManager.NPC_DATA (Java SummonedObject.java:25, SummonedHouseNpc.java:18), Npc's Objects.requireNonNull turns a missing template into a
// NullPointerException, and setupStatContainers gives the object its game and life stats. Expectations are derived by hand from
// SummonedObject.java, SummonedHouseNpc.java, Npc.java and HouseDecoration.java.
//
// Test doubles (standing in for bodies of later chunks): NPC_DATA, HOUSE_PARTS_DATA and HOUSE_DATA are holders bound from XML text,
// NPC_SKILL_DATA is empty, and the creator of the delegation test is an Npc with a CreatureLifeStats part of fixed HP (as CreatureBodiesTest
// does). SummonedObject itself keeps its real stat containers, so the test covers what the startup run runs.
//
// Stage 3, fixer run: TheFourSubclassOverridesKeepJavasEmptyMasterName pins the four real getMasterName() overrides
// (SummonedHouseNpc.cpp:64-66, Homing.cpp:41-43, Servant.cpp:39-41, Trap.cpp:72-74). Before it, this file only exercised a TestSummonedObject
// without an override and stated in its own expectation what the overrides would do, so rewriting any of them to call the base changed nothing
// here. SummonedHouseNpc is now built for real (from a House of a bound HOUSE_DATA); Homing, Servant and Trap are built through subclasses that
// replace only their AION_UNPORTED setupStatContainers (P5-01 owes the three stat containers, docs/deviations/P4-11a.md).

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/dataholders/HousePartsData.bind.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/gameobjects/Homing.h"
#include "aion/gameserver/model/gameobjects/HouseDecoration.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/NpcObjectType.h"
#include "aion/gameserver/model/gameobjects/Servant.h"
#include "aion/gameserver/model/gameobjects/SummonedHouseNpc.h"
#include "aion/gameserver/model/gameobjects/SummonedObject.h"
#include "aion/gameserver/model/gameobjects/Trap.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/stats/container/SummonedObjectGameStats.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/housing/HousePart.h"
#include "aion/gameserver/model/templates/housing/PartType.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

namespace aion::gameserver::model::gameobjects {
namespace {

using runtime::Ptr;
using runtime::Ref;

#define SUMMONED_TEST_SCOPE runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST))

constexpr int32_t BUTLER_NPC_ID = 800100;
constexpr int32_t CREATOR_NPC_ID = 210001;
constexpr int32_t UNKNOWN_NPC_ID = 999999;

const char* const NPC_TEMPLATES = R"(<npc_templates>)"
								  R"(<npc_template npc_id="800100" level="12" name_id="1" name="butler" rank="NOVICE" rating="NORMAL" tribe="GENERAL">)"
								  R"(<stats maxHp="500" maxMp="200"/></npc_template>)"
								  R"(<npc_template npc_id="210001" level="30" name_id="1" name="creator" rank="NOVICE" rating="NORMAL" tribe="GENERAL">)"
								  R"(<stats maxHp="700" maxMp="300"/></npc_template>)"
								  R"(</npc_templates>)";

const char* const HOUSE_PARTS = R"(<house_parts>)"
								R"(<house_part id="1001" name="basic_roof" quality="COMMON" type="ROOF" building_tags="basic"/>)"
								R"(</house_parts>)";

/** one land with one address and a default building: enough for `new House(address, 0)`, the creator of a SummonedHouseNpc */
const char* const HOUSE_LANDS = R"(<house_lands>)"
								R"(<land id="325001" teleport_npc="810003" manager_npc="810017" sign_home="810007")"
								R"( sign_waiting="810006" sign_sale="810005" sign_nosale="810004">)"
								R"(<addresses><address id="10001" map="700010000" town="1001" x="696.159973" y="1999.969971" z="174.42577"/></addresses>)"
								R"(<buildings><building id="350000" default="true" type="PERSONAL_FIELD" size="HOUSE"/></buildings>)"
								R"(<sale level="50" gold_price="1000000000" point_price="0"/>)"
								R"(<fee>20000000</fee>)"
								R"(<caps room="false" floor="false" emblemId="2" addon="true"/>)"
								R"(</land>)"
								R"(</house_lands>)";

/** Life stats with fixed HP for the creator npc (CreatureBodiesTest uses the same double) */
class FixedLifeStats final : public stats::container::CreatureLifeStats {
public:
	explicit FixedLifeStats(Creature& owner) : CreatureLifeStats(owner, 1000, 100) {}
};

/** The creator of the delegation test: an Npc with the real constructor chain and doubled stat containers */
class CreatorNpc final : public Npc {
	AION_MAKE_REF_FRIEND
public:
	CreatorNpc(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
		const templates::npc::NpcTemplate* objectTemplate)
		: Npc(key, std::move(controller), spawnTemplate, objectTemplate) {}

protected:
	~CreatorNpc() override = default;

	void setupStatContainers() override {
		setGameStats(std::make_unique<stats::container::NpcGameStats>(*this));
		setLifeStats(std::make_unique<FixedLifeStats>(*this));
	}
};

/**
 * SummonedObject has a protected constructor like every visible object; the test subclass adds nothing, so the real base initializer (template
 * lookup) and the real setupStatContainers run. It is SummonedObject itself under test, without the master name, known list and effect
 * controller a SummonedHouseNpc adds (SummonedHouseNpc.java:17-23) - the real class is built by
 * TheFourSubclassOverridesKeepJavasEmptyMasterName below.
 */
class TestSummonedObject final : public SummonedObject {
	AION_MAKE_REF_FRIEND
public:
	TestSummonedObject(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
		int8_t level, Ptr<VisibleObject> creator)
		: SummonedObject(key, std::move(controller), spawnTemplate, level, creator) {}

protected:
	~TestSummonedObject() override = default;
};

/**
 * Homing, Servant and Trap install HomingGameStats, ServantGameStats and TrapGameStats, none of which has a C++ header yet (P5-01), so their
 * real setupStatContainers is AION_UNPORTED and throws out of postConstruct (docs/deviations/P4-11a.md). Each test subclass replaces exactly
 * that one body - with the closest base Java derives the missing class from - and inherits everything else, in particular the constructor that
 * sets the empty master name and the getMasterName() override the class declares.
 */
class TestHoming final : public Homing {
	AION_MAKE_REF_FRIEND
public:
	TestHoming(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate, int8_t level,
		Creature& creator, int32_t skillId)
		: Homing(key, std::move(controller), spawnTemplate, level, creator, skillId) {}

protected:
	~TestHoming() override = default;

	void setupStatContainers() override { // Java: HomingGameStats, which derives SummonedObjectGameStats
		setGameStats(std::make_unique<stats::container::SummonedObjectGameStats>(*this));
		setLifeStats(std::make_unique<stats::container::NpcLifeStats>(*this));
	}
};

class TestServant final : public Servant {
	AION_MAKE_REF_FRIEND
public:
	TestServant(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate, int8_t level,
		Creature& creator)
		: Servant(key, std::move(controller), spawnTemplate, level, creator) {}

protected:
	~TestServant() override = default;

	void setupStatContainers() override { // Java: ServantGameStats, which derives SummonedObjectGameStats
		setGameStats(std::make_unique<stats::container::SummonedObjectGameStats>(*this));
		setLifeStats(std::make_unique<stats::container::NpcLifeStats>(*this));
	}
};

class TestTrap final : public Trap {
	AION_MAKE_REF_FRIEND
public:
	TestTrap(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate, Creature& creator)
		: Trap(key, std::move(controller), spawnTemplate, creator) {}

protected:
	~TestTrap() override = default;

	void setupStatContainers() override { // Java: TrapGameStats, which derives NpcGameStats (TrapGameStats.java:14)
		setGameStats(std::make_unique<stats::container::NpcGameStats>(*this));
		setLifeStats(std::make_unique<stats::container::NpcLifeStats>(*this));
	}
};

class SummonedObjectsTest : public ::testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 4));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		runtime::LeakCensus::getInstance().install();
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		xml::LoadContext context;
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, NPC_TEMPLATES));
		dataholders::DataManager::HOUSE_PARTS_DATA.publish(xml::bindString<dataholders::HousePartsData>(context, HOUSE_PARTS));
		dataholders::DataManager::HOUSE_DATA.publish(xml::bindString<dataholders::HouseData>(context, HOUSE_LANDS));
		butlerGroup = templates::spawns::SpawnGroup::create(210010000, BUTLER_NPC_ID, 0, nullptr);
		butlerSpawn = templates::spawns::SpawnTemplate::create(*butlerGroup, 10.0f, 20.0f, 30.0f, int8_t{0}, 0, std::nullopt, 0);
		creatorGroup = templates::spawns::SpawnGroup::create(210010000, CREATOR_NPC_ID, 0, nullptr);
		creatorSpawn = templates::spawns::SpawnTemplate::create(*creatorGroup, 11.0f, 21.0f, 31.0f, int8_t{0}, 0, std::nullopt, 0);
		unknownGroup = templates::spawns::SpawnGroup::create(210010000, UNKNOWN_NPC_ID, 0, nullptr);
		unknownSpawn = templates::spawns::SpawnTemplate::create(*unknownGroup, 12.0f, 22.0f, 32.0f, int8_t{0}, 0, std::nullopt, 0);
	}

	void TearDown() override {
		butlerSpawn.reset();
		creatorSpawn.reset();
		unknownSpawn.reset();
		butlerGroup.reset();
		creatorGroup.reset();
		unknownGroup.reset();
		runtime::Reclaimer::getInstance().drain();
		runtime::LeakCensus::getInstance().uninstall();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
		dataholders::DataManager::HOUSE_DATA.resetForTests();
		dataholders::DataManager::HOUSE_PARTS_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
	}

	Ref<TestSummonedObject> createSummoned(templates::spawns::SpawnTemplate& spawn, int8_t level, Ptr<VisibleObject> creator) {
		return VisibleObject::create<TestSummonedObject>(std::make_unique<controllers::NpcController>(), spawn, level, creator);
	}

	/** an Npc creator with the real constructor chain and doubled stat containers */
	Ref<CreatorNpc> createCreator() {
		return VisibleObject::create<CreatorNpc>(std::make_unique<controllers::NpcController>(), *creatorSpawn,
			dataholders::DataManager::NPC_DATA->getNpcTemplate(CREATOR_NPC_ID));
	}

	runtime::ManualClock clock{0};
	Ref<templates::spawns::SpawnGroup> butlerGroup;
	Ref<templates::spawns::SpawnGroup> creatorGroup;
	Ref<templates::spawns::SpawnGroup> unknownGroup;
	Ref<templates::spawns::SpawnTemplate> butlerSpawn;
	Ref<templates::spawns::SpawnTemplate> creatorSpawn;
	Ref<templates::spawns::SpawnTemplate> unknownSpawn;
};

TEST_F(SummonedObjectsTest, TheBaseInitializerResolvesTheNpcTemplateOfTheSpawn) {
	SUMMONED_TEST_SCOPE;
	Ref<TestSummonedObject> summoned = createSummoned(*butlerSpawn, int8_t{7}, nullptr);
	ASSERT_TRUE(summoned->getObjectTemplate());
	EXPECT_EQ(summoned->getObjectTemplate()->getTemplateId(), BUTLER_NPC_ID) << "DataManager.NPC_DATA.getNpcTemplate(spawnTemplate.getNpcId())";
	EXPECT_EQ(summoned->getObjectTemplate()->getLevel(), 12);
	EXPECT_EQ(summoned->getLevel(), 7) << "SummonedObject.getLevel returns the constructor argument, not the template level";
}

TEST_F(SummonedObjectsTest, SetupStatContainersGivesGameAndLifeStats) {
	SUMMONED_TEST_SCOPE;
	Ref<TestSummonedObject> summoned = createSummoned(*butlerSpawn, int8_t{7}, nullptr);
	ASSERT_TRUE(summoned->getGameStats()) << "Java: setGameStats(new SummonedObjectGameStats(this))";
	EXPECT_TRUE(dynamic_cast<stats::container::SummonedObjectGameStats*>(summoned->getGameStats().rawPointer()))
		<< "the part is the SummonedObjectGameStats of SummonedObject.setupStatContainers, no longer the NpcGameStats stand-in";
	// what that class changes and the base does not: the five combat stats Java lists carry the summoned bonus rate
	EXPECT_FLOAT_EQ(summoned->getGameStats()->getStat(stats::container::StatEnum::PHYSICAL_ATTACK, 100.0f)->getBonusRate(), 0.2f);
	EXPECT_FLOAT_EQ(summoned->getGameStats()->getStat(stats::container::StatEnum::MAXHP, 100.0f)->getBonusRate(), 1.0f);
	ASSERT_TRUE(summoned->getLifeStats()) << "Java: setLifeStats(new NpcLifeStats(this))";
	EXPECT_EQ(summoned->getLifeStats()->getMaxHp(), 500) << "the <stats maxHp> of the npc template";
	EXPECT_EQ(summoned->getLifeStats()->getCurrentHp(), 500) << "NpcLifeStats starts at the maximum";
	EXPECT_FALSE(summoned->getLifeStats()->isDead());
}

TEST_F(SummonedObjectsTest, AMissingNpcTemplateThrowsNullPointerException) {
	SUMMONED_TEST_SCOPE;
	// Java Npc: Objects.requireNonNull(objectTemplate) on the null lookup result
	EXPECT_THROW(static_cast<void>(createSummoned(*unknownSpawn, int8_t{7}, nullptr)), runtime::NullPointerException);
}

TEST_F(SummonedObjectsTest, WithoutACreatorTheObjectIsItsOwnMaster) {
	SUMMONED_TEST_SCOPE;
	Ref<TestSummonedObject> summoned = createSummoned(*butlerSpawn, int8_t{7}, nullptr);
	EXPECT_FALSE(summoned->getCreator());
	EXPECT_EQ(summoned->getMaster().get(), summoned.get()) << "SummonedObject.getMaster: this unless the creator is a Creature";
	EXPECT_FALSE(summoned->getMasterName()) << "Npc's master name, null while unset";
	EXPECT_EQ(summoned->getCreatorId(), 0);
}

TEST_F(SummonedObjectsTest, ACreatureCreatorSuppliesMasterNameCreatorIdAndMaster) {
	SUMMONED_TEST_SCOPE;
	Ref<CreatorNpc> creator = VisibleObject::create<CreatorNpc>(std::make_unique<controllers::NpcController>(), *creatorSpawn,
		dataholders::DataManager::NPC_DATA->getNpcTemplate(CREATOR_NPC_ID));
	Ref<TestSummonedObject> summoned = createSummoned(*butlerSpawn, int8_t{7}, Ptr<VisibleObject>(*creator));
	EXPECT_EQ(summoned->getCreator().get(), static_cast<VisibleObject*>(creator.get()));
	EXPECT_EQ(summoned->getMaster().get(), static_cast<Creature*>(creator.get())) << "the creator is a Creature";
	ASSERT_TRUE(summoned->getMasterName());
	EXPECT_EQ(*summoned->getMasterName(), creator->getName()) << "SummonedObject.getMasterName falls back to the creator's name";
	EXPECT_EQ(summoned->getCreatorId(), creator->getObjectId());
	EXPECT_EQ(summoned->getRace(), creator->getRace()) << "SummonedObject.getRace delegates to a Creature creator";
}

TEST_F(SummonedObjectsTest, AnEmptyMasterNameReadsAsNullWhichIsWhyTheSubclassesOverrideIt) {
	SUMMONED_TEST_SCOPE;
	Ref<CreatorNpc> creator = VisibleObject::create<CreatorNpc>(std::make_unique<controllers::NpcController>(), *creatorSpawn,
		dataholders::DataManager::NPC_DATA->getNpcTemplate(CREATOR_NPC_ID));
	Ref<TestSummonedObject> summoned = createSummoned(*butlerSpawn, int8_t{7}, Ptr<VisibleObject>(*creator));

	// What Homing.java:28, Servant.java:19, Trap.java:18 and SummonedHouseNpc.java:19-20 do in their constructors. Java's masterName field is
	// then "" and not null, so SummonedObject.getMasterName's `super.getMasterName() == null && creator != null` fallback does not fire and
	// Java returns "". The four real classes are pinned by TheFourSubclassOverridesKeepJavasEmptyMasterName below.
	summoned->setMasterName("");
	EXPECT_FALSE(summoned->Npc::getMasterName()) << "Npc::getMasterName maps the Field<std::string> value \"\" to std::nullopt (Java null)";
	ASSERT_TRUE(summoned->getMasterName());
	EXPECT_EQ(*summoned->getMasterName(), creator->getName())
		<< "so SummonedObject substitutes the creator's name where Java returns \"\": this is exactly what the C++-only getMasterName overrides "
		   "of SummonedHouseNpc, Trap, Homing and Servant prevent (header request objects-1). Deleting them changes behaviour";
	EXPECT_EQ(summoned->Npc::getMasterName().value_or(std::string()), "") << "what those overrides return: Java's \"\"";

	// a real master name is returned unchanged by the base and by the overrides alike (SummonedHouseNpc of a house with an owner)
	summoned->setMasterName("Rolandas");
	ASSERT_TRUE(summoned->getMasterName());
	EXPECT_EQ(*summoned->getMasterName(), "Rolandas");
	EXPECT_EQ(summoned->Npc::getMasterName().value_or(std::string()), "Rolandas");
}

TEST_F(SummonedObjectsTest, TheFourSubclassOverridesKeepJavasEmptyMasterName) {
	SUMMONED_TEST_SCOPE;
	Ref<CreatorNpc> creator = createCreator();

	// SummonedHouseNpc, the real class: HousingService.spawnHouses builds 2,060 of these at startup, and for a house without an owner
	// SummonedHouseNpc.java:19-20 sets the master name to "" (getOwnerName() is null there)
	Ref<house::House> theHouse = VisibleObject::create<house::House>(dataholders::DataManager::HOUSE_DATA->getAddress(10001), 0);
	ASSERT_TRUE(theHouse) << "the bound HOUSE_DATA has address 10001";
	ASSERT_FALSE(theHouse->getOwnerName()) << "a house without an owner";
	Ref<SummonedHouseNpc> butler = VisibleObject::create<SummonedHouseNpc>(std::make_unique<controllers::NpcController>(), *butlerSpawn, *theHouse);
	ASSERT_TRUE(butler->getMasterName());
	EXPECT_EQ(*butler->getMasterName(), "") << "SummonedHouseNpc::getMasterName answers Java's \"\" (SummonedHouseNpc.cpp:64-66)";
	EXPECT_EQ(butler->SummonedObject::getMasterName().value_or(std::string()), theHouse->getName())
		<< "what the base answers, and what a subclass without the override would send to every client in range";
	EXPECT_EQ(butler->getCreatorId(), 10001) << "SummonedHouseNpc::getCreatorId: the house address id";

	// Homing, Servant and Trap set the same "" in their constructors (Homing.java:28, Servant.java:19, Trap.java:18)
	Ref<TestHoming> homing = VisibleObject::create<TestHoming>(std::make_unique<controllers::NpcController>(), *butlerSpawn, int8_t{7}, *creator, 1101);
	ASSERT_TRUE(homing->getMasterName());
	EXPECT_EQ(*homing->getMasterName(), "") << "Homing::getMasterName (Homing.cpp:41-43)";
	EXPECT_EQ(homing->SummonedObject::getMasterName().value_or(std::string()), creator->getName()) << "what the base answers";
	EXPECT_EQ(homing->getNpcObjectType(), NpcObjectType::HOMING);
	EXPECT_EQ(homing->getSkillId(), 1101);

	Ref<TestServant> servant = VisibleObject::create<TestServant>(std::make_unique<controllers::NpcController>(), *butlerSpawn, int8_t{7}, *creator);
	ASSERT_TRUE(servant->getMasterName());
	EXPECT_EQ(*servant->getMasterName(), "") << "Servant::getMasterName (Servant.cpp:39-41)";
	EXPECT_EQ(servant->SummonedObject::getMasterName().value_or(std::string()), creator->getName()) << "what the base answers";

	Ref<TestTrap> trap = VisibleObject::create<TestTrap>(std::make_unique<controllers::NpcController>(), *butlerSpawn, *creator);
	ASSERT_TRUE(trap->getMasterName());
	EXPECT_EQ(*trap->getMasterName(), "") << "Trap::getMasterName (Trap.cpp:72-74)";
	EXPECT_EQ(trap->SummonedObject::getMasterName().value_or(std::string()), creator->getName()) << "what the base answers";
	EXPECT_EQ(trap->getNpcObjectType(), NpcObjectType::TRAP);
	EXPECT_EQ(trap->getLevel(), creator->getLevel()) << "Trap.getLevel: the creator's level (Trap.java:29-32)";

	// an owner name that is not empty passes through all four, so the overrides only cover Java's "" (a house with an owner sends that name)
	butler->setMasterName("Rolandas");
	homing->setMasterName("Rolandas");
	servant->setMasterName("Rolandas");
	trap->setMasterName("Rolandas");
	EXPECT_EQ(butler->getMasterName().value_or(std::string()), "Rolandas");
	EXPECT_EQ(homing->getMasterName().value_or(std::string()), "Rolandas");
	EXPECT_EQ(servant->getMasterName().value_or(std::string()), "Rolandas");
	EXPECT_EQ(trap->getMasterName().value_or(std::string()), "Rolandas");
}

TEST_F(SummonedObjectsTest, HouseDecorationReadsItsPartTemplate) {
	SUMMONED_TEST_SCOPE;
	Ref<HouseDecoration> decoration = HouseDecoration::create(4242, 1001);
	ASSERT_TRUE(decoration->getTemplate()) << "DataManager.HOUSE_PARTS_DATA.getPartById(templateId)";
	EXPECT_EQ(decoration->getTemplate()->getId(), 1001);
	EXPECT_EQ(decoration->getTemplate()->getType(), templates::housing::PartType::ROOF);
	EXPECT_EQ(decoration->getName(), "basic_roof") << "HouseDecoration.getName: getTemplate().getName()";
	EXPECT_FALSE(dataholders::DataManager::HOUSE_PARTS_DATA->getPartById(7)) << "Java: a null template for an unknown part id";
}

} // namespace
} // namespace aion::gameserver::model::gameobjects
