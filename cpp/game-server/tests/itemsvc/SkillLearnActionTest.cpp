// M5e C-03 (m5e-plan.md §5; P5-07): SkillLearnAction, the action of a skill book (`<skilllearn skillid="..." class="..." level="..."/>`),
// against SkillLearnAction.java:31-68. canAct refuses a player below the book's level, of another class (the book's class must be the player's
// class or its starting class), of the other race (unless the book is PC_ALL) or who knows the skill already; act broadcasts the usage
// animation, learns the skill through SkillLearnService.learnSkillBook (the skill tree's level of it for the player's class, race and level),
// says STR_USE_ITEM and deletes the book.
//
// The rows are item_templates.xml's 169500916 "Transformation: White Tiger I" (:853455-853461, the Ranger's book of skill 1), the same book
// for the Asmodians and the same book of the starting class SCOUT (both derived, for the race and the starting-class arms), skill_tree.xml's
// skill 1 for the Elyos Ranger (:3) and skill_templates.xml's skill 1 cut to its header. The player is ItemServicesTest's "Looter" (700101,
// ELYOS), made a Ranger or a Scout of level 10 per case.

#include "ItemServicesTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/dataholders/SkillTreeData.bind.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/SkillLearnAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test {
namespace {

using model::PlayerClass;
using model::templates::item::actions::SkillLearnAction;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t WHITE_TIGER_BOOK = 169500916;
constexpr int32_t WHITE_TIGER_BOOK_ASMODIAN = 169500917; // derived: the race arm
constexpr int32_t WHITE_TIGER_BOOK_SCOUT = 169500918;     // derived: the starting-class arm
constexpr int32_t WHITE_TIGER = 1;
constexpr int32_t BOOK = 840101; // the book's object id

/** item_templates.xml :853455-853461 verbatim, and two derived copies */
constexpr std::string_view BOOK_ROWS = R"xml(
	<item_template id="169500916" name="Transformation: White Tiger I" level="10" cName="skillbook_ra_ra_light_whitetiger_g1" mask="4168" item_group="SKILLBOOK" quality="RARE" price="5" race="ELYOS" desc="771052" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilllearn skillid="1" class="RANGER" level="10"/>
		</actions>
	</item_template>
	<item_template id="169500917" name="Transformation: White Tiger I (Asmodian)" level="10" cName="skillbook_ra_ra_dark_whitetiger_g1" mask="4168" item_group="SKILLBOOK" quality="RARE" price="5" race="ASMODIANS" desc="771052" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilllearn skillid="1" class="RANGER" level="10"/>
		</actions>
	</item_template>
	<item_template id="169500918" name="Transformation: White Tiger I (Scout)" level="10" cName="skillbook_ra_ra_light_whitetiger_g1" mask="4168" item_group="SKILLBOOK" quality="RARE" price="5" race="ELYOS" desc="771052" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilllearn skillid="1" class="SCOUT" level="10"/>
		</actions>
	</item_template>
)xml";

/** skill_templates.xml's skill 1, cut to its header */
constexpr std::string_view WHITE_TIGER_ROW = R"xml(
	<skill_template skill_id="1" name="Transformation: White Tiger" nameId="2285933" stack="RA_LIGHT_WHITETIGER" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0">
	</skill_template>
)xml";

/** skill_tree.xml :3 */
constexpr std::string_view SKILL_TREE_XML = R"xml(<skill_tree>
	<skill skillId="1" minLevel="10" race="ELYOS" classId="RANGER" />
</skill_tree>)xml";

std::string withRows(std::string_view base, std::string_view closingTag, std::string_view rows) {
	std::string xml(base);
	xml.insert(xml.rfind(closingTag), rows);
	return xml;
}

class SkillLearnActionTest : public ItemServicesTest {
protected:
	void SetUp() override {
		ItemServicesTest::SetUp();
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(
			xml::bindString<dataholders::ItemData>(context, withRows(ITEM_TEMPLATES_XML, "</item_templates>", BOOK_ROWS)));
		dataholders::DataManager::SKILL_DATA.resetForTests();
		dataholders::DataManager::SKILL_DATA.publish(
			xml::bindString<dataholders::SkillData>(context, withRows(SKILL_TEMPLATES_XML, "</skill_data>", WHITE_TIGER_ROW)));
		dataholders::DataManager::SKILL_TREE_DATA.publish(xml::bindString<dataholders::SkillTreeData>(context, SKILL_TREE_XML));
		makeLooter(PlayerClass::RANGER, 10);
		player().setSkillList(model::skill::PlayerSkillList::create(std::vector<Ptr<model::skill::PlayerSkillEntry>>{})); // no skills yet
	}

	void TearDown() override {
		ItemServicesTest::TearDown();
		dataholders::DataManager::SKILL_TREE_DATA.resetForTests();
	}

	void makeLooter(PlayerClass playerClass, int32_t level) {
		player().getCommonData()->setPlayerClass(playerClass);
		if (level >= 10)
			player().getCommonData()->setDaeva(true);
		player().getCommonData()->setLevel(level);
	}

	/** The book's only action, the SkillLearnAction its <skilllearn> binds */
	const SkillLearnAction& actionOf(int32_t itemId) {
		const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
		if (itemTemplate == nullptr || itemTemplate->getActions() == nullptr)
			throw runtime::NullPointerException("no actions of item " + std::to_string(itemId));
		const auto* action = dynamic_cast<const SkillLearnAction*>(itemTemplate->getActions()->getItemActions().front().get());
		if (action == nullptr)
			throw runtime::NullPointerException("item " + std::to_string(itemId) + " has no SkillLearnAction");
		return *action;
	}

	bool knows(int32_t skillId) { return player().getSkillList()->isSkillPresent(skillId); }

	xml::LoadContext context;
};

// SkillLearnAction.java:31-51: the Elyos Ranger of level 10 who does not know the skill may read the book
TEST_F(SkillLearnActionTest, ARangerOfTheBooksLevelAndRaceMayLearnIt) {
	Item& book = stored(BOOK, WHITE_TIGER_BOOK, 1);
	EXPECT_TRUE(actionOf(WHITE_TIGER_BOOK).canAct(player(), Ptr<Item>(book), nullptr));
}

// the four refusals: level, class (the starting-class rule both ways), race, known skill
TEST_F(SkillLearnActionTest, CanActRefusesTheLevelTheClassTheRaceAndAKnownSkill) {
	Item& book = stored(BOOK, WHITE_TIGER_BOOK, 1);

	makeLooter(PlayerClass::RANGER, 9);
	EXPECT_FALSE(actionOf(WHITE_TIGER_BOOK).canAct(player(), Ptr<Item>(book), nullptr)) << "1. below the book's level 10";

	makeLooter(PlayerClass::ASSASSIN, 10);
	EXPECT_FALSE(actionOf(WHITE_TIGER_BOOK).canAct(player(), Ptr<Item>(book), nullptr)) << "another class";
	makeLooter(PlayerClass::SCOUT, 10);
	EXPECT_FALSE(actionOf(WHITE_TIGER_BOOK).canAct(player(), Ptr<Item>(book), nullptr))
		<< "a Ranger's book is no Scout's: the rule compares the book's class with the player's starting class, not the reverse";
	makeLooter(PlayerClass::RANGER, 10);
	Item& scoutBook = stored(BOOK + 1, WHITE_TIGER_BOOK_SCOUT, 1);
	EXPECT_TRUE(actionOf(WHITE_TIGER_BOOK_SCOUT).canAct(player(), Ptr<Item>(scoutBook), nullptr)) << "a Scout's book is a Ranger's too";

	Item& asmodianBook = stored(BOOK + 2, WHITE_TIGER_BOOK_ASMODIAN, 1);
	EXPECT_FALSE(actionOf(WHITE_TIGER_BOOK_ASMODIAN).canAct(player(), Ptr<Item>(asmodianBook), nullptr)) << "4. the other race";

	player().getSkillList()->addSkill(player(), WHITE_TIGER, 1);
	EXPECT_FALSE(actionOf(WHITE_TIGER_BOOK).canAct(player(), Ptr<Item>(book), nullptr)) << "5. already learned";
}

// SkillLearnAction.java:53-64: the usage animation, the skill learned, STR_USE_ITEM, the book deleted
TEST_F(SkillLearnActionTest, ActLearnsTheSkillSaysSoAndDeletesTheBook) {
	Item& book = stored(BOOK, WHITE_TIGER_BOOK, 1);
	const std::string l10n = book.getL10n();
	clearSent();

	actionOf(WHITE_TIGER_BOOK).act(player(), Ptr<Item>(book), nullptr);

	EXPECT_TRUE(knows(WHITE_TIGER)) << "learnSkillBook: the skill tree's skill 1 for the Elyos Ranger of level 10";
	EXPECT_FALSE(player().getInventory().getItemByObjId(BOOK)) << "the book is deleted";
	const std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(std::count(packets.begin(), packets.end(),
				  serialized(network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION(player().getObjectId(), BOOK, WHITE_TIGER_BOOK))),
		1)
		<< "the usage animation (broadcastPacket(player, ..., true) sends it to the player too)";
	EXPECT_EQ(std::count(packets.begin(), packets.end(), serialized(SM_SYSTEM_MESSAGE::STR_USE_ITEM(l10n))), 1);
}

} // namespace
} // namespace aion::gameserver::services::item::test
