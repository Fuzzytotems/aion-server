// Stat listeners of P5-01 (wave 5a, work item B-04): ItemEquipmentListener.onItemEquipment for an equipped weapon and armor piece of a fresh
// character (template modifiers, the weapon modifiers that are skipped, the item's current modifiers) and TitleChangeListener.onBonusTitleChange.
// Static data is bound from XML text in the Java format of item_templates.xml and player_titles.xml.

#include <gtest/gtest.h>

#include <memory>
#include <string_view>
#include <unordered_set>

#include "StatsTestSupport.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemSetData.h"
#include "aion/gameserver/dataholders/TitleData.bind.h"
#include "aion/gameserver/dataholders/TitleData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/stats/listeners/ItemEquipmentListener.h"
#include "aion/gameserver/model/stats/listeners/TitleChangeListener.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/utils/stats/CalculationType.h"

namespace aion::gameserver::model::stats::test {
namespace {

using container::StatEnum;
using runtime::Ptr;
using runtime::Ref;

/** Binds XML text as T and keeps the object for the process (static data is immortal) */
template <class T>
const T* bindStatic(std::string_view xml) {
	xml::LoadContext context;
	return xml::bindString<T>(context, xml).release();
}

class StatListenersTest : public StatsPlayerTest {
protected:
	void SetUp() override {
		StatsPlayerTest::SetUp();
		dataholders::DataManager::ITEM_SET_DATA.publish(std::make_unique<dataholders::ItemSetData>());
	}

	void TearDown() override {
		dataholders::DataManager::ITEM_SET_DATA.resetForTests();
		dataholders::DataManager::TITLE_DATA.resetForTests();
		StatsPlayerTest::TearDown();
	}
};

TEST_F(StatListenersTest, EquippedWeaponAddsItsModifiersExceptTheWeaponSpeedOnes) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(2001, PlayerClass::WARRIOR);
	const templates::item::ItemTemplate* swordTemplate = bindStatic<templates::item::ItemTemplate>(
		R"(<item_template id="100000132" level="1" item_group="SWORD" attack_type="PHYSICAL">)"
		R"(<modifiers><add name="PHYSICAL_ATTACK" value="7" bonus="true"/><rate name="ATTACK_SPEED" value="-10" bonus="true"/>)"
		R"(<add name="BOOST_CASTING_TIME" value="5"/></modifiers>)"
		R"(<weapon_stats hit_count="2" attack_range="1500" parry="173" physical_accuracy="52" critical="50" attack_speed="1400" max_damage="20" min_damage="16"/>)"
		R"(</item_template>)");
	Ref<gameobjects::Item> sword = gameobjects::Item::create(9001, swordTemplate, 1, true, items::getSlotIdMask(items::ItemSlot::MAIN_HAND));
	Ptr<container::PlayerGameStats> stats = f.player->getGameStats();
	ASSERT_EQ(stats->getMainHandPAttack({utils::stats::CalculationType::DISPLAY})->getCurrent(), 18) << "the template attack before the item";

	listeners::ItemEquipmentListener::onItemEquipment(*sword, *f.player);

	// extractApplicableWeaponModifiers drops ATTACK_SPEED, PVP_ATTACK_RATIO and BOOST_CASTING_TIME of main and sub hand weapons
	Ptr<runtime::RcArrayList<Ref<calc::functions::StatFunction>>> current = sword->getCurrentModifiers();
	ASSERT_TRUE(current);
	ASSERT_EQ(current->size(), 1);
	EXPECT_EQ(current->get(0)->getName(), StatEnum::PHYSICAL_ATTACK);
	EXPECT_EQ(stats->getStatsSorted(StatEnum::PHYSICAL_ATTACK).size(), 1u);
	EXPECT_TRUE(stats->getStatsSorted(StatEnum::ATTACK_SPEED).empty());
	EXPECT_EQ(stats->getStatsSorted(StatEnum::PHYSICAL_ATTACK)[0]->getOwner().get(), static_cast<calc::StatOwner*>(sword.get()))
		<< "the template modifier has no owner: added as a proxy of the item";
	// the sword is not in the equipment, so the base is the template attack 18; the bonus modifier adds 7
	EXPECT_EQ(stats->getMainHandPAttack({utils::stats::CalculationType::DISPLAY})->getCurrent(), 25);
	std::unique_ptr<calc::Stat2> boost(stats->getStat(StatEnum::PHYSICAL_ATTACK, 10.0f));
	EXPECT_EQ(stats->getItemStatBoost(StatEnum::PHYSICAL_ATTACK, *boost).getCurrent(), 24) << "getItemStatBoost applies the item bonus again";

	stats->endEffect(*sword);
	EXPECT_EQ(stats->getMainHandPAttack({utils::stats::CalculationType::DISPLAY})->getCurrent(), 18);
}

TEST_F(StatListenersTest, EquippedArmorKeepsAllModifiersAndLoadSynchronizesTheLifeStats) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(2002, PlayerClass::WARRIOR);
	const templates::item::ItemTemplate* torsoTemplate = bindStatic<templates::item::ItemTemplate>(
		R"(<item_template id="110000001" level="1" item_group="CL_TORSO">)"
		R"(<modifiers><add name="MAXHP" value="50"/><add name="PHYSICAL_DEFENSE" value="12"/><rate name="ATTACK_SPEED" value="10" bonus="true"/></modifiers>)"
		R"(</item_template>)");
	Ref<gameobjects::Item> torso = gameobjects::Item::create(9002, torsoTemplate, 1, true, items::getSlotIdMask(items::ItemSlot::TORSO));
	listeners::ItemEquipmentListener::onItemEquipment(*torso, *f.player);

	Ptr<container::PlayerGameStats> stats = f.player->getGameStats();
	EXPECT_EQ(torso->getCurrentModifiers()->size(), 3) << "not a weapon slot: every modifier";
	EXPECT_EQ(stats->getMaxHp()->getCurrent(), 250);
	EXPECT_EQ(stats->getPDef()->getCurrent(), 12);
	// attack speed 1500 + 1500 * 10 / 100 = 1650
	EXPECT_EQ(stats->getAttackSpeed()->getCurrent(), 1650);
	// addEffect: onStatsChange scaled the current HP from 200 of 200 to 250 of 250
	EXPECT_EQ(f.player->getLifeStats()->getCurrentHp(), 250);
	f.player->getLifeStats()->synchronizeWithMaxStats();
	EXPECT_EQ(f.player->getLifeStats()->getCurrentHp(), 250);
	EXPECT_EQ(f.player->getLifeStats()->getCurrentMp(), 140);
}

TEST_F(StatListenersTest, BonusTitleAddsAndRemovesItsModifiers) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	{
		xml::LoadContext context;
		dataholders::DataManager::TITLE_DATA.publish(xml::bindString<dataholders::TitleData>(context,
			R"(<player_titles><title id="1" nameId="1100900" desc="Poeta's Protector" race="ELYOS"><modifiers>)"
			R"(<add name="MAXHP" value="20" bonus="true"/><add name="PHYSICAL_DEFENSE" value="5" bonus="true"/></modifiers></title>)"
			R"(<title id="2" nameId="1100901" desc="No bonus" race="ELYOS"/></player_titles>)"));
	}
	PlayerFixture f = makePlayer(2003, PlayerClass::WARRIOR);
	Ptr<container::PlayerGameStats> stats = f.player->getGameStats();
	listeners::TitleChangeListener::onBonusTitleChange(*stats, 1, true);
	EXPECT_EQ(stats->getMaxHp()->getCurrent(), 220);
	EXPECT_EQ(stats->getPDef()->getCurrent(), 5);
	listeners::TitleChangeListener::onBonusTitleChange(*stats, 1, false);
	EXPECT_EQ(stats->getMaxHp()->getCurrent(), 200);
	EXPECT_EQ(stats->getPDef()->getCurrent(), 0);
	EXPECT_NO_THROW(listeners::TitleChangeListener::onBonusTitleChange(*stats, 99, true)) << "an unknown title does nothing";
}

} // namespace
} // namespace aion::gameserver::model::stats::test
