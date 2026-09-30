// P6-Q ascension route (lane route-gen, 2026-09-29): the golden quest trace harness (phase6-inventory.md §7.6 item 3, "left for a build slot").
//
// Every case of cpp/tools/oracle/expected/quest/<id>.json (tools/oracle/questtrace, written from the Java handler, never from the C++) is driven
// through the real engine with the generated handler of that quest, registered with QuestEngine like the registry does (addQuestHandler, which
// calls its register_):
//
// - the case's given becomes the fixture state: a quester of the quest's race (the case's, when it names one) and level, the handler's
//   QuestState (status, var slots, reward group; canRepeat picks the complete count), the inventory counts, the target npc (an Npc of the
//   template standing in the fixture's Poeta instance), the dialog action of the QuestEnv and the quester's map id when the case names one
//   (player.worldId: the enter-world and level hooks of 1100 and 2100 compare it with WorldMapType.POETA / ISHALGEN);
// - ACTUAL: the case's hook of the generated handler runs on that state;
// - EXPECTED: the same state is built again (same player object id, same npc) and the case's effects, the calls Java makes on that path in
//   order, are replayed on the real helpers (AbstractQuestHandler through a PlainHandler of the quest, QuestService, the QuestState setters);
// - both runs are compared: the SM_DIALOG_WINDOW, SM_QUEST_ACTION and SM_PLAY_MOVIE packets byte for byte, the opcode sequence of every packet
//   sent, every QuestState of the player afterwards (status, vars, reward group, complete count), the inventory (item id -> count) and the
//   return value (a helper's result where Java returns it) or the NullPointerException Java throws.
//
// A case whose guards assumed a helper result (`if (QuestService.startQuest(env))`) needs a state in which the real helper returns it. The harness
// tries a short list of setups (the quest's own race and minimum level; that plus its finished prerequisites; level 1; the other race) and uses the
// first one whose replay reproduces every assumption; a case no setup satisfies is reported with its reason, not counted as passed.
//
// Review of 2026-09-29 (the route-gen review): the given state models only the inputs the Java hook reads itself, so a helper the path calls
// (sendQuestEndDialog, defaultOnKillEvent, defaultOnQuestCompletedEvent, removeQuestItem, checkQuestItems) often did nothing in both runs, and a
// deleted reward step or chain trigger passed. Each case now also runs under OVERLAYS: inputs the case's given leaves out, which the hook
// therefore never reads on that path (the path, its effects and its return value stay the same), but which the helpers read - a dialog action
// when the path reads none (the reward page, the finish, SET_SUCCEED, accept, refuse), the items a removeQuestItem names and the quest's collect
// items, each npc of a defaultOnKillEvent list as the target when the path reads no target, var slot 0 at the step of a step helper
// (defaultCloseDialog, checkQuestItems, changeQuestStep) when the path leaves it free (extract.py `free`), and the pre-quests of the chain
// helpers finished with the quest's first permitted class (at the quest's level and one level below). Each run of an overlay is compared like
// the case itself; a helper's own exception in an overlay's state (sendQuestEndDialog's finish without a target) is expected in both runs. A
// case with effects whose runs never change anything observable (no packet, no QuestState or inventory change) is VACUOUS: it must be listed in
// knownVacuous() with its reason. The oracle's `atHigh` entries (the same path with a ranged input at its high end) run as cases of their own.
// Every run reseeds the thread's Rnd with the same seed, on the fixture's ManualClock, so both runs of a case draw the same numbers at the same
// time. A negative control (TheHarnessFailsDeliberatelyWrongHandlers) proves each comparison fails a handler that differs there.
//
// The registration trace (phase6-inventory.md §7.6 item 2) registers each handler alone: its QuestNpc lists must hold the quest as often as the
// Java register() adds it, and the engine events (quest completed, level changed, enter world, enter zone, quest item use, can act) must reach it
// exactly for the registrations of the Java register().
//
// The static data are the real rows of the quests, npcs and items the cases name, filtered out of the Java tree's static_data (read once).

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <pugixml.hpp>

#include "../quest_handlers/QuestHandlerTestSupport.h"

#include "GoldenHandlers.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/dataholders/SkillTreeData.bind.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/questEngine/model/QuestActionType.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/services/GameTimeService.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/zone/ZoneName.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::golden {
namespace {

using json = nlohmann::json;
namespace fs = std::filesystem;
namespace DA = ::aion::gameserver::model::DialogAction;

const fs::path JAVA_DIR = fs::path(AION_GAMESERVER_JAVA_DIR);
const fs::path EXPECTED_DIR = JAVA_DIR / "../cpp/tools/oracle/expected/quest";
const fs::path STATIC_DATA = JAVA_DIR / "data/static_data";

/** tools/oracle/questtrace/extract.py OTHER_NPCS: the target of a case whose guards exclude every registered npc */
constexpr int32_t OTHER_NPCS[] = {200000, 200001, 201000};
constexpr int32_t GOLDEN_PLAYER = 830001;
constexpr int32_t GOLDEN_ITEM_BASE = 840001;
constexpr int32_t OPCODE_DIALOG = SM_DIALOG_WINDOW_OPCODE;
constexpr int32_t OPCODE_QUEST_ACTION = SM_QUEST_ACTION_OPCODE;
constexpr int32_t OPCODE_MOVIE = SM_PLAY_MOVIE_OPCODE;
/** the seed every run starts from (the fixture's DeterministicExecutor seeds 17 once; each run reseeds, so both runs draw the same numbers) */
constexpr uint64_t RUN_SEED = 17;

/** The dialog actions of a dialog overlay (AbstractQuestHandler.java: sendQuestEndDialog :412-472, sendQuestStartDialog :373-398) */
constexpr std::pair<const char*, int32_t> DIALOG_OVERLAYS[] = {
	{"SELECT_QUEST_REWARD", DA::SELECT_QUEST_REWARD},         // the reward page
	{"SELECTED_QUEST_NOREWARD", DA::SELECTED_QUEST_NOREWARD}, // finishQuest
	{"SET_SUCCEED", DA::SET_SUCCEED},                         // the pre-end npc closes the window
	{"QUEST_ACCEPT_1", DA::QUEST_ACCEPT_1},                   // startQuest
	{"QUEST_REFUSE_1", DA::QUEST_REFUSE_1},                   // page 1004
};

json readJson(const fs::path& path) {
	std::ifstream in(path, std::ios::binary);
	if (!in)
		throw std::runtime_error("cannot read " + path.string());
	return json::parse(in);
}

/** The quest ids of the oracle's expected directory, sorted */
std::vector<int32_t> expectedQuestIds() {
	std::vector<int32_t> ids;
	if (!fs::is_directory(EXPECTED_DIR))
		return ids;
	for (const fs::directory_entry& entry : fs::directory_iterator(EXPECTED_DIR)) {
		if (entry.path().extension() == ".json")
			ids.push_back(std::stoi(entry.path().stem().string()));
	}
	std::sort(ids.begin(), ids.end());
	return ids;
}

// --- the static data rows the cases need ----------------------------------------------------------------------------------------------

struct QuestRow {
	int32_t minLevel = 1;
	std::string race;           // race_permitted
	std::vector<std::pair<int32_t, int32_t>> finishedPrerequisites; // quest id, the reward group the condition names (-1: any)
	std::optional<gameserver::model::PlayerClass> classPermitted;    // the first class of <class_permitted>
	std::vector<std::pair<int32_t, int64_t>> collectItems; // <collect_items> and <inventory_items>: what collectItemCheck counts
};

struct GoldenData {
	std::string questsXml;
	std::string npcsXml;
	std::string itemsXml;       // rows to append to the item fixture
	std::map<int32_t, QuestRow> quests;
};

std::string printed(const pugi::xml_node& node) {
	std::ostringstream out;
	node.print(out, "", pugi::format_raw);
	return out.str();
}

void collectIds(const json& value, std::set<int32_t>& out) {
	if (value.is_number_integer()) {
		int64_t v = value.get<int64_t>();
		if (v > 0 && v < INT32_MAX)
			out.insert(static_cast<int32_t>(v));
	} else if (value.is_array()) {
		for (const json& element : value)
			collectIds(element, out);
	}
}

/** Reads the three static data files once and keeps the rows of every quest, npc and item the expected traces name */
const GoldenData& goldenData() {
	static const GoldenData data = [] {
		GoldenData d;
		std::set<int32_t> questIds, npcIds(std::begin(OTHER_NPCS), std::end(OTHER_NPCS)), itemIds;
		for (int32_t questId : expectedQuestIds()) {
			json doc = readJson(EXPECTED_DIR / (std::to_string(questId) + ".json"));
			questIds.insert(questId);
			if (doc["register"].is_array()) {
				for (const json& reg : doc["register"]) {
					if (reg.contains("npc"))
						npcIds.insert(reg["npc"].get<int32_t>());
					else if (reg["call"] == "registerQuestItem")
						itemIds.insert(reg["args"][0].get<int32_t>());
					else if (reg["call"] == "registerCanAct")
						npcIds.insert(reg["args"][1].get<int32_t>());
				}
			}
			for (const json& c : doc["cases"]) {
				const json& given = c["given"];
				if (given.contains("target") && given["target"].contains("npcId"))
					npcIds.insert(given["target"]["npcId"].get<int32_t>());
				if (given.contains("inventory")) {
					for (const auto& [item, count] : given["inventory"].items())
						itemIds.insert(std::stoi(item));
				}
				for (const json& e : c["effects"]) {
					const std::string call = e["call"];
					if (e["kind"] == "item")
						collectIds(e["args"], itemIds);
					if (call == "defaultOnQuestCompletedEvent" || call == "defaultOnLevelChangedEvent")
						collectIds(e["args"], questIds);
					if (call == "defaultOnKillEvent" && !e["args"].empty())
						collectIds(e["args"][0], npcIds);
				}
			}
		}
		pugi::xml_document quests;
		if (!quests.load_file((STATIC_DATA / "quest_data/quest_data.xml").c_str()))
			throw std::runtime_error("cannot read quest_data.xml");
		// the traced quests, then the quests their start conditions name (a finished prerequisite needs its template too)
		for (pugi::xml_node quest : quests.document_element().children("quest")) {
			if (!questIds.contains(quest.attribute("id").as_int()))
				continue;
			for (pugi::xml_node conditions : quest.children("start_conditions")) {
				for (pugi::xml_node finished : conditions.children("finished"))
					questIds.insert(finished.attribute("quest_id").as_int());
			}
		}
		std::string rows;
		const std::regex itemIdAttribute(R"(item_id="(\d+)\")");
		for (pugi::xml_node quest : quests.document_element().children("quest")) {
			int32_t id = quest.attribute("id").as_int();
			if (!questIds.contains(id))
				continue;
			QuestRow row;
			row.minLevel = quest.attribute("minlevel_permitted").as_int(1);
			row.race = quest.attribute("race_permitted").as_string();
			for (pugi::xml_node conditions : quest.children("start_conditions")) {
				for (pugi::xml_node finished : conditions.children("finished"))
					row.finishedPrerequisites.emplace_back(finished.attribute("quest_id").as_int(), finished.attribute("reward").as_int(-1));
			}
			std::istringstream classes(quest.child_value("class_permitted"));
			std::string first;
			if (classes >> first) {
				const auto& names = xml::EnumTraits<gameserver::model::PlayerClass>::names;
				auto it = std::find(names.begin(), names.end(), first);
				if (it != names.end())
					row.classPermitted = static_cast<gameserver::model::PlayerClass>(it - names.begin());
			}
			for (const char* list : {"collect_items", "inventory_items"}) {
				for (pugi::xml_node item : quest.child(list).children())
					row.collectItems.emplace_back(item.attribute("item_id").as_int(), item.attribute("count").as_llong(1));
			}
			std::string text = printed(quest);
			for (std::sregex_iterator it(text.begin(), text.end(), itemIdAttribute), end; it != end; ++it)
				itemIds.insert(std::stoi((*it)[1].str()));
			d.quests[id] = row;
			rows += text;
		}
		d.questsXml = "<quests>" + rows + "</quests>";

		pugi::xml_document npcs;
		if (!npcs.load_file((STATIC_DATA / "npcs/npc_templates.xml").c_str()))
			throw std::runtime_error("cannot read npc_templates.xml");
		rows.clear();
		for (pugi::xml_node npc : npcs.document_element().children("npc_template")) {
			if (npcIds.contains(npc.attribute("npc_id").as_int()))
				rows += printed(npc);
		}
		d.npcsXml = "<npc_templates>" + rows + "</npc_templates>";

		// the item fixture (ItemPacketTestSupport.h) and QuestHandlerTestSupport.h already hold some rows: never twice
		std::set<int32_t> fixtureItems;
		const std::regex templateId(R"(<item_template id="(\d+)\")");
		for (std::string_view fixture : {std::string_view(items::ITEM_TEMPLATES_XML), std::string_view(HANDLER_ITEMS_XML)}) {
			std::string text(fixture);
			for (std::sregex_iterator it(text.begin(), text.end(), templateId), end; it != end; ++it)
				fixtureItems.insert(std::stoi((*it)[1].str()));
		}
		pugi::xml_document itemsDoc;
		if (!itemsDoc.load_file((STATIC_DATA / "items/item_templates.xml").c_str()))
			throw std::runtime_error("cannot read item_templates.xml");
		rows.clear();
		for (pugi::xml_node item : itemsDoc.document_element().children("item_template")) {
			int32_t id = item.attribute("id").as_int();
			if (itemIds.contains(id) && !fixtureItems.contains(id))
				rows += printed(item);
		}
		d.itemsXml = rows;
		return d;
	}();
	return data;
}

// --- one run ------------------------------------------------------------------------------------------------------------------------

using QuestStateRow = std::tuple<int32_t, int32_t, int32_t, int32_t>; // status, vars, reward group (-1: none), complete count

struct Outcome {
	std::string thrown;                                         // "" or the Java exception name
	std::string thrownDetail;                                   // the exception's message (for the failure report)
	json returned;                                              // bool, null (void hook) or a HandlerResult name
	std::vector<bool> helperResults;                            // replay: the result of each effect
	std::vector<int32_t> opcodes;                               // every packet sent to the quester
	std::vector<std::vector<uint8_t>> keyPackets;               // SM_DIALOG_WINDOW, SM_QUEST_ACTION, SM_PLAY_MOVIE
	std::map<int32_t, QuestStateRow> questStates;
	std::map<int32_t, int64_t> inventory;                       // item id -> count
	bool observable = false;                                    // a packet was sent, or a QuestState or the inventory changed
	std::string replayError;                                    // an effect the replay does not model
};

struct CaseSetup {
	std::string name;
	gameserver::model::Race race = gameserver::model::Race::ELYOS;
	int32_t level = 1;
	bool prerequisites = false; // the quests the start conditions name are COMPLETE
	bool collectItems = false;  // the quest's collect and inventory items are held in their counts
	bool questListFull = false; // gameserver.basic.questsize.limit 0 (QuestService.checkQuestListSize fails)
};

/**
 * Inputs the case's given leaves out - the Java hook does not read them on the case's path, so the path, its effects and its return value are
 * the same - but a helper the path calls reads them (the header comment). The first overlay of a case is empty: the case as given.
 */
struct Overlay {
	std::string name;
	std::optional<int32_t> dialogActionId;
	std::optional<int32_t> targetNpcId;
	std::vector<std::pair<int32_t, int64_t>> items;  // held besides the given inventory
	std::vector<int32_t> completedQuests;            // COMPLETE besides the given quest states
	std::optional<int32_t> envQuestId;
	std::optional<int32_t> questVar0;                // var slot 0 of the handler's QuestState, when the path leaves it free
	bool classPermitted = false;                     // the quester is of the first class the quest permits
	bool prerequisites = false;
	bool collectItems = false;
	int32_t levelDelta = 0;                          // added to the setup's level (at least 1)
};

std::string_view resultName(HandlerResult r) {
	switch (r) {
		case HandlerResult::SUCCESS:
			return "SUCCESS";
		case HandlerResult::FAILED:
			return "FAILED";
		default:
			return "UNKNOWN";
	}
}

std::optional<QuestStatus> statusOf(const std::string& name) {
	if (name == "START")
		return QuestStatus::START;
	if (name == "REWARD")
		return QuestStatus::REWARD;
	if (name == "COMPLETE")
		return QuestStatus::COMPLETE;
	if (name == "LOCKED")
		return QuestStatus::LOCKED;
	return std::nullopt;
}

/** The case as given, then its atHigh entries (extract.py: the same path with one ranged input at its high end) as cases of their own */
std::vector<json> variantsOf(const json& c) {
	std::vector<json> out{c};
	if (c.contains("atHigh")) {
		for (const json& high : c["atHigh"]) {
			json v = c;
			v.erase("atHigh");
			v["id"] = c["id"].get<std::string>() + "@" + high["input"].get<std::string>() + "=" + high["value"].dump();
			v["given"] = high["given"];
			v["effects"] = high["effects"];
			v.erase("returns");
			v.erase("throws");
			if (high.contains("returns"))
				v["returns"] = high["returns"];
			if (high.contains("throws"))
				v["throws"] = high["throws"];
			out.push_back(std::move(v));
		}
	}
	return out;
}

/** The overlays of a case (the header comment); the first is the case as given */
std::vector<Overlay> overlaysFor(int32_t questId, const json& c) {
	std::vector<Overlay> out(1);
	const json& given = c["given"];
	const std::string hook = c["hook"];
	auto namedItem = [&](int32_t itemId) { return given.contains("inventory") && given["inventory"].contains(std::to_string(itemId)); };
	auto namedQuest = [&](int32_t q) {
		return q == questId || (given.contains("otherQuests") && given["otherQuests"].contains(std::to_string(q)));
	};
	if (hook == "onDialogEvent" && !given.contains("dialogAction")) {
		for (const auto& [name, id] : DIALOG_OVERLAYS) {
			Overlay o;
			o.name = std::string("dialog action ") + name;
			o.dialogActionId = id;
			out.push_back(std::move(o));
		}
	}
	Overlay held;
	held.name = "the items the helpers remove or count held";
	for (const json& e : c["effects"]) {
		const std::string call = e["call"];
		const json& a = e["args"];
		if (call == "removeQuestItem" && a.size() == 2 && a[0].is_number_integer() && a[1].is_number_integer() && a[1].get<int64_t>() > 0 &&
			!namedItem(a[0].get<int32_t>()))
			held.items.emplace_back(a[0].get<int32_t>(), a[1].get<int64_t>() + 1);
		if (call == "checkQuestItems" || call == "checkQuestItemsSimple" || call == "QuestService.collectItemCheck")
			held.collectItems = true;
	}
	if (!held.items.empty() || held.collectItems)
		out.push_back(std::move(held));
	if (!given.contains("target")) {
		for (const json& e : c["effects"]) {
			if (e["call"] == "defaultOnKillEvent" && !e["args"].empty() && e["args"][0].is_array()) {
				for (const json& npcId : e["args"][0]) {
					Overlay o;
					o.name = "target " + npcId.dump();
					o.targetNpcId = npcId.get<int32_t>();
					out.push_back(std::move(o));
				}
			}
		}
	}
	// a var slot the path does not read, or reads without a guard and uses nowhere (extract.py free_vars: `free`), may take the step a
	// step helper acts at (AbstractQuestHandler.java: defaultCloseDialog :486-529, checkQuestItems :531-575, changeQuestStep :298-328)
	const json& givenState = given.contains("questState") ? given["questState"] : json();
	bool var0Free = givenState.is_object() &&
		(!givenState.contains("vars") || !givenState["vars"].contains("0") ||
			(c.contains("free") && std::find(c["free"].begin(), c["free"].end(), "questState.vars.0") != c["free"].end()));
	for (const json& e : c["effects"]) {
		const std::string call = e["call"];
		const json& a = e["args"];
		if (!var0Free || a.empty() || !a[0].is_number_integer())
			continue;
		if (call != "defaultCloseDialog" && call != "checkQuestItems" && call != "checkQuestItemsSimple" && call != "changeQuestStep")
			continue;
		Overlay o;
		o.name = "var 0 at the step of " + call + " (" + a[0].dump() + ")";
		o.questVar0 = a[0].get<int32_t>();
		out.push_back(o);
		if (call == "checkQuestItems" || call == "checkQuestItemsSimple") {
			o.name += ", the collect items held";
			o.collectItems = true;
			out.push_back(std::move(o));
		}
	}
	bool levelGiven = given.contains("player") && given["player"].contains("level");
	for (const json& e : c["effects"]) {
		const std::string call = e["call"];
		if (call != "defaultOnQuestCompletedEvent" && call != "defaultOnLevelChangedEvent")
			continue;
		Overlay chain;
		chain.name = "the pre-quests of " + call + " finished";
		chain.prerequisites = true;
		chain.classPermitted = !(given.contains("player") && given["player"].contains("class"));
		for (const json& q : e["args"]) {
			if (!namedQuest(q.get<int32_t>()))
				chain.completedQuests.push_back(q.get<int32_t>());
		}
		// QuestEngine.onQuestCompleted hands the finished quest's id in env; the oracle takes env.getQuestId() as the handler's quest, so only a
		// path that reads nothing gets another one
		if (hook == "onQuestCompletedEvent" && c["guards"].empty() && !e["args"].empty())
			chain.envQuestId = e["args"].back().get<int32_t>();
		out.push_back(chain);
		if (!levelGiven) {
			Overlay below = chain;
			below.name += ", one level below the quest's";
			below.levelDelta = -1;
			out.push_back(std::move(below));
		}
	}
	return out;
}

class GoldenQuestTraceTest : public QuestHandlerTest {
protected:
	void SetUp() override {
		QuestHandlerTest::SetUp();
		// the quest list limit at Java's defaults (CustomConfig.java:95, MembershipConfig.java:43): the unit tests load no configuration
		savedQuestSizeLimit = configs::main::CustomConfig::BASIC_QUEST_SIZE_LIMIT.load();
		savedQuestLimitDisabled = configs::main::MembershipConfig::QUEST_LIMIT_DISABLED.load();
		configs::main::CustomConfig::BASIC_QUEST_SIZE_LIMIT.store(40);
		configs::main::MembershipConfig::QUEST_LIMIT_DISABLED.store(10);
		const GoldenData& data = goldenData();
		dataholders::DataManager::QUEST_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(contexts.emplace_back(), data.questsXml));
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), data.npcsXml));
		std::string itemsXml(items::ITEM_TEMPLATES_XML);
		itemsXml.erase(itemsXml.rfind("</item_templates>"));
		itemsXml += HANDLER_ITEMS_XML;
		itemsXml += data.itemsXml;
		itemsXml += "</item_templates>";
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(contexts.emplace_back(), itemsXml));
		// a finish's exp reward can level the quester up, and SM_STATS_INFO then reads the game time: its singleton loads from the database
		// (none in the unit tests: it logs the SQL failure) before the first run, not inside one run of a pair
		services::GameTimeService::getInstance();
		// a level up learns the skills of the skill tree (none: nothing the traces compare)
		dataholders::DataManager::SKILL_TREE_DATA.resetForTests();
		dataholders::DataManager::SKILL_TREE_DATA.publish(xml::bindString<dataholders::SkillTreeData>(contexts.emplace_back(), "<skill_tree/>"));
	}

	void TearDown() override {
		dataholders::DataManager::SKILL_TREE_DATA.resetForTests();
		configs::main::CustomConfig::BASIC_QUEST_SIZE_LIMIT.store(savedQuestSizeLimit);
		configs::main::MembershipConfig::QUEST_LIMIT_DISABLED.store(savedQuestLimitDisabled);
		QuestHandlerTest::TearDown();
	}

	int32_t savedQuestSizeLimit = 0;
	int8_t savedQuestLimitDisabled = 0;

	using Factory = std::unique_ptr<AbstractQuestHandler> (*)();

	/** Registers the handler like QuestEngine::init does for a registry entry (the generated one unless a factory is given); Immortal */
	AbstractQuestHandler& registerGenerated(int32_t questId, Factory factory = nullptr) {
		const GeneratedHandler* generated = generatedHandler(questId);
		if (factory == nullptr && generated == nullptr)
			throw std::runtime_error("no generated handler for quest " + std::to_string(questId));
		std::unique_ptr<AbstractQuestHandler> handler = factory != nullptr ? factory() : generated->factory();
		EXPECT_EQ(handler->getQuestId(), questId);
		AbstractQuestHandler& raw = *handler;
		QuestEngine::getInstance().addQuestHandler(std::move(handler));
		return raw;
	}

	void dropQuester(Quester* quester) {
		quester->player().setTarget(nullptr);
		quester->player().setClientConnection(nullptr);
		world::World::getInstance().removeObject(quester->player());
		quester->client.reset();
		quester->f = {};
		questers.erase(std::find(questers.begin(), questers.end(), quester));
		extraQuesters.erase(std::find_if(extraQuesters.begin(), extraQuesters.end(), [&](const auto& q) { return q.get() == quester; }));
	}

	std::vector<CaseSetup> setupsFor(int32_t questId, const json& given) {
		const QuestRow& row = goldenData().quests.at(questId);
		gameserver::model::Race own = row.race == "ASMODIANS" ? gameserver::model::Race::ASMODIANS : gameserver::model::Race::ELYOS;
		bool raceGiven = given.contains("player") && given["player"].contains("race");
		if (raceGiven)
			own = given["player"]["race"] == "ASMODIANS" ? gameserver::model::Race::ASMODIANS : gameserver::model::Race::ELYOS;
		gameserver::model::Race other = own == gameserver::model::Race::ELYOS ? gameserver::model::Race::ASMODIANS : gameserver::model::Race::ELYOS;
		int32_t level = std::max(row.minLevel, 1);
		bool prerequisites = !row.finishedPrerequisites.empty();
		std::vector<CaseSetup> setups{{"own race, minimum level", own, level}};
		if (prerequisites)
			setups.push_back({"own race, minimum level, prerequisites finished", own, level, true});
		if (!row.collectItems.empty())
			setups.push_back({"own race, minimum level, prerequisites finished, collect items held", own, level, prerequisites, true});
		if (level > 1)
			setups.push_back({"own race, level 1", own, 1});
		if (!raceGiven)
			setups.push_back({"other race", other, level});
		setups.push_back({"own race, minimum level, quest list full", own, level, prerequisites, false, true});
		return setups;
	}

	static void seedQuestState(Quester& quester, int32_t questId, const json& state) {
		if (state.is_null())
			return;
		std::optional<QuestStatus> status = statusOf(state.value("status", std::string("START")));
		if (!status)
			throw std::runtime_error("status " + state.value("status", std::string()));
		int32_t completeCount = 0;
		if (*status == QuestStatus::COMPLETE)
			completeCount = state.contains("canRepeat") && state["canRepeat"].get<bool>() ? 0 : 255;
		Ref<QuestState> qs = QuestState::create(questId, *status, 0, 0, completeCount, std::nullopt, std::nullopt, std::nullopt);
		if (state.contains("vars")) {
			for (const auto& [slot, value] : state["vars"].items())
				qs->setQuestVarById(std::stoi(slot), value.get<int32_t>());
		}
		if (state.contains("rewardGroup") && !state["rewardGroup"].is_null())
			qs->setRewardGroup(state["rewardGroup"].get<int32_t>());
		qs->setPersistentState(gameserver::model::gameobjects::Persistable::PersistentState::UPDATED);
		quester.player().getQuestStateList()->addQuest(questId, *qs);
	}

	static void seedCompleteUnlessHeld(Quester& quester, int32_t questId) {
		if (!quester.player().getQuestStateList()->getQuestState(questId))
			seedQuestState(quester, questId, json{{"status", "COMPLETE"}});
	}

	static std::map<int32_t, QuestStateRow> questStatesOf(gameserver::model::gameobjects::player::Player& player) {
		std::map<int32_t, QuestStateRow> out;
		for (const Ptr<QuestState>& qs : player.getQuestStateList()->getAllQuestState()) {
			std::optional<int32_t> reward = qs->getRewardGroup();
			out[qs->getQuestId()] = {static_cast<int32_t>(qs->getStatus()), qs->getQuestVars()->getQuestVars(), reward ? *reward : -1,
				qs->getCompleteCount()};
		}
		return out;
	}

	static std::map<int32_t, int64_t> inventoryOf(gameserver::model::gameobjects::player::Player& player) {
		std::map<int32_t, int64_t> out;
		for (const Ptr<gameserver::model::gameobjects::Item>& item : player.getInventory().getItemsWithKinah())
			out[item->getItemId()] += item->getItemCount();
		return out;
	}

	/**
	 * One run of a case: `replay` false runs the handler's hook, true replays the Java effects on the real helpers. The quester is created for
	 * the run and dropped after it, so both runs start from the same state and write the same object ids into their packets; the thread's Rnd
	 * is reseeded (RUN_SEED) and the clock is the fixture's ManualClock, which nothing advances.
	 */
	Outcome run(AbstractQuestHandler& handler, int32_t questId, const json& c, const CaseSetup& setup, const Overlay& overlay,
		const Ref<gameserver::model::gameobjects::Npc>& npc, bool replay) {
		Outcome out;
		const json& given = c["given"];
		Quester* quester = makeQuester(GOLDEN_PLAYER, "Golden", setup.race, std::max(setup.level + overlay.levelDelta, 1));
		gameserver::model::gameobjects::player::Player& player = quester->player();
		// the map the case names (extract.py: player.getWorldId(), compared with a WorldMapType id; 1100 and 2100): the quester stands in the
		// fixture's region as before, under that map id, which is what VisibleObject::getWorldId answers
		if (given.contains("player") && given["player"].contains("worldId")) {
			player.setPosition(world::WorldPosition::create(given["player"]["worldId"].get<int32_t>(), 100.0f, 100.0f, 50.0f, int8_t{0},
				mapInstance->getRegion(100.0f, 100.0f, 50.0f)));
			player.getPosition()->setIsSpawned(true);
		}
		// the parts every loaded player has that a finish reaches (QuestTemplate1bTestSupport.h makePlayer): npc factions (finishQuest, a level
		// change), an empty skill list and an empty recipe list
		player.setNpcFactions(std::make_unique<gameserver::model::gameobjects::player::npcFaction::NpcFactions>(player));
		player.setSkillList(gameserver::model::skill::PlayerSkillList::create());
		player.setRecipeList(gameserver::model::gameobjects::player::RecipeList::create());
		if (overlay.classPermitted && goldenData().quests.at(questId).classPermitted)
			quester->f.commonData->setPlayerClass(*goldenData().quests.at(questId).classPermitted);
		if (given.contains("player") && given["player"].contains("class")) {
			const auto& names = xml::EnumTraits<gameserver::model::PlayerClass>::names;
			auto it = std::find(names.begin(), names.end(), given["player"]["class"].get<std::string>());
			if (it == names.end())
				throw std::runtime_error("player class " + given["player"]["class"].dump());
			quester->f.commonData->setPlayerClass(static_cast<gameserver::model::PlayerClass>(it - names.begin()));
		}
		configs::main::CustomConfig::BASIC_QUEST_SIZE_LIMIT.store(setup.questListFull ? 0 : 40);
		// the states the case names first: a prerequisite or an overlay never replaces one
		if (given.contains("questState"))
			seedQuestState(*quester, questId, given["questState"]);
		if (overlay.questVar0) {
			if (Ptr<QuestState> qs = player.getQuestStateList()->getQuestState(questId))
				qs->setQuestVarById(0, *overlay.questVar0);
		}
		if (given.contains("otherQuests")) {
			for (const auto& [other, state] : given["otherQuests"].items())
				seedQuestState(*quester, std::stoi(other), state);
		}
		if (setup.prerequisites || overlay.prerequisites) {
			for (const auto& [pre, reward] : goldenData().quests.at(questId).finishedPrerequisites) {
				if (!quester->player().getQuestStateList()->getQuestState(pre))
					seedQuestState(*quester, pre, reward >= 0 ? json{{"status", "COMPLETE"}, {"rewardGroup", reward}} : json{{"status", "COMPLETE"}});
			}
		}
		for (int32_t completed : overlay.completedQuests)
			seedCompleteUnlessHeld(*quester, completed);
		int32_t itemObjId = GOLDEN_ITEM_BASE;
		auto named = [&](int32_t itemId) { return given.contains("inventory") && given["inventory"].contains(std::to_string(itemId)); };
		if (setup.collectItems || overlay.collectItems) {
			for (const auto& [itemId, count] : goldenData().quests.at(questId).collectItems) {
				if (!named(itemId) && count > 0)
					holdItem(*quester, itemObjId++, itemId, count);
			}
		}
		for (const auto& [itemId, count] : overlay.items) {
			if (!named(itemId))
				holdItem(*quester, itemObjId++, itemId, count);
		}
		if (given.contains("inventory")) {
			for (const auto& [item, count] : given["inventory"].items()) {
				if (count.get<int64_t>() > 0)
					holdItem(*quester, itemObjId++, std::stoi(item), count.get<int64_t>());
			}
		}
		Ptr<gameserver::model::gameobjects::VisibleObject> target = npc;
		if (target)
			player.setTarget(target);
		int32_t dialogActionId = given.contains("dialogAction") ? given["dialogAction"]["id"].get<int32_t>() : 0;
		if (overlay.dialogActionId)
			dialogActionId = *overlay.dialogActionId;
		Ref<QuestEnv> env = QuestEnv::create(target, player, overlay.envQuestId.value_or(questId), dialogActionId);
		std::map<int32_t, QuestStateRow> statesBefore = questStatesOf(player);
		std::map<int32_t, int64_t> inventoryBefore = inventoryOf(player);
		quester->clearSent();
		commons::utils::Rnd::seedCurrentThreadForTests(RUN_SEED);
		try {
			if (replay)
				replayEffects(questId, c, *env, player, out);
			else
				out.returned = callHook(handler, c, *env, player);
		} catch (const runtime::NullPointerException& e) {
			out.thrown = "NullPointerException";
			out.thrownDetail = e.what();
		} catch (const std::exception& e) {
			out.thrown = std::string("C++: ") + e.what();
		}
		for (const std::vector<uint8_t>& packet : quester->sent()) {
			int32_t opcode = items::javaOpcodeOf(packet);
			out.opcodes.push_back(opcode);
			if (opcode == OPCODE_DIALOG || opcode == OPCODE_QUEST_ACTION || opcode == OPCODE_MOVIE)
				out.keyPackets.push_back(packet);
		}
		out.questStates = questStatesOf(player);
		out.inventory = inventoryOf(player);
		out.observable = !out.opcodes.empty() || out.questStates != statesBefore || out.inventory != inventoryBefore;
		dropQuester(quester);
		return out;
	}

	json callHook(AbstractQuestHandler& handler, const json& c, QuestEnv& env, gameserver::model::gameobjects::player::Player& player) {
		const std::string hook = c["hook"];
		const json& args = c["given"].contains("args") ? c["given"]["args"] : json::object();
		if (hook == "onDialogEvent")
			return handler.onDialogEvent(env);
		if (hook == "onKillEvent")
			return handler.onKillEvent(env);
		if (hook == "onEnterWorldEvent")
			return handler.onEnterWorldEvent(env);
		if (hook == "onMovieEndEvent") {
			handler.onMovieEndEvent(env, args.value("movieId", 0));
			return nullptr;
		}
		if (hook == "onQuestCompletedEvent") {
			handler.onQuestCompletedEvent(env);
			return nullptr;
		}
		if (hook == "onLevelChangedEvent") {
			handler.onLevelChangedEvent(player);
			return nullptr;
		}
		if (hook == "onEnterZoneEvent") {
			const json& zone = args["zoneName"];
			const world::zone::ZoneName* zoneName =
				zone.is_string() ? world::zone::ZoneName::get(zone.get<std::string>()) : world::zone::ZoneName::createOrGet("GOLDEN_OTHER_ZONE");
			return handler.onEnterZoneEvent(env, zoneName);
		}
		if (hook == "onItemUseEvent") {
			int32_t itemId = args.value("itemId", 0);
			if (itemId == 0)
				itemId = registeredQuestItem;
			Ref<gameserver::model::gameobjects::Item> item =
				items::loadedItem(GOLDEN_ITEM_BASE + 900, itemId, 1, gameserver::model::items::storage::StorageType::CUBE);
			return std::string(resultName(handler.onItemUseEvent(env, *item)));
		}
		throw std::runtime_error("hook " + hook + " is not driven by the harness");
	}

	/** The Java effects of the case, in order, on the real helpers */
	void replayEffects(int32_t questId, const json& c, QuestEnv& env, gameserver::model::gameobjects::player::Player& player, Outcome& out) {
		PlainHandler plain(questId);
		auto qs = [&] { return player.getQuestStateList()->getQuestState(questId); };
		for (const json& e : c["effects"]) {
			const std::string call = e["call"];
			const json& a = e["args"];
			bool result = false;
			if (call == "sendQuestDialog")
				result = plain.sendQuestDialog(env, a[0].get<int32_t>());
			else if (call == "updateQuestStatus")
				plain.updateQuestStatus(env);
			else if (call == "qs.setQuestVarById")
				qs()->setQuestVarById(a[0].get<int32_t>(), a[1].get<int32_t>());
			else if (call == "qs.setQuestVar")
				qs()->setQuestVar(a[0].get<int32_t>());
			else if (call == "qs.setStatus")
				qs()->setStatus(*statusOf(a[0].get<std::string>()));
			else if (call == "qs.setRewardGroup")
				qs()->setRewardGroup(a[0].get<int32_t>());
			else if (call == "sendQuestEndDialog" && a.empty())
				result = plain.sendQuestEndDialog(env);
			else if (call == "sendQuestStartDialog" && a.empty())
				result = plain.sendQuestStartDialog(env);
			else if (call == "sendQuestSelectionDialog")
				result = plain.sendQuestSelectionDialog(env);
			else if (call == "closeDialogWindow")
				result = plain.closeDialogWindow(env);
			else if (call == "giveQuestItem" && a.size() == 2)
				result = plain.giveQuestItem(env, a[0].get<int32_t>(), a[1].get<int64_t>());
			else if (call == "removeQuestItem" && a.size() == 2)
				result = plain.removeQuestItem(env, a[0].get<int32_t>(), a[1].get<int64_t>());
			else if (call == "playQuestMovie" && a.size() == 1)
				result = plain.playQuestMovie(env, a[0].get<int32_t>());
			else if (call == "playQuestMovie" && a.size() == 2)
				result = plain.playQuestMovie(env, a[0].get<int32_t>(), a[1].get<bool>());
			else if (call == "QuestService.startQuest")
				result = services::QuestService::startQuest(env);
			else if (call == "QuestService.finishQuest")
				result = services::QuestService::finishQuest(env);
			else if (call == "QuestService.collectItemCheck")
				result = services::QuestService::collectItemCheck(env, a[0].get<bool>());
			else if (call == "env.setQuestId")
				env.setQuestId(a[0].get<int32_t>());
			else if (call == "defaultCloseDialog" && a.size() == 2)
				result = plain.defaultCloseDialog(env, a[0].get<int32_t>(), a[1].get<int32_t>());
			else if (call == "defaultCloseDialog" && a.size() == 4 && a[2].is_boolean())
				result = plain.defaultCloseDialog(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<bool>(), a[3].get<bool>());
			else if (call == "checkQuestItems" && a.size() == 5)
				result = plain.checkQuestItems(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<bool>(), a[3].get<int32_t>(), a[4].get<int32_t>());
			else if (call == "changeQuestStep" && a.size() == 3)
				result = plain.changeQuestStep(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<bool>());
			else if (call == "defaultOnKillEvent" && a.size() == 3 && a[0].is_array() && a[2].is_boolean()) {
				std::vector<int32_t> npcIds = a[0].get<std::vector<int32_t>>();
				result = plain.defaultOnKillEvent(env, std::span<const int32_t>(npcIds), a[1].get<int32_t>(), a[2].get<bool>());
			} else if (call == "defaultOnKillEvent" && a.size() == 3 && a[0].is_array()) {
				std::vector<int32_t> npcIds = a[0].get<std::vector<int32_t>>();
				result = plain.defaultOnKillEvent(env, std::span<const int32_t>(npcIds), a[1].get<int32_t>(), a[2].get<int32_t>());
			} else if (call == "defaultOnQuestCompletedEvent" && a.size() <= 6) {
				// Java varargs; the C++ helper takes an initializer_list (AbstractQuestHandler.h)
				std::vector<int32_t> q = a.get<std::vector<int32_t>>();
				size_t n = q.size();
				q.resize(6, 0);
				switch (n) {
					case 0: result = plain.defaultOnQuestCompletedEvent(env); break;
					case 1: result = plain.defaultOnQuestCompletedEvent(env, {q[0]}); break;
					case 2: result = plain.defaultOnQuestCompletedEvent(env, {q[0], q[1]}); break;
					case 3: result = plain.defaultOnQuestCompletedEvent(env, {q[0], q[1], q[2]}); break;
					case 4: result = plain.defaultOnQuestCompletedEvent(env, {q[0], q[1], q[2], q[3]}); break;
					case 5: result = plain.defaultOnQuestCompletedEvent(env, {q[0], q[1], q[2], q[3], q[4]}); break;
					default: result = plain.defaultOnQuestCompletedEvent(env, {q[0], q[1], q[2], q[3], q[4], q[5]}); break;
				}
			} else if (call == "defaultOnLevelChangedEvent" && a.size() <= 6) {
				std::vector<int32_t> q = a.get<std::vector<int32_t>>();
				size_t n = q.size();
				q.resize(6, 0);
				switch (n) {
					case 0: result = plain.defaultOnLevelChangedEvent(player); break;
					case 1: result = plain.defaultOnLevelChangedEvent(player, {q[0]}); break;
					case 2: result = plain.defaultOnLevelChangedEvent(player, {q[0], q[1]}); break;
					case 3: result = plain.defaultOnLevelChangedEvent(player, {q[0], q[1], q[2]}); break;
					case 4: result = plain.defaultOnLevelChangedEvent(player, {q[0], q[1], q[2], q[3]}); break;
					case 5: result = plain.defaultOnLevelChangedEvent(player, {q[0], q[1], q[2], q[3], q[4]}); break;
					default: result = plain.defaultOnLevelChangedEvent(player, {q[0], q[1], q[2], q[3], q[4], q[5]}); break;
				}
			}
			else if (call == "PacketSendUtility.sendPacket" && a.size() == 1 && a[0].value("new", std::string()) == "SM_DIALOG_WINDOW") {
				const json& p = a[0]["args"];
				int32_t targetObjectId =
					p[0] == "$targetObjectId" ? env.getVisibleObject()->getObjectId() : p[0].get<int32_t>();
				utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_DIALOG_WINDOW(targetObjectId, p[1].get<int32_t>()));
			} else {
				out.replayError = "the replay does not model " + call + " " + a.dump();
				return;
			}
			out.helperResults.push_back(result);
		}
		if (c.contains("returns"))
			out.returned = expectedReturn(c["returns"], out.helperResults);
	}

	static json expectedReturn(const json& returns, const std::vector<bool>& results) {
		if (returns.is_object() && returns.contains("resultOf")) {
			size_t k = returns["resultOf"].get<size_t>();
			return k < results.size() ? json(results[k]) : json("resultOf out of range");
		}
		if (returns.is_object() && returns.contains("fromBoolean")) {
			json inner = expectedReturn(returns["fromBoolean"], results);
			if (!inner.is_boolean())
				return inner;
			return std::string(inner.get<bool>() ? "SUCCESS" : "FAILED");
		}
		return returns;
	}

	/** The target npc of a run, one Npc for both runs (its object id is in the dialog packets): the overlay's, else the case's */
	Ref<gameserver::model::gameobjects::Npc> targetOf(const json& given, const Overlay& overlay) {
		if (overlay.targetNpcId) {
			npcOf(*overlay.targetNpcId);
			return npcs.back();
		}
		if (!given.contains("target") || given["target"].value("kind", std::string("none")) != "npc")
			return nullptr;
		npcOf(given["target"]["npcId"].get<int32_t>());
		return npcs.back();
	}

	struct Tally {
		int32_t passed = 0;
		int32_t failed = 0;
		int32_t runs = 0;                         // compared runs: every variant under every overlay a setup reproduces
		int32_t overlaysSkipped = 0;              // overlay runs no setup reproduces (not a failure; the case as given is compared)
		std::vector<std::string> unsatisfiable;
		std::vector<std::string> vacuous;         // cases with effects whose runs never changed anything observable
		std::set<std::string> failedChecks;       // "thrown" "returned" "packets" "opcodes" "questStates" "inventory"
	};

	/**
	 * Runs every case of one expected document on the registered handler (the generated one, or `factory`'s); a failure is reported with the
	 * case id unless `report` is false (the negative control counts them)
	 */
	Tally runQuest(int32_t questId, Factory factory = nullptr, bool report = true) {
		return runDoc(questId, readJson(EXPECTED_DIR / (std::to_string(questId) + ".json")), factory, report);
	}

	/** runQuest on a document in the oracle's format (a hand trace, HandTracesOfTheClassSkillQuests) */
	Tally runDoc(int32_t questId, const json& doc, Factory factory = nullptr, bool report = true) {
		Tally tally;
		AbstractQuestHandler& handler = registerGenerated(questId, factory);
		registeredQuestItem = 0;
		for (const json& reg : doc["register"]) {
			if (reg.value("call", std::string()) == "registerQuestItem")
				registeredQuestItem = reg["args"][0].get<int32_t>();
		}
		for (const json& base : doc["cases"]) {
			bool anyObservable = false;
			bool hasEffects = !base["effects"].empty();
			for (const json& c : variantsOf(base)) {
				const std::string id = c["id"];
				SCOPED_TRACE(std::to_string(questId) + " " + id);
				bool ok = true;
				bool reproduced = false;
				std::string why;
				for (const Overlay& overlay : overlaysFor(questId, c)) {
					Ref<gameserver::model::gameobjects::Npc> npc = targetOf(c["given"], overlay);
					std::optional<CaseSetup> chosen;
					Outcome expected;
					for (const CaseSetup& setup : setupsFor(questId, c["given"])) {
						Outcome replayed = run(handler, questId, c, setup, overlay, npc, true);
						if (!replayed.replayError.empty()) {
							why = replayed.replayError;
							break;
						}
						bool holds = true;
						if (c.contains("assume")) {
							for (const json& assumption : c["assume"]) {
								size_t k = assumption["effect"].get<size_t>();
								if (k >= replayed.helperResults.size() || replayed.helperResults[k] != assumption["returns"].get<bool>())
									holds = false;
							}
						}
						if (holds) {
							chosen = setup;
							expected = std::move(replayed);
							break;
						}
						why = "no setup makes the helpers return the assumed results " + c["assume"].dump();
					}
					if (!chosen) {
						if (!overlay.name.empty())
							tally.overlaysSkipped++;
						continue;
					}
					if (overlay.name.empty())
						reproduced = true;
					tally.runs++;
					anyObservable = anyObservable || expected.observable;
					Outcome actual = run(handler, questId, c, *chosen, overlay, npc, false);
					// Java's exception on the path, else one a helper throws in this state (the replay runs the helpers: sendQuestEndDialog's
					// `(Npc) env.getVisibleObject()` after a finish with no target, AbstractQuestHandler.java:423-424)
					std::string expectedThrow = c.contains("throws") ? c["throws"].get<std::string>() : expected.thrown;
					std::string where = std::to_string(questId) + " " + id + " (setup: " + chosen->name +
						(overlay.name.empty() ? "" : "; overlay: " + overlay.name) + "): ";
					auto check = [&](bool condition, const std::string& name, const std::string& what) {
						if (!condition) {
							ok = false;
							tally.failedChecks.insert(name);
							if (report)
								ADD_FAILURE() << where << what;
						}
					};
					check(actual.thrown == expectedThrow, "thrown",
						"thrown '" + actual.thrown + "' (" + actual.thrownDetail + "), Java '" + expectedThrow + "'");
					if (expectedThrow.empty())
						check(actual.returned == expected.returned, "returned", "returned " + actual.returned.dump() + ", Java " + expected.returned.dump());
					check(actual.keyPackets == expected.keyPackets, "packets", "the dialog, quest action and movie packets differ");
					check(actual.opcodes == expected.opcodes, "opcodes", "the packet opcode sequence differs");
					check(actual.questStates == expected.questStates, "questStates", "the quest states afterwards differ");
					check(actual.inventory == expected.inventory, "inventory", "the inventory afterwards differs");
				}
				if (!reproduced) {
					tally.unsatisfiable.push_back(id + ": " + why);
					continue;
				}
				(ok ? tally.passed : tally.failed)++;
			}
			if (hasEffects && !anyObservable)
				tally.vacuous.push_back(base["id"].get<std::string>());
		}
		if (report) {
			std::cout << "[golden] quest " << questId << ": " << tally.passed << " passed, " << tally.failed << " failed, " << tally.unsatisfiable.size()
								<< " not reproducible of " << doc["cases"].size() << " cases and their high ends; " << tally.runs << " runs compared, "
								<< tally.overlaysSkipped << " overlay runs not reproducible, " << tally.vacuous.size() << " vacuous\n";
			for (const std::string& line : tally.unsatisfiable)
				std::cout << "[golden]   not reproducible: " << line << "\n";
			for (const std::string& line : tally.vacuous)
				std::cout << "[golden]   vacuous: " << line << "\n";
		}
		return tally;
	}

	int32_t registeredQuestItem = 0;
};

/**
 * Cases no fixture state reproduces, each traced to its reason (GoldenQuestTraceTest's header comment): a case listed here must stay not
 * reproducible, and every other case must pass.
 */
const std::set<std::string>& knownNotReproducible() {
	// Empty. The first run of this harness found 9 cases no setup reproduces: the oracle took a helper's result as free when a guard branched
	// on it, and wrote paths Java cannot take (giveQuestItem of a non-zero item and count "-> false", AbstractQuestHandler.java:626-641;
	// collectItemCheck(env, true) "-> true" without a QuestState, QuestService.java:557-561). tools/oracle/questtrace/extract.py
	// (dead_assumption) now drops such paths, and the expected traces were regenerated without them.
	static const std::set<std::string> known{};
	return known;
}

/**
 * Cases with effects whose runs, under every overlay, change nothing observable (the header comment), "<questId> <case id>", each with its
 * reason. A case listed here must stay vacuous (a new overlay that makes it observable removes it), and no other case may be vacuous.
 */
const std::map<std::string, std::string>& knownVacuous() {
	// Each is a helper the path calls in a state the path itself fixes (its given names the status and the dialog action), in which the Java
	// helper does nothing; no overlay may change an input the path reads. The same helper is observable in other cases of its quest.
	static const std::string IDLE_END = "sendQuestEndDialog: the status and the dialog action the path read are not REWARD with a reward, select or "
																 "SET_SUCCEED action (AbstractQuestHandler.java:414-472)";
	static const std::string IDLE_START =
		"sendQuestStartDialog: no branch of its switch takes USE_OBJECT, which the path read (AbstractQuestHandler.java:373-397)";
	static const std::string NOT_STARTED = "QuestService.startQuest assumed false: it sends nothing then (QuestService.java:400-444)";
	static const std::map<std::string, std::string> known{
		{"1005 onDialogEvent#37", IDLE_END},
		{"1100 onDialogEvent#4", IDLE_START},
		{"1107 onDialogEvent#4", IDLE_END},
		{"1107 onDialogEvent#5", IDLE_END},
		{"1111 onDialogEvent#5", IDLE_START},
		{"1111 onDialogEvent#9", IDLE_END},
		{"1122 onDialogEvent#3", IDLE_START},
		{"1122 onDialogEvent#12", IDLE_START},
		{"1123 onDialogEvent#2", IDLE_START},
		{"1123 onDialogEvent#4", IDLE_START},
		{"1123 onDialogEvent#7", IDLE_END},
		{"2001 onDialogEvent#18", IDLE_END},
		{"2006 onDialogEvent#16", IDLE_END},
		{"2100 onDialogEvent#5", IDLE_START},
		{"2106 onDialogEvent#28", IDLE_END},
		{"2114 onDialogEvent#3", NOT_STARTED},
		{"2114 onDialogEvent#5", NOT_STARTED},
		{"2114 onDialogEvent#9", NOT_STARTED},
		{"2114 onDialogEvent#11", NOT_STARTED},
		{"2122 onDialogEvent#27", IDLE_END},
		{"2123 onDialogEvent#8", IDLE_START},
		{"2123 onDialogEvent#11", IDLE_START},
		{"2125 onDialogEvent#2", IDLE_START},
		{"2125 onDialogEvent#6", IDLE_START},
		{"2125 onDialogEvent#13", IDLE_END},
		{"2125 onDialogEvent#14", IDLE_END},
		{"2135 onDialogEvent#3", IDLE_START},
		{"2135 onDialogEvent#6", IDLE_END},
		{"2135 onDialogEvent#11", IDLE_START},
	};
	return known;
}

/** The quests whose every hook the oracle refuses (tools/oracle/questtrace: `new QuestEnv`, getStartingClass on a value): no case */
constexpr int32_t ORACLE_REFUSES_EVERY_HOOK[] = {1205, 2132};

TEST_F(GoldenQuestTraceTest, EveryExpectedDocumentHasAGeneratedHandlerAndEveryHandlerADocument) {
	std::vector<int32_t> ids = expectedQuestIds();
	ASSERT_FALSE(ids.empty()) << EXPECTED_DIR;
	for (int32_t questId : ids)
		EXPECT_NE(generatedHandler(questId), nullptr) << questId << " has no handler";
	for (const GeneratedHandler& handler : generatedHandlers())
		EXPECT_TRUE(std::binary_search(ids.begin(), ids.end(), handler.questId)) << handler.javaClass << " has no expected trace in " << EXPECTED_DIR;
}

class GoldenQuestCases : public GoldenQuestTraceTest, public ::testing::WithParamInterface<int32_t> {};

TEST_P(GoldenQuestCases, EveryCaseMatchesTheJavaTrace) {
	int32_t questId = GetParam();
	ASSERT_TRUE(fs::exists(EXPECTED_DIR / (std::to_string(questId) + ".json"))) << "no expected trace for " << questId
		<< " (python oracle.py quest-trace generate)";
	Tally tally = runQuest(questId);
	for (const std::string& line : tally.unsatisfiable) {
		std::string key = std::to_string(questId) + " " + line.substr(0, line.find(':'));
		EXPECT_TRUE(knownNotReproducible().contains(key)) << "not reproducible and not listed: " << questId << " " << line;
	}
	for (const std::string& id : tally.vacuous)
		EXPECT_TRUE(knownVacuous().contains(std::to_string(questId) + " " + id)) << "vacuous and not listed: " << questId << " " << id;
	for (const auto& [key, reason] : knownVacuous()) {
		if (key.starts_with(std::to_string(questId) + " "))
			EXPECT_NE(std::find(tally.vacuous.begin(), tally.vacuous.end(), key.substr(key.find(' ') + 1)), tally.vacuous.end())
				<< "listed as vacuous but observable now: " << key;
	}
	bool refused =
		std::find(std::begin(ORACLE_REFUSES_EVERY_HOOK), std::end(ORACLE_REFUSES_EVERY_HOOK), questId) != std::end(ORACLE_REFUSES_EVERY_HOOK);
	EXPECT_NE(tally.passed + tally.failed > 0, refused)
		<< questId << (refused ? " has cases now: take it off ORACLE_REFUSES_EVERY_HOOK" : " has no case");
	// every case and every high end of the document ran (counted from the document, not from variantsOf)
	json doc = readJson(EXPECTED_DIR / (std::to_string(questId) + ".json"));
	size_t variants = 0;
	for (const json& c : doc["cases"])
		variants += 1 + (c.contains("atHigh") ? c["atHigh"].size() : 0);
	EXPECT_EQ(tally.passed + tally.failed + tally.unsatisfiable.size(), variants);
	EXPECT_EQ(tally.failed, 0);
}

std::vector<int32_t> tracedQuestIds() {
	std::vector<int32_t> ids;
	for (const GeneratedHandler& handler : generatedHandlers())
		ids.push_back(handler.questId);
	return ids;
}

INSTANTIATE_TEST_SUITE_P(Generated, GoldenQuestCases, ::testing::ValuesIn(tracedQuestIds()),
	[](const ::testing::TestParamInfo<int32_t>& info) { return "Quest" + std::to_string(info.param); });

// --- hand traces of the quests the oracle refuses -----------------------------------------------------------------------------------------

json effect(const char* call, json args = json::array()) {
	return json{{"call", call}, {"kind", "hand"}, {"args", std::move(args)}};
}

/**
 * The trace of _1205ANewSkill.java / _2132ANewSkill.java (the same file but for the npc ids), written by hand in the oracle's format because
 * the oracle refuses both hooks (`new QuestEnv`, getStartingClass on a value). onLevelChangedEvent (:34-67): a player without the quest starts
 * it, and it goes to REWARD with var class+1 and reward group class (WARRIOR 0 .. ARTIST 5), then updateQuestStatus. onDialogEvent (:70-130):
 * REWARD only; at the npc of the player's starting class USE_OBJECT sends that class's page (1011, 1352, 1693, 2034, 2375, 2716) and every
 * other action sendQuestEndDialog; at another class's npc false.
 */
json classSkillTrace(const std::array<int32_t, 6>& npcIds) {
	static constexpr const char* CLASSES[] = {"WARRIOR", "SCOUT", "MAGE", "PRIEST", "ENGINEER", "ARTIST"};
	static constexpr int32_t PAGES[] = {1011, 1352, 1693, 2034, 2375, 2716};
	json cases = json::array();
	auto add = [&](const char* hook, json given, json effects, json returns, json assume = nullptr) {
		json c = {{"id", "hand#" + std::to_string(cases.size() + 1)}, {"hook", hook}, {"given", std::move(given)}, {"guards", json::array()},
			{"effects", std::move(effects)}, {"returns", std::move(returns)}};
		if (!assume.is_null())
			c["assume"] = std::move(assume);
		cases.push_back(std::move(c));
	};
	auto npc = [](int32_t npcId) { return json{{"kind", "npc"}, {"npcId", npcId}}; };
	for (int32_t i = 0; i < 6; i++) {
		json player = {{"class", CLASSES[i]}};
		json reward = {{"status", "REWARD"}, {"vars", {{"0", i + 1}}}, {"rewardGroup", i}};
		add("onLevelChangedEvent", {{"player", player}, {"questState", nullptr}},
			json::array({effect("QuestService.startQuest"), effect("qs.setStatus", {"REWARD"}), effect("qs.setQuestVar", {i + 1}),
				effect("qs.setRewardGroup", {i}), effect("updateQuestStatus")}),
			nullptr, json::array({{{"effect", 0}, {"returns", true}}}));
		add("onDialogEvent", {{"player", player}, {"questState", reward}, {"target", npc(npcIds[i])}, {"dialogAction", {{"id", DA::USE_OBJECT}}}},
			json::array({effect("sendQuestDialog", {PAGES[i]})}), {{"resultOf", 0}});
		add("onDialogEvent",
			{{"player", player}, {"questState", reward}, {"target", npc(npcIds[i])}, {"dialogAction", {{"id", DA::SELECT_QUEST_REWARD}}}},
			json::array({effect("sendQuestEndDialog")}), {{"resultOf", 0}});
		add("onDialogEvent",
			{{"player", player}, {"questState", reward}, {"target", npc(npcIds[(i + 1) % 6])}, {"dialogAction", {{"id", DA::USE_OBJECT}}}},
			json::array(), false);
	}
	add("onLevelChangedEvent", {{"questState", {{"status", "START"}}}}, json::array(), nullptr);       // hasQuest: nothing
	add("onDialogEvent", {{"questState", nullptr}, {"target", npc(npcIds[0])}}, json::array(), false); // no QuestState
	add("onDialogEvent", {{"player", {{"class", "WARRIOR"}}}, {"questState", {{"status", "START"}}}, {"target", npc(npcIds[0])}}, json::array(),
		false); // not REWARD
	return json{{"register", json::array()}, {"cases", cases}};
}

TEST_F(GoldenQuestTraceTest, HandTracesOfTheClassSkillQuests) {
	// 1205 and 2132 grant the class skill on the route; the oracle refuses their hooks (ORACLE_REFUSES_EVERY_HOOK), so their golden trace is
	// written by hand from the Java above and driven like an oracle document (overlays included)
	const std::pair<int32_t, std::array<int32_t, 6>> quests[] = {
		{1205, {203087, 203088, 203089, 203090, 801210, 801211}}, // _1205ANewSkill.java:26-31
		{2132, {203527, 203528, 203529, 203530, 801218, 801219}}, // _2132ANewSkill.java:26-31
	};
	for (const auto& [questId, npcIds] : quests) {
		SCOPED_TRACE(questId);
		QuestEngine::getInstance().clear();
		json doc = classSkillTrace(npcIds);
		Tally tally = runDoc(questId, doc);
		EXPECT_EQ(tally.failed, 0);
		EXPECT_EQ(tally.passed, static_cast<int32_t>(doc["cases"].size()));
		EXPECT_TRUE(tally.unsatisfiable.empty()) << tally.unsatisfiable.front();
		EXPECT_TRUE(tally.vacuous.empty()) << tally.vacuous.front();
	}
}

// --- the harness's own guards ------------------------------------------------------------------------------------------------------------

/** What the negative control changes in its copy of _1111InsomniaMedicine */
enum class Flip { NONE, PAGE, VAR, REWARD_GROUP, ITEM, STATUS, RETURN };

/**
 * A statement-by-statement copy of the generated _1111InsomniaMedicine.cpp (quest 1111) with one deliberate difference, chosen by `flip`: the
 * negative control of the harness. With Flip::NONE it must pass every case of 1111.json; with any other flip the comparison named beside it
 * in TheHarnessFailsDeliberatelyWrongHandlers must fail.
 */
class WrongInsomniaMedicine final : public AbstractQuestHandler {
public:
	static inline Flip flip = Flip::NONE;

	WrongInsomniaMedicine() : AbstractQuestHandler(1111) {}

	void register_() override {
		qe.registerQuestNpc(203075)->addOnQuestStart(questId);
		qe.registerQuestNpc(203075)->addOnTalkEvent(questId);
		qe.registerQuestNpc(203061)->addOnTalkEvent(questId);
	}

	bool onDialogEvent(QuestEnv& env) override {
		using gameserver::model::gameobjects::Npc;
		runtime::Ptr<gameserver::model::gameobjects::player::Player> player = env.getPlayer();
		int32_t targetId = 0;
		if (runtime::as<Npc>(env.getVisibleObject()) != nullptr)
			targetId = (runtime::cast<Npc>(env.getVisibleObject()))->getNpcId();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (targetId == 203075) {
			if (qs == nullptr) {
				if (env.getDialogActionId() == DA::QUEST_SELECT)
					return sendQuestDialog(env, 1011);
				else
					return sendQuestStartDialog(env);
			} else if (qs->getStatus() == QuestStatus::REWARD) {
				if (env.getDialogActionId() == DA::USE_OBJECT) {
					if (qs->getQuestVarById(0) == 2) {
						removeQuestItem(env, 182200222, 1);
						return sendQuestDialog(env, 2375);
					} else if (qs->getQuestVarById(0) == 3) {
						qs->setRewardGroup(1);
						removeQuestItem(env, 182200221, 1);
						return sendQuestDialog(env, 2716);
					}
					return false;
				}
				return sendQuestEndDialog(env);
			}
		} else if (targetId == 203061) {
			if (env.getDialogActionId() == DA::QUEST_SELECT) {
				if (qs->getQuestVarById(0) == 0)
					return sendQuestDialog(env, flip == Flip::PAGE ? 1353 : 1352);
				else if (qs->getQuestVarById(0) == 1)
					return sendQuestDialog(env, 1353);
				return false;
			} else if (env.getDialogActionId() == DA::CHECK_USER_HAS_QUEST_ITEM) {
				if (services::QuestService::collectItemCheck(env, true)) {
					qs->setQuestVarById(0, qs->getQuestVarById(0) + 1);
					updateQuestStatus(env);
					return sendQuestDialog(env, 1353);
				} else
					return sendQuestDialog(env, 1693);
			} else if (env.getDialogActionId() == DA::SETPRO1 && qs->getStatus() != QuestStatus::COMPLETE) {
				if (!giveQuestItem(env, 182200222, flip == Flip::ITEM ? 2 : 1))
					return true;
				qs->setQuestVarById(0, flip == Flip::VAR ? 3 : 2);
				qs->setRewardGroup(flip == Flip::REWARD_GROUP ? 2 : 0);
				qs->setStatus(flip == Flip::STATUS ? QuestStatus::START : QuestStatus::REWARD);
				updateQuestStatus(env);
				utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 10));
				return flip != Flip::RETURN;
			} else if (env.getDialogActionId() == DA::SETPRO2 && qs->getStatus() != QuestStatus::COMPLETE) {
				if (!giveQuestItem(env, 182200221, 1))
					return true;
				qs->setQuestVarById(0, 3);
				qs->setRewardGroup(1);
				qs->setStatus(QuestStatus::REWARD);
				updateQuestStatus(env);
				utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 10));
				return true;
			}
		}
		return false;
	}
};

std::unique_ptr<AbstractQuestHandler> wrongInsomniaMedicine() {
	return std::make_unique<WrongInsomniaMedicine>();
}

TEST_F(GoldenQuestTraceTest, TheHarnessFailsDeliberatelyWrongHandlers) {
	// each flip must fail at least one case, through the comparison that sees it: the page only in the dialog packet's bytes (the opcodes stay
	// the same), the reward group only in the QuestState (SM_QUEST_ACTION does not carry it), the item count only in the inventory (the item
	// packets are no key packets), the var and status in the QuestState, the return value in itself
	const std::pair<Flip, const char*> flips[] = {{Flip::NONE, ""}, {Flip::PAGE, "packets"}, {Flip::VAR, "questStates"},
		{Flip::REWARD_GROUP, "questStates"}, {Flip::ITEM, "inventory"}, {Flip::STATUS, "questStates"}, {Flip::RETURN, "returned"}};
	for (const auto& [flip, check] : flips) {
		SCOPED_TRACE("flip " + std::to_string(static_cast<int>(flip)));
		QuestEngine::getInstance().clear();
		WrongInsomniaMedicine::flip = flip;
		Tally tally = runQuest(1111, &wrongInsomniaMedicine, false);
		WrongInsomniaMedicine::flip = Flip::NONE;
		EXPECT_GT(tally.passed + tally.failed, 0);
		if (flip == Flip::NONE) {
			EXPECT_EQ(tally.failed, 0) << "the copy is not the generated handler any more";
		} else {
			EXPECT_GT(tally.failed, 0) << "the harness passes a wrong handler";
			EXPECT_TRUE(tally.failedChecks.contains(check)) << "the " << check << " comparison did not fail";
		}
	}
}

/** A handler whose dialog hook sends a page drawn from the thread's Rnd: both runs of a case must draw the same one */
class RandomPageHandler final : public AbstractQuestHandler {
public:
	RandomPageHandler() : AbstractQuestHandler(1111) {}
	void register_() override {}
	bool onDialogEvent(QuestEnv& env) override { return sendQuestDialog(env, 1000 + commons::utils::Rnd::nextInt(1000)); }
};

TEST_F(GoldenQuestTraceTest, EveryRunStartsFromTheSameRandomStateOnTheManualClock) {
	// phase6-inventory.md §7.6 item 3: ManualClock and a seeded Rnd. The fixture's DeterministicExecutor runs on its ManualClock, which nothing
	// advances; each run reseeds the thread's Rnd
	EXPECT_EQ(&utils::ThreadPoolManager::clock(), static_cast<const runtime::Clock*>(&clock));
	RandomPageHandler handler;
	json c = {{"id", "random"}, {"hook", "onDialogEvent"}, {"given", json::object()}, {"effects", json::array()}};
	Outcome first = run(handler, 1111, c, CaseSetup{"own race", gameserver::model::Race::ELYOS, 3}, Overlay{}, nullptr, false);
	Outcome second = run(handler, 1111, c, CaseSetup{"own race", gameserver::model::Race::ELYOS, 3}, Overlay{}, nullptr, false);
	ASSERT_EQ(first.keyPackets.size(), 1u);
	EXPECT_EQ(first.keyPackets, second.keyPackets);
}

// --- the registration trace (phase6-inventory.md §7.6 item 2) ------------------------------------------------------------------------------

/** For one npc registration of the Java trace, how often the quest id sits in the QuestNpc's list of that event */
int32_t registeredCount(int32_t npcId, const std::string& event, int32_t questId) {
	Ref<gameserver::model::templates::quest::QuestNpc> npc = QuestEngine::getInstance().getQuestNpc(npcId);
	auto count = [&](const std::vector<int32_t>& list) { return static_cast<int32_t>(std::count(list.begin(), list.end(), questId)); };
	if (event == "addOnQuestStart")
		return npc->getOnQuestStart().contains(questId) ? 1 : 0;
	if (event == "addOnTalkEvent")
		return count(npc->getOnTalkEvent().snapshot());
	if (event == "addOnKillEvent")
		return count(npc->getOnKillEvent().snapshot());
	if (event == "addOnAttackEvent")
		return count(npc->getOnAttackEvent().snapshot());
	if (event == "addOnAddAggroListEvent")
		return count(npc->getOnAddAggroListEvent().snapshot());
	if (event == "addOnAtDistanceEvent")
		return count(npc->getOnDistanceEvent().snapshot());
	return -1;
}

/**
 * Registered in place of a generated handler: its register_() is the generated one's (the engine keeps the quest id, not the object), and the
 * engine's events reach this object, which records them
 */
class RoutingSpy final : public AbstractQuestHandler {
public:
	RoutingSpy(std::unique_ptr<AbstractQuestHandler> inner, std::vector<std::string>& events)
		: AbstractQuestHandler(inner->getQuestId()), inner(std::move(inner)), events(events) {}

	void register_() override { inner->register_(); }
	void onQuestCompletedEvent(QuestEnv&) override { events.push_back("onQuestCompletedEvent"); }
	void onLevelChangedEvent(gameserver::model::gameobjects::player::Player&) override { events.push_back("onLevelChangedEvent"); }
	bool onEnterWorldEvent(QuestEnv&) override {
		events.push_back("onEnterWorldEvent");
		return false;
	}
	bool onEnterZoneEvent(QuestEnv&, const world::zone::ZoneName* zoneName) override {
		events.push_back("onEnterZoneEvent " + zoneName->name());
		return false;
	}
	HandlerResult onItemUseEvent(QuestEnv&, gameserver::model::gameobjects::Item& item) override {
		events.push_back("onItemUseEvent " + std::to_string(item.getItemId()));
		return HandlerResult::UNKNOWN;
	}
	bool onCanAct(QuestEnv& env, model::QuestActionType, std::span<const std::any>) override {
		events.push_back("onCanAct " + std::to_string(env.getTargetId()));
		return false;
	}

private:
	std::unique_ptr<AbstractQuestHandler> inner;
	std::vector<std::string>& events;
};

TEST_F(GoldenQuestTraceTest, RegistrationTraceMatchesJavaRegister) {
	// every generated handler with an expected document, registered alone: per npc and event the quest id sits in the QuestNpc lists as often
	// as the Java register() adds it and in no other npc's list, and every other registration routes exactly the engine events it names. A
	// can-act registration also comes from the first kill, talk, aggro or distance registration of the quest at an npc (QuestNpc.java:59-95),
	// and registerCanAct keeps only an npc whose template's AI is quest_use_item (QuestEngine.java registerCanAct)
	std::set<std::string> zones;
	std::set<int32_t> questItems, canActNpcs, allNpcs;
	auto usesQuestItemAi = [](int32_t npcId) {
		const gameserver::model::templates::npc::NpcTemplate* template_ = dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId);
		return template_ != nullptr && template_->getAiName() == "quest_use_item";
	};
	for (int32_t questId : expectedQuestIds()) {
		json doc = readJson(EXPECTED_DIR / (std::to_string(questId) + ".json"));
		for (const json& reg : doc["register"]) {
			if (reg.contains("npc"))
				allNpcs.insert(reg["npc"].get<int32_t>());
			else if (reg.value("call", std::string()) == "registerOnEnterZone")
				zones.insert(reg["args"][0].get<std::string>());
			else if (reg.value("call", std::string()) == "registerQuestItem")
				questItems.insert(reg["args"][0].get<int32_t>());
			else if (reg.value("call", std::string()) == "registerCanAct")
				canActNpcs.insert(reg["args"][1].get<int32_t>());
			if (reg.value("call", std::string()) == "registerCanAct")
				allNpcs.insert(reg["args"][1].get<int32_t>());
		}
	}
	int32_t checked = 0;
	for (const GeneratedHandler& generated : generatedHandlers()) {
		SCOPED_TRACE(std::string(generated.javaClass));
		json doc = readJson(EXPECTED_DIR / (std::to_string(generated.questId) + ".json"));
		ASSERT_TRUE(doc["register"].is_array()) << generated.questId << ": " << doc["register"].dump();
		QuestEngine::getInstance().clear();
		std::vector<std::string> events;
		QuestEngine::getInstance().addQuestHandler(std::make_unique<RoutingSpy>(generated.factory(), events));

		std::map<std::pair<int32_t, std::string>, int32_t> expected;
		std::set<int32_t> namedNpcs;
		std::vector<std::string> expectedEvents;
		for (const json& reg : doc["register"]) {
			if (reg.contains("npc")) {
				std::string event = reg["event"];
				int32_t npcId = reg["npc"].get<int32_t>();
				namedNpcs.insert(npcId);
				bool first = !expected.contains({npcId, event});
				if (event != "addOnQuestStart" || first) // onQuestStart is a set
					expected[{npcId, event}]++;
				if (first && usesQuestItemAi(npcId) &&
					(event == "addOnKillEvent" || event == "addOnTalkEvent" || event == "addOnAddAggroListEvent" || event == "addOnAtDistanceEvent"))
					expectedEvents.push_back("onCanAct " + std::to_string(npcId));
				continue;
			}
			const std::string call = reg["call"];
			if (call == "registerOnQuestCompleted")
				expectedEvents.push_back("onQuestCompletedEvent");
			else if (call == "registerOnLevelChanged")
				expectedEvents.push_back("onLevelChangedEvent");
			else if (call == "registerOnEnterWorld")
				expectedEvents.push_back("onEnterWorldEvent");
			else if (call == "registerOnEnterZone")
				expectedEvents.push_back("onEnterZoneEvent " + reg["args"][0].get<std::string>());
			else if (call == "registerQuestItem")
				expectedEvents.push_back("onItemUseEvent " + std::to_string(reg["args"][0].get<int32_t>()));
			else if (call == "registerCanAct") {
				if (usesQuestItemAi(reg["args"][1].get<int32_t>()))
					expectedEvents.push_back("onCanAct " + std::to_string(reg["args"][1].get<int32_t>()));
			}
			else
				ADD_FAILURE() << "the registration trace does not model " << reg.dump();
		}
		for (const auto& [key, count] : expected) {
			EXPECT_EQ(registeredCount(key.first, key.second, generated.questId), count) << "npc " << key.first << " " << key.second;
			checked++;
		}
		// an npc of any other registration must not list this quest
		for (int32_t npcId : allNpcs) {
			if (namedNpcs.contains(npcId))
				continue;
			for (const char* event : {"addOnQuestStart", "addOnTalkEvent", "addOnKillEvent"})
				EXPECT_EQ(registeredCount(npcId, event, generated.questId), 0) << "npc " << npcId << " " << event;
		}
		for (int32_t itemId : questItems)
			EXPECT_EQ(QuestEngine::getInstance().isRegisteredQuestItem(itemId),
				std::find(expectedEvents.begin(), expectedEvents.end(), "onItemUseEvent " + std::to_string(itemId)) != expectedEvents.end())
				<< "quest item " << itemId;

		// fire every event the documents name, once each
		const QuestRow& row = goldenData().quests.at(generated.questId);
		gameserver::model::Race race = row.race == "ASMODIANS" ? gameserver::model::Race::ASMODIANS : gameserver::model::Race::ELYOS;
		Quester* quester = makeQuester(GOLDEN_PLAYER, "Golden", race, std::max(row.minLevel, 1));
		gameserver::model::gameobjects::player::Player& player = quester->player();
		QuestEngine::getInstance().onQuestCompleted(player, 1);
		QuestEngine::getInstance().onLevelChanged(player);
		QuestEngine::getInstance().onEnterWorld(player);
		for (const std::string& zone : zones)
			QuestEngine::getInstance().onEnterZone(*QuestEnv::create(nullptr, player, 0), world::zone::ZoneName::createOrGet(zone));
		for (int32_t itemId : questItems) {
			Ref<gameserver::model::gameobjects::Item> item =
				items::loadedItem(GOLDEN_ITEM_BASE + 901, itemId, 1, gameserver::model::items::storage::StorageType::CUBE);
			QuestEngine::getInstance().onItemUseEvent(*QuestEnv::create(nullptr, player, 0), *item);
		}
		for (int32_t npcId : allNpcs) {
			if (!canActNpcs.contains(npcId) && !usesQuestItemAi(npcId))
				continue;
			npcOf(npcId);
			Ptr<gameserver::model::gameobjects::VisibleObject> npc = npcs.back();
			QuestEngine::getInstance().onCanAct(*QuestEnv::create(npc, player, 0), npcId, model::QuestActionType::ACTION_ITEM_USE);
		}
		dropQuester(quester);
		std::sort(events.begin(), events.end());
		std::sort(expectedEvents.begin(), expectedEvents.end());
		EXPECT_EQ(events, expectedEvents);
		checked += static_cast<int32_t>(expectedEvents.size());
	}
	EXPECT_GT(checked, 0);
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test::golden
