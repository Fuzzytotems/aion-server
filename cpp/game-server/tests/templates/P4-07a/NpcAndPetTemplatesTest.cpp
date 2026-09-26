// P4-07a npc and pet templates: NpcTemplate's setter, hook and logic methods, the NpcRating, PetFunctionType and FoodType companions, PetTemplate's
// functions (C++: computed without mutating the template, docs/deviations/P4-07a.md), PetFlavour's reward lookup and PetDopingBag (Java
// semantics of PetDopingBag.java, including the array growth and the dirty flag).

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemGroupsData.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/templates/npc/NpcRatingInfo.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/model/templates/pet/FoodTypeInfo.h"
#include "aion/gameserver/model/templates/pet/PetBuff.bind.h"
#include "aion/gameserver/model/templates/pet/PetDopingBag.h"
#include "aion/gameserver/model/templates/pet/PetFlavour.bind.h"
#include "aion/gameserver/model/templates/pet/PetFunctionTypeInfo.h"
#include "aion/gameserver/model/templates/pet/PetStatsTemplate.h"
#include "aion/gameserver/model/templates/pet/PetTemplate.bind.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/services/toypet/PetFeedProgress.h"

namespace aion::gameserver::model::templates {
namespace {

template <class T>
std::unique_ptr<T> bindXml(std::string_view text) {
	xml::LoadContext context;
	return xml::bindString<T>(context, text);
}

// ---- npc ---------------------------------------------------------------------------------------------------------------------------------------

static_assert(npc::getCongenitalSeeState(npc::NpcRating::JUNK) == gameobjects::state::CreatureSeeState::NORMAL);
static_assert(npc::getCongenitalSeeState(npc::NpcRating::ELITE) == gameobjects::state::CreatureSeeState::SEARCH1);
static_assert(npc::getCongenitalSeeState(npc::NpcRating::LEGENDARY) == gameobjects::state::CreatureSeeState::SEARCH2);

TEST(NpcTemplateTest, SetterAndDefaults) {
	std::unique_ptr<npc::NpcTemplate> npc =
		bindXml<npc::NpcTemplate>(R"(<npc_template npc_id="200001" level="12" name_id="301" name="Kerub" srange="5"/>)");
	EXPECT_EQ(npc->getTemplateId(), 200001);
	EXPECT_EQ(npc->getLevel(), 12);
	EXPECT_EQ(npc->getL10nId(), 301);
	EXPECT_EQ(npc->toString(), "Npc Template id: 200001 name: Kerub");
	EXPECT_EQ(npc->getAiName(), std::nullopt);
	EXPECT_EQ(npc->getMinimumShoutRange(), 10) << "srange below 10";
	EXPECT_EQ(npc->getNpcTemplateType(), npc::NpcTemplateType::NONE) << "absent type";
	EXPECT_EQ(npc->getAbyssNpcType(), npc::AbyssNpcType::NONE);
	EXPECT_EQ(npc->getTalkDistance(), 2) << "no talk_info";
	EXPECT_EQ(npc->getTalkDelay(), 0);
	EXPECT_EQ(npc->getFuncDialogIds(), nullptr);
	EXPECT_FALSE(npc->supportsAction(10));
	EXPECT_FALSE(npc->canInteract());
	EXPECT_FALSE(npc->isDialogNpc());
	EXPECT_EQ(npc->getAggroAngle(), 360);
	EXPECT_FLOAT_EQ(npc->getHeight(), 1.0f);
	EXPECT_THROW(npc->getMassiveLootCount(), runtime::NullPointerException) << "Java massiveLoot.getMLootCount() on null";
	EXPECT_EQ(bindXml<npc::NpcTemplate>(R"(<npc_template npc_id="1" level="1" name_id="1" srange="25"/>)")->getMinimumShoutRange(), 25);
}

TEST(NpcTemplateTest, HookTurnsTeleportersIntoSiegeTeleporters) {
	// Java: level > 1 && !"noaction".equals(ai) && abyss type TELEPORTER -> ai = "siege_teleporter"
	auto aiOf = [](std::string_view attributes) {
		return bindXml<npc::NpcTemplate>("<npc_template npc_id=\"1\" name_id=\"1\" " + std::string(attributes) + "/>")->getAiName();
	};
	EXPECT_EQ(aiOf(R"(level="2" abyss_type="TELEPORTER" ai="general")"), "siege_teleporter");
	EXPECT_EQ(aiOf(R"(level="2" abyss_type="TELEPORTER")"), "siege_teleporter") << "a null ai is replaced too";
	EXPECT_EQ(aiOf(R"(level="2" abyss_type="TELEPORTER" ai="noaction")"), "noaction");
	EXPECT_EQ(aiOf(R"(level="1" abyss_type="TELEPORTER" ai="general")"), "general") << "level 1 keeps its ai";
	EXPECT_EQ(aiOf(R"(level="30" abyss_type="GUARD" ai="general")"), "general");
}

TEST(NpcTemplateTest, TalkInfoAndMassiveLoot) {
	std::unique_ptr<npc::NpcTemplate> npc = bindXml<npc::NpcTemplate>(
		R"(<npc_template npc_id="1" level="1" name_id="1"><talk_info distance="4" delay="500" is_dialog="true" func_dialogs="10 21"/>)"
		R"(<massive_loot m_loot_count="3" m_loot_item="182400001" m_loot_min_level="10" m_loot_max_level="20"/></npc_template>)");
	EXPECT_EQ(npc->getTalkDistance(), 4);
	EXPECT_EQ(npc->getTalkDelay(), 500);
	ASSERT_NE(npc->getFuncDialogIds(), nullptr);
	EXPECT_EQ(*npc->getFuncDialogIds(), (std::vector<int32_t>{10, 21}));
	EXPECT_TRUE(npc->supportsAction(21));
	EXPECT_FALSE(npc->supportsAction(11));
	EXPECT_TRUE(npc->canInteract());
	EXPECT_TRUE(npc->isDialogNpc());
	EXPECT_EQ(npc->getMassiveLootCount(), 3);
	EXPECT_EQ(npc->getMassiveLootItem(), 182400001);
	EXPECT_EQ(npc->getMassiveLootMinLevel(), 10);
	EXPECT_EQ(npc->getMassiveLootMaxLevel(), 20);
	std::unique_ptr<npc::NpcTemplate> noDialogs =
		bindXml<npc::NpcTemplate>(R"(<npc_template npc_id="1" level="1" name_id="1"><talk_info/></npc_template>)");
	EXPECT_EQ(noDialogs->getFuncDialogIds(), nullptr) << "talk_info without func_dialogs: Java null list";
	EXPECT_TRUE(noDialogs->canInteract());
	EXPECT_FALSE(noDialogs->isDialogNpc());
}

// ---- pet ---------------------------------------------------------------------------------------------------------------------------------------

static_assert(pet::getId(pet::PetFunctionType::WAREHOUSE) == 0 && pet::isPlayerFunction(pet::PetFunctionType::MERCHANT));
static_assert(pet::getId(pet::PetFunctionType::APPEARANCE) == 1 && !pet::isPlayerFunction(pet::PetFunctionType::APPEARANCE));
static_assert(pet::getId(pet::PetFunctionType::WING) == -2 && !pet::isPlayerFunction(pet::PetFunctionType::BAG));
static_assert(pet::value(pet::FoodType::POPPY_SNACK_TASTY) == "POPPY_SNACK_TASTY");

TEST(PetTemplateTest, FoodTypeFromValueIsValueOf) {
	EXPECT_EQ(pet::fromValue("THORNS"), pet::FoodType::THORNS);
	try {
		pet::fromValue("thorns");
		FAIL() << "valueOf is case-sensitive";
	} catch (const commons::utils::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "No enum constant com.aionemu.gameserver.model.templates.pet.FoodType.thorns");
	}
}

TEST(PetTemplateTest, PetFunctionsAppendTheEmptyFunctionWithoutPlayerFunctions) {
	std::unique_ptr<pet::PetTemplate> none = bindXml<pet::PetTemplate>(R"(<pet id="1" name="a" nameid="2"/>)");
	std::vector<const pet::PetFunction*> functions = none->getPetFunctions();
	ASSERT_EQ(functions.size(), 1u) << "Java: a new list with CreateEmpty()";
	EXPECT_EQ(functions[0]->getPetFunctionType(), pet::PetFunctionType::NONE);
	EXPECT_EQ(none->getWarehouseFunction(), nullptr);
	EXPECT_TRUE(none->containsFunction(pet::PetFunctionType::NONE));
	EXPECT_FALSE(none->containsFunction(pet::PetFunctionType::BAG)) << "negative ids are never written";
	EXPECT_EQ(none->getTemplateId(), 1);
	EXPECT_EQ(none->getName(), "a");
	EXPECT_EQ(none->getL10nId(), 2);

	std::unique_ptr<pet::PetTemplate> appearance =
		bindXml<pet::PetTemplate>(R"(<pet id="1" name="a" nameid="2"><petfunction type="APPEARANCE" id="5"/><petfunction type="BAG" slots="6"/></pet>)");
	functions = appearance->getPetFunctions();
	ASSERT_EQ(functions.size(), 3u) << "no player function: the empty function is appended";
	EXPECT_EQ(functions[2]->getPetFunctionType(), pet::PetFunctionType::NONE);
	EXPECT_EQ(appearance->getPetFunction(pet::PetFunctionType::BAG)->getSlots(), 6);
	EXPECT_EQ(appearance->getPetFunctions().size(), 3u) << "calling it again gives the same functions (Java: hasPlayerFuncs is cached)";

	std::unique_ptr<pet::PetTemplate> warehouse =
		bindXml<pet::PetTemplate>(R"(<pet id="1" name="a" nameid="2"><petfunction type="WAREHOUSE" id="9" slots="12"/></pet>)");
	functions = warehouse->getPetFunctions();
	ASSERT_EQ(functions.size(), 1u) << "a player function: nothing appended";
	ASSERT_NE(warehouse->getWarehouseFunction(), nullptr);
	EXPECT_EQ(warehouse->getWarehouseFunction()->getSlots(), 12);
	EXPECT_TRUE(warehouse->containsFunction(pet::PetFunctionType::WAREHOUSE));
	EXPECT_FALSE(warehouse->containsFunction(pet::PetFunctionType::NONE));
	EXPECT_EQ(warehouse->getPetFunction(pet::PetFunctionType::LOOT), nullptr);
}

TEST(PetTemplateTest, ConcurrentPetFunctionReadsAreConsistent) {
	// Java race (fixed, docs/deviations/P4-07a.md): getPetFunctions() appended to the shared template's list without synchronization, so two
	// threads could append two empty functions or iterate while the other appends. The C++ template is immutable.
	std::unique_ptr<pet::PetTemplate> pet = bindXml<pet::PetTemplate>(R"(<pet id="1" name="a" nameid="2"><petfunction type="APPEARANCE"/></pet>)");
	std::vector<std::thread> threads;
	std::atomic<int32_t> wrongSizes{0};
	for (int t = 0; t < 4; ++t) {
		threads.emplace_back([&]() {
			for (int i = 0; i < 2000; ++i) {
				if (pet->getPetFunctions().size() != 2u || !pet->containsFunction(pet::PetFunctionType::NONE))
					wrongSizes.fetch_add(1);
			}
		});
	}
	for (std::thread& thread : threads)
		thread.join();
	EXPECT_EQ(wrongSizes.load(), 0);
}

TEST(PetTemplateTest, FlavourRewardGroupsAndBuffModifiers) {
	std::unique_ptr<pet::PetFlavour> flavour = bindXml<pet::PetFlavour>(
		R"(<flavour id="3" full_count="5" cd="10"><food group="BONES" loved="true"><result item="1"/></food><food group="FLUIDS"/></flavour>)");
	EXPECT_EQ(flavour->getFood().size(), 2u);
	EXPECT_TRUE(flavour->isLovedFood(pet::FoodType::BONES, 0));
	EXPECT_FALSE(flavour->isLovedFood(pet::FoodType::FLUIDS, 0));
	EXPECT_FALSE(flavour->isLovedFood(pet::FoodType::THORNS, 0)) << "no reward group";
	EXPECT_EQ(flavour->getFood()[0].getResults().size(), 1u);
	EXPECT_TRUE(flavour->getFood()[1].getResults().empty());
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<services::toypet::PetFeedProgress> progress = services::toypet::PetFeedProgress::create(0);
	EXPECT_EQ(flavour->processFeedResult(*progress, pet::FoodType::THORNS, 10, 10), nullptr) << "no reward group for the food type";

	std::unique_ptr<pet::PetBuff> buff = bindXml<pet::PetBuff>(R"(<buff id="1" feed_count="3"><modifiers/><modifiers/></buff>)");
	EXPECT_EQ(buff->getModifiers().size(), 2u);
	EXPECT_EQ(pet::PetStatsTemplate().getRunSpeed(), 0.0f);
}

TEST(PetTemplateTest, FlavourFoodTypeAsksTheItemGroups) {
	// Java PetFlavour.getFoodType: the first reward group whose type DataManager.ITEM_GROUPS_DATA.isFood accepts, else null (header request
	// pre-2). ItemGroupsData's afterUnmarshal (which fills the pet food sets) is not ported yet: isFood on the empty holder throws like Java's
	// null set until then
	EXPECT_EQ(pet::PetFlavour().getFoodType(182006999), std::nullopt) << "no reward group (the data always has one): the item groups are not read";
	std::unique_ptr<pet::PetFlavour> flavour =
		bindXml<pet::PetFlavour>(R"(<flavour id="3" full_count="5" cd="10"><food group="BONES"/></flavour>)");
	EXPECT_THROW(flavour->getFoodType(182006999), runtime::NullPointerException) << "ITEM_GROUPS_DATA is not published";
	dataholders::DataManager::ITEM_GROUPS_DATA.publish(std::make_unique<dataholders::ItemGroupsData>());
	EXPECT_THROW(flavour->getFoodType(182006999), runtime::NullPointerException) << "Java: the EnumMap has no EXCLUDES set before afterUnmarshal";
	dataholders::DataManager::ITEM_GROUPS_DATA.resetForTests();
}

TEST(PetDopingBagTest, SlotsGrowOnDemandAndMarkTheBagDirty) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<pet::PetDopingBag> bag = pet::PetDopingBag::create();
	EXPECT_EQ(bag->getFoodItem(), 0);
	EXPECT_EQ(bag->getDrinkItem(), 0);
	EXPECT_TRUE(bag->getItems().empty()) << "Java new int[0]";
	EXPECT_TRUE(bag->getScrollsUsed().empty());
	EXPECT_FALSE(bag->isDirty());

	bag->setDrinkItem(160010001);
	EXPECT_TRUE(bag->isDirty());
	EXPECT_EQ(bag->getItems(), (std::vector<int32_t>{0, 160010001})) << "new int[slot + 1]";
	EXPECT_EQ(bag->getFoodItem(), 0);
	EXPECT_EQ(bag->getDrinkItem(), 160010001);

	bag->setItem(169000004, 4);
	EXPECT_EQ(bag->getItems(), (std::vector<int32_t>{0, 160010001, 0, 0, 169000004})) << "Arrays.copyOf(itemBag, slot + 1)";
	EXPECT_EQ(bag->getScrollsUsed(), (std::vector<int32_t>{0, 0, 169000004})) << "copyOfRange(itemBag, 2, length)";
	bag->setFoodItem(160000001);
	EXPECT_EQ(bag->getItems(), (std::vector<int32_t>{160000001, 160010001, 0, 0, 169000004})) << "a lower slot does not shrink the bag";

	bag->switchItems(4, 6);
	EXPECT_EQ(bag->getItems(), (std::vector<int32_t>{160000001, 160010001, 0, 0, 0, 0, 169000004})) << "slot 6 grows the bag";
	bag->switchItems(1, 6);
	EXPECT_EQ(bag->getDrinkItem(), 160010001) << "food and drink are never switched";

	EXPECT_THROW(bag->setItem(1, 8), runtime::IllegalArgumentException) << "MAX_ITEMS";
	EXPECT_THROW(bag->setItem(1, -1), runtime::IllegalArgumentException);
	try {
		bag->setItem(7, 9);
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "Slot index 9 for item 7 is invalid.");
	}
	EXPECT_THROW(pet::PetDopingBag::create()->switchItems(2, 3), runtime::NullPointerException) << "Java: itemBag.length on null";
	pet::PetDopingBag::create()->switchItems(0, 3); // returns before reading the bag
}

} // namespace
} // namespace aion::gameserver::model::templates
