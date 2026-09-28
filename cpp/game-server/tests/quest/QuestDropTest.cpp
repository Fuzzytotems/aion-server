// P5-06, written by the M5b-3 loot lane under the I-03 lease on services/QuestService.cpp (m5b3-plan.md L-04): the four quest-drop bodies
// registerDrop calls on every kill - QuestService.getQuestDrop, isQuestDrop, allowLooting, regQuestDropItem (QuestService.java:666-796) -
// against the h03 signature, which adds to the caller's live set.
//
// Fixture rows are copied from the shipped data: quests 1001 and 1108 (quest_data.xml:9-19, :943-950), the alliance quest 30408
// (:66647-66660) and the mentee quest 37001 (:68633-68656); the npcs 210668 "pinkbeak airon" (npc_templates.xml:57842-57847), 210671
// "bigfoot kerubar" (:57863-57871), 217264 "loyal hetgolem" (:110796-110801) and 210165 "veteran tursin watcher" (:54701-54710), the <equipment>
// of the two that have one left out (no case reads it); and the four quest items (item_templates.xml:876830-876832, :876940-876942,
// :885993-885995, :884970-884972). The holder gives every <quest_drop> its quest's id while binding (QuestsData::afterUnmarshal, Java's
// QuestEngine.java:89), and the drops are registered the way QuestEngine::init does (QuestEngine.cpp:84-90: every <quest_drop> of every quest
// template, by npc id; one case runs init itself). No quest drop here has a chance attribute, so getChance() is 100 (QuestDrop.java:47-51)
// and `Rnd.chance() >= 100` never skips a drop: the cases are deterministic.
//
// The player is solo, so every case takes getQuestDrop's last arm (QuestService.java:726-730); the group and alliance arms need a team (M5g).
// An empty `players` stands for Java's null (m5b3-plan.md D11) - the case below pins that a solo kill passes it.

#include "../cm_ak/InWorldPacketRunSupport.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <vector>

#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/XMLQuests.bind.h"
#include "aion/gameserver/dataholders/XMLQuests.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/quest/HandlerSideDrop.h"
#include "aion/gameserver/model/templates/quest/QuestDrop.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/utils/cron/ThreadPoolManagerRunnableRunner.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::test {
namespace {

namespace cp = network::aion::clientpackets::testing;
using model::drop::DropItem;
using questEngine::model::QuestState;
using questEngine::model::QuestStatus;
using runtime::Ptr;
using runtime::Ref;

/** quest_data.xml:9-19 (quest 1001, collect 3 x 182200001, the drop on 210671 has collecting_step 7) and :943-950 (quest 1108, collect 3 x 182200204) */
constexpr const char* QUESTS_XML =
	R"(<quests>)"
	R"(<quest id="1001" name="The Kerubim Threat" nameId="1102001" quest_zone="Poeta" minlevel_permitted="2" max_repeat_count="1")"
	R"( cannot_share="true" cannot_giveup="true" race_permitted="ELYOS" category="MISSION">)"
	R"(<collect_items><collect_item item_id="182200001" count="3"/></collect_items>)"
	R"(<rewards exp="2100"><selectable_reward_item item_id="114100806" count="1"/><selectable_reward_item item_id="114300816" count="1"/>)"
	R"(<selectable_reward_item item_id="114500778" count="1"/></rewards>)"
	R"(<quest_drop npc_id="210671" item_id="182200001" drop_each_member="1" collecting_step="7"/>)"
	R"(</quest>)"
	R"(<quest id="1108" name="Uno's Ingredients" nameId="1102208" quest_zone="Poeta" minlevel_permitted="2" max_repeat_count="1" can_report="true")"
	R"( race_permitted="ELYOS" category="QUEST">)"
	R"(<collect_items><collect_item item_id="182200204" count="3"/></collect_items>)"
	R"(<rewards gold="1240" exp="671"/>)"
	R"(<quest_drop npc_id="210668" item_id="182200204"/>)"
	R"(<quest_drop npc_id="210205" item_id="182200204"/>)"
	R"(</quest>)"
	// quest_data.xml:66647-66660: target ALLIANCE
	R"(<quest id="30408" name="[Alliance] Dreams of Flight" nameId="1135408" quest_zone="Argent Manor" minlevel_permitted="99" max_repeat_count="1")"
	R"( cannot_share="true" race_permitted="ELYOS" category="QUEST" target="ALLIANCE" restricted="true">)"
	R"(<collect_items><collect_item item_id="182213016" count="1"/></collect_items>)"
	R"(<rewards gold="412800" exp="7511025"><selectable_reward_item item_id="162000077" count="65"/>)"
	R"(<selectable_reward_item item_id="162000078" count="65"/></rewards>)"
	R"(<quest_drop npc_id="217264" item_id="182213016" drop_each_member="2"/>)"
	R"(<quest_drop npc_id="217275" item_id="182213016" drop_each_member="2"/>)"
	R"(<start_conditions><finished quest_id="30402"/></start_conditions>)"
	R"(</quest>)"
	// quest_data.xml:68633-68656: mentor_type MENTE
	R"(<quest id="37001" name="[Daily] A Tursin's Fickle Flame" nameId="1126140" quest_zone="Kaisinel Academy" minlevel_permitted="99")"
	R"( maxlevel_permitted="20" max_repeat_count="255" cannot_share="true" race_permitted="ELYOS" category="FACTION" npcfaction_id="8")"
	R"( mentor_type="MENTE" restricted="true">)"
	R"(<collect_items><collect_item item_id="182210035" count="5"/></collect_items>)"
	R"(<rewards exp="41400"><reward_item item_id="186000002" count="10"/><reward_item item_id="188051123" count="1"/>)"
	R"(<reward_item item_id="164000227" count="1"/></rewards>)"
	R"(<quest_drop npc_id="210165" item_id="182210035"/><quest_drop npc_id="210166" item_id="182210035"/>)"
	R"(<quest_drop npc_id="210167" item_id="182210035"/><quest_drop npc_id="210170" item_id="182210035"/>)"
	R"(<quest_drop npc_id="210171" item_id="182210035"/><quest_drop npc_id="210175" item_id="182210035"/>)"
	R"(<quest_drop npc_id="210176" item_id="182210035"/><quest_drop npc_id="210177" item_id="182210035"/>)"
	R"(<quest_drop npc_id="210183" item_id="182210035"/><quest_drop npc_id="210184" item_id="182210035"/>)"
	R"(<quest_drop npc_id="210354" item_id="182210035"/><quest_drop npc_id="210355" item_id="182210035"/>)"
	R"(<quest_drop npc_id="210692" item_id="182210035"/><quest_drop npc_id="210693" item_id="182210035"/>)"
	R"(</quest>)"
	R"(</quests>)";

/** item_templates.xml:876830-876832 (Kerubar Fang), :876940-876942 (Airon Meat), :885993-885995 (Golem's Heart), :884970-884972 (Tursin Flame) */
constexpr const char* ITEMS_XML =
	R"(<item_templates>)"
	R"(<item_template id="182200001" name="Kerubar Fang" level="1" cName="quest_1001a" mask="28736" max_stack_count="20" item_group="QUEST")"
	R"( quality="COMMON" price="1" desc="1106001"><inventory id="2"/></item_template>)"
	R"(<item_template id="182200204" name="Airon Meat" level="1" cName="quest_1108a" mask="28736" max_stack_count="20" item_group="QUEST")"
	R"( quality="COMMON" price="1" desc="1106407"><inventory id="2"/></item_template>)"
	R"(<item_template id="182213016" name="Golem's Heart" level="1" cName="quest_30408a" mask="28736" max_stack_count="5" item_group="QUEST")"
	R"( quality="COMMON" price="1" race="ELYOS" desc="1135457"><inventory id="2"/></item_template>)"
	R"(<item_template id="182210035" name="Tursin Flame" level="1" cName="quest_37001a" mask="28736" max_stack_count="100" item_group="QUEST")"
	R"( quality="COMMON" price="1" race="ELYOS" desc="1133070"><inventory id="2"/></item_template>)"
	R"(</item_templates>)";

/** npc_templates.xml:57842-57847 */
constexpr const char* PINKBEAK_AIRON_XML =
	R"(<npc_template npc_id="210668" level="4" name="pinkbeak airon" name_id="301027" height="2.02" group_drop="HIIV" rank="DISCIPLINED")"
	R"( rating="NORMAL" race="BEAST" tribe="MONSTER" ai="aggressive" srange="8" sangle="240" arange="2" attack_speed="2142" hpgauge="3")"
	R"( floatcorpse="true"><stats maxHp="383"><speeds walk="0.955" group_walk="0.955" run="6.806" run_fight="5.3" group_run_fight="6.806" />)"
	R"(</stats><bound_radius front="0.35" side="0.78" upper="2.02" /></npc_template>)";

/** npc_templates.xml:57863-57871, without its <equipment> (:57867-57869) */
constexpr const char* BIGFOOT_KERUBAR_XML =
	R"(<npc_template npc_id="210671" level="4" name="bigfoot kerubar" name_id="301030" height="1.372" group_drop="CHERUBIM" rank="DISCIPLINED")"
	R"( rating="NORMAL" race="MAGICALMONSTER" tribe="MONSTER" type="MONSTER" ai="aggressive" srange="7" sangle="240" arange="2" attack_speed="2100")"
	R"( hpgauge="3"><stats maxHp="383"><speeds walk="0.6" group_walk="0.6" run="7" run_fight="5.5" group_run_fight="7" /></stats>)"
	R"(<bound_radius front="0.62999994" side="0.33" upper="1.372" /></npc_template>)";

/** npc_templates.xml:110796-110801, the first npc of quest 30408's drops */
constexpr const char* LOYAL_HETGOLEM_XML =
	R"(<npc_template npc_id="217264" level="60" name="loyal hetgolem" name_id="323133" height="2.96" group_drop="ROLLINGGOLEM" rank="VETERAN")"
	R"( rating="HERO" race="DEMIHUMANOID" tribe="XDRAKAN" type="MONSTER" ai="aggressive" srange="10" arange="20" attack_speed="2599" hpgauge="21")"
	R"( cancel_level="0"><stats maxHp="179681" msup="207"><speeds walk="1.4" group_walk="1.4" run="12" run_fight="12" group_run_fight="12" />)"
	R"(</stats><bound_radius front="1.36" side="0.96" upper="2.96" /></npc_template>)";

/** npc_templates.xml:54701-54710, without its <equipment> (:54705-54708): the first npc of quest 37001's drops */
constexpr const char* VETERAN_TURSIN_WATCHER_XML =
	R"(<npc_template npc_id="210165" level="16" name="veteran tursin watcher" name_id="300135" height="2.04" group_drop="KRALLSCOUT" rank="EXPERT")"
	R"( rating="NORMAL" race="KRALL" tribe="KRALL" ai="aggressive" srange="8" sangle="240" arange="20" attack_speed="2260" hpgauge="5")"
	R"( cancel_level="80"><stats maxHp="3020"><speeds walk="2" group_walk="2" run="8.8" run_fight="6" group_run_fight="8.8" /></stats>)"
	R"(<bound_radius front="0.77" side="0.3955" upper="2.04" /></npc_template>)";

constexpr int32_t AIRON_MEAT = 182200204;
constexpr int32_t KERUBAR_FANG = 182200001;
constexpr int32_t PINKBEAK_AIRON = 210668;
constexpr int32_t BIGFOOT_KERUBAR = 210671;

class QuestDropSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	explicit QuestDropSpawnTemplate(model::templates::spawns::SpawnGroup& group)
		: SpawnTemplate(group, 10.0f, 20.0f, 30.0f, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

class QuestDropTest : public cp::InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		// the npc templates name the "aggressive" AI and this executable links no AI handler: the warn mode puts AIEngine's substitute in place
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		// Player::postConstruct loads the toy pets from the database; the tests have none
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(
			[](model::gameobjects::player::Player&) { return std::vector<Ref<model::gameobjects::player::PetCommonData>>(); });
		// the real holder: QuestsData::afterUnmarshal gives every <quest_drop> its quest's id (Java QuestEngine.java:89); the fixture sets none
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(contexts.emplace_back(), QUESTS_XML));
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(contexts.emplace_back(), ITEMS_XML));
		// QuestEngine::init's drop loop (QuestEngine.cpp:84-90)
		for (const model::templates::QuestTemplate* quest : dataholders::DataManager::QUEST_DATA->getQuestTemplates()) {
			for (const model::templates::quest::QuestDrop& drop : quest->getQuestDrop())
				QuestService::addQuestDrop(*drop.getNpcId(), &drop);
		}
		f = cp::makePlayer(800001, 9801, "Collector");
		f.player->setQuestStateList(model::gameobjects::player::QuestStateList::create());
	}

	void TearDown() override {
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		QuestService::clearQuestDrops();
		handlerSideDrops.clear();
		npcs.clear();
		spawnGroups.clear();
		items.clear();
		f = {};
		InWorldPacketTest::TearDown();
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.resetForTests();
	}

	/** Java SpawnEngine: the spawn creates the Npc and gives it a known list */
	model::gameobjects::Npc& spawnNpc(const char* templateXml) {
		const model::templates::npc::NpcTemplate* template_ =
			xml::bindString<model::templates::npc::NpcTemplate>(contexts.emplace_back(), templateXml).release(); // static data: kept for the process
		Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, template_->getTemplateId(), 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<QuestDropSpawnTemplate>(*group));
		Ref<model::gameobjects::Npc> npc = model::gameobjects::VisibleObject::create<model::gameobjects::Npc>(
			std::make_unique<controllers::NpcController>(), spawn, template_);
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		return *npc;
	}

	void startQuest(int32_t questId, QuestStatus status) {
		Ref<QuestState> state = QuestState::create(questId, status);
		f.player->getQuestStateList()->addQuest(questId, *state);
	}

	/** An item row of the cube as the inventory DAO loads it (onLoadHandler: no packet) */
	void holdInCube(int32_t objId, int32_t itemId, int64_t count) {
		Ref<model::gameobjects::Item> item = model::gameobjects::Item::create(objId, itemId, count, std::nullopt, 0, "", 0, 0, false, false, 0,
			model::items::storage::getId(model::items::storage::StorageType::CUBE), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0);
		f.player->getInventory().onLoadHandler(*item);
		items.push_back(item);
	}

	static Ref<runtime::RcHashSet<Ref<DropItem>>> newDropSet() { return runtime::RcHashSet<Ref<DropItem>>::create(); }

	std::deque<xml::LoadContext> contexts;
	cp::PlayerFixture f;
	std::vector<Ref<model::gameobjects::Npc>> npcs;
	std::vector<Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<Ref<model::gameobjects::Item>> items;
	/** QuestEngine allocates its HandlerSideDrops for the process; the case's own are freed after clearQuestDrops (TearDown) */
	std::vector<std::unique_ptr<model::templates::quest::HandlerSideDrop>> handlerSideDrops;
};

// QuestService.java:667-670: an npc without a quest drop returns the index it was given and adds nothing - the first statement of every
// Poeta kill whose npc no quest names
TEST_F(QuestDropTest, AnNpcWithoutQuestDropsKeepsTheIndexAndAddsNothing) {
	startQuest(1108, QuestStatus::START);
	model::gameobjects::Npc& npc = spawnNpc(BIGFOOT_KERUBAR_XML);
	QuestService::clearQuestDrops(); // 210671 has a drop of quest 1001; without registrations no npc has one
	Ref<runtime::RcHashSet<Ref<DropItem>>> dropItems = newDropSet();

	EXPECT_EQ(QuestService::getQuestDrop(*dropItems, 7, npc, {}, *f.player), 7);
	EXPECT_TRUE(dropItems->isEmpty());
}

// isQuestDrop (:754-758): a character without the quest, or with the quest in any status but START, gets no quest drop - which is what every
// kill of a fresh M5b-3 character sees (no quest can start before M5d)
TEST_F(QuestDropTest, NoQuestDropWithoutTheQuestStartedOnTheCharacter) {
	model::gameobjects::Npc& npc = spawnNpc(PINKBEAK_AIRON_XML);
	Ref<runtime::RcHashSet<Ref<DropItem>>> dropItems = newDropSet();

	EXPECT_EQ(QuestService::getQuestDrop(*dropItems, 3, npc, {}, *f.player), 3) << "no quest state at all";
	EXPECT_TRUE(dropItems->isEmpty());

	startQuest(1108, QuestStatus::REWARD);
	EXPECT_EQ(QuestService::getQuestDrop(*dropItems, 3, npc, {}, *f.player), 3) << "a quest in REWARD collects nothing any more";
	EXPECT_TRUE(dropItems->isEmpty());
}

// The solo arm (:726-730) with the quest started and the collect count not reached (isQuestDrop :785-795: 0 < 3 of the drop's item):
// regQuestDropItem (:746-752) - one entry of the item, count 1, at the index given, visible to the looter only (setPlayerObjId), with the
// quest drop's chance on its Drop; the index moves on by one
TEST_F(QuestDropTest, AStartedQuestWhoseCollectCountIsNotReachedDropsOneItemForTheLooter) {
	startQuest(1108, QuestStatus::START);
	model::gameobjects::Npc& npc = spawnNpc(PINKBEAK_AIRON_XML);
	Ref<runtime::RcHashSet<Ref<DropItem>>> dropItems = newDropSet();

	EXPECT_EQ(QuestService::getQuestDrop(*dropItems, 4, npc, {}, *f.player), 5);
	std::vector<Ptr<DropItem>> entries = dropItems->snapshot();
	ASSERT_EQ(entries.size(), 1u);
	const Ptr<DropItem>& entry = entries.front();
	EXPECT_EQ(entry->getDropTemplate()->getItemId(), AIRON_MEAT);
	EXPECT_EQ(entry->getIndex(), 4);
	EXPECT_EQ(entry->getCount(), 1);
	EXPECT_EQ(entry->getDropTemplate()->getChance(), 100.0f) << "new Drop(itemId, 1, 1, drop.getChance())";
	EXPECT_EQ(entry->getPlayerObjIds().snapshot(), std::vector<int32_t>{f.player->getObjectId()}) << "only the looter sees a solo quest drop";
	EXPECT_TRUE(entry->canViewDropItem(f.player->getObjectId()));
	EXPECT_FALSE(entry->canViewDropItem(f.player->getObjectId() + 1));
}

// isQuestDrop (:789-795): the drop is for the collect item whose count the inventory has not reached; with 3 of 3 in the cube the quest
// needs no more, and with 2 of 3 it still does
TEST_F(QuestDropTest, NoQuestDropOnceTheCollectCountIsInTheInventory) {
	startQuest(1108, QuestStatus::START);
	model::gameobjects::Npc& npc = spawnNpc(PINKBEAK_AIRON_XML);
	holdInCube(900001, AIRON_MEAT, 2);
	Ref<runtime::RcHashSet<Ref<DropItem>>> dropItems = newDropSet();
	EXPECT_EQ(QuestService::getQuestDrop(*dropItems, 1, npc, {}, *f.player), 2) << "2 of 3: still collecting";
	EXPECT_EQ(dropItems->size(), 1);

	holdInCube(900002, AIRON_MEAT, 1);
	Ref<runtime::RcHashSet<Ref<DropItem>>> more = newDropSet();
	EXPECT_EQ(QuestService::getQuestDrop(*more, 1, npc, {}, *f.player), 1) << "3 of 3: nothing more to collect";
	EXPECT_TRUE(more->isEmpty());
}

// getQuestDrop adds to the caller's set (the h03 signature): entries registerDrop put there before (custom drops, :79) stay, and the index
// continues after them
TEST_F(QuestDropTest, TheQuestDropIsAddedToTheCallersSetBesideTheEntriesAlreadyThere) {
	startQuest(1108, QuestStatus::START);
	model::gameobjects::Npc& npc = spawnNpc(PINKBEAK_AIRON_XML);
	Ref<runtime::RcHashSet<Ref<DropItem>>> dropItems = newDropSet();
	Ref<DropItem> earlier = drop::DropRegistrationService::getInstance().regDropItem(1, 0, npc.getObjectId(), KERUBAR_FANG, 1);
	dropItems->add(earlier);

	EXPECT_EQ(QuestService::getQuestDrop(*dropItems, 2, npc, {}, *f.player), 3);
	EXPECT_EQ(dropItems->size(), 2);
	EXPECT_TRUE(dropItems->contains(earlier));
}

// isQuestDrop (:781-783): a drop a quest handler registered (QuestEngine.addHandlerSideQuestDrop -> HandlerSideDrop) asks the handler's needed
// amount, not the template's collect_items: with 3 of the 3 Airon Meat quest 1108 collects in the cube, the template's drop is done (:789-795)
// while a handler-side drop needing 5 still drops. HandlerSideDrop's constructor (HandlerSideDrop.java) is the one QuestEngine calls.
TEST_F(QuestDropTest, AHandlerSideDropAsksTheHandlersAmountInsteadOfTheCollectItems) {
	startQuest(1108, QuestStatus::START);
	model::gameobjects::Npc& npc = spawnNpc(PINKBEAK_AIRON_XML);
	holdInCube(900001, AIRON_MEAT, 3);
	Ref<runtime::RcHashSet<Ref<DropItem>>> xmlDrop = newDropSet();
	ASSERT_EQ(QuestService::getQuestDrop(*xmlDrop, 1, npc, {}, *f.player), 1) << "the template's drop: 3 of 3 collected";

	QuestService::clearQuestDrops();
	handlerSideDrops.push_back(std::make_unique<model::templates::quest::HandlerSideDrop>(1108, PINKBEAK_AIRON, AIRON_MEAT, 5, 100));
	QuestService::addQuestDrop(PINKBEAK_AIRON, handlerSideDrops.back().get());
	Ref<runtime::RcHashSet<Ref<DropItem>>> handlerDrop = newDropSet();
	EXPECT_EQ(QuestService::getQuestDrop(*handlerDrop, 1, npc, {}, *f.player), 2) << "5 needed > 3 held";
	ASSERT_EQ(handlerDrop->size(), 1);
	EXPECT_EQ(handlerDrop->snapshot().front()->getDropTemplate()->getItemId(), AIRON_MEAT);

	holdInCube(900002, AIRON_MEAT, 2);
	Ref<runtime::RcHashSet<Ref<DropItem>>> enough = newDropSet();
	EXPECT_EQ(QuestService::getQuestDrop(*enough, 1, npc, {}, *f.player), 1) << "5 held: the handler needs no more";
}

// isQuestDrop (:759-763): a drop with a collecting step compares it with the quest's first variable, QuestState.getQuestVarById(0) -
// P5-06's QuestState::getQuestVarById / QuestVars::getVarById, both AION_UNPORTED (m5b3-plan.md L-04, O-06). Reached only with the quest in
// START, which needs a quest that can start (M5d); until then this arm throws, and the case pins where.
TEST_F(QuestDropTest, ACollectingStepReachesTheUnportedQuestVariablesOnlyForAStartedQuest) {
	model::gameobjects::Npc& npc = spawnNpc(BIGFOOT_KERUBAR_XML);
	Ref<runtime::RcHashSet<Ref<DropItem>>> dropItems = newDropSet();
	startQuest(1001, QuestStatus::COMPLETE);
	EXPECT_EQ(QuestService::getQuestDrop(*dropItems, 1, npc, {}, *f.player), 1) << "not started: no quest variable is read";

	f.player->setQuestStateList(model::gameobjects::player::QuestStateList::create());
	startQuest(1001, QuestStatus::START);
	EXPECT_THROW(QuestService::getQuestDrop(*dropItems, 1, npc, {}, *f.player), runtime::UnportedException);
	EXPECT_TRUE(dropItems->isEmpty());
}

// isQuestDrop (:766-770): a quest whose target is ALLIANCE drops nothing for a player outside an alliance, even started and still collecting -
// quest 30408 (target="ALLIANCE", 0 of 1 Golem's Heart), killed solo. The drop has no collecting step, so no quest variable is read.
TEST_F(QuestDropTest, AnAllianceQuestDropsNothingForAPlayerOutsideAnAlliance) {
	startQuest(30408, QuestStatus::START);
	model::gameobjects::Npc& npc = spawnNpc(LOYAL_HETGOLEM_XML);
	Ref<runtime::RcHashSet<Ref<DropItem>>> dropItems = newDropSet();

	EXPECT_EQ(QuestService::getQuestDrop(*dropItems, 1, npc, {}, *f.player), 1);
	EXPECT_TRUE(dropItems->isEmpty());
}

// isQuestDrop (:771-780): a mentee quest (mentor_type="MENTE") drops nothing for a player outside a group, even started and still collecting -
// quest 37001 (0 of 5 Tursin Flames), killed solo. Inside a group it would ask for a mentor in range (M5g: no C++ packet forms a group).
TEST_F(QuestDropTest, AMenteeQuestDropsNothingForAPlayerOutsideAGroup) {
	startQuest(37001, QuestStatus::START);
	model::gameobjects::Npc& npc = spawnNpc(VETERAN_TURSIN_WATCHER_XML);
	Ref<runtime::RcHashSet<Ref<DropItem>>> dropItems = newDropSet();

	EXPECT_EQ(QuestService::getQuestDrop(*dropItems, 1, npc, {}, *f.player), 1);
	EXPECT_TRUE(dropItems->isEmpty());
}

// The quest id of a drop, on the server's own path (the loot review's blocker, docs/deviations/P5-06.md): Java's QuestEngine.init sets it
// (QuestEngine.java:89) and isQuestDrop unboxes it first thing (QuestService.java:755), on every kill of an npc that has a quest drop. C++
// templates are const once published, so QuestsData::afterUnmarshal (P4-09) sets it while binding and QuestEngine::init only reads it
// (QuestEngine.cpp:84-90). The fixture binds the shipped rows through that holder and sets no id itself; this case runs QuestEngine::init as
// GameServer does and asks what it registered - then kills 210668 with a character who has not taken quest 1108: no quest drop, and no
// NullPointerException (what a null quest id threw on every such kill).
TEST_F(QuestDropTest, TheHolderGivesEveryQuestDropItsQuestIdAndInitRegistersItForTheKill) {
	for (const model::templates::QuestTemplate* quest : dataholders::DataManager::QUEST_DATA->getQuestTemplates()) {
		for (const model::templates::quest::QuestDrop& drop : quest->getQuestDrop())
			EXPECT_EQ(drop.getQuestId(), std::optional<int32_t>(quest->getId())) << "quest " << quest->getId() << ", npc " << drop.getNpcId().value_or(0);
	}

	// QuestEngine::init's environment (tests/quest/QuestEngineTest.cpp's fixture): the cron service of its daily message, no XML quests, no
	// quest handler analysis; undone at the end of the case
	struct InitEnvironment {
		std::deque<xml::LoadContext>& contexts;
		bool analyzeQuestHandlers = configs::main::GSConfig::ANALYZE_QUESTHANDLERS.load();
		explicit InitEnvironment(std::deque<xml::LoadContext>& c) : contexts(c) {
			services::cron::CronService::resetForTests();
			services::cron::CronService::initSingleton(std::make_unique<utils::cron::ThreadPoolManagerRunnableRunner>(), std::chrono::locate_zone("UTC"),
				services::cron::CronService::Driver::EXECUTOR);
			dataholders::DataManager::XML_QUESTS.publish(xml::bindString<dataholders::XMLQuests>(contexts.emplace_back(), "<quest_scripts/>"));
			configs::main::GSConfig::ANALYZE_QUESTHANDLERS.store(false);
		}
		~InitEnvironment() {
			questEngine::QuestEngine::getInstance().clear();
			configs::main::GSConfig::ANALYZE_QUESTHANDLERS.store(analyzeQuestHandlers);
			dataholders::DataManager::XML_QUESTS.resetForTests();
			services::cron::CronService::resetForTests();
		}
	} environment(contexts);
	QuestService::clearQuestDrops(); // the fixture's own registration: init registers them again

	questEngine::QuestEngine::getInstance().init();

	std::vector<const model::templates::quest::QuestDrop*> airon = QuestService::getQuestDrop(PINKBEAK_AIRON);
	ASSERT_EQ(airon.size(), 1u);
	EXPECT_EQ(airon.front()->getQuestId(), std::optional<int32_t>(1108)) << "quest_data.xml:948 is a <quest_drop> of quest 1108";
	std::vector<const model::templates::quest::QuestDrop*> kerubar = QuestService::getQuestDrop(BIGFOOT_KERUBAR);
	ASSERT_EQ(kerubar.size(), 1u);
	EXPECT_EQ(kerubar.front()->getQuestId(), std::optional<int32_t>(1001)) << "quest_data.xml:18 is a <quest_drop> of quest 1001";
	Ref<runtime::RcHashSet<Ref<DropItem>>> dropItems = newDropSet();
	int32_t index = 0;
	EXPECT_NO_THROW(index = QuestService::getQuestDrop(*dropItems, 1, spawnNpc(PINKBEAK_AIRON_XML), {}, *f.player));
	EXPECT_EQ(index, 1) << "no quest state: isQuestDrop answers false";
	EXPECT_TRUE(dropItems->isEmpty());
}

} // namespace
} // namespace aion::gameserver::services::test
