// M5e T-01 (m5e-plan.md §5 stage 3; P5-07): the StigmaService bodies that were AION_UNPORTED - isEquipped (both), getPossibleStigmaCount,
// isCompleteQuest, getPossibleAdvancedStigmaCount and isPossibleEquippedStigma (through onPlayerLogin), add/removeStigmaSkills,
// removeLinkedStigmaSkills and getLinkedStigmaLearnSkill (through addLinkedStigmaSkills), and chargeStigma with its ItemUseObserver and task -
// against StigmaService.java:173-484.
//
// The rows are item_templates.xml's six chargeable Gladiator stigmas 140001103 Sure Strike, 104, 105, 106, 107 and 118 (each cut to its
// <stigma>), skill_templates.xml's FI_BURSERKLANCE group (691-693 Sure Strike I-III, cut to the header) and 662 Battle Banner / 731 Wind Lance,
// and skill_tree.xml's rows of those skills (cut to their level, class, race and stigma columns: no skillLearn chain). The player is
// ItemServicesTest's "Looter" (700101, ELYOS), made a Gladiator of the case's level. The membership settings are the shipped defaults
// (gameserver.quest.stigma.slot and gameserver.autolearn.stigma: 10), set per case because this binary loads no config.

#include "ItemServicesTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/dataholders/ItemSetData.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/SkillTreeData.bind.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/services/StigmaService.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test {
namespace {

using model::PlayerClass;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::StigmaService;

constexpr int32_t SURE_STRIKE = 140001103;
constexpr int32_t SPITE_STRIKE = 140001104;
constexpr int32_t SHARPNESS_HIT = 140001105;
constexpr int32_t KNEE_CRASH = 140001106;
constexpr int32_t WHIRL_DRAIN = 140001107;
constexpr int32_t DRAIN_SWORD = 140001118;

constexpr int64_t ADV_STIGMA1 = 1LL << 33;
constexpr int64_t ADV_STIGMA2 = 1LL << 34;
constexpr int64_t ADV_STIGMA3 = 1LL << 35;
constexpr int64_t STIGMA1 = 1LL << 30;
constexpr int64_t STIGMA2 = 1LL << 31;
constexpr int64_t STIGMA3 = 1LL << 32;

/** item_templates.xml, the six chargeable Gladiator stigmas, cut to their <stigma> */
constexpr std::string_view STIGMA_ROWS = R"xml(
	<item_template id="140001103" name="Sure Strike" level="45" cName="STIGMA_N_FI_burserklance_g1" mask="4222" item_group="STIGMA" quality="LEGEND" price="3970" restrict="0 45 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" desc="844617">
		<stigma gain_skill_group1="FI_BURSERKLANCE" chargeable="true"/>
	</item_template>
	<item_template id="140001104" name="Spite Strike" level="45" cName="STIGMA_N_FI_technicalcounter_g1" mask="4222" item_group="STIGMA" quality="LEGEND" price="3970" restrict="0 45 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" desc="844618">
		<stigma gain_skill_group1="FI_TECHNICALCOUNTER" chargeable="true"/>
	</item_template>
	<item_template id="140001105" name="Sharpness Hit" level="45" cName="STIGMA_N_FI_sharpnesshit_g1" mask="4222" item_group="STIGMA" quality="LEGEND" price="3970" restrict="0 45 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" desc="844619">
		<stigma gain_skill_group1="FI_SHARPNESSHIT" chargeable="true"/>
	</item_template>
	<item_template id="140001106" name="Knee Crash" level="45" cName="STIGMA_N_FI_kneecrash_g1" mask="4222" item_group="STIGMA" quality="LEGEND" price="3970" restrict="0 45 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" desc="844620">
		<stigma gain_skill_group1="FI_KNEECRASH" chargeable="true"/>
	</item_template>
	<item_template id="140001107" name="Whirl Drain" level="45" cName="STIGMA_N_FI_whirldrain_g1" mask="4222" item_group="STIGMA" quality="LEGEND" price="3970" restrict="0 45 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" desc="844621">
		<stigma gain_skill_group1="FI_WHIRLDRAIN" chargeable="true"/>
	</item_template>
	<item_template id="140001118" name="Drain Sword" level="55" cName="STIGMA_N_FI_drainsword_g1" mask="4222" item_group="STIGMA" quality="UNIQUE" price="3970" restrict="0 55 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" desc="844632">
		<stigma gain_skill_group1="FI_DRAINSWORD" chargeable="true"/>
	</item_template>
)xml";

/** skill_templates.xml, cut to the header: Sure Strike I-III (the FI_BURSERKLANCE group), one skill of each other stigma's group, Battle Banner
 * and Wind Lance */
constexpr std::string_view SKILL_ROWS = R"xml(
	<skill_template skill_id="691" name="Sure Strike" nameId="2287774" group="FI_BURSERKLANCE" stack="FI_BURSERKLANCE" lvl="1" skilltype="PHYSICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"></skill_template>
	<skill_template skill_id="692" name="Sure Strike" nameId="2287774" group="FI_BURSERKLANCE" stack="FI_BURSERKLANCE" lvl="2" skilltype="PHYSICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"></skill_template>
	<skill_template skill_id="693" name="Sure Strike" nameId="2287774" group="FI_BURSERKLANCE" stack="FI_BURSERKLANCE" lvl="3" skilltype="PHYSICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"></skill_template>
	<skill_template skill_id="64601" name="Spite Strike" nameId="1" group="FI_TECHNICALCOUNTER" stack="TEST_STIGMA_1" lvl="1" skilltype="PHYSICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"></skill_template>
	<skill_template skill_id="64602" name="Sharpness Hit" nameId="1" group="FI_SHARPNESSHIT" stack="TEST_STIGMA_2" lvl="1" skilltype="PHYSICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"></skill_template>
	<skill_template skill_id="64603" name="Knee Crash" nameId="1" group="FI_KNEECRASH" stack="TEST_STIGMA_3" lvl="1" skilltype="PHYSICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"></skill_template>
	<skill_template skill_id="64604" name="Whirl Drain" nameId="1" group="FI_WHIRLDRAIN" stack="TEST_STIGMA_4" lvl="1" skilltype="PHYSICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"></skill_template>
	<skill_template skill_id="64605" name="Drain Sword" nameId="1" group="FI_DRAINSWORD" stack="TEST_STIGMA_5" lvl="1" skilltype="PHYSICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"></skill_template>
	<skill_template skill_id="662" name="Battle Banner" nameId="2287761" group="FI_WARFLAG" stack="FI_LIGHT_WARFLAG" lvl="3" skilltype="MAGICAL" skillsubtype="SUMMON" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"></skill_template>
	<skill_template skill_id="731" name="Wind Lance" nameId="2287755" group="FI_BLADESHOCK" stack="FI_BLADESHOCK_1" lvl="3" skilltype="PHYSICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"></skill_template>
)xml";

/** skill_tree.xml :828-830, :799, :866 (cut: no skillLearn chain), and the test skills of the other stigmas (stigma="2": an advanced stigma) */
constexpr std::string_view SKILL_TREE_XML = R"xml(<skill_tree>
	<skill skillId="691" minLevel="45" classId="GLADIATOR" stigma="2" />
	<skill skillId="692" minLevel="49" classId="GLADIATOR" stigma="2" />
	<skill skillId="693" minLevel="53" classId="GLADIATOR" stigma="2" />
	<skill skillId="64601" minLevel="45" classId="GLADIATOR" stigma="2" />
	<skill skillId="64602" minLevel="45" classId="GLADIATOR" stigma="2" />
	<skill skillId="64603" minLevel="45" classId="GLADIATOR" stigma="2" />
	<skill skillId="64604" minLevel="45" classId="GLADIATOR" stigma="2" />
	<skill skillId="64605" minLevel="45" classId="GLADIATOR" stigma="2" />
	<skill skillId="662" minLevel="63" race="ELYOS" classId="GLADIATOR" stigma="4" />
	<skill skillId="731" minLevel="63" classId="GLADIATOR" stigma="4" />
</skill_tree>)xml";

/** player_experience_table.xml, all 66 rows (setLevel goes through setExp, which reads it; the base fixture publishes the first 16) */
constexpr std::string_view FULL_EXPERIENCE_TABLE_XML =
	"<player_experience_table><exp>0</exp><exp>400</exp><exp>1433</exp><exp>3820</exp><exp>9054</exp><exp>17655</exp><exp>30978</exp>"
	"<exp>52010</exp><exp>82982</exp><exp>126069</exp><exp>182252</exp><exp>260622</exp><exp>360825</exp><exp>490331</exp><exp>649169</exp>"
	"<exp>844378</exp><exp>1083018</exp><exp>1401356</exp><exp>1808613</exp><exp>2314771</exp><exp>2941893</exp><exp>3769257</exp>"
	"<exp>4811154</exp><exp>6110198</exp><exp>7632340</exp><exp>9377726</exp><exp>11395643</exp><exp>13731725</exp><exp>16339413</exp>"
	"<exp>19378549</exp><exp>23162749</exp><exp>27585843</exp><exp>32841197</exp><exp>39127217</exp><exp>47350762</exp><exp>57829684</exp>"
	"<exp>70654362</exp><exp>87571065</exp><exp>107018757</exp><exp>129815732</exp><exp>157211282</exp><exp>189272188</exp><exp>226933751</exp>"
	"<exp>267247400</exp><exp>310053925</exp><exp>355815203</exp><exp>404823687</exp><exp>456685353</exp><exp>511683757</exp>"
	"<exp>570162075</exp><exp>632268545</exp><exp>701585822</exp><exp>776831823</exp><exp>857090855</exp><exp>947120930</exp>"
	"<exp>1051346275</exp><exp>1175571620</exp><exp>1318550121</exp><exp>1484090156</exp><exp>1674064804</exp><exp>1913274732</exp>"
	"<exp>2162140395</exp><exp>2419819338</exp><exp>2700930959</exp><exp>3209499233</exp><exp>3794060468</exp></player_experience_table>";

std::string withRows(std::string_view base, std::string_view closingTag, std::string_view rows) {
	std::string xml(base);
	xml.insert(xml.rfind(closingTag), rows);
	return xml;
}

class StigmaServiceTest : public ItemServicesTest {
protected:
	void SetUp() override {
		ItemServicesTest::SetUp();
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(
			xml::bindString<dataholders::ItemData>(context, withRows(ITEM_TEMPLATES_XML, "</item_templates>", STIGMA_ROWS)));
		dataholders::DataManager::SKILL_DATA.resetForTests();
		dataholders::DataManager::SKILL_DATA.publish(
			xml::bindString<dataholders::SkillData>(context, withRows(SKILL_TEMPLATES_XML, "</skill_data>", SKILL_ROWS)));
		dataholders::DataManager::SKILL_TREE_DATA.publish(xml::bindString<dataholders::SkillTreeData>(context, SKILL_TREE_XML));
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(xml::bindString<dataholders::PlayerExperienceTable>(context, FULL_EXPERIENCE_TABLE_XML));
		// onPlayerLogin's unEquipItem asks the item sets (Equipment.unEquip -> ItemSetData): none
		dataholders::DataManager::ITEM_SET_DATA.publish(std::make_unique<dataholders::ItemSetData>());
		player().setSkillList(model::skill::PlayerSkillList::create(std::vector<Ptr<model::skill::PlayerSkillEntry>>{}));
		makeGladiator(50);
	}

	void TearDown() override {
		ItemServicesTest::TearDown();
		dataholders::DataManager::SKILL_TREE_DATA.resetForTests();
		dataholders::DataManager::ITEM_SET_DATA.resetForTests();
	}

	void makeGladiator(int32_t level) {
		player().getCommonData()->setPlayerClass(PlayerClass::GLADIATOR);
		player().getCommonData()->setDaeva(true);
		player().getCommonData()->setLevel(level);
	}

	/** the stigma quest 1929 (Elyos) in the given state */
	void stigmaQuest(questEngine::model::QuestStatus status, int32_t questVars = 0) {
		Ref<questEngine::model::QuestState> state =
			questEngine::model::QuestState::create(1929, status, questVars, 0, status == questEngine::model::QuestStatus::COMPLETE ? 1 : 0, std::nullopt,
				std::nullopt, std::nullopt);
		player().getQuestStateList()->addQuest(1929, *state);
	}

	/** an equipped stigma in `slot`, loaded like the inventory DAO does */
	Item& equipped(int32_t objId, int32_t itemId, int64_t slot, int32_t enchant = 0) {
		Ref<Item> item = loadedItem(objId, itemId, 1, StorageType::CUBE, slot, true);
		item->setEnchantLevel(enchant);
		items.push_back(item);
		player().getEquipment().onLoadHandler(*item);
		return *item;
	}

	int32_t skillLevel(int32_t skillId) {
		Ptr<model::skill::PlayerSkillEntry> entry = player().getSkillList()->getSkillEntry(skillId);
		return entry ? entry->getSkillLevel() : 0;
	}

	/** a seed whose first Rnd.chance() of this thread satisfies `accept` (the thread is left seeded with it) */
	template <class Predicate>
	static void seedWhereFirstChance(Predicate accept) {
		for (uint64_t seed = 1; seed < 100000; ++seed) {
			commons::utils::Rnd::seedCurrentThreadForTests(seed);
			if (accept(commons::utils::Rnd::chance())) {
				commons::utils::Rnd::seedCurrentThreadForTests(seed);
				return;
			}
		}
		FAIL() << "no seed";
	}

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return serialized(std::move(packet)); }

	int64_t countSent(const std::vector<uint8_t>& packet) {
		const std::vector<std::vector<uint8_t>> packets = sent();
		return std::count(packets.begin(), packets.end(), packet);
	}

	AtomicConfigScope<int8_t> stigmaSlotQuest{configs::main::MembershipConfig::STIGMA_SLOT_QUEST, 10};
	AtomicConfigScope<int8_t> stigmaAutolearn{configs::main::MembershipConfig::STIGMA_AUTOLEARN, 10};
	xml::LoadContext context;
};

// ------------------------------------------------------------------------------------------------------------------------------ isEquipped

// StigmaService.java:307-319: a single item; and "exactly neededCount of these" - not "at least"
TEST_F(StigmaServiceTest, IsEquippedCountsExactly) {
	equipped(812001, SURE_STRIKE, ADV_STIGMA1);
	equipped(812002, SPITE_STRIKE, ADV_STIGMA2);
	EXPECT_TRUE(StigmaService::isEquipped(player(), SURE_STRIKE));
	EXPECT_FALSE(StigmaService::isEquipped(player(), SHARPNESS_HIT));
	EXPECT_TRUE(StigmaService::isEquipped(player(), 2, {SURE_STRIKE, SPITE_STRIKE, SHARPNESS_HIT}));
	EXPECT_FALSE(StigmaService::isEquipped(player(), 1, {SURE_STRIKE, SPITE_STRIKE, SHARPNESS_HIT})) << "two equipped is not one";
	EXPECT_FALSE(StigmaService::isEquipped(player(), 3, {SURE_STRIKE, SPITE_STRIKE, SHARPNESS_HIT}));
}

// -------------------------------------------------------------------------------------------- the slot counts (through onPlayerLogin)

// StigmaService.java:360-414 through onPlayerLogin's isPossibleEquippedStigma: with the stigma quest done, a Gladiator of level 50 has two
// advanced slots - the stigma in ADV_STIGMA3 is unequipped (audited), the two others give their skills (691 and 692: 692 needs level 49)
TEST_F(StigmaServiceTest, AtLevelFiftyTwoAdvancedSlotsAreOpen) {
	stigmaQuest(questEngine::model::QuestStatus::COMPLETE);
	Item& first = equipped(812011, SURE_STRIKE, ADV_STIGMA1);
	Item& second = equipped(812012, SPITE_STRIKE, ADV_STIGMA2);
	Item& third = equipped(812013, SHARPNESS_HIT, ADV_STIGMA3);

	StigmaService::onPlayerLogin(player());

	EXPECT_TRUE(first.isEquipped());
	EXPECT_TRUE(second.isEquipped());
	EXPECT_FALSE(third.isEquipped()) << "the third advanced slot opens at 55";
	EXPECT_EQ(skillLevel(691), 1) << "addStigmaSkills: stigmaLevel 0 + 1";
	EXPECT_EQ(skillLevel(692), 1);
	EXPECT_EQ(skillLevel(693), 0) << "Sure Strike III needs level 53";
	EXPECT_EQ(skillLevel(64601), 1) << "Spite Strike's skill";
	EXPECT_EQ(skillLevel(64602), 0) << "the unequipped Sharpness Hit gives nothing";
}

// :366-372: level 45 opens one advanced slot, 55 all three
TEST_F(StigmaServiceTest, TheAdvancedSlotsOpenAtFortyFiveFiftyAndFiftyFive) {
	stigmaQuest(questEngine::model::QuestStatus::COMPLETE);
	makeGladiator(45);
	Item& a = equipped(812021, SURE_STRIKE, ADV_STIGMA1);
	Item& b = equipped(812022, SPITE_STRIKE, ADV_STIGMA2);
	StigmaService::onPlayerLogin(player());
	EXPECT_TRUE(a.isEquipped());
	EXPECT_FALSE(b.isEquipped()) << "level 45: ADV_STIGMA1 only";

	makeGladiator(55);
	Item& c = equipped(812023, SHARPNESS_HIT, ADV_STIGMA3);
	StigmaService::onPlayerLogin(player());
	EXPECT_TRUE(c.isEquipped()) << "level 55: all three";
}

// :339-358: without the quest no slot is open; the quest still in START with its last var (98 for the Elyos) counts as done
TEST_F(StigmaServiceTest, TheStigmaQuestOpensTheSlotsEvenAtItsLastStep) {
	Item& a = equipped(812031, SURE_STRIKE, ADV_STIGMA1);
	StigmaService::onPlayerLogin(player());
	EXPECT_FALSE(a.isEquipped()) << "no stigma quest: no slot";

	stigmaQuest(questEngine::model::QuestStatus::START, 97);
	Item& b = equipped(812032, SURE_STRIKE, ADV_STIGMA1);
	StigmaService::onPlayerLogin(player());
	EXPECT_FALSE(b.isEquipped()) << "START at var 97 is not done";

	player().getQuestStateList()->getQuestState(1929)->getQuestVars()->setVar(98);
	Item& c = equipped(812033, SURE_STRIKE, ADV_STIGMA1);
	StigmaService::onPlayerLogin(player());
	EXPECT_TRUE(c.isEquipped()) << "START at var 98 counts";
}

// :321-337: the regular slots - one below 30, two below 40, three from 40; the membership STIGMA_SLOT_QUEST opens all three without the quest
TEST_F(StigmaServiceTest, TheRegularSlotsOpenWithTheLevelOrTheMembership) {
	stigmaQuest(questEngine::model::QuestStatus::COMPLETE);
	makeGladiator(29);
	Item& a = equipped(812041, SURE_STRIKE, STIGMA1);
	Item& b = equipped(812042, SPITE_STRIKE, STIGMA2);
	StigmaService::onPlayerLogin(player());
	EXPECT_TRUE(a.isEquipped());
	EXPECT_FALSE(b.isEquipped()) << "level 29: STIGMA1 only";

	makeGladiator(30);
	Item& c = equipped(812043, SPITE_STRIKE, STIGMA2);
	Item& d = equipped(812044, SHARPNESS_HIT, STIGMA3);
	StigmaService::onPlayerLogin(player());
	EXPECT_TRUE(c.isEquipped()) << "level 30: two";
	EXPECT_FALSE(d.isEquipped());

	AtomicConfigScope<int8_t> everyone(configs::main::MembershipConfig::STIGMA_SLOT_QUEST, 0);
	player().getQuestStateList()->getQuestState(1929)->setStatus(questEngine::model::QuestStatus::START);
	Item& e = equipped(812045, SHARPNESS_HIT, STIGMA3);
	StigmaService::onPlayerLogin(player());
	EXPECT_TRUE(e.isEquipped()) << "the membership: three slots without the quest";
}

// --------------------------------------------------------------------------------------------------------------- removeStigmaSkills

// :470-484: the stigma's skills are removed, and with notifyPlayer the message names each skill once (Sure Strike I-III share their name)
TEST_F(StigmaServiceTest, RemovingAStigmaRemovesItsSkillsAndSaysSoOnce) {
	stigmaQuest(questEngine::model::QuestStatus::COMPLETE);
	Item& sureStrike = equipped(812051, SURE_STRIKE, ADV_STIGMA1);
	StigmaService::onPlayerLogin(player());
	ASSERT_EQ(skillLevel(691), 1);
	clearSent();

	const std::string name = dataholders::DataManager::SKILL_DATA->getSkillTemplate(691)->getL10n();
	StigmaService::removeStigmaSkills(player(), sureStrike.getItemTemplate()->getStigma(), 0, true);
	EXPECT_EQ(skillLevel(691), 0);
	EXPECT_EQ(skillLevel(692), 0);
	EXPECT_EQ(countSent(message(SM_SYSTEM_MESSAGE::STR_STIGMA_YOU_CANNOT_USE_THIS_SKILL_AFTER_UNEQUIP_STIGMA_STONE(name))), 1);

	StigmaService::onPlayerLogin(player());
	clearSent();
	StigmaService::removeStigmaSkills(player(), sureStrike.getItemTemplate()->getStigma(), 0, false);
	EXPECT_EQ(skillLevel(691), 0);
	EXPECT_EQ(countSent(message(SM_SYSTEM_MESSAGE::STR_STIGMA_YOU_CANNOT_USE_THIS_SKILL_AFTER_UNEQUIP_STIGMA_STONE(name))), 0) << "no notice";
}

// ----------------------------------------------------------------------------------------------------- the linked stigma skill

// :206-305: six chargeable stigmas give the linked skill of the lowest enchant + 1. Drain Sword with exactly two of Sure Strike / Spite Strike /
// Sharpness Hit is Wind Lance (731); with all three it is not "two", so the Elyos Gladiator gets Battle Banner (662). removeStigmaSkills ends
// with removeLinkedStigmaSkills, which takes it again
TEST_F(StigmaServiceTest, SixChargeableStigmasGiveTheLinkedSkillAndRemovingOneTakesIt) {
	AtomicConfigScope<int8_t> everyone(configs::main::MembershipConfig::STIGMA_SLOT_QUEST, 0); // three and three slots
	makeGladiator(65);
	equipped(812061, DRAIN_SWORD, ADV_STIGMA1, 2);
	Item& sureStrike = equipped(812062, SURE_STRIKE, ADV_STIGMA2, 1);
	equipped(812063, SPITE_STRIKE, ADV_STIGMA3, 3);
	equipped(812064, KNEE_CRASH, STIGMA1, 4);
	equipped(812065, WHIRL_DRAIN, STIGMA2, 5);
	Item& sixth = equipped(812066, SHARPNESS_HIT, STIGMA3, 6);

	StigmaService::addLinkedStigmaSkills(player());
	EXPECT_EQ(skillLevel(662), 2) << "three of the three: Battle Banner, at the lowest enchant (Sure Strike's 1) + 1";
	EXPECT_EQ(skillLevel(731), 0);
	static_cast<void>(sixth);

	StigmaService::removeStigmaSkills(player(), sureStrike.getItemTemplate()->getStigma(), 1, false);
	EXPECT_EQ(skillLevel(662), 0) << "removeLinkedStigmaSkills";
}

TEST_F(StigmaServiceTest, ExactlyTwoOfTheThreeWithDrainSwordIsWindLance) {
	AtomicConfigScope<int8_t> everyone(configs::main::MembershipConfig::STIGMA_SLOT_QUEST, 0);
	makeGladiator(65);
	equipped(812071, DRAIN_SWORD, ADV_STIGMA1, 2);
	equipped(812072, SURE_STRIKE, ADV_STIGMA2, 1);
	equipped(812073, SPITE_STRIKE, ADV_STIGMA3, 3);
	equipped(812074, KNEE_CRASH, STIGMA1, 4);
	equipped(812075, WHIRL_DRAIN, STIGMA2, 5);
	equipped(812076, KNEE_CRASH, STIGMA3, 6); // a sixth that is none of the three

	StigmaService::addLinkedStigmaSkills(player());
	EXPECT_EQ(skillLevel(731), 2) << "Wind Lance at the lowest enchant 1 + 1";
	EXPECT_EQ(skillLevel(662), 0);
}

// -------------------------------------------------------------------------------------------------------------------------- chargeStigma

// :416-423: another stigma, an enchanted charge stone and a stigma at 10 are refused before anything is sent
TEST_F(StigmaServiceTest, ChargeStigmaRefusesAnotherStoneAnEnchantedStoneAndAFullStigma) {
	Item& stigma = stored(812081, SURE_STRIKE, 1);
	Item& other = stored(812082, SPITE_STRIKE, 1);
	Item& enchantedStone = stored(812083, SURE_STRIKE, 1);
	enchantedStone.setEnchantLevel(1);
	clearSent();

	StigmaService::chargeStigma(player(), stigma, other);
	StigmaService::chargeStigma(player(), stigma, enchantedStone);
	stigma.setEnchantLevel(10);
	Item& stone = stored(812084, SURE_STRIKE, 1);
	StigmaService::chargeStigma(player(), stigma, stone);
	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::ITEM_USE));
}

// :425-458: at enchant 0 the chance is 100: 5 s later the stone is used up, the equipped stigma is +1 and its skills are learned again one level
// up, and the success message is sent
TEST_F(StigmaServiceTest, AChargedEquippedStigmaGoesUpOneLevelWithItsSkills) {
	stigmaQuest(questEngine::model::QuestStatus::COMPLETE);
	Item& stigma = equipped(812091, SURE_STRIKE, ADV_STIGMA1);
	StigmaService::onPlayerLogin(player());
	ASSERT_EQ(skillLevel(691), 1);
	Item& stone = stored(812092, SURE_STRIKE, 1);
	clearSent();

	StigmaService::chargeStigma(player(), stigma, stone);
	EXPECT_TRUE(player().getController().hasTask(model::TaskId::ITEM_USE));
	executor->advance(std::chrono::milliseconds(4999));
	EXPECT_EQ(stigma.getEnchantLevel(), 0);
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_EQ(stigma.getEnchantLevel(), 1);
	EXPECT_FALSE(player().getInventory().getItemByObjId(812092)) << "the stone is used up";
	EXPECT_EQ(skillLevel(691), 2) << "removeStigmaSkills then addStigmaSkills at enchant 1 + 1";
	EXPECT_EQ(countSent(message(SM_SYSTEM_MESSAGE::STR_MSG_STIGMA_ENCHANT_SUCCESS(stigma.getL10n()))), 1);
}

// :400-405: at enchant 9 the chance is max(25, 10) = 25; a roll of 25 or more fails: the stone and the stigma are used up
TEST_F(StigmaServiceTest, AFailedChargeDestroysTheStigma) {
	Item& stigma = stored(812101, SURE_STRIKE, 1);
	stigma.setEnchantLevel(9);
	const std::string l10n = stigma.getL10n();
	stored(812102, SURE_STRIKE, 1);
	Item& stone = *player().getInventory().getItemByObjId(812102);
	seedWhereFirstChance([](float chance) { return chance >= 25; });
	clearSent();

	StigmaService::chargeStigma(player(), stigma, stone);
	executor->advance(std::chrono::milliseconds(5000));
	EXPECT_FALSE(player().getInventory().getItemByObjId(812102)) << "the stone";
	EXPECT_FALSE(player().getInventory().getItemByObjId(812101)) << "and the stigma are used up";
	EXPECT_EQ(countSent(message(SM_SYSTEM_MESSAGE::STR_MSG_STIGMA_ENCHANT_FAIL(l10n))), 1);
}

// :427-437: an attack on the player aborts the charge (ItemUseObserver.attacked): the task is cancelled, STR_ITEM_CANCELED, nothing used
TEST_F(StigmaServiceTest, AnAttackAbortsTheCharge) {
	Item& stigma = stored(812111, SURE_STRIKE, 1);
	Item& stone = stored(812112, SURE_STRIKE, 1);
	clearSent();

	StigmaService::chargeStigma(player(), stigma, stone);
	player().getObserveController()->notifyAttackedObservers(player(), 0);
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::ITEM_USE));
	EXPECT_EQ(countSent(message(SM_SYSTEM_MESSAGE::STR_ITEM_CANCELED())), 1);
	executor->advance(std::chrono::milliseconds(5000));
	EXPECT_TRUE(player().getInventory().getItemByObjId(812112)) << "the stone is kept";
	EXPECT_EQ(stigma.getEnchantLevel(), 0);
}

} // namespace
} // namespace aion::gameserver::services::item::test
