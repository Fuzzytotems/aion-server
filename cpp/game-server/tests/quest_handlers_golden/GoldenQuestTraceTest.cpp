// P6-Q ascension route (lane route-gen, 2026-09-29): the golden quest trace harness (phase6-inventory.md §7.6 item 3, "left for a build slot").
//
// Every case of cpp/tools/oracle/expected/quest/<id>.json (tools/oracle/questtrace, written from the Java handler, never from the C++) is driven
// through the real engine with the generated handler of that quest, registered with QuestEngine like the registry does (addQuestHandler, which
// calls its register_):
//
// - the case's given becomes the fixture state: a quester of the quest's race (the case's, when it names one) and level, the handler's
//   QuestState (status, var slots, reward group; canRepeat picks the complete count), the inventory counts, the target npc (an Npc of the
//   template standing in the fixture's Poeta instance), the dialog action of the QuestEnv and the quester's map id when the case names one
//   (player.worldId: the enter-world and level hooks of 1100 and 2100 compare it with WorldMapType.POETA / ISHALGEN, the enter-world hooks
//   of 14050 with HEIRON and of 18602 with Kromede's Trial's 300230000; the held-back 14010 and 24010 compare it with VERTERON / ALTGARD);
// - ACTUAL: the case's hook of the generated handler runs on that state;
// - EXPECTED: the same state is built again (same player object id, same npc) and the case's effects, the calls Java makes on that path in
//   order, are replayed on the real helpers (AbstractQuestHandler through a PlainHandler of the quest, QuestService, the QuestState setters);
// - both runs are compared: the SM_DIALOG_WINDOW, SM_QUEST_ACTION, SM_PLAY_MOVIE (and since lane C SM_ITEM_USAGE_ANIMATION) packets byte
//   for byte, the opcode sequence of every packet sent, every QuestState of the player afterwards (status, vars, reward group, complete
//   count), the inventory (item id -> count) and the return value (a helper's result where Java returns it) or the NullPointerException Java throws.
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
// The Q10 review (2026-09-29): a case whose path reads the dialog action only through `!=` guards carries the excluded actions
// (extract.py dialogExcludes) and runs under free-dialog overlays too, with each action the start and end helpers act on (overlaysFor); a run
// that reaches an AION_UNPORTED engine body is reported unless knownUnported lists it; the negative control flips the opcodes, the thrown
// exception and an unported reach as well.
//
// The registration trace (phase6-inventory.md §7.6 item 2) registers each handler alone: its QuestNpc lists must hold the quest as often as the
// Java register() adds it, and the engine events (quest completed, level changed, enter world, enter zone, quest item use, can act) must reach it
// exactly for the registrations of the Java register().
//
// The static data are the real rows of the quests, npcs and items the cases name, filtered out of the Java tree's static_data (read once).
//
// P6-Q slice 2, chunk Q03 (2026-09-29): the 76 generated verteron and heiron handlers joined the table (GoldenHandlers.h), and with them
// the hooks, helper overloads, registrations and overlays they need (each marked "P6-Q slice 2 (Q03)" below; docs/deviations/Q03.md, Tests).
//
// Lane C, phase 6 step 1 (2026-10-05; phase6-transliterator.md §7): the harness drives the eight hooks the generated corpus overrides that it did
// not drive (onKillRankedEvent, onKillInWorldEvent, onDredgionRewardEvent, onInvisibleTimerEndEvent, onUseSkillEvent, onFailCraftEvent,
// onEnterWindStreamEvent, onLeaveZoneEvent), the registration trace models the ten registration kinds it did not (registerOnKillRanked and the
// nine of §3.5 there), and a case whose path schedules a task (ThreadPoolManager.schedule; the oracle runs the task after the hook and marks its
// effects with `task`) runs it in both runs: the generated handler's task by advancing the fixture's clock past the longest delay, the replay's
// by scheduling the task's effects with the same delay. AION_GOLDEN_EXPECTED_DIR and AION_GOLDEN_SAMPLE_TABLE (GoldenHandlers.h) build the
// out-of-tree sample of tools/gen/questgen/goldensample.py from these sources.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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
#include "GoldenKnownVacuousQ10.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/dataholders/SkillTreeData.bind.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/quest/QuestDrop.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/model/templates/quest/HandlerSideDrop.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestActionType.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
#include "aion/gameserver/model/templates/rewards/BonusType.h"
#include "aion/gameserver/services/GameTimeService.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/WarehouseService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"
#include "aion/gameserver/world/zone/ZoneName.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::golden {
namespace {

using json = nlohmann::json;
namespace fs = std::filesystem;
namespace DA = ::aion::gameserver::model::DialogAction;

const fs::path JAVA_DIR = fs::path(AION_GAMESERVER_JAVA_DIR);
#ifdef AION_GOLDEN_EXPECTED_DIR
const fs::path EXPECTED_DIR = fs::path(AION_GOLDEN_EXPECTED_DIR); // the out-of-tree sample (GoldenHandlers.h)
#else
const fs::path EXPECTED_DIR = JAVA_DIR / "../cpp/tools/oracle/expected/quest";
#endif
const fs::path STATIC_DATA = JAVA_DIR / "data/static_data";

/** tools/oracle/questtrace/extract.py OTHER_NPCS: the target of a case whose guards exclude every registered npc */
constexpr int32_t OTHER_NPCS[] = {200000, 200001, 201000};
/** The item of an item-use case whose given item id is 0 (an item no guard names): Kerub Grain Sack, a row of HANDLER_ITEMS_XML */
constexpr int32_t OTHER_ITEM = 182200201;
constexpr int32_t GOLDEN_PLAYER = 830001;
constexpr int32_t GOLDEN_ITEM_BASE = 840001;
/** The object id of the item an item-use case uses (callHook; the review of #79, item 2: held in the inventory) */
constexpr int32_t USED_ITEM_OBJECT = GOLDEN_ITEM_BASE + 900;
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

/**
 * P6-Q slice 2 (the Q10 review): the dialog actions of a free-dialog overlay (extract.py dialogExcludes), the ones above and the other two
 * sendQuestStartDialog acts on (AbstractQuestHandler.java:374-397)
 */
constexpr std::pair<const char*, int32_t> FREE_DIALOG_OVERLAYS[] = {
	{"SELECT_QUEST_REWARD", DA::SELECT_QUEST_REWARD},
	{"SELECTED_QUEST_NOREWARD", DA::SELECTED_QUEST_NOREWARD},
	{"SET_SUCCEED", DA::SET_SUCCEED},
	{"QUEST_ACCEPT_1", DA::QUEST_ACCEPT_1},
	{"QUEST_REFUSE_1", DA::QUEST_REFUSE_1},
	{"ASK_QUEST_ACCEPT", DA::ASK_QUEST_ACCEPT}, // page 4
	{"FINISH_DIALOG", DA::FINISH_DIALOG},       // the selection dialog
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

bool heldBack(int32_t questId) {
	return std::find(std::begin(GOLDEN_HELD_BACK), std::end(GOLDEN_HELD_BACK), questId) != std::end(GOLDEN_HELD_BACK);
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
	std::string experienceXml;  // the whole player_experience_table.xml (P6-Q slice 2: quests up to level 50; the fixture has 26 levels)
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
					// P6-Q slice 2 (Q10): the helpers that give or take an item besides their step (defaultCloseDialog's six ints, useQuestObject,
					// sendQuestStartDialog's start item) are no "item" effects; an item id is a nine-digit number, which no npc or quest id is
					std::set<int32_t> numbers;
					collectIds(e["args"], numbers);
					for (int32_t n : numbers) {
						if (n >= 100000000)
							itemIds.insert(n);
					}
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
		std::ifstream experience(STATIC_DATA / "player_experience_table.xml", std::ios::binary);
		if (!experience)
			throw std::runtime_error("cannot read player_experience_table.xml");
		d.experienceXml.assign(std::istreambuf_iterator<char>(experience), std::istreambuf_iterator<char>());
		return d;
	}();
	return data;
}

// --- one run ------------------------------------------------------------------------------------------------------------------------

using QuestStateRow = std::tuple<int32_t, int32_t, int32_t, int32_t>; // status, vars, reward group (-1: none), complete count

/**
 * What a run looks like at one instant of its task timeline (the review of #79, item 1): the packets sent so far, every QuestState and the
 * inventory. Taken after the hook, then one millisecond before and at each delay of the case's ThreadPoolManager.schedule effects
 */
struct Snapshot {
	int64_t at = 0;                                             // ms after the hook
	std::vector<int32_t> opcodes;
	std::map<int32_t, QuestStateRow> questStates;
	std::map<int32_t, int64_t> inventory;
	bool operator==(const Snapshot&) const = default;
};

struct Outcome {
	std::string thrown;                                         // "" or the Java exception name
	std::string thrownDetail;                                   // the exception's message (for the failure report)
	json returned;                                              // bool, null (void hook) or a HandlerResult name
	// replay: the result of each effect, 1 true, 0 false, -1 the effect never ran or threw (the review of #79, item 5: an effect that did not
	// run must not satisfy an assumed false)
	std::vector<int8_t> helperResults;
	std::vector<Snapshot> timeline;                             // the review of #79, item 1
	int64_t pendingTasks = 0;                                   // tasks the run added to the executor and left after the case's last delay
	std::vector<int32_t> opcodes;                               // every packet sent to the quester
	std::vector<std::vector<uint8_t>> keyPackets;               // SM_DIALOG_WINDOW, SM_QUEST_ACTION, SM_PLAY_MOVIE, SM_ITEM_USAGE_ANIMATION
	std::map<int32_t, QuestStateRow> questStates;
	std::map<int32_t, int64_t> inventory;                       // item id -> count
	bool observable = false;                                    // a packet was sent, or a QuestState or the inventory changed
	std::string replayError;                                    // an effect the replay does not model
	std::set<std::string> unported;                             // the AION_UNPORTED sites the run reached ("file:line function")
};

/** Every AION_UNPORTED site reached so far with its hits (runtime/base/Unported.h), keyed "file:line function" */
std::map<std::string, uint64_t> unportedSites() {
	std::map<std::string, uint64_t> out;
	for (const runtime::UnportedHit& hit : runtime::unportedHits())
		out[hit.file + ":" + std::to_string(hit.line) + " " + hit.function] = hit.hits;
	return out;
}

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
	std::optional<int32_t> startVar0;                // a QuestState (seedStatus) with var slot 0 at this value, when the path reads none
	std::string seedStatus = "START";
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

/**
 * Whether the case's guards on the dialog action also hold for the action `name` (P6-Q slice 2, the Q03 review): every guard naming the action
 * is `== X -> false` or `not in [X, ...] -> true` without `name` (the oracle picks USE_OBJECT for such a path); a guard of another form fixes it
 */
bool dialogGuardsAllow(const json& c, const std::string& name) {
	static const std::regex guard(R"(^(env\.getDialogActionId\(\)|dialogActionId) (==|not in) (.*) -> (true|false) @\d+$)");
	static const std::regex word(R"([A-Z_0-9]+)");
	bool any = false;
	for (const json& g : c["guards"]) {
		const std::string text = g.get<std::string>();
		if (text.find("ialog") == std::string::npos)
			continue;
		std::smatch m;
		if (!std::regex_match(text, m, guard))
			return false;
		bool negative = (m[2] == "==" && m[4] == "false") || (m[2] == "not in" && m[4] == "true");
		if (!negative)
			return false;
		const std::string names = m[3];
		for (std::sregex_iterator it(names.begin(), names.end(), word), end; it != end; ++it) {
			if (it->str() == name)
				return false;
		}
		any = true;
	}
	return any;
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
	// P6-Q slice 2 (the Q10 review, 2026-09-29): a path that reads the dialog action only through `!=` guards (extract.py dialogExcludes: the
	// else branch of `if (action == QUEST_SELECT) ... else return sendQuestStartDialog(env)`) takes the same path with every action outside
	// the list, while given holds one (USE_OBJECT) with which the start and end helpers do nothing: each action a helper acts on runs as an
	// overlay; the accept also with the quest's start prerequisites finished and its first permitted class, so that startQuest succeeds
	if (hook == "onDialogEvent" && c.contains("dialogExcludes")) {
		const json& excluded = c["dialogExcludes"];
		int32_t givenAction = given.contains("dialogAction") ? given["dialogAction"]["id"].get<int32_t>() : 0;
		bool classGiven = given.contains("player") && given["player"].contains("class");
		for (const auto& [name, id] : FREE_DIALOG_OVERLAYS) {
			if (id == givenAction || std::find(excluded.begin(), excluded.end(), id) != excluded.end())
				continue;
			Overlay o;
			o.name = std::string("dialog action ") + name + " (the path leaves it free)";
			o.dialogActionId = id;
			if (id == DA::QUEST_ACCEPT_1) {
				Overlay started = o;
				started.name += ", the quest's first permitted class";
				started.classPermitted = !classGiven;
				if (!given.contains("otherQuests") && !goldenData().quests.at(questId).finishedPrerequisites.empty()) {
					started.name += " and its start prerequisites finished";
					started.prerequisites = true;
				}
				out.push_back(std::move(started));
			}
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
		// P6-Q slice 2 (Q03): the item a step helper removes (defaultCloseDialog's removeItemId/removeItemCount, useQuestObject's)
		size_t removeAt = call == "defaultCloseDialog" && a.size() == 6 ? 4 : (call == "defaultCloseDialog" || call == "useQuestObject") && a.size() == 8 ? 6 : 0;
		if (removeAt > 0 && a[removeAt].is_number_integer() && a[removeAt + 1].is_number_integer() && a[removeAt].get<int32_t>() != 0 &&
			a[removeAt + 1].get<int64_t>() > 0 && !namedItem(a[removeAt].get<int32_t>()))
			held.items.emplace_back(a[removeAt].get<int32_t>(), a[removeAt + 1].get<int64_t>() + 1);
		if (call == "checkQuestItems" || call == "checkQuestItemsSimple" || call == "QuestService.collectItemCheck")
			held.collectItems = true;
		// P6-Q slice 2 (the Q03 review): the items sendQuestEndDialog(env, int[]) removes, whole stacks whatever the status or the action
		// (AbstractQuestHandler.java:401-411), held two each (_1220's and _1634's lists: no case holds them otherwise)
		if (call == "sendQuestEndDialog" && a.size() == 1 && a[0].is_array()) {
			for (const json& itemId : a[0]) {
				if (itemId.is_number_integer() && itemId.get<int32_t>() != 0 && !namedItem(itemId.get<int32_t>()))
					held.items.emplace_back(itemId.get<int32_t>(), 2);
			}
		}
	}
	if (!held.items.empty() || held.collectItems)
		out.push_back(std::move(held));
	// the review of #79, item 3: the success branches of two helpers the case's given does not reach. checkItemExistence (AbstractQuestHandler.java
	// :576-609: `getItemCountByItemId(itemId) >= itemCount`, the 10-argument form at var 0 == step) runs with the item held in exactly its count
	// and one short of it, so that a changed count, nextStep or checkOkId shows; sendQuestRewardDialog (:1151-1163) runs in REWARD with its
	// reward npc as the target when the path's guards never name the target (the given then holds none or an npc no guard read)
	const json& stateGiven = given.contains("questState") ? given["questState"] : json();
	const bool slot0Free = stateGiven.is_object() &&
		(!stateGiven.contains("vars") || !stateGiven["vars"].contains("0") ||
			(c.contains("free") && std::find(c["free"].begin(), c["free"].end(), "questState.vars.0") != c["free"].end()));
	bool targetGuarded = false;
	for (const json& g : c["guards"]) {
		const std::string text = g.get<std::string>();
		if (text.find("arget") != std::string::npos || text.find("VisibleObject") != std::string::npos)
			targetGuarded = true;
	}
	for (const json& e : c["effects"]) {
		const std::string call = e["call"];
		const json& a = e["args"];
		if (call == "checkItemExistence" && (a.size() == 3 || a.size() == 10)) {
			size_t at = a.size() == 3 ? 0 : 3;
			if (!a[at].is_number_integer() || !a[at + 1].is_number_integer() || namedItem(a[at].get<int32_t>()))
				continue;
			int32_t itemId = a[at].get<int32_t>();
			int64_t count = a[at + 1].get<int64_t>();
			for (int64_t heldCount : {count, count - 1}) {
				if (heldCount <= 0)
					continue;
				Overlay o;
				o.name = "the item of checkItemExistence held: " + std::to_string(heldCount) + " of " + std::to_string(itemId) +
					(heldCount == count ? "" : " (one short)");
				o.items.emplace_back(itemId, heldCount);
				if (a.size() == 10 && a[0].is_number_integer()) {
					if (slot0Free)
						o.questVar0 = a[0].get<int32_t>();
					else if (!given.contains("questState"))
						o.startVar0 = a[0].get<int32_t>();
					o.name += ", var 0 at its step " + a[0].dump();
				}
				out.push_back(std::move(o));
			}
		} else if (call == "sendQuestRewardDialog" && a.size() == 2 && a[0].is_number_integer() && !targetGuarded) {
			Overlay o;
			o.name = "the reward npc of sendQuestRewardDialog (" + a[0].dump() + ") as the target";
			o.targetNpcId = a[0].get<int32_t>();
			if (!given.contains("questState")) {
				o.startVar0 = 0;
				o.seedStatus = "REWARD";
				o.name += ", the quest in REWARD";
			}
			out.push_back(std::move(o));
		}
	}
	if (!given.contains("target")) {
		for (const json& e : c["effects"]) {
			if (e["call"] == "defaultOnKillEvent" && !e["args"].empty() && e["args"][0].is_array()) {
				for (const json& npcId : e["args"][0]) {
					Overlay o;
					o.name = "target " + npcId.dump();
					o.targetNpcId = npcId.get<int32_t>();
					out.push_back(std::move(o));
				}
			} else if (e["call"] == "defaultOnKillEvent" && !e["args"].empty() && e["args"][0].is_number_integer()) {
				// the single-npc overloads (AbstractQuestHandler.java defaultOnKillEvent(env, int npcId, ...); P6-Q slice 2, Q03, and Q10: 2223, 24016,
				// 4210)
				Overlay o;
				o.name = "target " + e["args"][0].dump();
				o.targetNpcId = e["args"][0].get<int32_t>();
				out.push_back(std::move(o));
			}
			// a kill hook that only returns the helpers (`return defaultOnKillEvent(env, mobs, 5, 7) || defaultOnKillEvent(env, mobs, 7, true)`,
			// _14014) reads neither the target nor var 0: its npc as the target and var 0 at the helper's start var, in the given START state
			// or in one the overlay starts (AbstractQuestHandler.java defaultOnKillEvent: `var >= startVar && var < endVar`, `var == startVar`)
			const json& a = e["args"];
			const json& state = given.contains("questState") ? given["questState"] : json();
			bool stateFree = !given.contains("questState");
			bool var0Unread = state.is_object() && state.value("status", std::string("START")) == "START" &&
				(!state.contains("vars") || !state["vars"].contains("0") ||
					(c.contains("free") && std::find(c["free"].begin(), c["free"].end(), "questState.vars.0") != c["free"].end()));
			if (e["call"] == "defaultOnKillEvent" && (stateFree || var0Unread) && a.size() >= 3 && a[1].is_number_integer() &&
				((a[0].is_array() && !a[0].empty()) || a[0].is_number_integer())) {
				Overlay o;
				o.targetNpcId = a[0].is_array() ? a[0][0].get<int32_t>() : a[0].get<int32_t>();
				(stateFree ? o.startVar0 : o.questVar0) = a[1].get<int32_t>();
				o.name = "target " + std::to_string(*o.targetNpcId) + ", var 0 at the start var " + a[1].dump();
				// and a ranged helper (env, npcs, startVar, endVar: var 0) at the last var of its range, where it takes var 0 to endVar (the Q03
				// review: _14014's `defaultOnKillEvent(env, mobs, 5, 7)`, whose endVar the start var never reads)
				if (a.size() == 3 && a[2].is_number_integer() && a[2].get<int32_t>() - 1 > a[1].get<int32_t>()) {
					Overlay last = o;
					(stateFree ? last.startVar0 : last.questVar0) = a[2].get<int32_t>() - 1;
					last.name = "target " + std::to_string(*o.targetNpcId) + ", var 0 at the last var of the range " + a[1].dump() + ".." + a[2].dump();
					out.push_back(std::move(last));
				}
				out.push_back(std::move(o));
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
		// P6-Q slice 2 (Q10): useQuestObject and defaultOnGetItemEvent take the step first too, defaultOnKillEvent second (after the npcs)
		size_t stepAt = call == "defaultOnKillEvent" ? 1 : 0;
		if (!var0Free || a.size() <= stepAt || !a[stepAt].is_number_integer())
			continue;
		// useQuestObject acts at its step like the others (AbstractQuestHandler.java useQuestObject: `var == step`; P6-Q slice 2, Q03 and Q10)
		if (call != "defaultCloseDialog" && call != "checkQuestItems" && call != "checkQuestItemsSimple" && call != "changeQuestStep" &&
			call != "useQuestObject" && call != "defaultOnGetItemEvent" && call != "defaultOnKillEvent" && call != "defaultOnKillRankedEvent")
			continue;
		Overlay o;
		o.name = "var 0 at the step of " + call + " (" + a[stepAt].dump() + ")";
		o.questVar0 = a[stepAt].get<int32_t>();
		out.push_back(o);
		// lane C: the kill-ranked helper (startVar, endVar) also acts at endVar - 1, its finishing kill (AbstractQuestHandler.java:772-778)
		if (call == "defaultOnKillRankedEvent" && a.size() >= 2 && a[1].is_number_integer() && a[1].get<int32_t>() - 1 > a[0].get<int32_t>()) {
			Overlay last;
			last.name = "var 0 at the last kill of " + call + " (" + std::to_string(a[1].get<int32_t>() - 1) + ")";
			last.questVar0 = a[1].get<int32_t>() - 1;
			out.push_back(std::move(last));
		}
		// and one step past it, where the helper answers false (P6-Q slice 2, Q03: _1559's `if (!changeQuestStep(env, 0, 1))` with var 0 free)
		Overlay past;
		past.name = "var 0 past the step of " + call + " (" + a[stepAt].dump() + ")";
		past.questVar0 = a[stepAt].get<int32_t>() + 1;
		out.push_back(std::move(past));
		if (call == "checkQuestItems" || call == "checkQuestItemsSimple") {
			o.name += ", the collect items held";
			o.collectItems = true;
			out.push_back(std::move(o));
		}
		// P6-Q slice 2 (Q03): at its step, the item a step helper removes held one more than it takes (_14052's useQuestObject(env, 3, 4,
		// false, 0, 0, 1, 182215344, 1): a count the removal reads, which neither overlay alone reaches)
		size_t removeAt = call == "defaultCloseDialog" && a.size() == 6 ? 4 : (call == "defaultCloseDialog" || call == "useQuestObject") && a.size() == 8 ? 6 : 0;
		if (removeAt > 0 && a[removeAt].is_number_integer() && a[removeAt + 1].is_number_integer() && a[removeAt].get<int32_t>() != 0 &&
			a[removeAt + 1].get<int64_t>() > 0 && !namedItem(a[removeAt].get<int32_t>())) {
			Overlay withItem;
			withItem.name = "var 0 at the step of " + call + " (" + a[stepAt].dump() + "), the item it removes held";
			withItem.questVar0 = a[stepAt].get<int32_t>();
			withItem.items.emplace_back(a[removeAt].get<int32_t>(), a[removeAt + 1].get<int64_t>() + 1);
			out.push_back(std::move(withItem));
		}
	}
	// P6-Q slice 2 (Q03, Q10): a hook that reads no QuestState of its quest but calls a step helper (Q03: _1157's and _18602's movie hooks,
	// changeQuestStep(env, 1, 2); Q10: a kill or get-item hook that calls the helper straight away) or finishQuest (_1170's) with it has none in
	// its given, so the helper found none in either run: the quest started at the helper's step, or in REWARD for the finish (the path is the
	// same without it). A kill helper's step is its second argument (after the npcs); without a target in the given, the kill block above
	// already made this overlay with the helper's npc as the target
	if (!given.contains("questState")) {
		for (const json& e : c["effects"]) {
			const std::string call = e["call"];
			const json& a = e["args"];
			size_t stepAt = call == "defaultOnKillEvent" ? 1 : 0;
			bool stepHelper = call == "defaultCloseDialog" || call == "checkQuestItems" || call == "checkQuestItemsSimple" || call == "changeQuestStep" ||
				call == "useQuestObject" || call == "defaultOnGetItemEvent" || (call == "defaultOnKillEvent" && given.contains("target")) ||
				call == "defaultOnKillRankedEvent";
			if (stepHelper && a.size() > stepAt && a[stepAt].is_number_integer()) {
				Overlay o;
				o.name = "the quest started at the step of " + call + " (" + a[stepAt].dump() + ")";
				o.startVar0 = a[stepAt].get<int32_t>();
				out.push_back(std::move(o));
				// lane C: and at the kill-ranked helper's finishing kill, endVar - 1 (AbstractQuestHandler.java:772-778)
				if (call == "defaultOnKillRankedEvent" && a.size() >= 2 && a[1].is_number_integer() && a[1].get<int32_t>() - 1 > a[0].get<int32_t>()) {
					Overlay last;
					last.name = "the quest started at the last kill of " + call + " (" + std::to_string(a[1].get<int32_t>() - 1) + ")";
					last.startVar0 = a[1].get<int32_t>() - 1;
					out.push_back(std::move(last));
				}
			} else if (call == "QuestService.finishQuest") {
				Overlay o;
				o.name = "the quest in REWARD for QuestService.finishQuest";
				o.startVar0 = 0;
				o.seedStatus = "REWARD";
				out.push_back(std::move(o));
			}
		}
	}
	// P6-Q slice 2 (Q03, Q10): a start helper checks the quest's start conditions (QuestService.startQuest: checkStartConditions), which the case's
	// given does not name: the finished quests of <start_conditions> and the first permitted class (_1540's and _1626's QUEST_ACCEPT_1). Q10's
	// overlay of the same name (only for a quest with prerequisites and a path that names no other quest) is this one's subset
	for (const json& e : c["effects"]) {
		if (e["call"] == "sendQuestStartDialog" || e["call"] == "QuestService.startQuest") {
			Overlay o;
			o.name = "the start conditions' finished quests and class";
			o.prerequisites = true;
			o.classPermitted = !(given.contains("player") && given["player"].contains("class"));
			out.push_back(std::move(o));
			break;
		}
	}
	// the Q03 review: a sendQuestStartDialog path that reads the dialog action only through guards an accept action satisfies too (the oracle
	// picks USE_OBJECT for `!= QUEST_SELECT`) takes the same path with it: QUEST_ACCEPT_1, else (a path that excludes that one)
	// QUEST_ACCEPT_SIMPLE, either of which starts the quest and gives its work item (AbstractQuestHandler.java:373-398,
	// sendQuestStartDialog(env, itemId, itemCount)); with the start conditions' finished quests and class
	const char* acceptName = dialogGuardsAllow(c, "QUEST_ACCEPT_1") ? "QUEST_ACCEPT_1" : dialogGuardsAllow(c, "QUEST_ACCEPT_SIMPLE") ? "QUEST_ACCEPT_SIMPLE" : nullptr;
	if (hook == "onDialogEvent" && given.contains("dialogAction") && acceptName != nullptr) {
		for (const json& e : c["effects"]) {
			if (e["call"] == "sendQuestStartDialog") {
				Overlay o;
				o.name = std::string("dialog action ") + acceptName + " (the path's guards allow it), the start conditions' finished quests and class";
				o.dialogActionId = std::string_view(acceptName) == "QUEST_ACCEPT_1" ? DA::QUEST_ACCEPT_1 : DA::QUEST_ACCEPT_SIMPLE;
				o.prerequisites = true;
				o.classPermitted = !(given.contains("player") && given["player"].contains("class"));
				out.push_back(std::move(o));
				break;
			}
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
		// the real experience table (QuestHandlerTestSupport.h publishes its first 26 levels): a quester of a level past it throws at creation
		// ("The given level is higher than possible max", PlayerExperienceTable.getStartExpForLevel), and P6-Q slice 2's quests start at levels up to
		// 48 (Q03's Heiron) and 50 (Q10's altgard and pandaemonium)
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(contexts.emplace_back(), data.experienceXml));
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
		// lane C: a path that reads the quester's level has it in its given (extract.py player.level): every setup takes that level
		bool levelGiven = given.contains("player") && given["player"].contains("level");
		if (levelGiven)
			level = given["player"]["level"].get<int32_t>();
		// lane C: a quester exists only up to the experience table's last level (65: PlayerExperienceTable.getLevelForExp); the quests of
		// quest_data.xml with minlevel_permitted 99 (16940, 18910, 26940, 28910, 38006, 38007, 48006, 48007: not reachable in 4.8) run at it
		level = std::min(level, dataholders::DataManager::PLAYER_EXPERIENCE_TABLE->getMaxLevel() - 1);
		bool prerequisites = !row.finishedPrerequisites.empty();
		std::vector<CaseSetup> setups{{"own race, minimum level", own, level}};
		if (prerequisites)
			setups.push_back({"own race, minimum level, prerequisites finished", own, level, true});
		if (!row.collectItems.empty())
			setups.push_back({"own race, minimum level, prerequisites finished, collect items held", own, level, prerequisites, true});
		if (level > 1 && !levelGiven)
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
	 * is reseeded (RUN_SEED) and the clock is the fixture's ManualClock, which only a case that schedules a task advances. The review of #79
	 * (item 1): both runs step the clock through each delay of the case's schedules, with a snapshot after the hook, one millisecond before
	 * and at each delay (the timeline the runs compare), count the tasks still pending after the last delay (compared too: a task where Java
	 * schedules none stays pending), then drain the executor so that no task of this run reaches a later one.
	 */
	Outcome run(AbstractQuestHandler& handler, int32_t questId, const json& c, const CaseSetup& setup, const Overlay& overlay,
		const Ref<gameserver::model::gameobjects::Npc>& npc, bool replay) {
		Outcome out;
		const json& given = c["given"];
		Quester* quester = makeQuester(GOLDEN_PLAYER, "Golden", setup.race, std::max(setup.level + overlay.levelDelta, 1));
		gameserver::model::gameobjects::player::Player& player = quester->player();
		// the map the case names (extract.py: player.getWorldId(), compared with a WorldMapType id or a map id; 1100, 2100, 14050 and 18602 of
		// the table): the quester stands in the fixture's region as before, under that map id, which is what VisibleObject::getWorldId answers
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
		if (overlay.startVar0 && !player.getQuestStateList()->getQuestState(questId))
			seedQuestState(*quester, questId, json{{"status", overlay.seedStatus}, {"vars", {{"0", *overlay.startVar0}}}});
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
		// the review of #79, item 2: the item an item-use hook is called with is in the inventory, as in Java (an item can only be used from the
		// inventory), under the object id callHook hands the hook: the given stack of its item id, else one of its own
		const int32_t usedItem = c["hook"] == "onItemUseEvent" ? usedItemId(c) : 0;
		bool usedHeld = false;
		if (given.contains("inventory")) {
			for (const auto& [item, count] : given["inventory"].items()) {
				if (count.get<int64_t>() > 0) {
					bool used = std::stoi(item) == usedItem && !usedHeld;
					holdItem(*quester, used ? USED_ITEM_OBJECT : itemObjId++, std::stoi(item), count.get<int64_t>());
					usedHeld = usedHeld || used;
				}
			}
		}
		if (usedItem != 0 && !usedHeld && !named(usedItem)) // a given count 0 of the used item: a path Java cannot take with it held
			holdItem(*quester, USED_ITEM_OBJECT, usedItem, 1);
		Ptr<gameserver::model::gameobjects::VisibleObject> target = npc;
		if (target)
			player.setTarget(target);
		int32_t dialogActionId = given.contains("dialogAction") ? given["dialogAction"]["id"].get<int32_t>() : 0;
		if (overlay.dialogActionId)
			dialogActionId = *overlay.dialogActionId;
		Ref<QuestEnv> env = QuestEnv::create(target, player, overlay.envQuestId.value_or(questId), dialogActionId);
		// P6-Q slice 2 (Q03): the env's pre-quest continuation flag the path reads (extract.py `env.continuation`, _1626's QUEST_SELECT)
		if (given.contains("env") && given["env"].contains("continuation"))
			env->setDialogContinuationFromPreQuest(given["env"]["continuation"].get<bool>());
		// a COMPLETE state whose canRepeat the path read must answer it: a template with max_repeat_count 255 always repeats
		// (QuestState.java canRepeat), so such a path is dead Java code for that quest (P6-Q slice 2, Q03: _1687)
		if (given.contains("questState") && given["questState"].is_object() && given["questState"].contains("canRepeat")) {
			Ptr<QuestState> seeded = player.getQuestStateList()->getQuestState(questId);
			if (seeded && seeded->getStatus() == QuestStatus::COMPLETE && seeded->canRepeat() != given["questState"]["canRepeat"].get<bool>())
				out.replayError = "the quest template cannot make canRepeat " + given["questState"]["canRepeat"].dump() + " (max_repeat_count)";
		}
		std::map<int32_t, QuestStateRow> statesBefore = questStatesOf(player);
		std::map<int32_t, int64_t> inventoryBefore = inventoryOf(player);
		quester->clearSent();
		commons::utils::Rnd::seedCurrentThreadForTests(RUN_SEED);
		// the Q10 review: a run that reaches an unported engine body throws the same UnportedException in both runs, which the thrown comparison
		// passes; runDoc reports such a run instead (knownUnported)
		uint64_t unportedBefore = runtime::unportedHitCount();
		std::map<std::string, uint64_t> sitesBefore = unportedBefore == 0 ? std::map<std::string, uint64_t>() : unportedSites();
		out.helperResults.assign(c["effects"].size(), int8_t{-1});
		// tasks of the fixture or of an earlier run's quester that the drain left (a delay above an hour) are not this run's
		const int64_t pendingBefore = static_cast<int64_t>(executor->pendingTaskCount());
		try {
			if (out.replayError.empty()) { // else the state cannot hold the case: nothing runs
				if (replay)
					replayEffects(questId, c, *env, player, out, -1);
				else
					out.returned = callHook(handler, c, *env, player);
			}
		} catch (const runtime::NullPointerException& e) {
			out.thrown = "NullPointerException";
			out.thrownDetail = e.what();
		} catch (const std::exception& e) {
			out.thrown = std::string("C++: ") + e.what();
		}
		// lane C, and the review of #79 (items 1 and 6): the tasks run when the clock reaches their delays. The generated hook's are the
		// executor's; the replay runs the effects of each task (`task`) when the clock reaches its delay. A task's exception is the pool's in
		// both runs (ThreadPoolManager logs it, as Java's pool does): the task stops, the run goes on
		const auto snapshot = [&](int64_t at) {
			Snapshot shot{at, {}, questStatesOf(player), inventoryOf(player)};
			for (const std::vector<uint8_t>& packet : quester->sent())
				shot.opcodes.push_back(items::javaOpcodeOf(packet));
			out.timeline.push_back(std::move(shot));
		};
		if (out.replayError.empty()) {
			snapshot(0);
			int64_t elapsed = 0;
			for (const auto& [delay, schedules] : taskDelays(c)) {
				if (delay > 0) {
					if (delay - 1 > elapsed)
						executor->advance(std::chrono::milliseconds(delay - 1 - elapsed));
					snapshot(delay - 1);
					executor->advance(std::chrono::milliseconds(1));
				} else {
					executor->runReady();
				}
				elapsed = std::max<int64_t>(elapsed, delay);
				if (replay) {
					for (size_t scheduled : schedules) {
						try {
							replayEffects(questId, c, *env, player, out, static_cast<int64_t>(scheduled));
						} catch (const std::exception&) {
							// the pool's: the task stops at its exception (ThreadPoolManager, Java's ThreadPoolManager log and drop it)
						}
					}
				}
				snapshot(delay);
			}
			out.pendingTasks = static_cast<int64_t>(executor->pendingTaskCount()) - pendingBefore;
		}
		if (runtime::unportedHitCount() != unportedBefore) {
			for (const auto& [site, hits] : unportedSites()) {
				auto before = sitesBefore.find(site);
				if (before == sitesBefore.end() || before->second < hits)
					out.unported.insert(site);
			}
		}
		for (const std::vector<uint8_t>& packet : quester->sent()) {
			int32_t opcode = items::javaOpcodeOf(packet);
			out.opcodes.push_back(opcode);
			// lane C: the item use animation of the item-use hooks and their tasks too (its object ids are the same in both runs)
			if (opcode == OPCODE_DIALOG || opcode == OPCODE_QUEST_ACTION || opcode == OPCODE_MOVIE || opcode == items::SM_ITEM_USAGE_ANIMATION_OPCODE)
				out.keyPackets.push_back(packet);
		}
		out.questStates = questStatesOf(player);
		out.inventory = inventoryOf(player);
		out.observable = !out.opcodes.empty() || out.questStates != statesBefore || out.inventory != inventoryBefore;
		drainTasks();
		dropQuester(quester);
		return out;
	}

	/**
	 * The review of #79, item 1: runs every task left in the executor (in due order, at most an hour of clock and 1,000 steps: a periodic task
	 * stops there), after the run's outcome was taken and before its quester is dropped, so that no task of one run reaches a later run
	 */
	void drainTasks() {
		for (int32_t step = 0; step < 1000 && executor->pendingTaskCount() > 0; step++) {
			std::optional<std::chrono::steady_clock::time_point> next = executor->nextDueTime();
			if (!next) {
				executor->runReady();
				break;
			}
			auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(*next - clock.now());
			if (wait > std::chrono::hours(1))
				break;
			executor->advance(std::max(wait, std::chrono::milliseconds(0)));
		}
	}

	/**
	 * The zone argument of a zone hook: the case's, or (a path that reads no zone, P6-Q slice 2, Q03: _1607's var guard comes first) another.
	 * Lane C: the case's one zone argument whatever the Java parameter is called (`zoneName`, beluslan/_24051's `name`)
	 */
	static const world::zone::ZoneName* zoneArg(const json& args) {
		const std::string zone = zoneNameArg(args);
		return !zone.empty() ? world::zone::ZoneName::get(zone) : world::zone::ZoneName::createOrGet("GOLDEN_OTHER_ZONE");
	}

	/** The zone a case's arguments name (a string; `{anyExcept: [...]}` names none), else "" */
	static std::string zoneNameArg(const json& args) {
		for (const auto& [name, value] : args.items()) {
			if (value.is_string())
				return value.get<std::string>();
		}
		return "";
	}

	/** The delays of the case's ThreadPoolManager.schedule effects in order, each with its schedule effects in schedule order (lane C) */
	static std::map<int64_t, std::vector<size_t>> taskDelays(const json& c) {
		std::map<int64_t, std::vector<size_t>> out;
		const json& effects = c["effects"];
		for (size_t k = 0; k < effects.size(); k++) {
			const json& e = effects[k];
			if (e["call"] == "ThreadPoolManager.schedule" && e["args"].size() == 1 && e["args"][0].is_number_integer())
				out[e["args"][0].get<int64_t>()].push_back(k);
		}
		return out;
	}

	/** The item id an item-use case uses: the item its path reads, else (0) one no guard names, else the quest's registered item */
	int32_t usedItemId(const json& c) const {
		const json& args = c["given"].contains("args") ? c["given"]["args"] : json::object();
		int32_t itemId = args.value("itemId", 0);
		// P6-Q slice 2 (Q03, Q10): a hook that reads the item's id has it as given.item.itemId (extract.py); 0 is an item no guard names
		// (Q03 used the fixture's kinah for it, Q10 OTHER_ITEM; such a path has no effect in either chunk's traces)
		if (c["given"].contains("item")) {
			itemId = c["given"]["item"].value("itemId", 0);
			if (itemId == 0)
				itemId = OTHER_ITEM;
		}
		return itemId != 0 ? itemId : registeredQuestItem;
	}

	/** An object id of the oracle's effects (lane C): the quester's, the used item's (callHook's item), the target's, or a number */
	static int32_t objectIdOf(const json& v, QuestEnv& env, gameserver::model::gameobjects::player::Player& player) {
		if (v == "$playerObjectId")
			return player.getObjectId();
		if (v == "$itemObjectId")
			return USED_ITEM_OBJECT;
		if (v == "$targetObjectId")
			return env.getVisibleObject()->getObjectId();
		return v.get<int32_t>();
	}

	/** The int argument of a hook (lane C): the case's one int argument, else 0 */
	static int32_t intArg(const json& args) {
		for (const auto& [name, value] : args.items()) {
			if (value.is_number_integer())
				return value.get<int32_t>();
		}
		return 0;
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
		if (hook == "onEnterZoneEvent")
			return handler.onEnterZoneEvent(env, zoneArg(args));
		// lane C (phase 6 step 1): the hooks of the corpus the harness did not drive (AbstractQuestHandler.java: onLeaveZoneEvent :127,
		// onUseSkillEvent :143, onInvisibleTimerEndEvent :196, onKillRankedEvent :204, onKillInWorldEvent :208, onFailCraftEvent :216,
		// onEnterWindStreamEvent :265, onDredgionRewardEvent :273). An int argument is the case's one hook argument, whatever the Java parameter is
		// called (skillUsedId, itemId, teleportId); a path that reads none gets 0, which no guard names
		if (hook == "onLeaveZoneEvent")
			return handler.onLeaveZoneEvent(env, zoneArg(args));
		if (hook == "onKillRankedEvent")
			return handler.onKillRankedEvent(env);
		if (hook == "onKillInWorldEvent")
			return handler.onKillInWorldEvent(env);
		if (hook == "onDredgionRewardEvent")
			return handler.onDredgionRewardEvent(env);
		if (hook == "onInvisibleTimerEndEvent")
			return handler.onInvisibleTimerEndEvent(env);
		if (hook == "onUseSkillEvent")
			return handler.onUseSkillEvent(env, intArg(args));
		if (hook == "onFailCraftEvent")
			return handler.onFailCraftEvent(env, intArg(args));
		if (hook == "onEnterWindStreamEvent")
			return handler.onEnterWindStreamEvent(env, intArg(args));
		// P6-Q slice 2 (Q03, Q10): the hooks the chunks' handlers add; each takes the env only (AbstractQuestHandler.java)
		if (hook == "onLogOutEvent")
			return handler.onLogOutEvent(env);
		if (hook == "onQuestTimerEndEvent")
			return handler.onQuestTimerEndEvent(env);
		if (hook == "onAtDistanceEvent")
			return handler.onAtDistanceEvent(env);
		if (hook == "onGetItemEvent")
			return handler.onGetItemEvent(env);
		if (hook == "onDieEvent")
			return handler.onDieEvent(env);
		if (hook == "onItemUseEvent") {
			// the review of #79, item 2: the item held in the inventory (run), else (a given count 0 of it) one outside it
			if (Ptr<gameserver::model::gameobjects::Item> held = player.getInventory().getItemByObjId(USED_ITEM_OBJECT))
				return std::string(resultName(handler.onItemUseEvent(env, *held)));
			Ref<gameserver::model::gameobjects::Item> item =
				items::loadedItem(USED_ITEM_OBJECT, usedItemId(c), 1, gameserver::model::items::storage::StorageType::CUBE);
			return std::string(resultName(handler.onItemUseEvent(env, *item)));
		}
		throw std::runtime_error("hook " + hook + " is not driven by the harness");
	}

	/**
	 * The Java effects of the case, in order, on the real helpers: the hook's own (`task` -1), or those of the task the ThreadPoolManager.schedule
	 * effect `task` scheduled (lane C; extract.py run_tasks marks them with `task`), which run() replays when the clock reaches the task's delay
	 */
	void replayEffects(int32_t questId, const json& c, QuestEnv& env, gameserver::model::gameobjects::player::Player& player, Outcome& out,
		int64_t task) {
		PlainHandler plain(questId);
		auto qs = [&] { return player.getQuestStateList()->getQuestState(questId); };
		const json& effects = c["effects"];
		auto replay = [&](size_t k) -> bool {
			const json& e = effects[k];
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
			} else if (call == "defaultOnQuestCompletedEvent" && a.size() <= 8) {
				// Java varargs; the C++ helper takes an initializer_list (AbstractQuestHandler.h)
				std::vector<int32_t> q = a.get<std::vector<int32_t>>();
				size_t n = q.size();
				q.resize(8, 0);
				switch (n) {
					case 0: result = plain.defaultOnQuestCompletedEvent(env); break;
					case 1: result = plain.defaultOnQuestCompletedEvent(env, {q[0]}); break;
					case 2: result = plain.defaultOnQuestCompletedEvent(env, {q[0], q[1]}); break;
					case 3: result = plain.defaultOnQuestCompletedEvent(env, {q[0], q[1], q[2]}); break;
					case 4: result = plain.defaultOnQuestCompletedEvent(env, {q[0], q[1], q[2], q[3]}); break;
					case 5: result = plain.defaultOnQuestCompletedEvent(env, {q[0], q[1], q[2], q[3], q[4]}); break;
					case 6: result = plain.defaultOnQuestCompletedEvent(env, {q[0], q[1], q[2], q[3], q[4], q[5]}); break;
					// lane C: the corpus outside the tree has lists of 7 (ishalgen/_2007) and up to 8
					case 7: result = plain.defaultOnQuestCompletedEvent(env, {q[0], q[1], q[2], q[3], q[4], q[5], q[6]}); break;
					default: result = plain.defaultOnQuestCompletedEvent(env, {q[0], q[1], q[2], q[3], q[4], q[5], q[6], q[7]}); break;
				}
			} else if (call == "defaultOnLevelChangedEvent" && a.size() <= 8) {
				std::vector<int32_t> q = a.get<std::vector<int32_t>>();
				size_t n = q.size();
				q.resize(8, 0);
				switch (n) {
					case 0: result = plain.defaultOnLevelChangedEvent(player); break;
					case 1: result = plain.defaultOnLevelChangedEvent(player, {q[0]}); break;
					case 2: result = plain.defaultOnLevelChangedEvent(player, {q[0], q[1]}); break;
					case 3: result = plain.defaultOnLevelChangedEvent(player, {q[0], q[1], q[2]}); break;
					case 4: result = plain.defaultOnLevelChangedEvent(player, {q[0], q[1], q[2], q[3]}); break;
					case 5: result = plain.defaultOnLevelChangedEvent(player, {q[0], q[1], q[2], q[3], q[4]}); break;
					case 6: result = plain.defaultOnLevelChangedEvent(player, {q[0], q[1], q[2], q[3], q[4], q[5]}); break;
					// lane C: the corpus outside the tree has lists of 7 (ishalgen/_2007) and up to 8
					case 7: result = plain.defaultOnLevelChangedEvent(player, {q[0], q[1], q[2], q[3], q[4], q[5], q[6]}); break;
					default: result = plain.defaultOnLevelChangedEvent(player, {q[0], q[1], q[2], q[3], q[4], q[5], q[6], q[7]}); break;
				}
			}
			// P6-Q slice 2: the helper overloads the verteron and heiron handlers (Q03) and the altgard and pandaemonium ones (Q10) add, picked by the
			// argument kinds of the Java call (AbstractQuestHandler.java; a JSON bool is a Java boolean argument, an array an int[] or varargs). A
			// useQuestObject whose die argument is true never gets here (runDoc: killsTarget)
			else if (call == "sendQuestStartDialog" && a.size() == 2)
				result = plain.sendQuestStartDialog(env, a[0].get<int32_t>(), a[1].get<int64_t>());
			else if (call == "sendQuestEndDialog" && a.size() == 1 && a[0].is_array()) {
				std::vector<int32_t> removed = a[0].get<std::vector<int32_t>>();
				result = plain.sendQuestEndDialog(env, std::span<const int32_t>(removed));
			} else if (call == "super.onDialogEvent" && a.empty())
				result = plain.onDialogEvent(env); // PlainHandler keeps AbstractQuestHandler's own onDialogEvent
			else if (call == "QuestService.abandonQuest" && a.size() == 1)
				result = services::QuestService::abandonQuest(player, a[0].get<int32_t>());
			else if (call == "changeQuestStep" && a.size() == 2)
				result = plain.changeQuestStep(env, a[0].get<int32_t>(), a[1].get<int32_t>());
			else if (call == "changeQuestStep" && a.size() == 4)
				result = plain.changeQuestStep(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<bool>(), a[3].get<int32_t>());
			else if (call == "checkQuestItems" && a.size() == 7)
				result = plain.checkQuestItems(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<bool>(), a[3].get<int32_t>(), a[4].get<int32_t>(),
					a[5].get<int32_t>(), a[6].get<int32_t>());
			else if (call == "defaultCloseDialog" && a.size() == 6)
				result = plain.defaultCloseDialog(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<int32_t>(), a[3].get<int64_t>(),
					a[4].get<int32_t>(), a[5].get<int64_t>());
			else if (call == "defaultCloseDialog" && a.size() == 8 && a[2].is_boolean())
				result = plain.defaultCloseDialog(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<bool>(), a[3].get<bool>(), a[4].get<int32_t>(),
					a[5].get<int64_t>(), a[6].get<int32_t>(), a[7].get<int64_t>());
			else if (call == "useQuestObject" && a.size() == 10)
				result = plain.useQuestObject(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<bool>(), a[3].get<int32_t>(), a[4].get<int32_t>(),
					a[5].get<int32_t>(), a[6].get<int32_t>(), a[7].get<int32_t>(), a[8].get<int32_t>(), a[9].get<bool>());
			else if (call == "defaultOnKillEvent" && a.size() == 3 && a[0].is_number_integer() && a[2].is_boolean())
				result = plain.defaultOnKillEvent(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<bool>());
			else if (call == "defaultOnKillEvent" && a.size() == 3 && a[0].is_number_integer())
				result = plain.defaultOnKillEvent(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<int32_t>());
			else if (call == "defaultOnKillEvent" && a.size() == 4 && a[0].is_number_integer() && a[2].is_boolean())
				result = plain.defaultOnKillEvent(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<bool>(), a[3].get<int32_t>());
			else if (call == "defaultOnKillEvent" && a.size() == 4 && a[0].is_number_integer())
				result = plain.defaultOnKillEvent(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<int32_t>(), a[3].get<int32_t>());
			else if (call == "defaultOnKillEvent" && a.size() == 4 && a[0].is_array() && a[2].is_boolean()) {
				std::vector<int32_t> npcIds = a[0].get<std::vector<int32_t>>();
				result = plain.defaultOnKillEvent(env, std::span<const int32_t>(npcIds), a[1].get<int32_t>(), a[2].get<bool>(), a[3].get<int32_t>());
			} else if (call == "defaultOnKillEvent" && a.size() == 4 && a[0].is_array()) {
				std::vector<int32_t> npcIds = a[0].get<std::vector<int32_t>>();
				result = plain.defaultOnKillEvent(env, std::span<const int32_t>(npcIds), a[1].get<int32_t>(), a[2].get<int32_t>(), a[3].get<int32_t>());
			} else if (call == "useQuestObject" && a.size() == 4 && a[3].is_boolean() && !a[3].get<bool>())
				result = plain.useQuestObject(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<bool>(), false);
			else if (call == "useQuestObject" && a.size() == 4 && a[3].is_boolean()) {
				// dieObject: `npc.getController().die(player)` on the target (AbstractQuestHandler.java:911-916). The fixture's npc is not spawned
				// (no map region: NpcController.onDie throws), and both runs share it, so the second run would find it dead: not modelled
				out.replayError = "the replay does not model useQuestObject's dieObject on the fixture's unspawned npc " + a.dump();
				return false;
			}
			else if (call == "useQuestObject" && a.size() == 4)
				result = plain.useQuestObject(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<bool>(), a[3].get<int32_t>());
			else if (call == "useQuestObject" && a.size() == 8)
				result = plain.useQuestObject(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<bool>(), a[3].get<int32_t>(), a[4].get<int32_t>(),
					a[5].get<int32_t>(), a[6].get<int32_t>(), a[7].get<int32_t>());
			else if (call == "defaultOnGetItemEvent" && a.size() == 3)
				result = plain.defaultOnGetItemEvent(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<bool>());
			else if (call == "PacketSendUtility.sendPacket" && a.size() == 1 && a[0].value("new", std::string()) == "SM_DIALOG_WINDOW") {
				const json& p = a[0]["args"];
				int32_t targetObjectId =
					p[0] == "$targetObjectId" ? env.getVisibleObject()->getObjectId() : p[0].get<int32_t>();
				utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_DIALOG_WINDOW(targetObjectId, p[1].get<int32_t>()));
			}
			// lane C (phase 6 step 1): the effects of the hooks and closures the harness drives since (extract.py: ThreadPoolManager.schedule,
			// PacketSendUtility.broadcastPacket, inventory.decreaseByObjectId), the kill-ranked helper (AbstractQuestHandler.java:749-787) and the
			// helper overloads of the corpus outside the tree (sendQuestRewardDialog :1151, checkItemExistence :576-609, defaultCloseDialog with
			// a given item, the chain helpers' longer pre-quest lists)
			else if (call == "ThreadPoolManager.schedule" && a.size() == 1 && a[0].is_number_integer()) {
				result = true; // run() replays the task's effects at its delay
			} else if (call == "PacketSendUtility.broadcastPacket" && a.size() == 2 && a[0].value("new", std::string()) == "SM_ITEM_USAGE_ANIMATION" &&
				a[0]["args"].size() == 6) {
				const json& p = a[0]["args"];
				utils::PacketSendUtility::broadcastPacket(player,
					network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION(objectIdOf(p[0], env, player), objectIdOf(p[1], env, player), p[2].get<int32_t>(),
						p[3].get<int32_t>(), p[4].get<int32_t>(), p[5].get<int32_t>()),
					a[1].get<bool>());
			} else if (call == "inventory.decreaseByObjectId" && a.size() == 2)
				result = player.getInventory().decreaseByObjectId(objectIdOf(a[0], env, player), a[1].get<int64_t>());
			else if (call == "defaultOnKillRankedEvent" && a.size() == 3)
				result = plain.defaultOnKillRankedEvent(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<bool>());
			else if (call == "defaultOnKillRankedEvent" && a.size() == 4)
				result = plain.defaultOnKillRankedEvent(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<bool>(), a[3].get<bool>());
			else if (call == "sendQuestRewardDialog" && a.size() == 2)
				result = plain.sendQuestRewardDialog(env, a[0].get<int32_t>(), a[1].get<int32_t>());
			else if (call == "checkItemExistence" && a.size() == 3)
				result = plain.checkItemExistence(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<bool>());
			else if (call == "checkItemExistence" && a.size() == 10)
				result = plain.checkItemExistence(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<bool>(), a[3].get<int32_t>(), a[4].get<int32_t>(),
					a[5].get<bool>(), a[6].get<int32_t>(), a[7].get<int32_t>(), a[8].get<int32_t>(), a[9].get<int32_t>());
			else if (call == "defaultCloseDialog" && a.size() == 4 && a[2].is_number_integer())
				result = plain.defaultCloseDialog(env, a[0].get<int32_t>(), a[1].get<int32_t>(), a[2].get<int32_t>(), a[3].get<int64_t>());
			else {
				out.replayError = "the replay does not model " + call + " " + a.dump();
				return false;
			}
			out.helperResults[k] = result ? 1 : 0;
			return true;
		};
		for (size_t k = 0; k < effects.size(); k++) {
			if (effects[k].value("task", int64_t{-1}) == task && !replay(k))
				return;
		}
		if (task < 0 && c.contains("returns"))
			out.returned = expectedReturn(c["returns"], out.helperResults);
	}

	static json expectedReturn(const json& returns, const std::vector<int8_t>& results) {
		if (returns.is_object() && returns.contains("resultOf")) {
			size_t k = returns["resultOf"].get<size_t>();
			if (k >= results.size())
				return json("resultOf out of range");
			return results[k] < 0 ? json("resultOf an effect that did not run") : json(results[k] == 1);
		}
		if (returns.is_object() && returns.contains("fromBoolean")) {
			json inner = expectedReturn(returns["fromBoolean"], results);
			if (!inner.is_boolean())
				return inner;
			return std::string(inner.get<bool>() ? "SUCCESS" : "FAILED");
		}
		return returns;
	}

	static constexpr const char* KILLS_TARGET = "useQuestObject kills the target npc, which the pair of runs cannot share";

	/** True if an effect of the case kills its target npc: useQuestObject with its die argument (AbstractQuestHandler.java useQuestObject) */
	static bool killsTarget(const json& c) {
		for (const json& e : c["effects"]) {
			const json& a = e["args"];
			if (e["call"] == "useQuestObject" && !a.empty() && a.back().is_boolean() && a.back().get<bool>() && (a.size() == 4 || a.size() == 10))
				return true;
		}
		return false;
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
		std::set<std::string> failedChecks;       // "thrown" "returned" "packets" "opcodes" "questStates" "inventory" "timeline" "pendingTasks"
		std::map<std::string, std::set<std::string>> unported; // variant id -> the AION_UNPORTED sites a compared run of it reached
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
		// P6-Q slice 2 (Q03): the zones the document names exist before the handler registers, as the server's zone data makes them
		// (ZoneName.get answers NONE for a name no zone created, so _1607's four `zoneName == ZoneName.get(...)` would all hold)
		// lane C: a register() the oracle refuses is an object with the reason ({"unsupported": ...}), no list
		const json registrations = doc["register"].is_array() ? doc["register"] : json::array();
		for (const json& reg : registrations) {
			const std::string call = reg.value("call", std::string());
			if ((call == "registerOnEnterZone" || call == "registerOnLeaveZone") && reg["args"][0].is_string())
				world::zone::ZoneName::createOrGet(reg["args"][0].get<std::string>());
		}
		for (const json& c : doc["cases"]) {
			const json& given = c["given"];
			if (given.contains("args") && !zoneNameArg(given["args"]).empty())
				world::zone::ZoneName::createOrGet(zoneNameArg(given["args"]));
		}
		AbstractQuestHandler& handler = registerGenerated(questId, factory);
		registeredQuestItem = 0;
		for (const json& reg : registrations) {
			if (reg.value("call", std::string()) == "registerQuestItem")
				registeredQuestItem = reg["args"][0].get<int32_t>();
		}
		for (const json& base : doc["cases"]) {
			bool anyObservable = false;
			bool anyReproduced = false; // a case no setup reproduces is listed as such, not also as vacuous (P6-Q slice 2, Q03)
			bool killed = false; // not run (killsTarget): listed as not reproducible, not as vacuous
			bool hasEffects = !base["effects"].empty();
			for (const json& c : variantsOf(base)) {
				const std::string id = c["id"];
				SCOPED_TRACE(std::to_string(questId) + " " + id);
				bool ok = true;
				bool reproduced = false;
				std::string why;
				if (killsTarget(c)) {
					killed = true;
					// P6-Q slice 2 (Q10): useQuestObject's die kills the target, and the fixture's npc stands in no world, so its death throws in
					// NpcController.onDie and leaves it dead: the second run of the pair cannot start from the first one's state, and a fresh npc
					// would change the object id the movie packet names. Listed in knownNotReproducible; covered by tests/quest_handlers_asmodae
					tally.unsatisfiable.push_back(id + ": " + KILLS_TARGET);
					continue;
				}
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
								// the review of #79, item 5: an effect that threw or never ran (-1) satisfies neither result
								if (k >= replayed.helperResults.size() || replayed.helperResults[k] != (assumption["returns"].get<bool>() ? 1 : 0))
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
					// the case as given, or (P6-Q slice 2, Q03) a target overlay of a path that reads no target: a kill hook that returns
					// defaultOnKillEvent(env, mobs, ...) without a guard of its own gets the helper's assumed result only with one of its npcs
					// (and a var 0 overlay of a path that leaves var 0 free: a step helper's assumed result depends on the var it reads itself)
					if (overlay.name.empty() || (overlay.targetNpcId && !c["given"].contains("target")) || overlay.questVar0)
						reproduced = true;
					tally.runs++;
					anyObservable = anyObservable || expected.observable;
					Outcome actual = run(handler, questId, c, *chosen, overlay, npc, false);
					// a pending-task count that differs once may be an engine singleton's first use (its periodic task starts in whichever run
					// reaches it first: the first item given in a process): the pair runs again, and the second pair is the one compared
					if (actual.pendingTasks != expected.pendingTasks) {
						expected = run(handler, questId, c, *chosen, overlay, npc, true);
						actual = run(handler, questId, c, *chosen, overlay, npc, false);
					}
					// Java's exception on the path, else one a helper throws in this state (the replay runs the helpers: sendQuestEndDialog's
					// `(Npc) env.getVisibleObject()` after a finish with no target, AbstractQuestHandler.java:423-424)
					std::string expectedThrow = c.contains("throws") ? c["throws"].get<std::string>() : expected.thrown;
					std::string where = std::to_string(questId) + " " + id + " (setup: " + chosen->name +
						(overlay.name.empty() ? "" : "; overlay: " + overlay.name) + "): ";
					// each failure names its check in brackets (goldensample.py counts them by name; the review of #79, item 9)
					auto check = [&](bool condition, const std::string& name, const std::string& what) {
						if (!condition) {
							ok = false;
							tally.failedChecks.insert(name);
							if (report)
								ADD_FAILURE() << where << "[" << name << "] " << what;
						}
					};
					check(actual.thrown == expectedThrow, "thrown",
						"thrown '" + actual.thrown + "' (" + actual.thrownDetail + "), Java '" + expectedThrow + "'");
					if (expectedThrow.empty())
						check(actual.returned == expected.returned, "returned", "returned " + actual.returned.dump() + ", Java " + expected.returned.dump());
					check(actual.keyPackets == expected.keyPackets, "packets", "the dialog, quest action, movie and item use packets differ");
					check(actual.opcodes == expected.opcodes, "opcodes", "the packet opcode sequence differs");
					check(actual.questStates == expected.questStates, "questStates", "the quest states afterwards differ");
					check(actual.inventory == expected.inventory, "inventory", "the inventory afterwards differs");
					// the review of #79, item 1: what each run looked like before and at each task delay, and the tasks left after the last one
					check(actual.timeline == expected.timeline, "timeline",
						"the task timeline differs (a task ran before or after its delay, or the hook ran it inline)");
					check(actual.pendingTasks == expected.pendingTasks, "pendingTasks",
						"tasks left after the last delay: " + std::to_string(actual.pendingTasks) + ", Java " + std::to_string(expected.pendingTasks));
					if (!expected.unported.empty() || !actual.unported.empty()) {
						std::set<std::string>& sites = tally.unported[id];
						sites.insert(expected.unported.begin(), expected.unported.end());
						sites.insert(actual.unported.begin(), actual.unported.end());
					}
				}
				if (!reproduced) {
					tally.unsatisfiable.push_back(id + ": " + why);
					continue;
				}
				anyReproduced = true;
				(ok ? tally.passed : tally.failed)++;
			}
			if (hasEffects && anyReproduced && !anyObservable && !killed)
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
			for (const auto& [id, sites] : tally.unported) {
				for (const std::string& site : sites)
					std::cout << "[golden]   reaches AION_UNPORTED: " << id << ": " << site << "\n";
			}
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
	// The first run of this harness found 9 cases no setup reproduces: the oracle took a helper's result as free when a guard branched on it,
	// and wrote paths Java cannot take (giveQuestItem of a non-zero item and count "-> false", AbstractQuestHandler.java:626-641;
	// collectItemCheck(env, true) "-> true" without a QuestState, QuestService.java:557-561). tools/oracle/questtrace/extract.py
	// (dead_assumption) now drops such paths, and the expected traces were regenerated without them.
	//
	// P6-Q slice 2, chunk Q03 (verteron, heiron). Ten, each a state the fixture cannot hold:
	// - 1612 onDialogEvent#8-#11: useQuestObject(env, step, next, reward, true) kills the target (AbstractQuestHandler.java:911-916,
	//   npc.getController().die(player)). The fixture's npc is not spawned (NpcController.onDie throws without a map region) and both runs share
	//   it, so the second run would find it dead. Since the integration of slice 2 runDoc skips such a case before any run (Q10's killsTarget);
	//   Q03's replay refused dieObject before. The dialog pages of the same hook are compared; tests/quest_handlers_q03 (LepharistSecretsTest)
	//   covers the path on a spawned npc.
	// - 1687 onDialogEvent#7 #23, 80217-80220 onDialogEvent#9: a COMPLETE state whose canRepeat is false. The templates have max_repeat_count
	//   255 and are not time based (quest_data.xml), so QuestState.canRepeat is always true for them (QuestState.java canRepeat): paths Java
	//   cannot take for these quests (the oracle reads no template and treats canRepeat as free).
	//
	// P6-Q slice 2, chunk Q10 (altgard, pandaemonium), 2026-09-29:
	// - 24012 onDialogEvent#12 and its high end: useQuestObject's die kills the target (KILLS_TARGET, runDoc); the path is covered by
	//   tests/quest_handlers_asmodae on a spawned npc. (2223 onDialogEvent#21, the same kind, left with 2223, held back at the integration of
	//   slice 2: GoldenHandlers.h.)
	// - (4210 onKillEvent#1, #2 were listed here: the path reads neither a QuestState nor the target (`defaultOnKillEvent(env, 215056, 0, 1, 1) ||
	//   defaultOnKillEvent(env, 215080, 0, 1, 2)`, _4210MissingHaorunerk.java:73), so no setup of the case as given makes a kill helper return
	//   the assumed true. Since the integration of slice 2 Q03's rule counts a target overlay of a path that reads no target as reproducing the
	//   case (runDoc), and the kill overlay "target 215056, var 0 at the start var 0" does: both pass.)
	static const std::set<std::string> known{
		"1612 onDialogEvent#8",
		"1612 onDialogEvent#9",
		"1612 onDialogEvent#10",
		"1612 onDialogEvent#11",
		"1687 onDialogEvent#7",
		"1687 onDialogEvent#23",
		"80217 onDialogEvent#9",
		"80218 onDialogEvent#9",
		"80219 onDialogEvent#9",
		"80220 onDialogEvent#9",
		"24012 onDialogEvent#12",
		"24012 onDialogEvent#12@questState.vars.0=4",
		// Phase 6 step 2, chunk Q08 (lane C, 2026-10-05): _21460AShulacksStory.java:65 `if (removeQuestItem(env, 182209520, 1))` at
		// SELECT_QUEST_REWARD: the path reads no count of the item (the oracle's given holds none), so removeQuestItem cannot answer true in a
		// setup of the case as given; the overlay that holds the item (overlaysFor, "the items the helpers remove or count held") runs and
		// compares the path, which does not count as reproducing the case as given
		"21460 onDialogEvent#20",
		// Chunk Q01 (lane C, 2026-10-05): a COMPLETE state whose canRepeat is false, which these templates cannot make (max_repeat_count
		// 255, quest_data.xml; QuestState.java canRepeat), as 1687 above: paths Java cannot take for these quests
		"1718 onDialogEvent#7",
		"1718 onDialogEvent#30",
		"1718 onItemUseEvent#3",
		"2718 onDialogEvent#7",
		"2718 onDialogEvent#50",
		"2718 onItemUseEvent#3",
		"3718 onDialogEvent#3",
		"3718 onDialogEvent#13",
		"4718 onDialogEvent#3",
		"4718 onDialogEvent#13",
		// Chunk Q02 (lane C, 2026-10-05). 11006 onItemUseEvent#1 #2: useQuestItem, which the replay does not model (its Runnable needs the
		// used item in the inventory and a delay of its own). 11289 #5 #13 and onItemUseEvent#4: canRepeat false, which the template cannot
		// make (as above). 11289 #14 and 11460 #20: a helper assumed true (checkItemExistence, removeQuestItem) that needs an item the path's
		// given does not hold (as 21460 #20); the overlay that holds it runs and compares the path
		"11006 onItemUseEvent#1",
		"11006 onItemUseEvent#2",
		"11289 onDialogEvent#5",
		"11289 onDialogEvent#13",
		"11289 onItemUseEvent#4",
		"11289 onDialogEvent#14",
		"11460 onDialogEvent#20",
	};
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
	// P6-Q slice 2. Both reviews of 2026-09-29 emptied the reason "sendQuestStartDialog: no branch of its switch takes USE_OBJECT, which the
	// path read" (AbstractQuestHandler.java:373-397) for the cases the harness runs: Q03's accept overlay (the accept action the path's guards
	// allow, overlaysFor) and Q10's free-dialog overlays (extract.py dialogExcludes: each action the start and end helpers act on) run those
	// paths with an action the helper acts on. Its last two rows, 1100 #4 and 2100 #5, were measured before those overlays while 1100 and 2100
	// were held back; with P6-Q prologue landed on slice 2 (C++ 51ef338e4) both are observable, so the reason is gone. Q10's
	// overlays also made 7 slice-1 IDLE_END cases observable (1005 #37, 1111 #9, 1123 #7, 2001 #18, 2006 #16, 2106 #28, 2122 #27). The four
	// 2114 cases "QuestService.startQuest assumed false" are observable since the start-condition overlay (both lanes found it): with the
	// prerequisites finished only the quest-list-full setup still refuses the start, and the refusal's message is compared.
	static const std::string REWARD_PAGE = "sendQuestDialog of a reward page (SELECT_QUEST_REWARD_WINDOW1, 5) outside REWARD sends nothing: the "
		"reward packet exploitation fix (AbstractQuestHandler.java:330-340)";
	static const std::string ENV_ONLY = "env.setQuestId changes the env only, which no run observes (QuestEnv.java:71-73)";
	static const std::string STEP_FALSE = "changeQuestStep assumed false: it changes nothing then (AbstractQuestHandler.java:307-327)";
	static const std::string KILL_FALSE = "defaultOnKillEvent assumed false (every call of the path): it changes nothing then "
		"(AbstractQuestHandler.java:672-747)";
	static const std::string KILL_NO_TARGET = "defaultOnKillEvent with the path's target none (the path reads the target through != guards "
		"only, which the oracle satisfies with no target): no npc of its list is the target, nothing changes (AbstractQuestHandler.java:726-747)";
	static const std::string STEP_NOT_MET = "defaultCloseDialog at a var the path read that is not the helper's step: it does nothing then "
		"(AbstractQuestHandler.java:486-529)";
	static const std::string REMOVE_FALSE_REWARD_PAGE = "removeQuestItem assumed false changes nothing (AbstractQuestHandler.java:644-659), and "
		"sendQuestDialog of the reward page 5 outside REWARD sends nothing (AbstractQuestHandler.java:330-340)";
	// chunk Q02: _11001KindMeira.java:129 and _11008LetterOfEncouragement.java:100 name their own quest as the pre-quest, which must be
	// COMPLETE while the quest has no state (a non-mission returns at once, AbstractQuestHandler.java:988-1004): the level hook never starts
	// them (a Java bug kept, docs/deviations/Q02.md)
	static const std::string ITEM_CHECK_FALSE = "checkItemExistence assumed false changes nothing (AbstractQuestHandler.java:576-609)";
	static const std::string KILLS_ASSUMED_FALSE = "every kill helper of the path assumed false: it changes nothing then (AbstractQuestHandler.java "
		"defaultOnKillEvent)";
	static const std::map<std::string, std::string> known = [] {
		std::map<std::string, std::string> rows{
		{"1107 onDialogEvent#4", IDLE_END},
		{"1107 onDialogEvent#5", IDLE_END},
		{"2125 onDialogEvent#13", IDLE_END},
		{"2125 onDialogEvent#14", IDLE_END},
		{"2135 onDialogEvent#6", IDLE_END},
		// chunk Q03 (verteron, heiron; P6-Q slice 2). The integration of slice 2 made 15 of Q03's 51 rows observable (removed): Q03's traces
		// carry dialogExcludes since the merged oracle regenerated them, and Q10's free-dialog overlays run those paths with the actions the
		// end helper acts on (13 IDLE_END: 1157 #10, 1158 #16, 1194 #11, 1559 #14, 1636 #26, 1692 #20, 14011 #13, 14054 #21, 18600 #11,
		// 80217-80220 #12) and the ones AbstractQuestHandler.onDialogEvent acts on (the kind SUPER_IDLE, gone: 1605 #9 #15)
		{"1163 onDialogEvent#10", IDLE_END},
		{"1163 onDialogEvent#13", IDLE_END},
		{"1183 onDialogEvent#15", IDLE_END},
		{"1192 onDialogEvent#13", IDLE_END},
		{"1192 onDialogEvent#16", IDLE_END},
		{"1218 onDialogEvent#13", IDLE_END},
		{"1218 onDialogEvent#14", IDLE_END},
		{"1220 onDialogEvent#13", IDLE_END},
		{"1527 onDialogEvent#16", IDLE_END},
		{"1527 onDialogEvent#17", IDLE_END},
		{"1528 onDialogEvent#16", IDLE_END},
		{"1528 onDialogEvent#17", IDLE_END},
		{"1553 onDialogEvent#22", IDLE_END},
		{"1553 onDialogEvent#23", IDLE_END},
		{"1559 onDialogEvent#18", STEP_FALSE},
		{"1559 onDialogEvent#23", STEP_FALSE},
		{"1559 onDialogEvent#30", STEP_FALSE},
		{"1559 onDialogEvent#35", STEP_FALSE},
		{"1559 onDialogEvent#40", STEP_FALSE},
		{"1560 onDialogEvent#8", IDLE_END},
		{"1561 onItemUseEvent#5", ENV_ONLY},
		{"1561 onItemUseEvent#6", ENV_ONLY},
		{"1573 onDialogEvent#19", IDLE_END},
		{"1574 onDialogEvent#8", IDLE_END},
		{"1578 onDialogEvent#22", IDLE_END},
		{"1578 onDialogEvent#23", IDLE_END},
		{"1605 onDialogEvent#22", IDLE_END},
		{"1605 onDialogEvent#23", IDLE_END},
		{"1609 onDialogEvent#16", IDLE_END},
		{"1609 onDialogEvent#17", IDLE_END},
		{"1628 onDialogEvent#16", IDLE_END},
		{"1628 onDialogEvent#17", IDLE_END},
		{"14013 onKillEvent#1", KILL_NO_TARGET},
		{"14014 onKillEvent#3", KILL_FALSE},
		{"14050 onDialogEvent#5", REWARD_PAGE},
		{"14054 onKillEvent#3", KILL_FALSE},
		// Phase 6 step 2, chunk Q08 (lane C, 2026-10-05): _21460AShulacksStory.java:65-67 in START, removeQuestItem assumed false and the
		// reward page 5 outside REWARD (Java's own path: the dialog stays silent)
		{"21460 onDialogEvent#21", REMOVE_FALSE_REWARD_PAGE},
		// chunk Q01 (lane C, 2026-10-05): sendQuestEndDialog in a COMPLETE or START state the path read (IDLE_END), and 24040's reward page 5
		// in START (_24040VotansOrders.java:47-48)
		{"1799 onDialogEvent#22", IDLE_END},
		{"1799 onDialogEvent#23", IDLE_END},
		{"1845 onDialogEvent#12", IDLE_END},
		{"1845 onDialogEvent#13", IDLE_END},
		{"2721 onDialogEvent#22", IDLE_END},
		{"2721 onDialogEvent#23", IDLE_END},
		{"2724 onDialogEvent#22", IDLE_END},
		{"2724 onDialogEvent#23", IDLE_END},
		{"2727 onDialogEvent#23", IDLE_END},
		{"2767 onDialogEvent#8", IDLE_END},
		{"24040 onDialogEvent#4", REWARD_PAGE},
		// chunk Q02 (lane C, 2026-10-05)
		{"11000 onDialogEvent#19", IDLE_END},
		{"11001 onDialogEvent#6", IDLE_END},
		{"11005 onDialogEvent#16", IDLE_END},
		{"11008 onDialogEvent#10", IDLE_END},
		{"11046 onDialogEvent#4", IDLE_END},
		{"11046 onDialogEvent#5", IDLE_END},
		{"11227 onKillEvent#5", KILLS_ASSUMED_FALSE},
		{"11289 onDialogEvent#15", ITEM_CHECK_FALSE},
		{"11460 onDialogEvent#21", REMOVE_FALSE_REWARD_PAGE},
		};
		// P6-Q slice 2 (Q10): the altgard and pandaemonium traces (GoldenKnownVacuousQ10.h)
		for (const auto& [key, kind] : Q10_VACUOUS) {
			const std::string& reason = kind == Vacuous::IDLE_END ? IDLE_END : kind == Vacuous::STEP_NOT_MET ? STEP_NOT_MET : KILLS_ASSUMED_FALSE;
			rows.emplace(std::string(key), reason);
		}
		return rows;
	}();
	return known;
}

/**
 * The Q10 review (2026-09-29): cases a compared run of which reaches an AION_UNPORTED engine body, "<questId> <variant id>", each with its
 * reason. Both runs throw the same UnportedException there, which the comparisons pass, so such a case proves nothing until the body is ported;
 * a case listed here must keep reaching it, and no other case may (docs/deviations/Q10.md, "Landed with an unported engine body on the path").
 */
const std::map<std::string, std::string>& knownUnported() {
	static const std::map<std::string, std::string> known{
		{"2985 onDialogEvent#18", "the finish overlays (SELECTED_QUEST_NOREWARD) reward extend_inventory 2 (quest_data.xml), QuestService.giveReward's "
			"WarehouseService.expand: AION_UNPORTED (WarehouseService.cpp)"},
	};
	return known;
}

/**
 * The quests whose every hook the oracle refuses (tools/oracle/questtrace): no case. 1205 and 2132: `new QuestEnv`, getStartingClass on a value.
 * P6-Q slice 2 (Q03): 1640 (TeleportService.teleportTo), 1647 (player.getEquipment, spawnForFiveMinutesInFrontOf). P6-Q slice 2 (Q10):
 * 2925 (getEquipment), 2938 (TeleportService.teleportTo), 2952 (the whole var field), 4966-4969 (tryDecreaseKinah); their registration traces
 * are checked, their hooks only by parity, the drift test and the chunks' unit cases. (Q10's 2213, getEffectController and SkillEngine, is
 * held back since the integration of slice 2: GoldenHandlers.h).
 * Phase 6 step 2, chunk Q08 (lane C): gelkmaros 21004, 21027, 21033, 21036, 21071 (a status read after sendQuestNoneDialog), 21105, 21249
 * (npc.getController()), enshar 25052 (spawnForFiveMinutes). Chunk Q01 (lane C): reshanta 2798 (a status read after sendQuestNoneDialog).
 * Chunk Q02 (lane C): inggison 11031-11033 (a scheduled task that would throw; a status read after sendQuestNoneDialog), 11053
 * (tryDecreaseKinah), 11118 (getUseArea in the oracle; a status read after sendQuestNoneDialog)
 */
constexpr int32_t ORACLE_REFUSES_EVERY_HOOK[] = {1205, 2132, 1640, 1647, 2925, 2938, 2952, 4966, 4967, 4968, 4969, 21004, 21027, 21033, 21036,
	21071, 21105, 21249, 25052, 2798, 11031, 11032, 11033, 11053, 11118};

TEST_F(GoldenQuestTraceTest, EveryExpectedDocumentHasAGeneratedHandlerAndEveryHandlerADocument) {
	std::vector<int32_t> ids = expectedQuestIds();
	ASSERT_FALSE(ids.empty()) << EXPECTED_DIR;
	for (int32_t questId : ids)
		EXPECT_NE(generatedHandler(questId) != nullptr, heldBack(questId))
			<< questId << (heldBack(questId) ? " is held back but in the table" : " has no handler");
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
	// the Q10 review: no compared run may reach an unported engine body (both runs would throw alike and pass) unless knownUnported lists it
	for (const auto& [id, sites] : tally.unported) {
		for (const std::string& site : sites)
			EXPECT_TRUE(knownUnported().contains(std::to_string(questId) + " " + id)) << "reaches AION_UNPORTED and not listed: " << questId << " " << id
				<< ": " << site;
	}
	for (const auto& [key, reason] : knownUnported()) {
		if (key.starts_with(std::to_string(questId) + " "))
			EXPECT_TRUE(tally.unported.contains(key.substr(key.find(' ') + 1))) << "listed as reaching AION_UNPORTED but it does not now: " << key;
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

/**
 * What the negative control changes in its copy of _1111InsomniaMedicine. OPCODES and THROW (the Q03 review, 2026-09-29): a system message
 * (no key packet) and a NullPointerException before the QUEST_SELECT page. OPCODES_AFTER_STEP, THROW_AFTER_STEP and UNPORTED (the Q10
 * review, the same day): one more packet that is no key packet, a NullPointerException after the step's effects, and an unported engine
 * body reached that the tally must report: WarehouseService::expand, the AION_UNPORTED body knownUnported's 2985 case reaches (the Q10
 * review used defaultStartFollowEvent, which M5d stage 3 E-07 ported on C++, fed04d229, before slice 2 landed there)
 */
enum class Flip { NONE, PAGE, VAR, REWARD_GROUP, ITEM, STATUS, RETURN, OPCODES, THROW, OPCODES_AFTER_STEP, THROW_AFTER_STEP, UNPORTED };

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
				if (qs->getQuestVarById(0) == 0) {
					// the Q03 review: a packet no other comparison sees (not a key packet, no state), and an exception on the path
					if (flip == Flip::OPCODES)
						utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_RACE());
					if (flip == Flip::THROW)
						throw runtime::NullPointerException("the negative control's deliberate exception");
					return sendQuestDialog(env, flip == Flip::PAGE ? 1353 : 1352);
				} else if (qs->getQuestVarById(0) == 1)
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
				if (flip == Flip::OPCODES_AFTER_STEP)
					utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CAN_NOT_GET_LORE_ITEM("flip"));
				if (flip == Flip::THROW_AFTER_STEP)
					throw runtime::NullPointerException("the negative control's flip");
				if (flip == Flip::UNPORTED) {
					services::WarehouseService::expand(*player, false);
					return true;
				}
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
	// packets are no key packets), the var and status in the QuestState, the return value in itself; the Q03 and Q10 reviews added a packet
	// only the opcode sequence sees (a system message, no key packet) and an exception the Java path does not throw, before and after the
	// step's effects, and an unported engine body reached (thrown alike by both runs, reported by the tally)
	const std::pair<Flip, const char*> flips[] = {{Flip::NONE, ""}, {Flip::PAGE, "packets"}, {Flip::VAR, "questStates"},
		{Flip::REWARD_GROUP, "questStates"}, {Flip::ITEM, "inventory"}, {Flip::STATUS, "questStates"}, {Flip::RETURN, "returned"},
		{Flip::OPCODES, "opcodes"}, {Flip::THROW, "thrown"}, {Flip::OPCODES_AFTER_STEP, "opcodes"}, {Flip::THROW_AFTER_STEP, "thrown"},
		{Flip::UNPORTED, "thrown"}};
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
		EXPECT_EQ(!tally.unported.empty(), flip == Flip::UNPORTED) << "the unported engine body the run reached";
	}
}

/** How TaskHandler schedules its task (the review of #79, items 1 and 6) */
enum class TaskMode { RIGHT, SHORTER, ZERO, LONGER, INLINE, EXTRA, THROWS };

/**
 * Quest 1111's dialog hook as a handler with a scheduled task: at the npc 203075, a task 100 ms later sends page 1011 (TaskMode::RIGHT), or
 * the same task with another delay, run inline, followed by a second task the trace does not have, or (THROWS) followed in the task by a
 * NullPointerException, as Java's `qs.setQuestVar(1)` on the null QuestState throws; at any other npc it schedules a task only with
 * TaskMode::EXTRA. Driven against the hand trace of taskTrace()
 */
class TaskHandler final : public AbstractQuestHandler {
public:
	static inline TaskMode mode = TaskMode::RIGHT;

	TaskHandler() : AbstractQuestHandler(1111) {}

	void register_() override { qe.registerQuestNpc(203075)->addOnTalkEvent(questId); }

	bool onDialogEvent(QuestEnv& env) override {
		if (env.getTargetId() != 203075) {
			if (mode == TaskMode::EXTRA)
				utils::ThreadPoolManager::getInstance().schedule({this, &env}, [this, &env] { sendQuestDialog(env, 1012); }, 100);
			return false;
		}
		if (mode == TaskMode::INLINE) {
			sendQuestDialog(env, 1011);
			return true;
		}
		int64_t delay = mode == TaskMode::SHORTER ? 99 : mode == TaskMode::ZERO ? 0 : mode == TaskMode::LONGER ? 101 : 100;
		utils::ThreadPoolManager::getInstance().schedule({this, &env}, [this, &env] {
			sendQuestDialog(env, 1011);
			if (mode == TaskMode::THROWS)
				env.getPlayer()->getQuestStateList()->getQuestState(questId)->setQuestVar(1);
		}, delay);
		return true;
	}
};

std::unique_ptr<AbstractQuestHandler> taskHandler() {
	return std::make_unique<TaskHandler>();
}

/**
 * The hand trace TaskHandler is driven against (extract.py's form): at 203075 without a QuestState, a task of 100 ms that sends page 1011, and
 * with `throws`, then sets the var on the null QuestState (its NullPointerException is the pool's: the hook returned true); at the npc
 * 200000 nothing
 */
json taskTrace(bool throws) {
	json task = json::array({{{"call", "ThreadPoolManager.schedule"}, {"kind", "task"}, {"args", {100}}},
		{{"call", "sendQuestDialog"}, {"kind", "dialog"}, {"args", {1011}}, {"task", 0}}});
	if (throws)
		task.push_back({{"call", "qs.setQuestVar"}, {"kind", "var"}, {"args", {1}}, {"task", 0}});
	json cases = json::array({
		{{"id", "task#1"}, {"hook", "onDialogEvent"}, {"given", {{"target", {{"kind", "npc"}, {"npcId", 203075}}}, {"questState", nullptr}}},
			{"guards", json::array()}, {"effects", task}, {"returns", true}},
		{{"id", "task#2"}, {"hook", "onDialogEvent"}, {"given", {{"target", {{"kind", "npc"}, {"npcId", 200000}}}, {"questState", nullptr}}},
			{"guards", json::array()}, {"effects", json::array()}, {"returns", false}},
	});
	return json{{"register", json::array()}, {"cases", cases}};
}

TEST_F(GoldenQuestTraceTest, TheTaskTimelineFailsATaskAtTheWrongTimeOrWhereJavaHasNone) {
	// the review of #79, item 1: a task 1 ms early, at once, 1 ms late or inline fails the timeline; a task scheduled where the trace has none
	// stays pending (pendingTasks: the tasks a run added and left after the case's last delay); the right handler passes
	const std::pair<TaskMode, const char*> modes[] = {{TaskMode::RIGHT, ""}, {TaskMode::SHORTER, "timeline"}, {TaskMode::ZERO, "timeline"},
		{TaskMode::LONGER, "timeline"}, {TaskMode::INLINE, "timeline"}, {TaskMode::EXTRA, "pendingTasks"}};
	for (const auto& [mode, check] : modes) {
		SCOPED_TRACE("mode " + std::to_string(static_cast<int>(mode)));
		QuestEngine::getInstance().clear();
		TaskHandler::mode = mode;
		Tally tally = runDoc(1111, taskTrace(false), &taskHandler, false);
		TaskHandler::mode = TaskMode::RIGHT;
		EXPECT_EQ(tally.passed + tally.failed, 2);
		if (mode == TaskMode::RIGHT) {
			EXPECT_EQ(tally.failed, 0) << "the right task fails";
		} else {
			EXPECT_GT(tally.failed, 0) << "the harness passes a wrong task";
			EXPECT_TRUE(tally.failedChecks.contains(check)) << "the " << check << " comparison did not fail";
		}
	}
}

TEST_F(GoldenQuestTraceTest, ATaskExceptionIsThePoolsInBothRuns) {
	// the review of #79, item 6: the task's NullPointerException after page 1011 stops the task in both runs (the pool logs it, Java's and
	// ThreadPoolManager's); neither run throws, and both sent the page
	QuestEngine::getInstance().clear();
	TaskHandler::mode = TaskMode::THROWS;
	Tally tally = runDoc(1111, taskTrace(true), &taskHandler, true);
	TaskHandler::mode = TaskMode::RIGHT;
	EXPECT_EQ(tally.failed, 0);
	EXPECT_EQ(tally.passed, 2);
	EXPECT_TRUE(tally.unsatisfiable.empty()) << tally.unsatisfiable.front();
}

TEST_F(GoldenQuestTraceTest, TheCorrectedLevelHooksOf11001And11008StartTheQuest) {
	// chunk Q02, the owner's correction of 2026-10-05 (docs/deviations/Q02.md): Java's level hooks name the quest itself as its pre-quest
	// (_11001KindMeira.java:129, _11008LetterOfEncouragement.java:100), so defaultOnLevelChangedEvent never started them; the generated
	// handlers call it without one (questgen's OWNER_CORRECTIONS). An Elyos at the quest's level gets the quest on a level change; one level
	// below, or an Asmodian, does not
	for (int32_t questId : {11001, 11008}) {
		AbstractQuestHandler& handler = registerGenerated(questId);
		const QuestRow& row = goldenData().quests.at(questId);
		Quester* below = makeQuester(GOLDEN_PLAYER, "Golden", gameserver::model::Race::ELYOS, row.minLevel - 1);
		handler.onLevelChangedEvent(below->player());
		EXPECT_EQ(below->player().getQuestStateList()->getQuestState(questId), nullptr) << questId << " one level below";
		dropQuester(below);
		Quester* other = makeQuester(GOLDEN_PLAYER, "Golden", gameserver::model::Race::ASMODIANS, row.minLevel);
		handler.onLevelChangedEvent(other->player());
		EXPECT_EQ(other->player().getQuestStateList()->getQuestState(questId), nullptr) << questId << " the other race";
		dropQuester(other);
		Quester* quester = makeQuester(GOLDEN_PLAYER, "Golden", gameserver::model::Race::ELYOS, row.minLevel);
		handler.onLevelChangedEvent(quester->player());
		Ptr<QuestState> qs = quester->player().getQuestStateList()->getQuestState(questId);
		ASSERT_NE(qs, nullptr) << questId;
		EXPECT_EQ(qs->getStatus(), QuestStatus::START) << questId;
		dropQuester(quester);
		QuestEngine::getInstance().clear();
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
	// phase6-inventory.md §7.6 item 3: ManualClock and a seeded Rnd. The fixture's DeterministicExecutor runs on its ManualClock, which only a
	// case with scheduled tasks advances (run(): its timeline, then the drain); each run reseeds the thread's Rnd
	EXPECT_EQ(&utils::ThreadPoolManager::clock(), static_cast<const runtime::Clock*>(&clock));
	RandomPageHandler handler;
	json c = {{"id", "random"}, {"hook", "onDialogEvent"}, {"given", json::object()}, {"effects", json::array()}};
	Outcome first = run(handler, 1111, c, CaseSetup{"own race", gameserver::model::Race::ELYOS, 3}, Overlay{}, nullptr, false);
	Outcome second = run(handler, 1111, c, CaseSetup{"own race", gameserver::model::Race::ELYOS, 3}, Overlay{}, nullptr, false);
	ASSERT_EQ(first.keyPackets.size(), 1u);
	EXPECT_EQ(first.keyPackets, second.keyPackets);
}

// --- the registration trace (phase6-inventory.md §7.6 item 2) ------------------------------------------------------------------------------

/** A Java string literal of the registration trace as written (extract.py keeps the quotes) without its quotes */
std::string unquoted(const std::string& literal) {
	return literal.size() >= 2 && literal.front() == '"' && literal.back() == '"' ? literal.substr(1, literal.size() - 2) : literal;
}

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

/** A handler-side quest drop as the registration trace names it: item, amount, chance, collecting step (QuestEngine.addHandlerSideQuestDrop) */
using DropRow = std::tuple<int32_t, int32_t, int32_t, int32_t>;
using DropsByNpc = std::map<int32_t, std::multiset<DropRow>>;

/** The addHandlerSideQuestDrop rows of a registration trace, per npc (args: quest, npc, item, amount, chance[, step]) */
DropsByNpc expectedDropsOf(const json& doc) {
	DropsByNpc out;
	for (const json& reg : doc["register"]) {
		if (reg.value("call", std::string()) != "addHandlerSideQuestDrop")
			continue;
		const json& a = reg["args"];
		out[a[1].get<int32_t>()].insert({a[2].get<int32_t>(), a[3].get<int32_t>(), a[4].get<int32_t>(), a.size() > 5 ? a[5].get<int32_t>() : 0});
	}
	return out;
}

using DropTable = std::map<int32_t, std::set<const gameserver::model::templates::quest::QuestDrop*>>;

/** The quest's drops in QuestService's table at each npc of `npcs` */
DropTable questDropsAt(int32_t questId, const DropsByNpc& npcs) {
	DropTable out;
	for (const auto& [npcId, unused] : npcs) {
		std::set<const gameserver::model::templates::quest::QuestDrop*>& drops = out[npcId];
		for (const gameserver::model::templates::quest::QuestDrop* drop : services::QuestService::getQuestDrop(npcId)) {
			if (drop->getQuestId() == questId)
				drops.insert(drop);
		}
	}
	return out;
}

/**
 * The drops of `after` that `before` does not hold, per npc, as rows (the table is static: every registration of a handler adds its drops
 * again). After the static data only QuestEngine::addHandlerSideQuestDrop adds to it, always a HandlerSideDrop
 */
DropsByNpc addedDrops(const DropTable& before, const DropTable& after) {
	DropsByNpc out;
	for (const auto& [npcId, drops] : after) {
		std::multiset<DropRow>& rows = out[npcId];
		auto known = before.find(npcId);
		for (const gameserver::model::templates::quest::QuestDrop* drop : drops) {
			if (known != before.end() && known->second.contains(drop))
				continue;
			const auto& side = static_cast<const gameserver::model::templates::quest::HandlerSideDrop&>(*drop);
			rows.insert({side.getItemId().value_or(0), side.getNeededAmount(), side.getChance(), side.getCollectingStep()});
		}
	}
	return out;
}

/** 2213's register() with one of its drop's values changed: the negative control of the drop comparison */
class WrongDropHandler final : public AbstractQuestHandler {
public:
	WrongDropHandler(int32_t itemId, int32_t amount, int32_t chance) : AbstractQuestHandler(2213), itemId(itemId), amount(amount), chance(chance) {}
	void register_() override { qe.addHandlerSideQuestDrop(questId, 700057, itemId, amount, chance); } // _2213PoisonRootPotentFruit.java:26

private:
	const int32_t itemId, amount, chance;
};

TEST_F(GoldenQuestTraceTest, TheDropComparisonFailsAWrongDrop) {
	// the registration trace's drop rows (RegistrationTraceMatchesJavaRegister) against 2213.json's addHandlerSideQuestDrop(2213, 700057,
	// 182203208, 1, 100): the right drop passes, a changed item, amount or chance fails
	json doc = readJson(EXPECTED_DIR / "2213.json");
	DropsByNpc expected = expectedDropsOf(doc);
	ASSERT_EQ(expected.size(), 1u);
	const std::tuple<int32_t, int32_t, int32_t, bool> variants[] = {
		{182203208, 1, 100, true}, {182203209, 1, 100, false}, {182203208, 2, 100, false}, {182203208, 1, 50, false}};
	for (const auto& [itemId, amount, chance, same] : variants) {
		SCOPED_TRACE(std::to_string(itemId) + " " + std::to_string(amount) + " " + std::to_string(chance));
		QuestEngine::getInstance().clear();
		DropTable before = questDropsAt(2213, expected);
		QuestEngine::getInstance().addQuestHandler(std::make_unique<WrongDropHandler>(itemId, amount, chance));
		EXPECT_EQ(addedDrops(before, questDropsAt(2213, expected)) == expected, same);
	}
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
	// P6-Q slice 2 (Q03, Q10): the engine-wide lists and the get-item registrations of the chunks' handlers
	bool onLogOutEvent(QuestEnv&) override {
		events.push_back("onLogOutEvent");
		return false;
	}
	bool onQuestTimerEndEvent(QuestEnv&) override {
		events.push_back("onQuestTimerEndEvent");
		return false;
	}
	bool onDieEvent(QuestEnv&) override {
		events.push_back("onDieEvent");
		return false;
	}
	bool onGetItemEvent(QuestEnv& env) override {
		events.push_back("onGetItemEvent " + std::to_string(env.getQuestId()));
		return false;
	}
	bool onNpcReachTargetEvent(QuestEnv&) override {
		events.push_back("onNpcReachTargetEvent");
		return false;
	}
	bool onNpcLostTargetEvent(QuestEnv&) override {
		events.push_back("onNpcLostTargetEvent");
		return false;
	}
	// lane C (phase 6 step 1): the ten registration kinds the trace did not model (QuestEngine.java registerOnKillRanked, registerOnKillInWorld,
	// registerOnLeaveZone, registerOnPassFlyingRings, registerOnInvisibleTimerEnd, registerQuestSkill, registerOnFailCraft,
	// registerOnDredgionReward, registerOnBonusApply, registerOnEnterWindStream). A hook that gets no argument naming what fired it records the
	// test's `firing` text (the rank or the map the test fires)
	bool onKillRankedEvent(QuestEnv&) override {
		events.push_back("onKillRankedEvent " + firing);
		return false;
	}
	bool onKillInWorldEvent(QuestEnv&) override {
		events.push_back("onKillInWorldEvent " + firing);
		return false;
	}
	bool onLeaveZoneEvent(QuestEnv&, const world::zone::ZoneName* zoneName) override {
		events.push_back("onLeaveZoneEvent " + zoneName->name());
		return false;
	}
	bool onPassFlyingRingEvent(QuestEnv&, std::string_view flyingRing) override {
		events.push_back("onPassFlyingRingEvent " + std::string(flyingRing));
		return false;
	}
	bool onInvisibleTimerEndEvent(QuestEnv&) override {
		events.push_back("onInvisibleTimerEndEvent");
		return false;
	}
	bool onUseSkillEvent(QuestEnv&, int32_t skillId) override {
		events.push_back("onUseSkillEvent " + std::to_string(skillId));
		return false;
	}
	bool onFailCraftEvent(QuestEnv&, int32_t itemId) override {
		events.push_back("onFailCraftEvent " + std::to_string(itemId));
		return false;
	}
	bool onDredgionRewardEvent(QuestEnv&) override {
		events.push_back("onDredgionRewardEvent");
		return false;
	}
	HandlerResult onBonusApplyEvent(QuestEnv&, gameserver::model::templates::rewards::BonusType bonusType,
		std::vector<gameserver::model::templates::quest::QuestItems>&) override {
		events.push_back("onBonusApplyEvent " + std::string(xml::EnumTraits<gameserver::model::templates::rewards::BonusType>::names[
			static_cast<size_t>(bonusType)]));
		return HandlerResult::UNKNOWN;
	}
	bool onEnterWindStreamEvent(QuestEnv&, int32_t) override {
		events.push_back("onEnterWindStreamEvent");
		return false;
	}

	static inline std::string firing;

private:
	std::unique_ptr<AbstractQuestHandler> inner;
	std::vector<std::string>& events;
};

TEST_F(GoldenQuestTraceTest, RegistrationTraceMatchesJavaRegister) {
	// every generated handler with an expected document, registered alone: per npc and event the quest id sits in the QuestNpc lists as often
	// as the Java register() adds it and in no other npc's list, and every other registration routes exactly the engine events it names. A
	// can-act registration also comes from the first kill, talk, aggro or distance registration of the quest at an npc (QuestNpc.java:59-95),
	// and registerCanAct keeps only an npc whose template's AI is quest_use_item (QuestEngine.java registerCanAct)
	std::set<std::string> zones, leaveZones, rings, bonusTypes;
	std::set<int32_t> questItems, canActNpcs, allNpcs, getItems, killWorlds, skills, failCraftItems;
	auto usesQuestItemAi = [](int32_t npcId) {
		const gameserver::model::templates::npc::NpcTemplate* template_ = dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId);
		return template_ != nullptr && template_->getAiName() == "quest_use_item";
	};
	for (int32_t questId : expectedQuestIds()) {
		json doc = readJson(EXPECTED_DIR / (std::to_string(questId) + ".json"));
		if (!doc["register"].is_array())
			continue; // a register() the oracle refuses: reported per handler below
		for (const json& reg : doc["register"]) {
			// lane C: the arguments of the kinds the trace models since
			const std::string kind = reg.value("call", std::string());
			if (kind == "registerOnLeaveZone")
				leaveZones.insert(reg["args"][0].get<std::string>());
			else if (kind == "registerOnKillInWorld")
				killWorlds.insert(reg["args"][0].get<int32_t>());
			else if (kind == "registerOnPassFlyingRings")
				rings.insert(unquoted(reg["args"][0].get<std::string>()));
			else if (kind == "registerQuestSkill")
				skills.insert(reg["args"][0].get<int32_t>());
			else if (kind == "registerOnFailCraft")
				failCraftItems.insert(reg["args"][0].get<int32_t>());
			else if (kind == "registerOnBonusApply")
				bonusTypes.insert(reg["args"][1].get<std::string>());
			if (reg.contains("npc"))
				allNpcs.insert(reg["npc"].get<int32_t>());
			else if (reg.value("call", std::string()) == "registerOnEnterZone")
				zones.insert(reg["args"][0].get<std::string>());
			else if (reg.value("call", std::string()) == "registerQuestItem")
				questItems.insert(reg["args"][0].get<int32_t>());
			else if (reg.value("call", std::string()) == "registerCanAct")
				canActNpcs.insert(reg["args"][1].get<int32_t>());
			else if (reg.value("call", std::string()) == "registerOnGetItem")
				getItems.insert(reg["args"][0].get<int32_t>());
			if (reg.value("call", std::string()) == "registerCanAct")
				allNpcs.insert(reg["args"][1].get<int32_t>());
		}
	}
	// the zones exist before the first handler registers (P6-Q slice 2, Q03: runDoc's reason; else the first handler's register() of a zone
	// would find it only when an earlier handler's events had created it)
	for (const std::string& zone : zones)
		world::zone::ZoneName::createOrGet(zone);
	for (const std::string& zone : leaveZones)
		world::zone::ZoneName::createOrGet(zone);
	const auto& rankNames = xml::EnumTraits<utils::stats::AbyssRankEnum>::names;
	const auto& bonusNames = xml::EnumTraits<gameserver::model::templates::rewards::BonusType>::names;
	int32_t checked = 0;
	for (const GeneratedHandler& generated : generatedHandlers()) {
		SCOPED_TRACE(std::string(generated.javaClass));
		json doc = readJson(EXPECTED_DIR / (std::to_string(generated.questId) + ".json"));
		// lane C: a register() the oracle refuses fails this handler and the trace goes on with the next (the sample of goldensample.py
		// registers hundreds; in the tree every register() is traced)
		if (!doc["register"].is_array()) {
			ADD_FAILURE() << "the oracle refuses the register() of " << generated.questId << ": " << doc["register"].dump();
			continue;
		}
		QuestEngine::getInstance().clear();
		std::vector<std::string> events;
		// addHandlerSideQuestDrop adds to QuestService's static drop table, which QuestEngine::clear keeps: the drops the registration adds, per
		// npc, must be the document's rows (item, amount, chance, step; the Q10 review: a count alone let a changed item pass)
		DropsByNpc expectedDrops = expectedDropsOf(doc);
		DropTable dropsBefore = questDropsAt(generated.questId, expectedDrops);
		QuestEngine::getInstance().addQuestHandler(std::make_unique<RoutingSpy>(generated.factory(), events));
		for (const auto& [npcId, rows] : addedDrops(dropsBefore, questDropsAt(generated.questId, expectedDrops))) {
			EXPECT_EQ(rows, expectedDrops[npcId]) << "quest drops at npc " << npcId;
			checked++;
		}

		std::map<std::pair<int32_t, std::string>, int32_t> expected;
		std::set<int32_t> namedNpcs;
		std::vector<std::string> expectedEvents;
		for (const json& reg : doc["register"]) {
			if (reg.contains("npc")) {
				std::string event = reg["event"];
				int32_t npcId = reg["npc"].get<int32_t>();
				namedNpcs.insert(npcId);
				bool first = !expected.contains({npcId, event});
				// every add* keeps the quest once (QuestNpc.java: `if (!onTalkEvent.contains(questId))`, onQuestStart a set); a register() may name
				// one twice (P6-Q slice 2: _2213PoisonRootPotentFruit.java:25 and :27)
				if (first)
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
			// P6-Q slice 2 (Q03, Q10): QuestEngine.java registerOnDie, registerOnLogOut, registerOnQuestTimerEnd, registerAddOnReachTargetEvent,
			// registerAddOnLostTargetEvent (engine-wide lists) and registerOnGetItem (per item: onItemGet, the quest in the env)
			else if (call == "registerOnLogOut")
				expectedEvents.push_back("onLogOutEvent");
			else if (call == "registerOnQuestTimerEnd")
				expectedEvents.push_back("onQuestTimerEndEvent");
			else if (call == "registerOnDie")
				expectedEvents.push_back("onDieEvent");
			else if (call == "registerOnGetItem")
				expectedEvents.push_back("onGetItemEvent " + std::to_string(reg["args"][1].get<int32_t>()));
			else if (call == "registerAddOnReachTargetEvent")
				expectedEvents.push_back("onNpcReachTargetEvent");
			else if (call == "registerAddOnLostTargetEvent")
				expectedEvents.push_back("onNpcLostTargetEvent");
			// lane C (phase 6 step 1). registerOnKillRanked(rank): every rank whose id is at least the rank's (QuestEngine.java:792-798; the ids
			// grow in declaration order, AbyssRankEnum.java). registerOnKillInWorld, registerOnLeaveZone, registerOnPassFlyingRings,
			// registerQuestSkill: per map, zone, ring and skill; registerOnFailCraft(item): the first quest of the item (putIfAbsent), when the
			// quester holds none of it (QuestEngine.java onFailCraft); registerOnBonusApply(quest, type): per bonus type;
			// registerOnInvisibleTimerEnd, registerOnDredgionReward, registerOnEnterWindStream: engine-wide lists
			else if (call == "registerOnKillRanked") {
				auto at = std::find(rankNames.begin(), rankNames.end(), reg["args"][0].get<std::string>());
				if (at == rankNames.end())
					ADD_FAILURE() << "no AbyssRankEnum " << reg["args"][0];
				for (auto rank = at; rank != rankNames.end(); ++rank)
					expectedEvents.push_back("onKillRankedEvent " + std::string(*rank));
			} else if (call == "registerOnKillInWorld")
				expectedEvents.push_back("onKillInWorldEvent " + std::to_string(reg["args"][0].get<int32_t>()));
			else if (call == "registerOnLeaveZone")
				expectedEvents.push_back("onLeaveZoneEvent " + reg["args"][0].get<std::string>());
			else if (call == "registerOnPassFlyingRings")
				expectedEvents.push_back("onPassFlyingRingEvent " + unquoted(reg["args"][0].get<std::string>()));
			else if (call == "registerQuestSkill")
				expectedEvents.push_back("onUseSkillEvent " + std::to_string(reg["args"][0].get<int32_t>()));
			else if (call == "registerOnFailCraft") {
				std::string event = "onFailCraftEvent " + std::to_string(reg["args"][0].get<int32_t>());
				if (std::find(expectedEvents.begin(), expectedEvents.end(), event) == expectedEvents.end())
					expectedEvents.push_back(event);
			}
			// the review of #79, item 9: as Java. registerOnInvisibleTimerEnd, registerOnDredgionReward and registerOnEnterWindStream keep a quest
			// once (`if (!list.contains(questId)) add`, QuestEngine.java:813-816, :843-846, :862-865); registerOnBonusApply adds it each time, but
			// onBonusApplyEvent returns after the first handler of the type (QuestEngine.java onBonusApplyEvent): one event per type either way
			else if (call == "registerOnBonusApply" || call == "registerOnInvisibleTimerEnd" || call == "registerOnDredgionReward" ||
				call == "registerOnEnterWindStream") {
				std::string event = call == "registerOnBonusApply"           ? "onBonusApplyEvent " + reg["args"][1].get<std::string>()
					: call == "registerOnInvisibleTimerEnd" ? std::string("onInvisibleTimerEndEvent")
					: call == "registerOnDredgionReward"    ? std::string("onDredgionRewardEvent")
																									: std::string("onEnterWindStreamEvent");
				if (std::find(expectedEvents.begin(), expectedEvents.end(), event) == expectedEvents.end())
					expectedEvents.push_back(event);
			}
			else if (call == "addHandlerSideQuestDrop")
				continue; // counted above
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
		// lane C: at most the experience table's last level (setupsFor: the quests of minlevel_permitted 99)
		Quester* quester = makeQuester(GOLDEN_PLAYER, "Golden", race,
			std::clamp(row.minLevel, 1, dataholders::DataManager::PLAYER_EXPERIENCE_TABLE->getMaxLevel() - 1));
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
		QuestEngine::getInstance().onLogOut(*QuestEnv::create(nullptr, player, 0));
		QuestEngine::getInstance().onQuestTimerEnd(*QuestEnv::create(nullptr, player, 0));
		QuestEngine::getInstance().onDie(*QuestEnv::create(nullptr, player, 0));
		QuestEngine::getInstance().onNpcReachTarget(*QuestEnv::create(nullptr, player, 0));
		QuestEngine::getInstance().onNpcLostTarget(*QuestEnv::create(nullptr, player, 0));
		for (int32_t itemId : getItems)
			QuestEngine::getInstance().onItemGet(player, itemId);
		// lane C: the events of the kinds above, once each (the kill-ranked one per rank, the kill-in-world one per map)
		for (size_t rank = 0; rank < rankNames.size(); rank++) {
			RoutingSpy::firing = std::string(rankNames[rank]);
			QuestEngine::getInstance().onKillRanked(*QuestEnv::create(nullptr, player, 0), static_cast<utils::stats::AbyssRankEnum>(rank));
		}
		for (int32_t worldId : killWorlds) {
			RoutingSpy::firing = std::to_string(worldId);
			QuestEngine::getInstance().onKillInWorld(*QuestEnv::create(nullptr, player, 0), worldId);
		}
		RoutingSpy::firing.clear();
		for (const std::string& zone : leaveZones)
			QuestEngine::getInstance().onLeaveZone(*QuestEnv::create(nullptr, player, 0), world::zone::ZoneName::createOrGet(zone));
		for (const std::string& ring : rings)
			QuestEngine::getInstance().onPassFlyingRing(*QuestEnv::create(nullptr, player, 0), ring);
		for (int32_t skillId : skills)
			QuestEngine::getInstance().onUseSkill(*QuestEnv::create(nullptr, player, 0), skillId);
		for (int32_t itemId : failCraftItems)
			QuestEngine::getInstance().onFailCraft(*QuestEnv::create(nullptr, player, 0), itemId);
		for (const std::string& type : bonusTypes) {
			auto at = std::find(bonusNames.begin(), bonusNames.end(), type);
			if (at == bonusNames.end()) {
				ADD_FAILURE() << "no BonusType " << type;
				continue;
			}
			std::vector<gameserver::model::templates::quest::QuestItems> rewardItems;
			QuestEngine::getInstance().onBonusApplyEvent(*QuestEnv::create(nullptr, player, 0),
				static_cast<gameserver::model::templates::rewards::BonusType>(at - bonusNames.begin()), rewardItems);
		}
		QuestEngine::getInstance().onInvisibleTimerEnd(*QuestEnv::create(nullptr, player, 0));
		QuestEngine::getInstance().onDredgionReward(*QuestEnv::create(nullptr, player, 0));
		QuestEngine::getInstance().onEnterWindStream(*QuestEnv::create(nullptr, player, 0), 0);
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
