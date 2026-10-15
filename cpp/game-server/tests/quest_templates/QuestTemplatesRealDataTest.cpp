// P5-06c, M5d T-04 (m5d-plan.md §7, §18.3) on the real static data: all 4,184 XML quests of quest_script_data registered with QuestEngine
// the way QuestEngine.init's loop will register them after the D3 join (QuestEngine.java:104-105; in C++ that loop is still the AION_PARTIAL at
// QuestEngine.cpp:111, so these cases call each XMLQuest::register_ themselves):
// - the registration order: XMLQuests.getAllQuests in Java's HashMap<Integer, XMLQuest> order and the lists the registration builds - every
//   npc's onTalkEvent, onQuestStart and onKillEvent list, and the enter-world and level-change lists as far as a player can observe them (the
//   stigma quests they start, in list order) - against `oracle.py m5d-quests --registration-order --no-profile` (G-01), whose output is
//   expected/m5d_registration_order.json. An std::unordered_map iteration anywhere on the way would reorder them (m5d-plan.md risk 5). And no
//   other npc - of npc_templates.xml, npc 0 (the id an absent npc attribute binds as) or the oracle's other lists - has one of the three lists
//   (the engine keeps its QuestNpc map private, so these candidates stand for it).
// - the smoke: for every npc of npc_templates.xml with a talk or kill list, a talk (USE_OBJECT without a quest, as TalkEventHandler sends it)
//   and, for every quest of its talk list, QUEST_SELECT and USE_OBJECT with that quest, by a level-65 player of the quest's race without the
//   quest and one holding it in START; a kill of the npc by both, and by a player holding each quest of its kill list in START; an enter
//   world and a level change of a level-65 player of each race. 0 error lines of any logger, 0 AION_UNPORTED and 0 AION_PARTIAL hits; the
//   talk and kill npcs are as many as the oracle's (1,989 and 3,062).
//   Left out, with the reason: the USE_OBJECT of a kill_spawned quest in START (at its spawner it needs the spawner's spawn in SPAWNS_DATA and
//   a map instance of the spawner's world; the player stands in the fixture's Poeta instance, as no player can at such an object).
// The holders are bound from the Java tree's data/static_data like QuestModelsRealDataTest's (the static_data.xml imports) and published over
// the fixture's rows for the case: quest_data, quest_script_data, the npc templates and the dimensional vortices for both cases, and for the
// smoke the item templates (a fountain's inventory check names its coin, QuestService.inventoryItemCheck).

#include "QuestTemplate1bTestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <nlohmann/json.hpp>

#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/VortexData.bind.h"
#include "aion/gameserver/dataholders/VortexData.h"
#include "aion/gameserver/dataholders/XMLQuests.bind.h"
#include "aion/gameserver/dataholders/XMLQuests.h"
#include "aion/gameserver/dataholders/loadingutils/BindContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataImports.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/questEngine/handlers/models/KillSpawnedData.h"
#include "aion/gameserver/runtime/base/Unported.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::templates {
namespace {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::Npc;
using json = nlohmann::json;

const std::filesystem::path STATIC_DATA = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data";
const std::filesystem::path EXPECTED_ORDER =
	std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "../cpp/game-server/tests/quest_templates/expected/m5d_registration_order.json";

std::vector<int32_t> idsOf(const json& list) {
	std::vector<int32_t> ids;
	for (const json& id : list)
		ids.push_back(id.get<int32_t>());
	return ids;
}

/**
 * Java's iteration order of a `new HashSet<>(0)` (QuestNpc.onQuestStart, QuestNpc.java:27) filled by add() in `insertion` order, which the
 * oracle prints for onQuestStart (m5d/registry.py): HashMap.putVal and resize from one bucket (tableSizeFor(0)), the threshold (int)
 * (capacity * 0.75) below 16 buckets and doubled from 16 up, a doubling keeping each bucket's order, a new key appended to bucket
 * (h ^ h >>> 16) & (n - 1); a ninth key in a bucket of a table below 64 buckets doubles it (treeifyBin). The runtime HashSet the C++ QuestNpc
 * keeps is in insertion order, which is registration order.
 */
std::vector<int32_t> javaHashSetOrder(const std::vector<int32_t>& insertion) {
	std::vector<std::vector<int32_t>> table;
	size_t threshold = 1;
	size_t size = 0;
	auto bucketOf = [](int32_t key, size_t capacity) {
		const uint32_t h = static_cast<uint32_t>(key);
		return static_cast<size_t>((h ^ (h >> 16)) & (capacity - 1));
	};
	auto resize = [&] {
		const size_t oldCapacity = table.size();
		const size_t newCapacity = oldCapacity > 0 ? oldCapacity << 1 : threshold;
		threshold = oldCapacity >= 16 ? threshold << 1 : newCapacity * 3 / 4;
		std::vector<std::vector<int32_t>> newTable(newCapacity);
		for (const std::vector<int32_t>& bucket : table) {
			for (int32_t key : bucket)
				newTable[bucketOf(key, newCapacity)].push_back(key);
		}
		table = std::move(newTable);
	};
	for (int32_t id : insertion) {
		if (table.empty())
			resize();
		std::vector<int32_t>& bucket = table[bucketOf(id, table.size())];
		if (std::find(bucket.begin(), bucket.end(), id) != bucket.end())
			continue;
		bucket.push_back(id);
		if (bucket.size() > 8 && table.size() < 64)
			resize();
		if (++size > threshold)
			resize();
	}
	std::vector<int32_t> order;
	for (const std::vector<int32_t>& bucket : table)
		order.insert(order.end(), bucket.begin(), bucket.end());
	return order;
}

class QuestTemplatesRealDataTest : public QuestTemplate1bTest {
protected:
	void SetUp() override {
		QuestTemplate1bTest::SetUp();
		if (!std::filesystem::exists(STATIC_DATA / "static_data.xml") || !std::filesystem::exists(EXPECTED_ORDER))
			GTEST_SKIP() << "Java data tree or the registration-order expectation not found: " << STATIC_DATA << ", " << EXPECTED_ORDER;
		imports = xml::StaticDataImports::resolve(STATIC_DATA / "static_data.xml", 0, true);
	}

	void TearDown() override {
		QuestTemplate1bTest::TearDown();
		dataholders::DataManager::VORTEX_DATA.resetForTests();
	}

	/** Binds the import `file` of static_data.xml into a new H with its hooks and publishes it over the fixture's holder */
	template <class H, class Ref>
	void publishReal(Ref& holderRef, std::string_view file) {
		for (const xml::StaticDataImport& entry : imports) {
			if (entry.file != file)
				continue;
			std::vector<std::unique_ptr<xml::XmlDocument>>& parsed = documents.emplace_back(xml::StaticDataLoader::parseFiles(entry.files, false));
			std::vector<const xml::XmlDocument*> roots;
			for (const std::unique_ptr<xml::XmlDocument>& document : parsed)
				roots.push_back(document.get());
			xml::LoadContext& context = contexts.emplace_back();
			auto holder = std::make_unique<H>();
			xml::BindContext binding(context);
			binding.bindHolder(*holder, roots, context.root());
			holderRef.resetForTests();
			holderRef.publish(std::move(holder));
			return;
		}
		FAIL() << "no import " << file;
	}

	/** QuestEngine.init's loop (QuestEngine.java:104-105) over every XML quest, in XMLQuests.getAllQuests order */
	static void registerAll() {
		for (const models::XMLQuest* quest : dataholders::DataManager::XML_QUESTS->getAllQuests())
			quest->register_(QuestEngine::getInstance());
	}

	/** expected/m5d_registration_order.json, the oracle's registration report (see the file comment) */
	static json expectedOrder() {
		std::ifstream in(EXPECTED_ORDER, std::ios::binary);
		return json::parse(in);
	}

	/** A level-65 player of the race (a Daeva) with the parts every loaded player has (makePlayer), and no quest */
	Quester* player65(int32_t objectId, gameserver::model::Race race) {
		return makePlayer(objectId, race == gameserver::model::Race::ELYOS ? "SmokeE" : "SmokeA", race, 65);
	}

	static void forgetQuests(Quester& quester) {
		quester.player().setQuestStateList(gameserver::model::gameobjects::player::QuestStateList::create());
	}

	/** The SM_QUEST_ACTION(ADD) packets of the quests ReportOnLevelUp starts in REWARD (ReportOnLevelUp.java:60-66), in the given order */
	static std::vector<std::vector<uint8_t>> startedInReward(const std::vector<int32_t>& questIds) {
		std::vector<std::vector<uint8_t>> packets;
		for (int32_t questId : questIds)
			packets.push_back(questAction(1, questId, REWARD));
		return packets;
	}

	std::vector<xml::StaticDataImport> imports;
	std::deque<std::vector<std::unique_ptr<xml::XmlDocument>>> documents;
};

// XMLQuests.getAllQuests is Java's HashMap order, and registering every quest in it builds each npc's lists and the enter-world and
// level-change lists in the oracle's order
TEST_F(QuestTemplatesRealDataTest, RegistrationFollowsJavasHashMapOrder) {
	publishReal<dataholders::QuestsData>(dataholders::DataManager::QUEST_DATA, "quest_data/quest_data.xml");
	publishReal<dataholders::XMLQuests>(dataholders::DataManager::XML_QUESTS, "quest_script_data");
	publishReal<dataholders::VortexData>(dataholders::DataManager::VORTEX_DATA, "vortex/dimensional_vortex.xml");
	publishReal<dataholders::NpcData>(dataholders::DataManager::NPC_DATA, "npcs");
	const json expected = expectedOrder();
	ASSERT_EQ(expected.at("format").get<std::string>(), "aion-m5d-registration-order");

	std::vector<int32_t> order;
	for (const models::XMLQuest* quest : dataholders::DataManager::XML_QUESTS->getAllQuests())
		order.push_back(quest->getId());
	EXPECT_EQ(order.size(), 4184u);
	EXPECT_EQ(order, idsOf(expected.at("order")));

	registerAll();
	EXPECT_EQ(QuestEngine::getInstance().getQuestHandlerCount(), 4184);
	// the npcs a registration can reach: every npc template, npc 0 (a kind whose npc attribute is absent binds 0) and the oracle's
	std::set<int32_t> candidates{0};
	for (const gameserver::model::templates::npc::NpcTemplate* npcTemplate : dataholders::DataManager::NPC_DATA->getNpcData())
		candidates.insert(npcTemplate->getTemplateId());
	for (const char* name : {"onTalkEvent", "onQuestStart", "onKillEvent"}) {
		for (const auto& [npc, ids] : expected.at(name).items())
			candidates.insert(std::stoi(npc));
	}
	const std::map<std::string, std::vector<int32_t> (*)(int32_t)> lists{
		{"onTalkEvent", [](int32_t npcId) { return QuestEngine::getInstance().getQuestNpc(npcId)->getOnTalkEvent().snapshot(); }},
		{"onQuestStart",
			[](int32_t npcId) { return javaHashSetOrder(QuestEngine::getInstance().getQuestNpc(npcId)->getOnQuestStart().snapshot()); }},
		{"onKillEvent", [](int32_t npcId) { return QuestEngine::getInstance().getQuestNpc(npcId)->getOnKillEvent().snapshot(); }}};
	for (const auto& [name, listOf] : lists) {
		int32_t compared = 0;
		std::ostringstream differences;
		int32_t different = 0;
		for (const auto& [npc, ids] : expected.at(name).items()) {
			const int32_t npcId = std::stoi(npc);
			std::vector<int32_t> actual = listOf(npcId);
			compared++;
			if (actual != idsOf(ids) && ++different <= 10) {
				differences << "npc " << npcId << ": expected " << ids.dump() << ", actual [";
				for (size_t i = 0; i < actual.size(); ++i)
					differences << (i ? "," : "") << actual[i];
				differences << "]\n";
			}
		}
		EXPECT_EQ(different, 0) << name << ": " << different << " of " << compared << " npcs differ\n" << differences.str();
		EXPECT_GT(compared, 0) << name;
		// and no other npc has such a list: a registration Java does not make (a quest on npc 0, on an npc the oracle lists for another list)
		std::ostringstream extra;
		int32_t extraNpcs = 0;
		for (int32_t npcId : candidates) {
			if (!listOf(npcId).empty() && !expected.at(name).contains(std::to_string(npcId)) && ++extraNpcs <= 10)
				extra << npcId << " ";
		}
		EXPECT_EQ(extraNpcs, 0) << name << ": npcs Java registers nothing on: " << extra.str();
	}

	// questOnEnterWorld and questOnLevelUp are private to the engine: a level-65 player's enter world and level change start the stigma quests
	// (ReportOnLevelUp) of the race in the order of those lists (the invasion quests of questOnEnterWorld answer false outside their worlds)
	for (const auto& [race, raceName, objectId] : {std::tuple{gameserver::model::Race::ELYOS, "ELYOS", 812001},
			 std::tuple{gameserver::model::Race::ASMODIANS, "ASMODIANS", 812002}}) {
		const std::vector<int32_t> levelUp = idsOf(expected.at("questOnLevelUp").at(raceName));
		std::vector<int32_t> onEnterWorld;
		for (int32_t id : idsOf(expected.at("questOnEnterWorld"))) {
			if (std::find(levelUp.begin(), levelUp.end(), id) != levelUp.end())
				onEnterWorld.push_back(id);
		}
		EXPECT_EQ(onEnterWorld.size(), 5u) << raceName;
		Quester* q = player65(objectId, race);
		enterWorld(*q);
		EXPECT_EQ(sentOf(*q, SM_QUEST_ACTION_OPCODE), startedInReward(onEnterWorld)) << raceName << ": the enter world";
		forgetQuests(*q);
		levelChanged(*q);
		EXPECT_EQ(sentOf(*q, SM_QUEST_ACTION_OPCODE), startedInReward(levelUp)) << raceName << ": the level change";
	}
}

// The smoke over every registered quest (see the file comment)
TEST_F(QuestTemplatesRealDataTest, EveryRegisteredQuestAnswersTalksAndKillsWithoutAnErrorOrAnUnportedHit) {
	publishReal<dataholders::QuestsData>(dataholders::DataManager::QUEST_DATA, "quest_data/quest_data.xml");
	publishReal<dataholders::XMLQuests>(dataholders::DataManager::XML_QUESTS, "quest_script_data");
	publishReal<dataholders::NpcData>(dataholders::DataManager::NPC_DATA, "npcs");
	publishReal<dataholders::ItemData>(dataholders::DataManager::ITEM_DATA, "items/item_templates.xml");
	publishReal<dataholders::VortexData>(dataholders::DataManager::VORTEX_DATA, "vortex/dimensional_vortex.xml");
	Quester* elyos = player65(812003, gameserver::model::Race::ELYOS);
	Quester* asmodian = player65(812004, gameserver::model::Race::ASMODIANS);
	auto questerFor = [&](int32_t questId) {
		const gameserver::model::templates::QuestTemplate* quest = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
		return quest != nullptr && quest->getRacePermitted() == gameserver::model::Race::ASMODIANS ? asmodian : elyos;
	};

	runtime::resetUnportedHitsForTests();
	runtime::resetPartialHitsForTests();
	// the quest engine logs every exception a handler throws; services and utils (QuestService's own catch, the scheduled tasks' wrapper). The
	// database's errors are left out: the unit tests have none (RecipeList's and GameTimeService's DAO calls log their SQL failures there)
	network::test::LogCapture log(
		{"com.aionemu.gameserver.questEngine", "com.aionemu.gameserver.services", "com.aionemu.gameserver.utils"}, spdlog::level::err);
	std::ostringstream failures;
	int32_t failed = 0;
	int32_t errorLines = 0;
	auto check = [&](const std::string& what) {
		int32_t lines = log.count("error|");
		if (lines != errorLines && ++failed <= 20)
			failures << what << "\n";
		errorLines = lines;
	};

	registerAll();
	check("register_ of every XML quest");
	ASSERT_EQ(QuestEngine::getInstance().getQuestHandlerCount(), 4184);

	int32_t talkNpcs = 0;
	int32_t killNpcs = 0;
	int32_t dialogs = 0;
	int32_t kills = 0;
	for (const gameserver::model::templates::npc::NpcTemplate* npcTemplate : dataholders::DataManager::NPC_DATA->getNpcData()) {
		const int32_t npcId = npcTemplate->getTemplateId();
		const std::vector<int32_t> talkList = QuestEngine::getInstance().getQuestNpc(npcId)->getOnTalkEvent().snapshot();
		const std::vector<int32_t> killList = QuestEngine::getInstance().getQuestNpc(npcId)->getOnKillEvent().snapshot();
		if (talkList.empty() && killList.empty())
			continue;
		Npc& npc = npcOf(npcId);
		auto dialog = [&](Quester& q, int32_t questId, int32_t dialogActionId) {
			QuestEngine::getInstance().onDialog(*envOf(q, questId, dialogActionId, at(npc)));
			q.clearSent();
			dialogs++;
			check("npc " + std::to_string(npcId) + " quest " + std::to_string(questId) + " action " + std::to_string(dialogActionId) + " (" +
				  (q.player().getQuestStateList()->hasQuest(questId) ? "START" : "no state") + ")");
		};
		auto killBy = [&](Quester& q, std::string_view state) {
			QuestEngine::getInstance().onKill(*QuestEnv::create(at(npc), q.player(), 0));
			q.clearSent();
			kills++;
			check("kill of npc " + std::to_string(npcId) + " (" + std::string(state) + ")");
		};
		if (!talkList.empty())
			talkNpcs++;
		if (!killList.empty())
			killNpcs++;
		for (Quester* q : {elyos, asmodian}) {
			forgetQuests(*q);
			if (!talkList.empty()) {
				dialog(*q, 0, DialogAction::USE_OBJECT);
				for (int32_t questId : talkList) {
					dialog(*q, questId, DialogAction::QUEST_SELECT);
					dialog(*q, questId, DialogAction::USE_OBJECT);
				}
			}
			if (!killList.empty())
				killBy(*q, "no state");
		}
		for (int32_t questId : talkList) {
			Quester* q = questerFor(questId);
			forgetQuests(*q);
			hold(*q, questId, QuestStatus::START);
			dialog(*q, questId, DialogAction::QUEST_SELECT);
			if (!dynamic_cast<const models::KillSpawnedData*>(dataholders::DataManager::XML_QUESTS->getQuest(questId)))
				dialog(*q, questId, DialogAction::USE_OBJECT);
		}
		for (int32_t questId : killList) {
			Quester* q = questerFor(questId);
			forgetQuests(*q);
			hold(*q, questId, QuestStatus::START);
			killBy(*q, "START of " + std::to_string(questId));
		}
	}
	for (Quester* q : {elyos, asmodian}) {
		forgetQuests(*q);
		QuestEngine::getInstance().onEnterWorld(q->player());
		check("enter world");
		forgetQuests(*q);
		QuestEngine::getInstance().onLevelChanged(q->player());
		check("level change");
		q->clearSent();
	}
	executor->advance(std::chrono::seconds(10)); // the use bars the talks started (xml_quest's <npc_use>)
	check("the scheduled tasks");

	std::cout << "smoke: " << talkNpcs << " talk npcs, " << killNpcs << " kill npcs, " << dialogs << " dialogs, " << kills << " kills" << std::endl;
	EXPECT_EQ(failed, 0) << failed << " calls logged errors; the first:\n" << failures.str() << log.dump().substr(0, 20000);
	// a site reached before the reset (by an earlier case of the process) stays listed with 0 hits
	std::ostringstream unported;
	int32_t unportedSites = 0;
	for (const runtime::UnportedHit& hit : runtime::unportedHits()) {
		if (hit.hits > 0 && ++unportedSites)
			unported << hit.function << " at " << hit.file << ":" << hit.line << " (" << hit.hits << ")\n";
	}
	EXPECT_EQ(unportedSites, 0) << unported.str();
	std::ostringstream partial;
	int32_t partialSites = 0;
	for (const runtime::PartialHit& hit : runtime::partialHits()) {
		if (hit.hits > 0 && ++partialSites)
			partial << hit.function << " at " << hit.file << ":" << hit.line << " (" << hit.hits << ")\n";
	}
	EXPECT_EQ(partialSites, 0) << partial.str();
	// every npc the oracle has a talk or kill list for, and no other one (the registration-order case compares the lists themselves)
	const json expected = expectedOrder();
	EXPECT_EQ(talkNpcs, expected.at("talkLists").at("npcs").get<int32_t>());
	EXPECT_EQ(talkNpcs, static_cast<int32_t>(expected.at("onTalkEvent").size()));
	EXPECT_EQ(killNpcs, static_cast<int32_t>(expected.at("onKillEvent").size()));
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test::templates
