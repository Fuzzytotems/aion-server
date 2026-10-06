// M5b-3 leftovers, item actions batch A (P5-07): AdoptPetAction, DecorateAction, FireworksUseAction, TitleAddAction, MegaphoneAction,
// AnimationAddAction, EmotionLearnAction, CosmeticItemAction and PolishAction against their Java (model/templates/item/actions). The item rows
// are item_templates.xml's (the line in each comment), the title player_titles.xml's 101, the cosmetic cosmetic_items.xml's
// test_hair_type_li_m_01a. The player is ItemServicesTest's "Looter" (700101, ELYOS, a MALE warrior of level 1).

#include "ItemServicesTestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/CosmeticItemsData.bind.h"
#include "aion/gameserver/dataholders/CosmeticItemsData.h"
#include "aion/gameserver/dataholders/ItemRandomBonusData.bind.h"
#include "aion/gameserver/dataholders/ItemRandomBonusData.h"
#include "aion/gameserver/dataholders/TitleData.bind.h"
#include "aion/gameserver/dataholders/TitleData.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/emotion/EmotionList.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/model/items/IdianStone.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/AdoptPetAction.h"
#include "aion/gameserver/model/templates/item/actions/AnimationAddAction.h"
#include "aion/gameserver/model/templates/item/actions/CosmeticItemAction.h"
#include "aion/gameserver/model/templates/item/actions/DecorateAction.h"
#include "aion/gameserver/model/templates/item/actions/EmotionLearnAction.h"
#include "aion/gameserver/model/templates/item/actions/FireworksUseAction.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/MegaphoneAction.h"
#include "aion/gameserver/model/templates/item/actions/PolishAction.h"
#include "aion/gameserver/model/templates/item/actions/TitleAddAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test {
namespace {

using namespace std::chrono_literals;
using model::templates::item::actions::AbstractItemAction;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t FIRECRACKER = 164000136;
constexpr int32_t TITLE_CARD = 169610000;
constexpr int32_t MEGAPHONE = 188910000;
constexpr int32_t NINJA_SET = 188500000;
constexpr int32_t ABRASIVE = 166050000;
constexpr int32_t COSMETIC_HAIR = 169800001;
constexpr int32_t COSMETIC_PRESET = 169890001;
constexpr int32_t EMOTION_CARD = 169620999; // synthetic: the 4.8 item rows have no <learnemotion>
constexpr int32_t ADOPT_EGG = 190020001;    // synthetic rows of the two Java no-op actions
constexpr int32_t HOUSE_DECO = 190020002;
constexpr int32_t TRAINING_SWORD = 100000094; // the base rows' level-1 polishable sword
constexpr int32_t ITEM = 850101;              // the used item's object id

constexpr std::string_view ACTION_ROWS = R"xml(
	<!-- :833135 -->
	<item_template id="164000136" name="Firecracker" level="10" cName="item_firecracker_02" mask="12414" max_stack_count="1000" quality="COMMON" price="980" desc="764376" activate_target="STANDALONE" activate_count="1">
		<actions>
			<fireworkact/>
		</actions>
		<uselimits usedelay="15000" usedelayid="33"/>
	</item_template>
	<!-- :856770 -->
	<item_template id="169610000" name="[Title Card] 'Settler of Aion'" level="1" cName="cash_add_title_01" mask="4168" quality="COMMON" price="0" desc="741825" activate_count="1">
		<actions>
			<titleadd titleid="101"/>
		</actions>
	</item_template>
	<!-- :930361 -->
	<item_template id="188910000" name="Amplifier Test Item" level="1" cName="world_cash_test_item_megaphone" mask="12410" max_stack_count="1000" quality="RARE" price="5" restrict="10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10" desc="809133" activate_target="STANDALONE" activate_count="1">
		<actions>
			<megaphone color="000000"/>
		</actions>
		<uselimits usedelay="1000" usedelayid="146"/>
	</item_template>
	<!-- :927783 -->
	<item_template id="188500000" name="Ninja Set" level="1" cName="cash_add_customize_motion_ninja" casting_delay="1000" mask="4168" quality="COMMON" price="5" desc="770130" activate_target="STANDALONE" activate_count="1">
		<actions>
			<animation idle="1" run="2" jump="3" rest="4" minutes="60"/>
		</actions>
	</item_template>
	<!-- :838119 -->
	<item_template id="166050000" name="TestAbrasive_1" level="30" cName="test_polish_enchant_1" mask="12414" max_stack_count="100" quality="COMMON" price="880" desc="806945" activate_count="1">
		<actions>
			<polish set_id="2"/>
		</actions>
	</item_template>
	<!-- :861704 -->
	<item_template id="169890001" name="Test_Elyos_Male_Preset_001" level="1" cName="test_item_preset_type_li_m_01a" casting_delay="5000" mask="4222" quality="COMMON" price="5" race="ELYOS" desc="746566">
		<actions>
			<cosmetic name="test_preset_type_li_m_01a"/>
		</actions>
		<uselimits gender="MALE"/>
	</item_template>
	<!-- :859312 -->
	<item_template id="169800001" name="Test_Elyos_Male_Head_001" level="1" cName="test_item_hair_type_li_m_01a" casting_delay="5000" mask="4222" quality="COMMON" price="5" race="ELYOS" desc="746510">
		<actions>
			<cosmetic name="test_hair_type_li_m_01a"/>
		</actions>
		<uselimits gender="MALE"/>
	</item_template>
	<item_template id="169620999" name="Emotion Card (synthetic)" level="1" cName="test_emotion_card" mask="4168" quality="COMMON" price="5" desc="741825" activate_count="1">
		<actions>
			<learnemotion emotionid="64" minutes="10"/>
		</actions>
	</item_template>
	<item_template id="190020001" name="Pet Egg (synthetic)" level="1" cName="test_pet_egg" mask="4168" quality="COMMON" price="5" desc="741825">
		<actions>
			<adoptpet petId="1" minutes="0"/>
		</actions>
	</item_template>
	<item_template id="190020002" name="House Deco (synthetic)" level="1" cName="test_house_deco" mask="4168" quality="COMMON" price="5" desc="741825">
		<actions>
			<housedeco/>
		</actions>
	</item_template>
)xml";

/** player_titles.xml :643-649 */
constexpr std::string_view TITLES_XML = R"xml(<player_titles>
	<title id="101" nameId="1101600" desc="Settler of Aion" race="PC_ALL">
		<modifiers>
			<add name="PHYSICAL_CRITICAL" value="3" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="1" bonus="true"/>
			<add name="PARRY" value="8" bonus="true"/>
		</modifiers>
	</title>
</player_titles>)xml";

/** cosmetic_items.xml :3 and :337-347 */
constexpr std::string_view COSMETICS_XML = R"xml(<cosmetic_items>
	<cosmetic_item type="hair_type" cosmetic_name="test_hair_type_li_m_01a" id="1" race="ELYOS" gender_permitted="MALE"/>
	<cosmetic_item type="preset_name" cosmetic_name="test_preset_type_li_m_01a" race="ELYOS" gender_permitted="MALE">
		<preset>
			<scale>1.000000</scale>
			<hair_type>1</hair_type>
			<face_type>0</face_type>
			<hair_color>1515812</hair_color>
			<lip_color>10660564</lip_color>
			<eye_color>5402006</eye_color>
			<skin_color>13228789</skin_color>
		</preset>
	</cosmetic_item>
</cosmetic_items>)xml";

/** item_random_bonuses.xml :20550-20560 (the abrasive's set 2) */
constexpr std::string_view POLISH_BONUSES_XML = R"xml(<random_bonuses>
	<random_bonus type="POLISH" id="2">
		<modifiers chance="50.0">
			<rate name="BOOST_HATE" value="-100" bonus="true"/>
		</modifiers>
		<modifiers chance="20.0">
			<rate name="BOOST_HATE" value="100" bonus="true"/>
		</modifiers>
		<modifiers chance="30.0">
			<rate name="BOOST_HATE" value="-50" bonus="true"/>
		</modifiers>
	</random_bonus>
</random_bonuses>)xml";

std::string withRows(std::string_view base, std::string_view closingTag, std::string_view rows) {
	std::string xml(base);
	xml.insert(xml.rfind(closingTag), rows);
	return xml;
}

class SimpleItemActionsTest : public ItemServicesTest {
protected:
	void SetUp() override {
		ItemServicesTest::SetUp();
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(
			xml::bindString<dataholders::ItemData>(context, withRows(ITEM_TEMPLATES_XML, "</item_templates>", ACTION_ROWS)));
		dataholders::DataManager::TITLE_DATA.publish(xml::bindString<dataholders::TitleData>(context, TITLES_XML));
		dataholders::DataManager::COSMETIC_ITEMS_DATA.publish(xml::bindString<dataholders::CosmeticItemsData>(context, COSMETICS_XML));
		dataholders::DataManager::ITEM_RANDOM_BONUSES.publish(xml::bindString<dataholders::ItemRandomBonusData>(context, POLISH_BONUSES_XML));
		player().getCommonData()->setGender(model::Gender::MALE);
		player().getTitleList().setOwner(player()); // Java: Player.setTitleList binds the owner (PlayerService.getPlayer)
		player().setMotions(std::make_unique<model::gameobjects::player::motion::MotionList>(player()));
		publishPoetaCastWorldDataOnce(); // SM_MEGAPHONE's broadcastToWorld asks the World
	}

	void TearDown() override {
		ItemServicesTest::TearDown();
		dataholders::DataManager::ITEM_RANDOM_BONUSES.resetForTests();
		dataholders::DataManager::COSMETIC_ITEMS_DATA.resetForTests();
		dataholders::DataManager::TITLE_DATA.resetForTests();
	}

	/** The item's only action */
	template <class A>
	const A& actionOf(int32_t itemId) {
		const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
		if (itemTemplate == nullptr || itemTemplate->getActions() == nullptr)
			throw runtime::NullPointerException("no actions of item " + std::to_string(itemId));
		const auto* action = dynamic_cast<const A*>(itemTemplate->getActions()->getItemActions().front().get());
		if (action == nullptr)
			throw runtime::NullPointerException("item " + std::to_string(itemId) + " has another action");
		return *action;
	}

	int64_t count(const std::vector<std::vector<uint8_t>>& packets, const std::vector<uint8_t>& packet) {
		return std::count(packets.begin(), packets.end(), packet);
	}

	xml::LoadContext context;
};

// ---- the two Java no-ops ----------------------------------------------------------------------------------------------------------------------

TEST_F(SimpleItemActionsTest, AdoptPetAndDecorateNeverAct) {
	Item& egg = stored(ITEM, ADOPT_EGG, 1);
	EXPECT_FALSE(actionOf<model::templates::item::actions::AdoptPetAction>(ADOPT_EGG).canAct(player(), Ptr<Item>(egg), nullptr));
	Item& deco = stored(ITEM + 1, HOUSE_DECO, 1);
	const auto& decorate = actionOf<model::templates::item::actions::DecorateAction>(HOUSE_DECO);
	EXPECT_FALSE(decorate.canAct(player(), Ptr<Item>(deco), nullptr));
	EXPECT_EQ(decorate.getTemplateId(), 0) << "no id attribute: addons missing in the client";
	clearSent();
	decorate.act(player(), Ptr<Item>(deco), nullptr);
	EXPECT_TRUE(sent().empty());
}

// ---- FireworksUseAction (FireworksUseAction.java:21-35) -----------------------------------------------------------------------------------

TEST_F(SimpleItemActionsTest, AFirecrackerSpendsAnActivationOrTheItem) {
	const auto& fireworks = actionOf<model::templates::item::actions::FireworksUseAction>(FIRECRACKER);
	Item& firecracker = stored(ITEM, FIRECRACKER, 3);
	EXPECT_TRUE(fireworks.canAct(player(), Ptr<Item>(firecracker), nullptr));
	firecracker.setActivationCount(2);
	clearSent();
	fireworks.act(player(), Ptr<Item>(firecracker), nullptr);
	EXPECT_EQ(firecracker.getActivationCount(), 1) << "an activation is spent, the stack stays";
	EXPECT_EQ(firecracker.getItemCount(), 3);
	const std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(count(packets, serialized(SM_SYSTEM_MESSAGE::STR_USE_ITEM(firecracker.getL10n()))), 1);
	EXPECT_EQ(count(packets, serialized(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), ITEM, FIRECRACKER, 0, 1, 0))), 1);
	fireworks.act(player(), Ptr<Item>(firecracker), nullptr);
	EXPECT_EQ(firecracker.getItemCount(), 2) << "the last activation: one of the stack";
}

// ---- TitleAddAction (TitleAddAction.java:28-51) -----------------------------------------------------------------------------------------

TEST_F(SimpleItemActionsTest, ATitleCardAddsItsTitleOnceAndIsDeleted) {
	const auto& titleAdd = actionOf<model::templates::item::actions::TitleAddAction>(TITLE_CARD);
	EXPECT_FALSE(titleAdd.canAct(player(), nullptr, nullptr)) << "no item";
	Item& card = stored(ITEM, TITLE_CARD, 1);
	EXPECT_TRUE(titleAdd.canAct(player(), Ptr<Item>(card), nullptr));
	clearSent();
	titleAdd.act(player(), Ptr<Item>(card), nullptr);
	EXPECT_TRUE(player().getTitleList().contains(101));
	EXPECT_FALSE(player().getInventory().getItemByObjId(ITEM)) << "the card is deleted";
	EXPECT_EQ(count(sent(), serialized(SM_SYSTEM_MESSAGE::STR_USE_ITEM(card.getL10n()))), 1);

	Item& second = stored(ITEM + 1, TITLE_CARD, 1);
	clearSent();
	EXPECT_FALSE(titleAdd.canAct(player(), Ptr<Item>(second), nullptr));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_TOOLTIP_LEARNED_TITLE())}));
}

// ---- MegaphoneAction (MegaphoneAction.java:26-45) ----------------------------------------------------------------------------------------

TEST_F(SimpleItemActionsTest, AMegaphoneParsesItsColourAndSpendsOne) {
	const auto& megaphone = actionOf<model::templates::item::actions::MegaphoneAction>(MEGAPHONE);
	EXPECT_EQ(megaphone.getColor(), 0) << "Integer.parseInt(\"000000\", 16)";
	Item& item = stored(ITEM, MEGAPHONE, 5);
	clearSent();
	megaphone.act(player(), Ptr<Item>(item), nullptr, {std::any(std::string("hello world"))});
	EXPECT_EQ(item.getItemCount(), 4);
	EXPECT_EQ(count(sent(), serialized(SM_SYSTEM_MESSAGE::STR_USE_ITEM(item.getL10n()))), 1);
}

// ---- AnimationAddAction (AnimationAddAction.java:37-103) ----------------------------------------------------------------------------------

TEST_F(SimpleItemActionsTest, AMotionSetAddsItsFourMotionsAfterTheCastingDelay) {
	const auto& animation = actionOf<model::templates::item::actions::AnimationAddAction>(NINJA_SET);
	EXPECT_FALSE(animation.canAct(player(), nullptr, nullptr)) << "no item";
	Item& set = stored(ITEM, NINJA_SET, 1);
	EXPECT_TRUE(animation.canAct(player(), Ptr<Item>(set), nullptr));
	clearSent();
	animation.act(player(), Ptr<Item>(set), nullptr);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), ITEM, NINJA_SET, 1000, 0, 0))})) << "the cast bar";
	EXPECT_TRUE(player().getController().hasTask(model::TaskId::ITEM_USE));
	executor->advance(999ms);
	EXPECT_TRUE(player().getInventory().getItemByObjId(ITEM)) << "nothing before casting_delay";
	executor->advance(1ms);
	EXPECT_FALSE(player().getInventory().getItemByObjId(ITEM)) << "decreaseItemCount(item, 1): the set is used up";
	Ptr<runtime::RcLinkedHashMap<int32_t, Ref<model::gameobjects::player::motion::Motion>>> active = player().getMotions().getActiveMotions();
	ASSERT_TRUE(active);
	EXPECT_EQ(active->size(), 4) << "idle 1, run 2, jump 3, rest 4";
	EXPECT_EQ(count(sent(), serialized(SM_SYSTEM_MESSAGE::STR_USE_ITEM(set.getL10n()))), 1);
}

TEST_F(SimpleItemActionsTest, MovingDuringTheMotionBarCancelsIt) {
	const auto& animation = actionOf<model::templates::item::actions::AnimationAddAction>(NINJA_SET);
	Item& set = stored(ITEM, NINJA_SET, 1);
	animation.act(player(), Ptr<Item>(set), nullptr);
	clearSent();
	player().getController().onMove();
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::ITEM_USE));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_ITEM_CANCELED()),
						  serialized(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), ITEM, NINJA_SET, 0, 3, 0))}))
		<< "sendPacket, end 3 (AnimationAddAction.java:62-63)";
	executor->advance(1000ms);
	EXPECT_TRUE(player().getInventory().getItemByObjId(ITEM)) << "the set stays";
}

// ---- EmotionLearnAction (EmotionLearnAction.java:39-79) --------------------------------------------------------------------------------

TEST_F(SimpleItemActionsTest, AnEmotionCardTeachesItsEmotionOnce) {
	const auto& learn = actionOf<model::templates::item::actions::EmotionLearnAction>(EMOTION_CARD);
	EXPECT_TRUE(model::templates::item::actions::EmotionLearnAction::isLearnable(64)) << "afterUnmarshal registered it";
	const std::vector<int32_t> learnable = model::templates::item::actions::EmotionLearnAction::getLearnableEmotionIds();
	EXPECT_TRUE(std::ranges::is_sorted(learnable));
	EXPECT_NE(std::ranges::find(learnable, 64), learnable.end());

	player().setEmotions(std::make_unique<model::gameobjects::player::emotion::EmotionList>(player()));
	Item& card = stored(ITEM, EMOTION_CARD, 1);
	EXPECT_TRUE(learn.canAct(player(), Ptr<Item>(card), nullptr));
	learn.act(player(), Ptr<Item>(card), nullptr);
	EXPECT_TRUE(player().getEmotions()->contains(64));
	EXPECT_FALSE(player().getInventory().getItemByObjId(ITEM)) << "the card is deleted";
	Item& second = stored(ITEM + 1, EMOTION_CARD, 1);
	clearSent();
	EXPECT_FALSE(learn.canAct(player(), Ptr<Item>(second), nullptr));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_TOOLTIP_LEARNED_EMOTION())}));
}

// ---- CosmeticItemAction (CosmeticItemAction.java:31-86) ---------------------------------------------------------------------------------

TEST_F(SimpleItemActionsTest, ACosmeticChecksRaceAndGender) {
	const auto& cosmetic = actionOf<model::templates::item::actions::CosmeticItemAction>(COSMETIC_HAIR);
	Item& item = stored(ITEM, COSMETIC_HAIR, 1);
	EXPECT_TRUE(cosmetic.canAct(player(), Ptr<Item>(item), nullptr)) << "an Elyos male";
	player().getCommonData()->setGender(model::Gender::FEMALE);
	clearSent();
	EXPECT_FALSE(cosmetic.canAct(player(), Ptr<Item>(item), nullptr));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_GENDER())}));
	player().getCommonData()->setRace(model::Race::ASMODIANS);
	clearSent();
	EXPECT_FALSE(cosmetic.canAct(player(), Ptr<Item>(item), nullptr));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_RACE())})) << "the race comes first";
}

// correction of the Java code (owner's decision 2026-10-05, both branches): act deletes the used coupon (parentItem); Java deleted targetItem,
// which CM_USE_ITEM leaves null for a plain use, and threw a NullPointerException with the coupon kept (CosmeticItemAction.java:84)
TEST_F(SimpleItemActionsTest, ACosmeticIsUsedUpAfterTheAppearanceChanged) {
	const auto& cosmetic = actionOf<model::templates::item::actions::CosmeticItemAction>(COSMETIC_HAIR);
	Item& item = stored(ITEM, COSMETIC_HAIR, 1);
	try {
		cosmetic.act(player(), Ptr<Item>(item), nullptr);
	} catch (const runtime::NullPointerException& e) {
		// the last statement, onChangedPlayerAttributes, sends the player info packets, which ask HousingService for the player's house (the
		// house data and a database this fixture lacks); Java's Storage.delete(null) threw before the delete instead
		EXPECT_NE(std::string_view(e.what()).find("HouseData"), std::string_view::npos) << e.what();
	}
	EXPECT_EQ(player().getPlayerAppearance()->getHair(), 1) << "hair_type 1";
	EXPECT_FALSE(player().getInventory().getItemByObjId(ITEM)) << "the coupon is used up";
}

// correction of the Java code (owner's decision 2026-10-05, both branches): the preset's skin colour is its skin_color; Java set the eye
// colour as the skin colour (CosmeticItemAction.java:70)
TEST_F(SimpleItemActionsTest, ACosmeticPresetSetsItsOwnSkinColour) {
	const auto& cosmetic = actionOf<model::templates::item::actions::CosmeticItemAction>(COSMETIC_PRESET);
	Item& item = stored(ITEM, COSMETIC_PRESET, 1);
	try {
		cosmetic.act(player(), Ptr<Item>(item), nullptr);
	} catch (const runtime::NullPointerException& e) { // onChangedPlayerAttributes' house lookup, as above
		EXPECT_NE(std::string_view(e.what()).find("HouseData"), std::string_view::npos) << e.what();
	}
	EXPECT_EQ(player().getPlayerAppearance()->getSkinRGB(), 13228789) << "skin_color, not eye_color 5402006";
	EXPECT_EQ(player().getPlayerAppearance()->getEyeRGB(), 5402006);
}

// ---- PolishAction (PolishAction.java:34-101) ------------------------------------------------------------------------------------------------

TEST_F(SimpleItemActionsTest, AnAbrasiveAboveTheWeaponsLevelIsRefused) {
	const auto& polish = actionOf<model::templates::item::actions::PolishAction>(ABRASIVE);
	Item& abrasive = stored(ITEM, ABRASIVE, 1);
	Item& sword = stored(ITEM + 1, TRAINING_SWORD, 1);
	clearSent();
	EXPECT_FALSE(polish.canAct(player(), Ptr<Item>(abrasive), Ptr<Item>(sword)));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_POLISH_WRONG_LEVEL())})) << "abrasive level 30 > sword level 1";
	EXPECT_EQ(polish.getPolishSetId(), 2);
}

// without the bonus set 2 (only set 3 here) the polish fails after the bar: the abrasive is spent, no idian stone
TEST_F(SimpleItemActionsTest, APolishWithoutABonusFailsAfterTheBar) {
	dataholders::DataManager::ITEM_RANDOM_BONUSES.resetForTests();
	dataholders::DataManager::ITEM_RANDOM_BONUSES.publish(xml::bindString<dataholders::ItemRandomBonusData>(context, R"(<random_bonuses><random_bonus type="POLISH" id="3"><modifiers chance="100.0"><rate name="BOOST_HATE" value="-100" bonus="true"/></modifiers></random_bonus></random_bonuses>)"));
	const auto& polish = actionOf<model::templates::item::actions::PolishAction>(ABRASIVE);
	Item& abrasive = stored(ITEM, ABRASIVE, 2);
	Item& sword = stored(ITEM + 1, TRAINING_SWORD, 1);
	clearSent();
	polish.act(player(), Ptr<Item>(abrasive), Ptr<Item>(sword));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), ITEM, ABRASIVE, 5000, 0, 0))}));
	executor->advance(5000ms);
	EXPECT_EQ(abrasive.getItemCount(), 1);
	EXPECT_EQ(count(sent(), serialized(SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_FAILED(abrasive.getL10n()))), 1);
	EXPECT_FALSE(sword.getIdianStone());
}

// PolishAction.java:76-99: the bonus set's roll (1-3 of set 2) becomes a new idian stone of the abrasive's item id with 1,000,000 charge
TEST_F(SimpleItemActionsTest, APolishGivesTheWeaponANewIdianStone) {
	const auto& polish = actionOf<model::templates::item::actions::PolishAction>(ABRASIVE);
	Item& abrasive = stored(ITEM, ABRASIVE, 2);
	Item& sword = stored(ITEM + 1, TRAINING_SWORD, 1);
	polish.act(player(), Ptr<Item>(abrasive), Ptr<Item>(sword));
	clearSent();
	executor->advance(5000ms);
	EXPECT_EQ(abrasive.getItemCount(), 1);
	EXPECT_EQ(count(sent(), serialized(SM_SYSTEM_MESSAGE::STR_MSG_POLISH_SUCCEED(sword.getL10n()))), 1);
	ASSERT_TRUE(sword.getIdianStone());
	EXPECT_EQ(sword.getIdianStone()->getItemId(), ABRASIVE);
	EXPECT_GE(sword.getIdianStone()->getPolishNumber(), 1);
	EXPECT_LE(sword.getIdianStone()->getPolishNumber(), 3);
	EXPECT_EQ(sword.getIdianStone()->getPolishCharge(), 1000000);
}

TEST_F(SimpleItemActionsTest, APolishOfAWeaponThatLeftTheInventoryIsCancelled) {
	const auto& polish = actionOf<model::templates::item::actions::PolishAction>(ABRASIVE);
	Item& abrasive = stored(ITEM, ABRASIVE, 2);
	Item& sword = loose(ITEM + 1, TRAINING_SWORD, 1);
	polish.act(player(), Ptr<Item>(abrasive), Ptr<Item>(sword));
	clearSent();
	executor->advance(5000ms);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_NO_TARGET_ITEM()),
						  serialized(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), ITEM, ABRASIVE, 0, 2, 0))}));
	EXPECT_EQ(abrasive.getItemCount(), 2);
}

} // namespace
} // namespace aion::gameserver::services::item::test
