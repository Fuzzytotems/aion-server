// P4-07b quest, event, challenge, flight, gathering and ai templates: hooks on small XML fixtures and the logic methods, with expected values
// derived by hand from QuestTemplate.java, XMLStartCondition.java, QuestDrop.java, HandlerSideDrop.java, QuestNpc.java, EventTemplate.java,
// Buff.java, SummonGroup.java, FlyPathEntry.java, FlyRingTemplate.java, Material.java and the enum sources of these packages.

#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/configs/main/CraftConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataException.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/templates/QuestTemplate.bind.h"
#include "aion/gameserver/model/templates/StorageExpansionTemplate.bind.h"
#include "aion/gameserver/model/templates/TitleTemplate.bind.h"
#include "aion/gameserver/model/templates/ai/SummonGroup.bind.h"
#include "aion/gameserver/model/templates/challenge/ChallengeTypeInfo.h"
#include "aion/gameserver/model/templates/challenge/RewardTypeInfo.h"
#include "aion/gameserver/model/templates/cp/CPRank.bind.h"
#include "aion/gameserver/model/templates/event/Buff.bind.h"
#include "aion/gameserver/model/templates/event/EventTemplate.bind.h"
#include "aion/gameserver/model/templates/event/upgradearcade/ArcadeLevels.bind.h"
#include "aion/gameserver/model/templates/flypath/FlyPathEntry.bind.h"
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.h"
#include "aion/gameserver/model/templates/gather/GatherableTemplate.bind.h"
#include "aion/gameserver/model/templates/npcskill/ConjunctionTypeInfo.h"
#include "aion/gameserver/model/templates/npcskill/QueuedNpcSkillTemplate.h"
#include "aion/gameserver/model/templates/quest/HandlerSideDrop.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/model/templates/quest/QuestRepeatCycleInfo.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::model::templates {
namespace {

template <class T>
std::unique_ptr<T> bindXml(std::string_view text) {
	xml::LoadContext context;
	return xml::bindString<T>(context, text);
}

// ---- quests ------------------------------------------------------------------------------------------------------------------------------------

TEST(QuestTemplatesTest, ListsAndFlags) {
	std::unique_ptr<QuestTemplate> quest =
	  bindXml<QuestTemplate>(R"(<quest id="1000" nameId="1100000" max_repeat_count="3" use_class_reward="2")"
	                         R"( combine_skillpoint="399" category="EVENT" mentor_type="MENTOR" repeat_cycle="MON TUE">)"
	                         R"(<rewards gold="10" ccheck="1 2"><reward_item item_id="5" count="2"/><selectable_reward_item item_id="6"/></rewards>)"
	                         R"(<quest_drop npc_id="1" item_id="2"/><quest_drop npc_id="3" item_id="4" chance="50" drop_each_member="2"/>)"
	                         R"(<quest_kill seq="1" npc_ids="10 11" count="2"/>)"
	                         R"(<class_permitted>GLADIATOR CLERIC</class_permitted>)"
	                         R"(<fighter_selectable_reward item_id="7" count="3"/><priest_selectable_reward item_id="8"/>)"
	                         R"(<collect_items start_check="true"><collect_item item_id="9" count="1"/></collect_items>)"
	                         R"(</quest>)");
	EXPECT_EQ(quest->getL10nId(), 1100000);
	EXPECT_EQ(quest->getL10n(), utils::ChatUtil::l10n(1100000)) << "L10n default method";
	ASSERT_EQ(quest->getRewards().size(), 1u);
	const quest::Rewards& rewards = quest->getRewards()[0];
	EXPECT_EQ(rewards.getKinah(), 10);
	ASSERT_EQ(rewards.getRewardItem().size(), 1u);
	EXPECT_EQ(rewards.getRewardItem()[0].getCount(), 2);
	ASSERT_EQ(rewards.getSelectableRewardItem().size(), 1u);
	EXPECT_EQ(rewards.getSelectableRewardItem()[0].getCount(), 1) << "QuestItems count defaults to 1";
	EXPECT_EQ(rewards.getCollectItemChecks(), (std::vector<int32_t>{1, 2}));

	ASSERT_EQ(quest->getQuestDrop().size(), 2u);
	EXPECT_EQ(quest->getQuestDrop()[0].getChance(), 100) << "no chance attribute";
	EXPECT_FALSE(quest->getQuestDrop()[0].isDropEachMemberGroup());
	EXPECT_FALSE(quest->getQuestDrop()[0].isDropEachMemberAlliance());
	EXPECT_EQ(quest->getQuestDrop()[1].getChance(), 50);
	EXPECT_FALSE(quest->getQuestDrop()[1].isDropEachMemberGroup());
	EXPECT_TRUE(quest->getQuestDrop()[1].isDropEachMemberAlliance());
	EXPECT_EQ(quest->getQuestDrop()[1].getQuestId(), std::nullopt) << "set by the holder, not by binding";

	ASSERT_EQ(quest->getQuestKill().size(), 1u);
	EXPECT_EQ(quest->getQuestKill()[0].getNpcIds(), (std::vector<int32_t>{10, 11}));
	EXPECT_EQ(quest->getQuestKill()[0].getNpcIds(), (std::vector<int32_t>{10, 11})) << "a second call returns the same ids";
	EXPECT_EQ(quest->getClassPermitted(), (std::vector<PlayerClass>{PlayerClass::GLADIATOR, PlayerClass::CLERIC}));
	EXPECT_EQ(quest->getSelectableRewardByClass(PlayerClass::GLADIATOR).size(), 1u);
	EXPECT_EQ(quest->getSelectableRewardByClass(PlayerClass::GLADIATOR)[0].getCount(), 3);
	EXPECT_EQ(quest->getSelectableRewardByClass(PlayerClass::CLERIC).size(), 1u) << "priest rewards";
	EXPECT_TRUE(quest->getSelectableRewardByClass(PlayerClass::TEMPLAR).empty());
	EXPECT_TRUE(quest->getSelectableRewardByClass(PlayerClass::WARRIOR).empty()) << "base classes fall through the switch";
	EXPECT_TRUE(quest->getCollectItems()->getStartCheck());
	EXPECT_EQ(quest->getCollectItems()->getCollectItem().size(), 1u);

	EXPECT_FALSE(quest->isClassRewardOnEveryRepeat());
	EXPECT_TRUE(quest->isSingleTimeClassReward());
	EXPECT_TRUE(quest->isRepeatable());
	EXPECT_TRUE(quest->isMentor());
	EXPECT_TRUE(quest->isTimeBased());
	EXPECT_FALSE(quest->isDaily());
	EXPECT_TRUE(quest->isWeekly());
	EXPECT_FALSE(quest->isMaster());
	EXPECT_TRUE(quest->isExpert());
	EXPECT_TRUE(quest->isProfession());
	EXPECT_FALSE(quest->isMission());
	EXPECT_TRUE(quest->isNoCount());
	EXPECT_EQ(quest->getRequiredConditionCount(), 0) << "no start conditions";

	std::unique_ptr<QuestTemplate> plain = bindXml<QuestTemplate>(R"(<quest id="1" repeat_cycle="ALL" category="MISSION"/>)");
	EXPECT_TRUE(plain->getRewards().empty());
	EXPECT_TRUE(plain->getClassPermitted().empty());
	EXPECT_FALSE(plain->isRepeatable()) << "max_repeat_count defaults to 1";
	EXPECT_TRUE(plain->isDaily());
	EXPECT_FALSE(plain->isWeekly());
	EXPECT_TRUE(plain->isMission());
	EXPECT_FALSE(plain->isNoCount());
	EXPECT_FALSE(bindXml<QuestTemplate>(R"(<quest id="2"/>)")->isTimeBased());
}

TEST(QuestTemplatesTest, RequiredConditionCount) {
	const std::string conditions = R"(<start_conditions><finished quest_id="1"/></start_conditions>)"
	                               R"(<start_conditions><finished quest_id="2" reward="1"/><acquired>3</acquired></start_conditions>)"
	                               R"(<start_conditions><unfinished>4 5</unfinished></start_conditions>)";
	std::unique_ptr<QuestTemplate> quest = bindXml<QuestTemplate>(R"(<quest id="1">)" + conditions + "</quest>");
	ASSERT_EQ(quest->getXMLStartConditions().size(), 3u);
	EXPECT_TRUE(quest->getXMLStartConditions()[0].isOptional());
	EXPECT_TRUE(quest->getXMLStartConditions()[1].isOptional());
	EXPECT_FALSE(quest->getXMLStartConditions()[2].isOptional());
	EXPECT_EQ(quest->getRequiredConditionCount(), 2) << "min(1, 2 optional) + 1 mandatory";

	auto& maxMaster = configs::main::CraftConfig::MAX_MASTER_CRAFTING_SKILLS;
	int32_t previous = maxMaster.load();
	maxMaster.store(2);
	std::unique_ptr<QuestTemplate> master = bindXml<QuestTemplate>(R"(<quest id="1" combine_skillpoint="499">)" + conditions + "</quest>");
	EXPECT_TRUE(master->isMaster());
	EXPECT_EQ(master->getRequiredConditionCount(), 1) << "2 + 1 - MAX_MASTER_CRAFTING_SKILLS";
	maxMaster.store(previous);
}

TEST(QuestTemplatesTest, HandlerSideDrop) {
	xml::LoadContext context;
	struct Unpublish { // also when an exception ends the test early
		~Unpublish() { dataholders::DataManager::QUEST_DATA.resetForTests(); }
	} unpublish;
	dataholders::DataManager::QUEST_DATA.publish(
	  xml::bindString<dataholders::QuestsData>(context, R"(<quests>)"
	                                                    R"(<quest id="1500"><quest_drop npc_id="700157" item_id="182201001" drop_each_member="1"/>)"
	                                                    R"(<quest_drop npc_id="210671" item_id="182200001" drop_each_member="2"/></quest></quests>)"));
	quest::HandlerSideDrop drop(1500, 210671, 182200001, 5, 40);
	EXPECT_EQ(drop.getQuestId(), 1500);
	EXPECT_EQ(drop.getNpcId(), 210671);
	EXPECT_EQ(drop.getItemId(), 182200001);
	EXPECT_EQ(drop.getChance(), 40);
	EXPECT_EQ(drop.getNeededAmount(), 5);
	EXPECT_FALSE(drop.isDropEachMemberGroup()) << "drop_each_member 2 copied from the xml drop";
	EXPECT_TRUE(drop.isDropEachMemberAlliance());
	EXPECT_EQ(drop.getCollectingStep(), 0);
	quest::HandlerSideDrop stepped(1500, 1, 2, 3, 100, 7);
	EXPECT_EQ(stepped.getCollectingStep(), 7);
	EXPECT_FALSE(stepped.isDropEachMemberAlliance()) << "no matching xml drop";
	EXPECT_THROW(quest::HandlerSideDrop(999, 1, 2, 3, 4), runtime::NullPointerException) << "unknown quest";
}

TEST(QuestTemplatesTest, QuestNpcRegistrationsAndOtherQuestTypes) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<quest::QuestNpc> npc = quest::QuestNpc::create(203000);
	EXPECT_EQ(npc->getQuestRange(), 20);
	npc->addOnQuestStart(1);
	npc->addOnQuestStart(1);
	npc->addOnAttackEvent(2);
	npc->addOnAttackEvent(2);
	npc->addOnAttackEvent(3);
	EXPECT_EQ(npc->getOnQuestStart().size(), 1);
	EXPECT_EQ(npc->getOnAttackEvent().size(), 2) << "no duplicates";
	EXPECT_EQ(npc->findAllRegisteredQuestIds([](int32_t questId) { return questId != 3; }), (std::unordered_set<int32_t>{1, 2}));

	quest::QuestItems items(186000001, 7);
	EXPECT_EQ(items.getItemId(), 186000001);
	EXPECT_EQ(items.getCount(), 7);
	EXPECT_EQ(quest::QuestItems().getCount(), 1);

	EXPECT_EQ(quest::getDay(quest::QuestRepeatCycle::SUN), 7);
	EXPECT_EQ(quest::getL10nId(quest::QuestRepeatCycle::SUN), 900330);
	EXPECT_EQ(quest::getL10nId(quest::QuestRepeatCycle::MON), 900331);
	EXPECT_EQ(quest::getL10n(quest::QuestRepeatCycle::ALL), utils::ChatUtil::l10n(0));

	npcskill::QueuedNpcSkillTemplate queued(17000, 3);
	EXPECT_EQ(queued.getSkillId(), 17000);
	EXPECT_EQ(queued.getSkillLevel(), 3);
	EXPECT_EQ(queued.getProbability(), 100);
	EXPECT_EQ(queued.nextSkillTime, -1);
	EXPECT_EQ(queued.target, npcskill::NpcSkillTargetAttribute::MOST_HATED);
	npcskill::QueuedNpcSkillTemplate targeted(1, 2, 3000, npcskill::NpcSkillTargetAttribute::ME);
	EXPECT_EQ(targeted.nextSkillTime, 3000);
	EXPECT_EQ(targeted.target, npcskill::NpcSkillTargetAttribute::ME);
	EXPECT_EQ(npcskill::value(npcskill::ConjunctionType::XOR), "XOR");
	EXPECT_EQ(npcskill::fromValue("OR"), npcskill::ConjunctionType::OR);
	EXPECT_THROW(npcskill::fromValue("or"), commons::utils::IllegalArgumentException);
}

// ---- events, challenges, conqueror ranks, titles ---------------------------------------------------------------------------------------------

TEST(EventTemplatesTest, ConfigPropertiesQuestsAndPeriod) {
	std::unique_ptr<event::EventTemplate> eventTemplate = bindXml<event::EventTemplate>(
	  R"(<event name="Summer" start="2026-07-01T10:00:00" end="2026-08-01T00:00:00">)"
	  R"(<config_properties><property>gameserver.rates.xp = 2</property><property>gameserver.a=b\</property><property>  c</property>)"
	  R"(</config_properties><quests><startable>80001 80002</startable></quests>)"
	  R"(<buffs><buff skill_ids="1 2" pool="3" team="true"><trigger condition="ENTER_MAP" chance="50"/></buff></buffs></event>)");
	EXPECT_TRUE(eventTemplate->hasConfigProperties());
	commons::configuration::Properties properties = eventTemplate->loadConfigProperties();
	EXPECT_EQ(properties.getProperty("gameserver.rates.xp"), "2");
	EXPECT_EQ(properties.getProperty("gameserver.a"), "bc") << "the lines are joined with \\n, a trailing backslash continues the line";
	EXPECT_EQ(eventTemplate->getStartableQuests(), (std::vector<int32_t>{80001, 80002}));
	EXPECT_TRUE(eventTemplate->getMaintainableQuests().empty());
	using namespace std::chrono;
	auto at = [](year_month_day day, int hours) { return local_time<milliseconds>(local_days(day) + std::chrono::hours(hours)); };
	EXPECT_FALSE(eventTemplate->isInEventPeriod(at(2026y / 7 / 1, 9)));
	EXPECT_TRUE(eventTemplate->isInEventPeriod(at(2026y / 7 / 1, 10))) << "the start is inclusive";
	EXPECT_FALSE(eventTemplate->isInEventPeriod(at(2026y / 8 / 1, 0))) << "the end is exclusive";
	ASSERT_TRUE(eventTemplate->getBuffs().has_value());
	const event::Buff& buff = eventTemplate->getBuffs()->at(0);
	EXPECT_EQ(buff.getPool(), 3) << "a pool above the skill count only logs a warning";
	EXPECT_TRUE(buff.isTeam());
	EXPECT_FLOAT_EQ(buff.getTriggers().at(0).getChance(), 50.0f);

	std::unique_ptr<event::EventTemplate> open = bindXml<event::EventTemplate>(R"(<event name="Open"/>)");
	EXPECT_FALSE(open->hasConfigProperties());
	EXPECT_THROW(open->loadConfigProperties(), runtime::NullPointerException);
	EXPECT_TRUE(open->getStartableQuests().empty());
	EXPECT_TRUE(open->isInEventPeriod(at(1999y / 1 / 1, 0))) << "no start and no end";

	std::unique_ptr<event::upgradearcade::ArcadeLevels> levels =
	  bindXml<event::upgradearcade::ArcadeLevels>(R"(<levels min_resumable_level="2"><level level="1"/><level level="2"/></levels>)");
	EXPECT_EQ(levels->getMaxUpgradeLevel(), &levels->getLevels().back());
	EXPECT_THROW(bindXml<event::upgradearcade::ArcadeLevels>(R"(<levels/>)")->getMaxUpgradeLevel(), commons::utils::IndexOutOfBoundsException);
}

TEST(EventTemplatesTest, EnumCompanionsAndModifiers) {
	EXPECT_EQ(challenge::getId(challenge::ChallengeType::LEGION), 1);
	EXPECT_EQ(challenge::getId(challenge::ChallengeType::TOWN), 2);
	EXPECT_EQ(challenge::value(challenge::ChallengeType::TOWN), "TOWN");
	EXPECT_EQ(challenge::fromValue<challenge::ChallengeType>("LEGION"), challenge::ChallengeType::LEGION);
	EXPECT_EQ(challenge::fromValue<challenge::RewardType>("SPAWN"), challenge::RewardType::SPAWN);
	try {
		challenge::fromValue<challenge::RewardType>("BONUS");
		FAIL() << "unknown constant";
	} catch (const commons::utils::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "No enum constant com.aionemu.gameserver.model.templates.challenge.RewardType.BONUS");
	}

	std::unique_ptr<cp::CPRank> rank = bindXml<cp::CPRank>(R"(<rank type="CONQUEROR" rank_num="1"/>)");
	EXPECT_TRUE(rank->getStatModifiers().empty()) << "Collections.emptyList() without modifiers";
	std::unique_ptr<TitleTemplate> title = bindXml<TitleTemplate>(R"(<title id="1" race="ELYOS" nameId="7"/>)");
	EXPECT_EQ(title->getModifiers(), nullptr);
	EXPECT_EQ(title->getL10nId(), 7);
}

// ---- ai, flights, gathering, storage expansion ----------------------------------------------------------------------------------------------

TEST(MiscTemplatesTest, SummonGroupHook) {
	EXPECT_EQ(bindXml<ai::SummonGroup>(R"(<summon npcId="1" minCount="3"/>)")->getMaxCount(), 3) << "maxCount defaults to minCount";
	EXPECT_EQ(bindXml<ai::SummonGroup>(R"(<summon npcId="1"/>)")->getMaxCount(), 1);
	EXPECT_EQ(bindXml<ai::SummonGroup>(R"(<summon npcId="1" minCount="2" maxCount="4"/>)")->getMaxCount(), 4);
	try {
		bindXml<ai::SummonGroup>(R"(<summon npcId="5" minCount="0"/>)");
		FAIL() << "minCount 0";
	} catch (const xml::StaticDataException& e) {
		EXPECT_NE(std::string(e.what()).find("minCount (0) for npc group 5 must be greater than zero"), std::string::npos) << e.what();
	}
	try {
		bindXml<ai::SummonGroup>(R"(<summon npcId="5" minCount="3" maxCount="2"/>)");
		FAIL() << "maxCount below minCount";
	} catch (const xml::StaticDataException& e) {
		EXPECT_NE(std::string(e.what()).find("maxCount (2) for npc group 5 must be greater than minCount (3)"), std::string::npos) << e.what();
	}
}

TEST(MiscTemplatesTest, FlightsGatheringAndExpansions) {
	EXPECT_EQ(bindXml<flypath::FlyPathEntry>(R"(<flypath_location id="1" sx="0" sy="0" sz="0" sworld="1" ex="0" ey="0" ez="0" eworld="1")"
	                                         R"( time="1.5"/>)")
	            ->getTimeInMs(),
	          1500);
	EXPECT_EQ(bindXml<flypath::FlyPathEntry>(R"(<flypath_location id="1" sx="0" sy="0" sz="0" sworld="1" ex="0" ey="0" ez="0" eworld="1")"
	                                         R"( time="3e7"/>)")
	            ->getTimeInMs(),
	          std::numeric_limits<int32_t>::max())
	  << "Java's (int) cast saturates";

	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		runtime::Ref<geometry::Point3D> center = geometry::Point3D::create(1.0f, 2.0f, 3.0f);
		runtime::Ref<geometry::Point3D> p1 = geometry::Point3D::create(4.0f, 5.0f, 6.0f);
		runtime::Ref<geometry::Point3D> p2 = geometry::Point3D::create(7.0f, 8.0f, 9.0f);
		flyring::FlyRingTemplate ring("ROAH_WING_1", 300070000, *center, *p1, *p2, 10);
		EXPECT_EQ(ring.getName(), "ROAH_WING_1");
		EXPECT_EQ(ring.getMap(), 300070000);
		EXPECT_FLOAT_EQ(ring.getRadius(), 10.0f);
		EXPECT_FLOAT_EQ(ring.getCenter()->getY(), 2.0f);
		EXPECT_FLOAT_EQ(ring.getP1()->getZ(), 6.0f);
		EXPECT_FLOAT_EQ(ring.getP2()->getX(), 7.0f);
	}

	std::unique_ptr<gather::GatherableTemplate> gatherable = bindXml<gather::GatherableTemplate>(
	  R"(<gatherable_template id="400101" nameId="3")"
	  R"(><materials><material itemid="1" rate="30"/><material itemid="2" rate="70"/></materials><exmaterials/></gatherable_template>)");
	ASSERT_EQ(gatherable->getMaterials()->getMaterial().size(), 2u);
	const gather::Material& low = gatherable->getMaterials()->getMaterial()[0];
	const gather::Material& high = gatherable->getMaterials()->getMaterial()[1];
	EXPECT_EQ(low.compareTo(high), 40) << "o.rate - rate: higher rates first";
	EXPECT_EQ(high.compareTo(low), -40);
	EXPECT_TRUE(gatherable->getExtraMaterials()->getMaterial().empty());

	std::unique_ptr<StorageExpansionTemplate> expansion = bindXml<StorageExpansionTemplate>(
	  R"(<expansion_npc ids="1 2"><expand level="3" price="300"/><expand level="1" price="100"/><expand level="1" price="150"/></expansion_npc>)");
	EXPECT_EQ(expansion->getMinExpansionLevel(), 1);
	EXPECT_EQ(expansion->getMaxExpansionLevel(), 3);
	EXPECT_EQ(expansion->getPrice(1), 100) << "the first expand of the level";
	EXPECT_EQ(expansion->getPrice(2), std::nullopt);
}

} // namespace
} // namespace aion::gameserver::model::templates
