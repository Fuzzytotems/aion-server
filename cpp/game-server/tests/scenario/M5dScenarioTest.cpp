// The M5d scenario gate (m5d-plan.md G-03 and G-04, §10): one login server and one game server as child processes on their own test schemas,
// two accounts, and the quest path of the XML templates end to end - a fresh Elyos Warrior on account A plays 1101 "Sleeping on the Job"
// (report_to, elpas 203049 -> mires 203057), 1102 "Kerubar Hunt" (monster_hunt, three striped kerubs) and accepts and abandons 1103 "Grain
// Thieves" (item_collecting); a fresh Asmodian Warrior on account B (D15) plays 2101 "On Your Feet!" (asak 203500 -> vandar 203504) and 2102
// "A Bloody Task" (four sprigg workers, an item reward) - with the dialogs, the markers, the kills, the rewards, the follow-up windows, two
// relogs and the journal's "report" refused, then the reports the server writes at shutdown.
//
// Every expectation is independent of the C++ server code, exactly as the earlier gates are: server packets are read with the decoders of
// tests/scenario/decoders (QuestDecoders.h for SM_QUEST_ACTION, SM_NEARBY_QUESTS and SM_STATUPDATE_EXP, EconomyDecoders.h for the dialog
// window, ItemDecoders.h and PacketDecoders.h, all written from the Java writeImpl methods, m5a-plan.md D9), and every number comes from
// `tools/oracle/oracle.py` - m5d-quests (the nearby-quest sets with their grey markers), m5d-quest (the page each template answers each dialog
// action with, the kill runs, the rewards after rates, the follow-up window at the end npc and where the level-up falls), m5a-creation (the
// spawn, the starter kinah and bandages), m5a-spawns (where the four npcs stand) and m5b-monster (the monsters' spots and the level-2 exp
// need) - or from the Java method an assertion is about, cited at the line. The m5d oracles get the gate's own server properties as their
// `--profile`, so the owner's `config/mygs.properties` never reaches an expectation.
//
// **What registers since the plan was written.** The phase-6 slice 1 (p6q-ascension-route.md §1) registers 42 Java quest handlers of Poeta,
// Ishalgen and ascension/ beside the 4,184 XML quests; the plan's D9 ("until phase 6 the C++ registry has no Java quest") no longer holds.
// The gate's subject stays the XML quests of §10, and it reads the Java handlers where they act on its path:
//  - the nearby-quest sets (Y1, Y13) are the oracle's `withJava` set restricted to the XML quests and the Java handlers the C++ tree
//    registers (REGISTERED_JAVA_QUESTS; the four first-login handlers 1000, 1100, 2000 and 2100 are held back, p6q §5 item 1): for a level-1
//    Elyos in Poeta that adds 1111 "Insomnia Medicine" (grey), for the Asmodian in Ishalgen nothing;
//  - the Elyos's level-up to 2 inside 1102's reward (Y11) runs the generated `_1205ANewSkill.onLevelChangedEvent`, which starts 1205 and
//    puts it straight into REWARD with var 1 (_1205ANewSkill.java:40-65): two more SM_QUEST_ACTIONs and their SM_NEARBY_QUESTS in that burst,
//    and 1205 in the quest list of every later enter world (Y12). No other registered handler starts anything on this path: the missions
//    1001-1005 and 2001-2007 need 1100/2100, which never start (held back), 2132 needs level 3, and no Java handler names elpas, mires, asak,
//    vandar or the four monsters' npc ids in a way that acts without a quest state (m5d-plan.md §18.4);
//  - `gameserver.simple.secondclass.enable` is pinned to false (§18.4, §18.7): a gate's server reads the owner's mygs.properties, and the key
//    decides whether 1006/2008/1007/2009 register. None of them acts at level 1-2.
//
// It holds TWO gates: M5dScenario.Run (gs.scenario.m5d, geo off) and M5dScenarioGeo.Run (gs.scenario.m5d_geo, geo on), the same script through
// one shared body; the comment above TEST(M5dScenarioGeo, Run) says what the geo run adds (§10.5).
//
// **This file deliberately does not share the other gates' helpers** (M5b3ScenarioTest.cpp gives the reason: each gate owns one pair of
// server processes and its helpers live in an anonymous namespace). What is duplicated is scaffolding - the case log, the burst collector, the
// login conversation, the report readers - never an assertion. The kills fight through GameSession::fightUntil as M5b's do; FightSupport.h's
// waitForRespawnAt is not used, because it knows one session's recording and a kill after a relog has to tell the corpses of the earlier
// session's kills by their object ids (killAt).

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "AsyncAllowed.h"
#include "FakeLoginClient.h"
#include "GameSession.h"
#include "InventoryModel.h"
#include "Oracle.h"
#include "PacketSequence.h"
#include "ScenarioDatabase.h"
#include "ScenarioServers.h"
#include "decoders/CombatDecoders.h"
#include "decoders/EconomyDecoders.h"
#include "decoders/ItemDecoders.h"
#include "decoders/PacketDecoders.h"
#include "decoders/QuestDecoders.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {

namespace decoders {
// GoogleTest prints the two quest list entries of a failed comparison with these (ADL finds them in the entries' namespace); internal to this
// file, so no other test file sees them
static void PrintTo(const QuestEntry& entry, std::ostream* out) {
	*out << entry.questId << ":s" << +entry.status << ":v" << entry.questVarsAndFlags << ":c" << +entry.completeCount;
}
static void PrintTo(const QuestCompletedEntry& entry, std::ostream* out) {
	*out << entry.questId << ":c" << +entry.completeCount << ":r" << +entry.repeatFlag;
}
} // namespace decoders

namespace {

using namespace std::chrono_literals;
using decoders::DecodeError;
using nlohmann::json;
using Packet = GameSession::Packet;

/** the quiet period that ends a burst of server packets (m5a-plan.md §5.4) */
constexpr std::chrono::milliseconds QUIET = 1000ms;
constexpr std::chrono::milliseconds BURST_LIMIT = 90s;

/** SM_CREATE_CHARACTER response codes (SM_CREATE_CHARACTER.java) */
constexpr int32_t RESPONSE_OK = 0;
constexpr int32_t RESPONSE_OPEN_CREATION_WINDOW = 22;
/** SM_ENTER_WORLD_CHECK's first byte for a character that may enter */
constexpr uint8_t ENTER_WORLD_OK = 0;

constexpr int32_t ELYOS_START_MAP = 210010000;
constexpr int32_t ASMODIAN_START_MAP = 220010000;

/** §10.1 "Targets": the four quest npcs and the two monster pairs (the oracle confirms each in C0) */
constexpr int32_t ELPAS = 203049;
constexpr int32_t MIRES = 203057;
constexpr int32_t ASAK = 203500;
constexpr int32_t VANDAR = 203504;
constexpr std::array<int32_t, 2> KERUBS{210133, 210134};
constexpr std::array<int32_t, 2> SPRIGG_WORKERS{210363, 210364};

/** the quests of §10.2 (D6) */
constexpr int32_t Q1101 = 1101;
constexpr int32_t Q1102 = 1102;
constexpr int32_t Q1103 = 1103;
constexpr int32_t Q1104 = 1104;
constexpr int32_t Q2101 = 2101;
constexpr int32_t Q2102 = 2102;
constexpr int32_t Q2103 = 2103;
/** the generated phase-6 handler that starts on the Elyos's level-up to 2 (_1205ANewSkill.java:40-65; the header comment) */
constexpr int32_t Q1205 = 1205;

/**
 * The Java quest handlers the C++ registry holds (phase 6 slice 1, p6q-ascension-route.md §1; `AION_QUEST_HANDLER` in
 * game-server/handlers/aion/gameserver/handlers/quest/{poeta,ishalgen,ascension}): Q05 1001-1005, 1107, 1111, 1114, 1122, 1123, 1205; Q09
 * 2001-2007, 2106, 2114, 2122, 2123, 2125, 2132, 2135, 2136; Q06 1006, 1007, 1913-1916, 19070, 19071, 2008, 2009, 2901-2904, 29070, 29071.
 * The oracle's `withJava` sets model Java's whole registry (1,035 handlers); the gate's expectation keeps an id of it that is not an XML
 * quest only when it is here. A new phase-6 chunk whose handler starts on a start map at level 1 changes Y1 or Y13, and the failure names
 * the id: add it here with the chunk.
 */
constexpr std::array<int32_t, 42> REGISTERED_JAVA_QUESTS{1001, 1002, 1003, 1004, 1005, 1107, 1111, 1114, 1122, 1123, 1205, 2001, 2002, 2003,
                                                         2004, 2005, 2006, 2007, 2106, 2114, 2122, 2123, 2125, 2132, 2135, 2136, 1006, 1007,
                                                         1913, 1914, 1915, 1916, 19070, 19071, 2008, 2009, 2901, 2902, 2903, 2904, 29070, 29071};

/** DialogAction ids (model/DialogAction.java:38, 46, 123; the two 1000s ids are QUEST_ACCEPT_1 and SELECT_QUEST_REWARD) */
constexpr uint16_t SELECTED_QUEST_NOREWARD = 23;
constexpr uint16_t QUEST_SELECT = 31;
constexpr uint16_t SELECTED_QUEST_AUTO_REWARD = 108;
constexpr uint16_t QUEST_ACCEPT_1 = 1002;
constexpr uint16_t SELECT_QUEST_REWARD = 1009;

/** DialogPage.getStartPageId (DialogPage.java:113-126): 10 with a quest interaction (or a function), 1011 the default dialog */
constexpr uint16_t PAGE_SELECT_QUEST = 10;
constexpr uint16_t PAGE_SELECT1 = 1011;

/** ItemId.KINAH and the Bandage 2102 pays (quest_data.xml, D6) */
constexpr int32_t KINAH_ITEM = 182400001;
constexpr int32_t BANDAGE = 169300002;

/** SM_SYSTEM_MESSAGE ids (SM_SYSTEM_MESSAGE.java:10223-10225, :16349-16351) */
constexpr int32_t STR_DIALOG_TOO_FAR_TO_TALK = 1300346;
constexpr int32_t STR_GET_EXP = 1370000;
/** ActionAnimation.LEVEL_UP (model/animations/ActionAnimation.java:12), what PlayerController.onLevelChange broadcasts (:587) */
constexpr uint16_t ACTION_ANIMATION_LEVEL_UP = 0;

/** QuestStatus.value() of the player_quests enum (QuestStatus.java:11-14): the `status` column stores the name */
constexpr std::string_view DB_START = "START";
constexpr std::string_view DB_REWARD = "REWARD";
constexpr std::string_view DB_COMPLETE = "COMPLETE";

/** where the character stands to talk (inside the talk distance of 5 + 1, PositionUtil.isInTalkRange) and C7's "from 15 m" (§10.2) */
constexpr double TALK_DISTANCE = 3.0;
constexpr double TOO_FAR_DISTANCE = 15.0;
/** the melee distance of m5b-plan.md K4b: inside the Warrior's attack range */
constexpr double MELEE_DISTANCE = 2.0;
/**
 * A Warrior below this share of its max HP rests before the next fight. The fights are real (§11 risk 8): a kerub hits back for minutes of
 * a gate, and the Warrior must not die - a death would be the gate's, not the quest engine's. Resting is reading packets while HP regenerates.
 */
constexpr double REST_BELOW = 0.6;

// ---- small helpers ----------------------------------------------------------------------------------------------------------------------

std::string join(const std::vector<std::string>& values, std::string_view separator = ", ") {
	std::string text;
	for (size_t i = 0; i < values.size(); i++) {
		if (i > 0)
			text += separator;
		text += values[i];
	}
	return text;
}

template <typename Numbers>
std::string joinNumbers(const Numbers& values) {
	std::vector<std::string> texts;
	for (const auto& value : values)
		texts.push_back(std::to_string(value));
	return join(texts);
}

double distance2d(double x1, double y1, double x2, double y2) {
	const double dx = x1 - x2, dy = y1 - y2;
	return std::sqrt(dx * dx + dy * dy);
}

std::vector<std::string> namesOf(const std::vector<Packet>& packets) {
	std::vector<std::string> names;
	names.reserve(packets.size());
	for (const Packet& packet : packets)
		names.push_back(packet.name);
	return names;
}

const Packet* firstOfName(const std::vector<Packet>& packets, std::string_view name) {
	for (const Packet& packet : packets)
		if (packet.name == name)
			return &packet;
	return nullptr;
}

std::vector<Packet> ofName(const std::vector<Packet>& packets, std::string_view name) {
	std::vector<Packet> result;
	for (const Packet& packet : packets)
		if (packet.name == name)
			result.push_back(packet);
	return result;
}

/** the packets [from, to) of a session's recording */
std::vector<Packet> slice(const GameSession& session, size_t from, std::optional<size_t> to = std::nullopt) {
	const std::vector<Packet>& packets = session.recorded();
	const size_t end = std::min(packets.size(), to.value_or(packets.size()));
	if (from >= end)
		return {};
	return std::vector<Packet>(packets.begin() + static_cast<std::ptrdiff_t>(from), packets.begin() + static_cast<std::ptrdiff_t>(end));
}

std::set<int32_t> toSet(const std::vector<int32_t>& values) {
	return {values.begin(), values.end()};
}

/**
 * SM_SYSTEM_MESSAGE (SM_SYSTEM_MESSAGE.java:28940-28953) with its parameter lists, as M5bScenarioTest.cpp's R1 (a) reads it: STR_GET_EXP(name,
 * num1) writes every parameter with `writeS(param.toString())`, so the gained exp is on the wire as `params[1]`
 */
struct SystemMessage {
	uint8_t chatType = 0;
	int32_t senderObjectId = 0;
	int32_t messageId = 0;
	std::vector<std::string> params;
	std::vector<std::string> specialParams;
};

SystemMessage decodeSystemMessage(std::span<const uint8_t> body) {
	decoders::BodyReader reader(body, "SM_SYSTEM_MESSAGE");
	SystemMessage message;
	message.chatType = reader.C();
	reader.expectC(0, "SM_SYSTEM_MESSAGE text encoding (writeC(0x00))");
	message.senderObjectId = reader.D();
	message.messageId = reader.D();
	const uint8_t params = reader.C();
	for (uint8_t i = 0; i < params; i++)
		message.params.push_back(reader.S());
	const uint8_t specials = reader.C();
	for (uint8_t i = 0; i < specials; i++)
		message.specialParams.push_back(reader.S());
	reader.expectFullyConsumed();
	return message;
}

/** SM_ACTION_ANIMATION (SM_ACTION_ANIMATION.java:28-30): writeD(targetObjectId), writeH(the ActionAnimation's id), writeD(levelOrObjectId) */
struct ActionAnimationPacket {
	int32_t objectId = 0;
	uint16_t animation = 0;
	int32_t levelOrObjectId = 0;
};

ActionAnimationPacket decodeActionAnimation(std::span<const uint8_t> body) {
	decoders::BodyReader reader(body, "SM_ACTION_ANIMATION");
	ActionAnimationPacket packet;
	packet.objectId = reader.D();
	packet.animation = reader.H();
	packet.levelOrObjectId = reader.D();
	reader.expectFullyConsumed();
	return packet;
}

/** SM_ENTER_WORLD_CHECK's first byte (SM_ENTER_WORLD_CHECK.java: writeC(msg), then the rest) */
std::optional<uint8_t> enterWorldCheck(const std::vector<Packet>& burst) {
	const Packet* check = firstOfName(burst, "SM_ENTER_WORLD_CHECK");
	if (check == nullptr || check->data.empty())
		return std::nullopt;
	return check->data[0];
}

// ---- the quest events of a window --------------------------------------------------------------------------------------------------------

/**
 * One decoded pass over a window's quest-relevant packets. `tokens` lists them in arrival order in a compact notation the §10.3 order
 * patterns compare with (every other packet - the npc's SM_LOOKATOBJECT, the level-up's stats and skills, the async set - is not a token):
 *   "ADD <quest> s<status> v<vars>" / "UPDATE <quest> s<status> v<vars>" / "ABANDON <quest>" / "TIMER <quest>" / "EMPTY" - SM_QUEST_ACTION
 *   ("v" is the wire int of vars and flags, QuestDecoders.h), "NEARBY" - SM_NEARBY_QUESTS, "DW <npc> <page> <quest>" - SM_DIALOG_WINDOW
 *   (the npc as its label, or its object id), "EXP" - SM_STATUPDATE_EXP, "GET_EXP <n>" - STR_GET_EXP's gained exp, "MSG <id>" - any
 *   other system message, "ITEM <template>x<count>" - SM_INVENTORY_UPDATE_ITEM (the template of the updated object as the inventory model
 *   knows it: the packet names the object only), "ADD_ITEM <template>x<count>" - SM_INVENTORY_ADD_ITEM,
 *   "LEVEL_UP <level>" - the character's own SM_ACTION_ANIMATION(LEVEL_UP).
 */
struct QuestEvents {
	std::vector<std::string> tokens;
	std::vector<decoders::QuestAction> actions;
	std::vector<decoders::NearbyQuests> nearby;
	std::vector<decoders::DialogWindow> windows;
	std::vector<decoders::StatUpdateExp> exps;
	std::vector<int64_t> gainedExp;
	std::vector<int32_t> otherMessages;
	std::vector<decoders::InventoryUpdateItem> itemUpdates;
	std::vector<decoders::InventoryItem> itemAdds;
	std::vector<std::string> decodeFailures;

	/** the tokens of the window in one line, for a failure message */
	std::string describe() const { return "[" + join(tokens, " | ") + "]"; }
};

QuestEvents questEvents(const std::vector<Packet>& packets, int32_t playerId, const std::map<int32_t, std::string>& labels,
	const std::function<int32_t(int32_t)>& itemIdOf) {
	QuestEvents events;
	const auto label = [&](int32_t objectId) {
		const auto found = labels.find(objectId);
		return found == labels.end() ? std::to_string(objectId) : found->second;
	};
	for (size_t i = 0; i < packets.size(); i++) {
		const Packet& packet = packets[i];
		try {
			if (packet.name == "SM_QUEST_ACTION") {
				const decoders::QuestAction action = decoders::decodeQuestAction(packet.data);
				events.actions.push_back(action);
				if (action.empty)
					events.tokens.push_back("EMPTY");
				else if (action.actionType == decoders::QUEST_ACTION_ADD || action.actionType == decoders::QUEST_ACTION_UPDATE)
					events.tokens.push_back(std::string(action.actionType == decoders::QUEST_ACTION_ADD ? "ADD " : "UPDATE ") +
					                        std::to_string(action.questId) + " s" + std::to_string(action.status) + " v" +
					                        std::to_string(action.questVarsAndFlags));
				else if (action.actionType == decoders::QUEST_ACTION_ABANDON)
					events.tokens.push_back("ABANDON " + std::to_string(action.questId));
				else if (action.actionType == decoders::QUEST_ACTION_TIMER)
					events.tokens.push_back("TIMER " + std::to_string(action.questId));
				else
					events.tokens.push_back("QUEST_ACTION " + std::to_string(action.actionType) + " " + std::to_string(action.questId));
			} else if (packet.name == "SM_NEARBY_QUESTS") {
				events.nearby.push_back(decoders::decodeNearbyQuests(packet.data));
				events.tokens.push_back("NEARBY");
			} else if (packet.name == "SM_DIALOG_WINDOW") {
				const decoders::DialogWindow window = decoders::decodeDialogWindow(packet.data);
				events.windows.push_back(window);
				events.tokens.push_back("DW " + label(window.targetObjectId) + " " + std::to_string(window.dialogPageId) + " " +
				                        std::to_string(window.questId));
			} else if (packet.name == "SM_STATUPDATE_EXP") {
				events.exps.push_back(decoders::decodeStatUpdateExp(packet.data));
				events.tokens.push_back("EXP");
			} else if (packet.name == "SM_SYSTEM_MESSAGE") {
				const SystemMessage message = decodeSystemMessage(packet.data);
				if (message.messageId == STR_GET_EXP && message.params.size() >= 2) {
					events.gainedExp.push_back(std::stoll(message.params[1]));
					events.tokens.push_back("GET_EXP " + message.params[1]);
				} else {
					events.otherMessages.push_back(message.messageId);
					events.tokens.push_back("MSG " + std::to_string(message.messageId));
				}
			} else if (packet.name == "SM_INVENTORY_UPDATE_ITEM") {
				const decoders::InventoryUpdateItem update = decoders::decodeInventoryUpdateItem(packet.data);
				events.itemUpdates.push_back(update);
				// SM_INVENTORY_UPDATE_ITEM names the object, not the template (ItemDecoders.h): the item id is the inventory model's
				events.tokens.push_back("ITEM " + std::to_string(itemIdOf(update.item.objectId)) + "x" +
				                        (update.item.general ? std::to_string(update.item.general->count) : std::string("?")));
			} else if (packet.name == "SM_INVENTORY_ADD_ITEM") {
				for (const decoders::InventoryItem& item : decoders::decodeInventoryAddItem(packet.data).items) {
					events.itemAdds.push_back(item);
					events.tokens.push_back("ADD_ITEM " + std::to_string(item.templateId) + "x" +
					                        (item.general ? std::to_string(item.general->count) : std::string("?")));
				}
			} else if (packet.name == "SM_ACTION_ANIMATION") {
				const ActionAnimationPacket animation = decodeActionAnimation(packet.data);
				if (animation.objectId == playerId && animation.animation == ACTION_ANIMATION_LEVEL_UP)
					events.tokens.push_back("LEVEL_UP " + std::to_string(animation.levelOrObjectId));
			}
		} catch (const std::exception& error) {
			events.decodeFailures.push_back(packet.name + " #" + std::to_string(i) + ": " + error.what());
		}
	}
	return events;
}

/** "ADD <quest> s<status> v<vars>" - the token of an ADD/UPDATE, for the expected patterns */
std::string questToken(std::string_view type, int32_t questId, uint8_t status, int32_t vars) {
	return std::string(type) + " " + std::to_string(questId) + " s" + std::to_string(status) + " v" + std::to_string(vars);
}

std::string windowToken(std::string_view npc, uint16_t page, int32_t questId) {
	return "DW " + std::string(npc) + " " + std::to_string(page) + " " + std::to_string(questId);
}

// ---- the case-by-case report (the M5a §5.7 Q8 shape) -----------------------------------------------------------------------------------

struct CaseResult {
	std::string id;
	std::string title;
	bool ran = false;
	bool failed = false;
	std::string error;
	std::chrono::milliseconds duration{0};
};

/** the failed assertions of the running test so far (HasFailure() cannot tell a later case's failure from an earlier one's) */
int32_t failedAssertions(bool fatalOnly = false) {
	const ::testing::TestResult* result = ::testing::UnitTest::GetInstance()->current_test_info()->result();
	int32_t failed = 0;
	for (int i = 0; i < result->total_part_count(); i++)
		if (fatalOnly ? result->GetTestPartResult(i).fatally_failed() : result->GetTestPartResult(i).failed())
			failed++;
	return failed;
}

/**
 * Runs the cases in order, records their result and never lets one case's exception end the run silently (M5b3ScenarioTest.cpp's CaseLog).
 * @return whether the NEXT case can run: false after an exception or a fatal (ASSERT_*) failure, which leave the character's state unknown;
 * true after non-fatal (EXPECT_*) failures only, so a mutant that breaks one row still shows every later row of the gate
 */
class CaseLog {
public:
	bool run(std::string_view id, std::string_view title, const std::function<void()>& body) {
		CaseResult result;
		result.id = id;
		result.title = title;
		result.ran = true;
		const int32_t failedBefore = failedAssertions();
		const int32_t fatalBefore = failedAssertions(true);
		bool threw = false;
		const auto started = std::chrono::steady_clock::now();
		{
			SCOPED_TRACE(std::string(id) + ": " + std::string(title));
			try {
				body();
			} catch (const std::exception& exception) {
				threw = true;
				result.error = exception.what();
				ADD_FAILURE() << id << " (" << title << ") ended with an exception: " << exception.what();
			}
		}
		result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
		result.failed = failedAssertions() > failedBefore;
		results.push_back(result);
		return !threw && failedAssertions(true) == fatalBefore;
	}

	void skip(std::string_view id, std::string_view title, std::string_view reason) {
		CaseResult result;
		result.id = id;
		result.title = title;
		result.error = std::string("not run: ") + std::string(reason);
		results.push_back(result);
	}

	std::string report(std::string_view testName) const {
		std::ostringstream text;
		text << testName << ", case by case:\n";
		for (const CaseResult& result : results) {
			text << "  " << result.id << " " << result.title << ": ";
			if (!result.ran)
				text << "NOT RUN (" << result.error << ")";
			else
				text << (result.failed ? "FAILED" : "passed") << " in " << result.duration.count() << " ms"
				     << (result.error.empty() ? "" : " [" + result.error + "]");
			text << "\n";
		}
		return text.str();
	}

private:
	std::vector<CaseResult> results;
};

// ---- packet stream helpers -------------------------------------------------------------------------------------------------------------

/** The object ids the server announced as npcs (SM_NPC_INFO), for the npc half of the async-allowed set (m5b-plan.md D2) */
class AnnouncedNpcs {
public:
	void follow(const GameSession* next) {
		session = next;
		scanned = 0;
		ids.clear();
	}

	bool contains(int32_t objectId) {
		scan();
		return ids.contains(objectId);
	}

	std::function<bool(int32_t)> predicate() {
		return [this](int32_t objectId) { return contains(objectId); };
	}

private:
	void scan() {
		if (session == nullptr)
			return;
		const std::vector<Packet>& packets = session->recorded();
		for (; scanned < packets.size(); scanned++) {
			if (packets[scanned].name != "SM_NPC_INFO")
				continue;
			try {
				ids.emplace(decoders::decodeNpcInfo(packets[scanned].data).objectId);
			} catch (const DecodeError&) {
				try {
					ids.emplace(decoders::decodeNpcInfoObjectId(packets[scanned].data));
				} catch (const DecodeError&) {
					// the packet announced no id at all
				}
			}
		}
	}

	const GameSession* session = nullptr;
	size_t scanned = 0;
	std::set<int32_t> ids;
};

/**
 * The answer to a request the server may take a while to start answering (an enter world loads the character from the database): the first
 * packet the async set does not explain is waited for up to `firstWithin`, then the burst runs on as collectBurst's does. collectBurst alone
 * gives up after its quiet second when the first packet is later than that, which a relog on a loaded machine can be.
 */
std::vector<Packet> collectAnswer(GameSession& session, const AsyncAllowed& async, std::chrono::milliseconds firstWithin);

/** A burst ends `quiet` after the last packet the async-allowed set does not explain (M5bScenarioTest.cpp's collectBurst) */
std::vector<Packet> collectBurst(GameSession& session, const AsyncAllowed& async, std::chrono::milliseconds quiet = QUIET,
	std::chrono::milliseconds limit = BURST_LIMIT) {
	std::vector<Packet> collected;
	const auto start = std::chrono::steady_clock::now();
	const auto deadline = start + limit;
	auto lastAwaited = start;
	for (;;) {
		const auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
			break;
		const auto quietLeft = std::chrono::duration_cast<std::chrono::milliseconds>(lastAwaited + quiet - now);
		if (quietLeft <= 0ms)
			break;
		std::optional<Packet> packet = session.next(std::min(quietLeft, std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)));
		if (!packet)
			break;
		if (!async.allows(packet->name, std::span<const uint8_t>(packet->data)))
			lastAwaited = std::chrono::steady_clock::now();
		collected.push_back(std::move(*packet));
	}
	return collected;
}

std::vector<Packet> collectAnswer(GameSession& session, const AsyncAllowed& async, std::chrono::milliseconds firstWithin) {
	std::vector<Packet> collected;
	const auto deadline = std::chrono::steady_clock::now() + firstWithin;
	for (;;) {
		const auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
			return collected;
		std::optional<Packet> packet = session.next(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
		if (!packet)
			return collected;
		const bool awaited = !async.allows(packet->name, std::span<const uint8_t>(packet->data));
		collected.push_back(std::move(*packet));
		if (awaited)
			break;
	}
	for (Packet& packet : collectBurst(session, async))
		collected.push_back(std::move(packet));
	return collected;
}

/** Reads and records everything that arrives within a FIXED window; the gate waits by reading, never by sleeping on a live socket */
std::vector<Packet> collectFor(GameSession& session, std::chrono::milliseconds window) {
	std::vector<Packet> collected;
	const auto deadline = std::chrono::steady_clock::now() + window;
	for (;;) {
		const auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
			break;
		std::optional<Packet> packet = session.next(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
		if (!packet) {
			if (session.client.socket.isClosed())
				break;
			continue;
		}
		collected.push_back(std::move(*packet));
	}
	return collected;
}

/**
 * Reads until `accept` answers true for a packet, recording everything on the way, or until `timeout`. The predicate sees every packet once.
 * @return the index in recorded() of the accepted packet, std::nullopt on timeout or close
 */
std::optional<size_t> readUntil(GameSession& session, const std::function<bool(const Packet&)>& accept, std::chrono::milliseconds timeout) {
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	for (;;) {
		const auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
			return std::nullopt;
		std::optional<Packet> packet = session.next(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
		if (!packet) {
			if (session.client.socket.isClosed())
				return std::nullopt;
			continue;
		}
		if (accept(*packet))
			return session.recorded().size() - 1;
	}
}

/** Reads until a packet with that name arrives and records everything on the way. @throws std::runtime_error on timeout or close */
Packet waitFor(GameSession& session, std::string_view name, std::chrono::milliseconds timeout = 15s) {
	const std::optional<size_t> index = readUntil(session, [name](const Packet& packet) { return packet.name == name; }, timeout);
	if (!index)
		throw std::runtime_error("timeout waiting for " + std::string(name) + (session.client.socket.isClosed() ? " (the connection closed)" : ""));
	return session.recorded()[*index];
}

/** Reads until a packet with that name arrives, skipping the async-allowed set. @throws std::runtime_error on timeout or close */
Packet expectNext(GameSession& session, std::string_view name, const AsyncAllowed& async, std::chrono::milliseconds timeout = 15s) {
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	for (;;) {
		const auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
			throw std::runtime_error("timeout waiting for " + std::string(name));
		std::optional<Packet> packet = session.next(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
		if (!packet)
			throw std::runtime_error("expected " + std::string(name) + ", got nothing (" +
			                         (session.client.socket.isClosed() ? "connection closed" : "timeout") + ")");
		if (packet->name == name)
			return *packet;
		if (async.allows(packet->name, packet->data))
			continue;
		throw std::runtime_error("expected " + std::string(name) + ", got " + packet->name);
	}
}

/** Matches the recorded names against the §5.8 notation with the async-allowed set of §5.9 */
void expectSequence(const std::vector<Packet>& packets, std::string_view pattern, const AsyncAllowed& async, std::string_view row) {
	const PacketSequence sequence = PacketSequence::parse(pattern);
	const std::vector<std::string> names = namesOf(packets);
	const PacketSequence::Result result = sequence.match(names, async.predicate(packets));
	EXPECT_TRUE(result.matched) << row << ": " << result.message << "\n  expected: " << sequence.toString() << "\n  got (" << names.size()
	                            << "): " << join(names);
}

/**
 * The CM_ENTER_WORLD part of m5a-plan.md §5.8, as M5b3ScenarioTest.cpp replays it (SM_INVENTORY_INFO and SM_WAREHOUSE_INFO as `+`). A
 * first enter carries the first-enter level change 0 -> 1 in front (PlayerEnterWorldService.java:204 -> PlayerController.onLevelChange:
 * SM_STATS_INFO, SM_ACTION_ANIMATION LEVEL_UP, SM_NEARBY_QUESTS), which is the first SM_NEARBY_QUESTS Y1 reads.
 */
std::string enterWorldPattern(bool firstEnter) {
	std::string pattern;
	if (firstEnter)
		pattern += "SM_STATS_INFO, SM_ACTION_ANIMATION, SM_NEARBY_QUESTS, ";
	pattern += "SM_HOUSE_SCRIPTS, SM_UNK_3_5_1, SM_ENTER_WORLD_CHECK, ";
	pattern += "SM_SKILL_LIST+, [SM_SKILL_COOLDOWN], [SM_ITEM_COOLDOWN], ";
	pattern += "SM_QUEST_COMPLETED_LIST+, SM_QUEST_LIST, SM_TITLE_INFO{2}, SM_MOTION, ";
	pattern += "SM_AFTER_TIME_CHECK_4_7_5, [SM_UI_SETTINGS]{0..3}, ";
	pattern += "SM_INVENTORY_INFO+, ";
	pattern += "SM_CHANNEL_INFO, SM_BIND_POINT_INFO, SM_PLAYER_SPAWN, SM_GAME_TIME, ";
	pattern += "SM_WAREHOUSE_INFO+, ";
	pattern += "SM_TITLE_INFO, SM_EMOTION_LIST, SM_PRICES, SM_FRIEND_LIST, SM_BLOCK_LIST, ";
	pattern += "SM_INSTANCE_INFO, SM_ABYSS_RANK, SM_STATS_INFO, ";
	pattern += "SM_LEGION_DOMINION_LOC_INFO, SM_MAIL_SERVICE, ";
	pattern += "[SM_SYSTEM_MESSAGE]{0..2}, [SM_ATREIAN_PASSPORT], ";
	pattern += "SM_MACRO_LIST+, SM_RECIPE_LIST, SM_HOUSE_OWNER_INFO";
	return pattern;
}

/** The CM_LEVEL_READY part of m5a-plan.md §5.8 (#33 to #44); its SM_NEARBY_QUESTS is the second one Y1 reads (CM_LEVEL_READY.cpp:108) */
std::string levelReadyPattern() {
	return "SM_PLAYER_INFO, SM_PLAYER_STATE, SM_ACCOUNT_PROPERTIES, SM_MOTION, "
	       "SM_WINDSTREAM_ANNOUNCE*, "
	       "(SM_NPC_INFO | SM_GATHERABLE_INFO)+, "
	       "SM_RIFT_ANNOUNCE, "
	       "SM_NEARBY_QUESTS, [SM_QUEST_REPEAT], [SM_WEATHER], "
	       "SM_ABNORMAL_STATE, SM_CUBE_UPDATE";
}

// ---- the oracle answers this gate reads (tools/oracle/m5d/quests.py, m5a/spawns.py, m5b/monster.py) ----------------------------------------

std::set<int32_t> intSet(const json& node) {
	std::set<int32_t> values;
	if (node.is_array())
		for (const json& value : node)
			values.insert(value.get<int32_t>());
	return values;
}

/** oracle.py m5d-quests --map M --race R --level N: the SM_NEARBY_QUESTS set and its grey bits (§10.3 Y1, Y13) */
struct NearbyAnswer {
	std::set<int32_t> xmlOnly, xmlOnlyGrey, withJava, withJavaGrey;
	bool exact = false;
	/** what the C++ registry sends: `withJava` without the Java quests the C++ tree does not register (REGISTERED_JAVA_QUESTS) */
	std::set<int32_t> expected, expectedGrey;
	/** the Java-handled quests of `expected` */
	std::set<int32_t> javaAdded;
};

NearbyAnswer parseNearby(const std::string& text) {
	const json root = json::parse(text);
	const json& nearby = root.at("nearby");
	NearbyAnswer answer;
	answer.xmlOnly = intSet(nearby.at("xmlOnly"));
	answer.xmlOnlyGrey = intSet(nearby.at("xmlOnlyGrey"));
	answer.withJava = intSet(nearby.at("withJava"));
	answer.withJavaGrey = intSet(nearby.at("withJavaGrey"));
	answer.exact = root.value("exact", false);
	const std::set<int32_t> registered(REGISTERED_JAVA_QUESTS.begin(), REGISTERED_JAVA_QUESTS.end());
	for (const int32_t questId : answer.withJava) {
		const bool xml = answer.xmlOnly.contains(questId);
		if (!xml && !registered.contains(questId))
			continue;
		answer.expected.insert(questId);
		if (answer.withJavaGrey.contains(questId))
			answer.expectedGrey.insert(questId);
		if (!xml)
			answer.javaAdded.insert(questId);
	}
	return answer;
}

/** a window an m5d-quest step or follow-up answers with: SM_DIALOG_WINDOW(npc, page, questId) */
struct OracleWindow {
	uint16_t page = 0;
	int32_t questId = 0;
	bool operator==(const OracleWindow&) const = default;
};

/** oracle.py m5d-quest --quest ID [--completed ...] [--exp X]: what §10.3 reads of one quest */
struct QuestAnswer {
	int32_t questId = 0;
	std::string kind;
	bool inCppRegistry = false;
	std::vector<int32_t> startNpcs, endNpcs;
	std::string race;
	/** the first completion's payments (SELECTED_QUEST_NOREWARD), in payment order */
	int64_t kinah = 0, exp = 0;
	std::vector<std::pair<int32_t, int64_t>> items;
	/** the kill run of a monster_hunt: its npc ids and the count (§10.3 Y8, Y10, Y13) */
	std::vector<int32_t> killNpcIds;
	int32_t killCount = 0;
	/** the follow-up at the (first) end npc: the window sendQuestEndDialog opens after finishQuest, std::nullopt when the oracle has none */
	std::optional<int32_t> followUp;
	std::optional<OracleWindow> followUpWindow;
	std::vector<int32_t> levelsSinceEnterWorld;
	int64_t expBeforeReward = 0, expAfterReward = 0;
	json steps;

	/** the window the step of `phase` answers `actionId` with; std::nullopt when no step lists the action or its window is null */
	std::optional<OracleWindow> window(std::string_view phase, int32_t actionId) const {
		for (const json& step : steps) {
			if (step.value("phase", std::string()) != phase)
				continue;
			const json& action = step.at("action");
			bool listed = false;
			if (action.is_array()) {
				for (const json& one : action)
					listed = listed || one.at("id").get<int32_t>() == actionId;
			} else if (action.is_object()) {
				listed = action.at("id").get<int32_t>() == actionId;
			}
			if (!listed)
				continue;
			const json& window = step.at("window");
			if (!window.is_object())
				return std::nullopt;
			return OracleWindow{window.at("page").get<uint16_t>(), window.at("questId").get<int32_t>()};
		}
		return std::nullopt;
	}
};

QuestAnswer parseQuest(const std::string& text) {
	const json root = json::parse(text);
	QuestAnswer answer;
	answer.questId = root.at("questId").get<int32_t>();
	const json& handler = root.at("handler");
	answer.inCppRegistry = handler.value("inCppRegistry", false);
	if (handler.contains("xml") && handler.at("xml").is_object())
		answer.kind = handler.at("xml").value("kind", std::string());
	const json& targets = root.at("targets");
	if (targets.contains("talk")) {
		for (const json& npc : targets.at("talk").at("start"))
			answer.startNpcs.push_back(npc.get<int32_t>());
		for (const json& npc : targets.at("talk").at("end"))
			answer.endNpcs.push_back(npc.get<int32_t>());
	}
	answer.race = root.at("prerequisites").value("race", std::string());
	const json& first = root.at("rewards").at("firstCompletion");
	answer.kinah = first.value("kinah", int64_t{0});
	answer.exp = first.value("exp", int64_t{0});
	for (const json& item : first.at("items"))
		answer.items.emplace_back(item.at("itemId").get<int32_t>(), item.at("count").get<int64_t>());
	for (const json& kill : root.at("kills")) {
		for (const json& npc : kill.at("npcIds"))
			answer.killNpcIds.push_back(npc.get<int32_t>());
		answer.killCount += kill.at("count").get<int32_t>();
	}
	const json& followUp = root.at("followUp");
	const json& character = followUp.at("character");
	for (const json& level : character.at("levelsSinceEnterWorld"))
		answer.levelsSinceEnterWorld.push_back(level.get<int32_t>());
	answer.expBeforeReward = character.value("expBeforeReward", int64_t{0});
	answer.expAfterReward = character.value("expAfterReward", int64_t{0});
	const json& atEnd = followUp.at("atEndNpcs");
	if (atEnd.is_array() && !atEnd.empty()) {
		const json& end = atEnd.front();
		if (end.at("followUp").is_number())
			answer.followUp = end.at("followUp").get<int32_t>();
		if (end.at("window").is_object())
			answer.followUpWindow = OracleWindow{end.at("window").at("page").get<uint16_t>(), end.at("window").at("questId").get<int32_t>()};
	}
	answer.steps = root.at("steps");
	return answer;
}

/**
 * What a new character of `creation` holds of `itemId` (m5a-creation's `items`, the kinah row included): the starter count the Y12 and Y13
 * ledgers begin with. 0 when the class starts without the item.
 */
int64_t starterCount(const OracleCreation& creation, int32_t itemId) {
	int64_t count = 0;
	for (const OracleItem& item : creation.items)
		if (item.itemId == itemId)
			count += item.count;
	return count;
}

/** the first spot of `npcId` in an m5a-spawns answer (the four quest npcs have one spot each on their map, `spawnMaps` in m5d-quest) */
std::optional<OracleSpot> spotOf(const OracleSpawns& spawns, int32_t npcId) {
	for (const OracleSpot& spot : spawns.spots)
		if (spot.npcId == npcId && spot.spawned)
			return spot;
	return std::nullopt;
}

/** one spot the gate fights at: the npc template standing there and the spot (m5b-monster) */
struct KillSpot {
	int32_t templateId = 0;
	OracleMonsterSpot spot;
};

/**
 * The `count` fixed, spawned plain spots (static id 0, m5b-plan.md D11) of the monsters nearest to (x, y), nearest first: where the gate
 * fights for its kill count. Several spots let a kill take the npc that stands at another spot instead of waiting at one spot for its
 * respawn (12-15 s a kill).
 */
std::vector<KillSpot> nearestPlainSpots(const std::vector<OracleMonster>& monsters, float x, float y, size_t count) {
	std::vector<std::pair<double, KillSpot>> spots;
	for (const OracleMonster& monster : monsters)
		for (const OracleMonsterSpot& spot : monster.spots)
			if (spot.fixed && spot.spawned && spot.staticId == 0)
				spots.push_back({distance2d(spot.x, spot.y, x, y), KillSpot{monster.npcId, spot}});
	std::ranges::stable_sort(spots, {}, [](const auto& entry) { return entry.first; });
	std::vector<KillSpot> nearest;
	for (size_t i = 0; i < spots.size() && i < count; i++)
		nearest.push_back(spots[i].second);
	return nearest;
}

/** how many of the nearest spots the kills rotate over (KillSpot) */
constexpr size_t KILL_SPOTS = 3;

// ---- the scenario clients ---------------------------------------------------------------------------------------------------------------

struct ScenarioClient {
	std::string label; // "A" (Elyos) or "B" (Asmodian)
	std::string account;
	std::string password = "m5dPassword1";
	std::string name;
	bool asmodian = false;
	std::unique_ptr<FakeLoginClient> login;
	std::unique_ptr<GameSession> game;
	FakeLoginClient::SessionKey key;
	int32_t playerId = 0;
	InventoryModel model;
	AnnouncedNpcs npcs;
	AsyncAllowed async = AsyncAllowed::m5aDefault();
	/** the walk cursor: where the server has the character after its last CM_MOVE (or its spawn) */
	float x = 0, y = 0, z = 0;
	/** the last enter-world and level-ready bursts */
	std::vector<Packet> lastEnterWorld, lastLevelReady;
	/** the npc labels of the SM_DIALOG_WINDOW tokens: object id -> "elpas" ... */
	std::map<int32_t, std::string> labels;
	/**
	 * Every exp the character gained, in order, as STR_GET_EXP carried it (quest rewards and kills) - the "exp events recorded so far" the
	 * oracle places the level-up with (§10.2 C0). expScanned is how far into this session's recording they were read.
	 */
	std::vector<int64_t> expEvents;
	size_t expScanned = 0;

	size_t mark() const { return game ? game->recorded().size() : 0; }
	std::vector<Packet> since(size_t from) const { return game ? slice(*game, from) : std::vector<Packet>{}; }
	int64_t kinah() {
		model.sync();
		return model.kinah();
	}
	int64_t countOf(int32_t itemId) {
		model.sync();
		int64_t count = 0;
		for (const auto& [id, item] : model.items)
			if (item.itemId == itemId)
				count += item.count;
		return count;
	}
	/** reads the STR_GET_EXP of this session's recording that were not read yet into expEvents */
	void scanExp() {
		if (!game)
			return;
		const std::vector<Packet>& packets = game->recorded();
		for (; expScanned < packets.size(); expScanned++) {
			if (packets[expScanned].name != "SM_SYSTEM_MESSAGE")
				continue;
			try {
				const SystemMessage message = decodeSystemMessage(packets[expScanned].data);
				if (message.messageId == STR_GET_EXP && message.params.size() >= 2)
					expEvents.push_back(std::stoll(message.params[1]));
			} catch (const std::exception&) {
				// a message that does not decode is not an exp event; the row that reads it fails on its own
			}
		}
	}
	int64_t totalExp() {
		scanExp();
		int64_t total = 0;
		for (const int64_t exp : expEvents)
			total += exp;
		return total;
	}
	/** the character's HP from its last SM_STATUPDATE_HP (the packet is its own, SM_STATUPDATE_HP.java) */
	std::optional<decoders::StatUpdateHp> lastHp() const {
		if (!game)
			return std::nullopt;
		const std::vector<Packet>& packets = game->recorded();
		for (size_t i = packets.size(); i-- > 0;)
			if (packets[i].name == "SM_STATUPDATE_HP") {
				try {
					return decoders::decodeStatUpdateHp(packets[i].data);
				} catch (const DecodeError&) {
					return std::nullopt;
				}
			}
		return std::nullopt;
	}
};

/** m5a-plan.md §5.2: the login server conversation and the game server login up to SM_CHARACTER_LIST */
decoders::CharacterList logIn(ScenarioServers& servers, ScenarioClient& client) {
	client.login = std::make_unique<FakeLoginClient>(servers.loginClientPort());
	client.login->login(client.account, client.password);
	const FakeLoginClient::ServerList list = client.login->requestServerList();
	bool listed = false;
	for (const FakeLoginClient::GameServerEntry& entry : list.servers)
		if (entry.id == 1)
			listed = entry.online;
	EXPECT_TRUE(listed) << client.label << ": game server 1 is not listed as online (" << list.servers.size() << " servers)";
	client.key = client.login->play(1);

	client.game = std::make_unique<GameSession>(servers.gameClientPort());
	client.model.follow(client.game.get());
	client.npcs.follow(client.game.get());
	client.expScanned = 0;
	client.async = AsyncAllowed::m5aDefault();
	client.game->readKey();
	client.game->send(GameSession::CM_VERSION_CHECK, GameSession::buildCM_VERSION_CHECK());
	expectNext(*client.game, "SM_VERSION_CHECK", client.async);
	client.game->send(GameSession::CM_L2AUTH_LOGIN_CHECK,
	                  GameSession::buildCM_L2AUTH_LOGIN_CHECK(client.key.playOk2, client.key.playOk1, client.key.accountId, client.key.loginOk));
	client.game->send(GameSession::CM_MAC_ADDRESS, GameSession::buildCM_MAC_ADDRESS());
	expectNext(*client.game, "SM_L2AUTH_LOGIN_CHECK", client.async);
	client.game->send(GameSession::CM_TIME_CHECK, GameSession::buildCM_TIME_CHECK(1));
	expectNext(*client.game, "SM_AFTER_TIME_CHECK_4_7_5", client.async);
	expectNext(*client.game, "SM_TIME_CHECK", client.async);
	client.game->send(GameSession::CM_CHARACTER_LIST, GameSession::buildCM_CHARACTER_LIST(client.key.playOk2));
	expectNext(*client.game, "SM_ACCOUNT_PROPERTIES", client.async);
	return decoders::decodeCharacterList(expectNext(*client.game, "SM_CHARACTER_LIST", client.async).data);
}

/** CM_ENTER_WORLD and its burst (the §5.8 sequence); the character must be let in (SM_ENTER_WORLD_CHECK 0) and spawned. @return the burst */
std::vector<Packet> enterWorld(ScenarioClient& client, bool firstEnter) {
	client.async = AsyncAllowed::m5aDefault();
	client.async.selfPlayerState(client.playerId);
	client.async.npcActivity(client.npcs.predicate());
	client.game->send(GameSession::CM_ENTER_WORLD, GameSession::buildCM_ENTER_WORLD(client.playerId));
	std::vector<Packet> burst = collectAnswer(*client.game, client.async, 30s);
	if (burst.empty())
		throw std::runtime_error(client.label + ": no packet after CM_ENTER_WORLD");
	const std::optional<uint8_t> check = enterWorldCheck(burst);
	if (!check || *check != ENTER_WORLD_OK)
		throw std::runtime_error(client.label + ": the enter world was refused (SM_ENTER_WORLD_CHECK " +
		                         (check ? std::to_string(*check) : std::string("missing")) + "): " + join(namesOf(burst)));
	const Packet* spawn = firstOfName(burst, "SM_PLAYER_SPAWN");
	if (spawn == nullptr)
		throw std::runtime_error(client.label + ": no SM_PLAYER_SPAWN after CM_ENTER_WORLD: " + join(namesOf(burst)));
	const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(spawn->data);
	client.x = spawned.x;
	client.y = spawned.y;
	client.z = spawned.z;
	expectSequence(burst, enterWorldPattern(firstEnter), client.async, client.label + " enter world");
	client.model.sync();
	client.lastEnterWorld = burst;
	return burst;
}

/**
 * CM_LEVEL_READY and its burst. Only a first enter asserts the §5.8 sequence (as M5b3ScenarioTest.cpp's levelReady(true)): a relog near the
 * gate's monster spots reads their activity - an npc's SM_DELETE when a corpse decays - inside the burst, which the async set does not
 * explain and which is not the quest engine's
 */
std::vector<Packet> levelReady(ScenarioClient& client, bool assertSequence) {
	client.game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
	std::vector<Packet> burst = collectAnswer(*client.game, client.async, 30s);
	if (burst.empty())
		throw std::runtime_error(client.label + ": no packet after CM_LEVEL_READY");
	if (assertSequence)
		expectSequence(burst, levelReadyPattern(), client.async, client.label + " level ready");
	client.model.sync();
	client.lastLevelReady = burst;
	return burst;
}

/** CM_QUIT(0): the character leaves the world and the connection ends - the only state in which a database read sees its last save */
void disconnect(ScenarioClient& client) {
	if (!client.game)
		return;
	client.scanExp();
	client.game->send(GameSession::CM_QUIT, GameSession::buildCM_QUIT(false));
	waitFor(*client.game, "SM_QUIT_RESPONSE", 30s);
	if (!client.game->waitClosed(30s))
		throw std::runtime_error(client.label + ": the socket stayed open after CM_QUIT(0)");
	client.model.sync();
	client.npcs.follow(nullptr);
	client.model.follow(nullptr);
	client.game.reset();
	client.login.reset();
}

/** a new login and the stored character into the world (M5a's Q5). @return the character list and the enter-world burst */
std::pair<decoders::CharacterList, std::vector<Packet>> relogIn(ScenarioServers& servers, ScenarioClient& client) {
	decoders::CharacterList list = logIn(servers, client);
	client.game->send(GameSession::CM_MAY_LOGIN_INTO_GAME, GameSession::buildCM_MAY_LOGIN_INTO_GAME());
	expectNext(*client.game, "SM_MAY_LOGIN_INTO_GAME", client.async);
	std::this_thread::sleep_for(1500ms); // gameserver.character.reentry.time is 1 s in the scenario profile
	std::vector<Packet> burst = enterWorld(client, false);
	levelReady(client, false);
	return {std::move(list), std::move(burst)};
}

/** the walk of the M5b gates: 5 m steps, each followed by a short read, then a stop */
void walkTo(ScenarioClient& client, float toX, float toY, float toZ) {
	const double total = distance2d(client.x, client.y, toX, toY);
	const int32_t steps = std::max(1, static_cast<int32_t>(total / 5.0));
	const float fromX = client.x, fromY = client.y, fromZ = client.z;
	for (int32_t step = 1; step <= steps; step++) {
		const float t = static_cast<float>(step) / static_cast<float>(steps);
		client.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(fromX + (toX - fromX) * t, fromY + (toY - fromY) * t,
		                                                                    fromZ + (toZ - fromZ) * t, 0, static_cast<int8_t>(0xE0), toX, toY, toZ));
		collectFor(*client.game, 120ms);
	}
	client.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(toX, toY, toZ, 0, 0));
	client.x = toX;
	client.y = toY;
	client.z = toZ;
	collectFor(*client.game, 1s);
	client.model.sync();
}

/** a point `distance` metres from (x, y, z) on the line towards (fromX, fromY) */
std::array<float, 3> pointNear(float x, float y, float z, double distance, float fromX, float fromY) {
	const double dx = fromX - x, dy = fromY - y;
	const double length = std::sqrt(dx * dx + dy * dy);
	const double scale = length <= 0.001 ? 0.0 : distance / length;
	return {static_cast<float>(x + dx * scale), static_cast<float>(y + dy * scale), z};
}

/** the object id of the npc of `templateId` nearest to (x, y) within 5 m, from every SM_NPC_INFO this session recorded (the npcs are fixed) */
std::optional<int32_t> npcObject(const ScenarioClient& client, int32_t templateId, float x, float y) {
	std::optional<int32_t> found;
	double best = 5.0;
	if (!client.game)
		return found;
	for (const Packet& packet : client.game->recorded()) {
		if (packet.name != "SM_NPC_INFO")
			continue;
		try {
			const decoders::NpcInfo npc = decoders::decodeNpcInfo(packet.data);
			if (npc.templateId != templateId)
				continue;
			const double distance = distance2d(npc.x, npc.y, x, y);
			if (distance < best) {
				best = distance;
				found = npc.objectId;
			}
		} catch (const DecodeError&) {
			// a packet that does not decode is not this npc
		}
	}
	return found;
}

/**
 * The packets of a talk window that are not news: the async set and an announced npc turning to or from anyone (onSimpleTalk sets the
 * dialog npc's target, and NpcController.onTargetChanged broadcasts SM_LOOKATOBJECT with the talker as the target, NpcController.java:78-93;
 * the async set only allows it for an npc target) - M5cScenarioTest.cpp's isNoise, one character online at a time
 */
bool isNoise(ScenarioClient& client, const Packet& packet) {
	if (client.async.allows(packet.name, std::span<const uint8_t>(packet.data)))
		return true;
	try {
		if (packet.name == "SM_LOOKATOBJECT")
			return client.npcs.contains(decoders::decodeLookAtObject(packet.data).objectId);
	} catch (const DecodeError&) {
		return false;
	}
	return false;
}

std::vector<Packet> news(ScenarioClient& client, const std::vector<Packet>& packets) {
	std::vector<Packet> result;
	for (const Packet& packet : packets)
		if (!isNoise(client, packet))
			result.push_back(packet);
	return result;
}

// ---- the check output reports (Y14) -------------------------------------------------------------------------------------------------------

/** One section of m5d_partial_allowlist.txt: §A hit at least once, §B hit exactly zero times, §C counted but not pinned */
enum class AllowlistSection { HitAtLeastOnce, HitNever, NotPinned };

struct AllowlistEntry {
	std::string site;
	AllowlistSection section = AllowlistSection::NotPinned;
};

/** Reads tests/scenario/m5d_partial_allowlist.txt with its three sections ("# --- SECTION A/B/C" marker lines, as the M5b lists) */
std::vector<AllowlistEntry> readAllowlist() {
	std::vector<AllowlistEntry> entries;
	std::ifstream in(AION_SCENARIO_M5D_PARTIAL_ALLOWLIST, std::ios::binary);
	std::string line;
	AllowlistSection section = AllowlistSection::NotPinned;
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (line.starts_with("# --- SECTION A"))
			section = AllowlistSection::HitAtLeastOnce;
		else if (line.starts_with("# --- SECTION B"))
			section = AllowlistSection::HitNever;
		else if (line.starts_with("# --- SECTION C"))
			section = AllowlistSection::NotPinned;
		if (line.empty() || line.starts_with('#'))
			continue;
		entries.push_back({line, section});
	}
	return entries;
}

std::string_view sectionName(AllowlistSection section) {
	switch (section) {
		case AllowlistSection::HitAtLeastOnce:
			return "A";
		case AllowlistSection::HitNever:
			return "B";
		default:
			return "C";
	}
}

/** an entry WITH a line number must match the whole site; one without is an explicit whole-file wildcard */
bool allowlistEntryMatches(const std::string& entry, const std::string& site) {
	if (entry.find(':') != std::string::npos)
		return entry == site;
	return site.starts_with(entry) && (site.size() == entry.size() || site[entry.size()] == ':');
}

struct PartialHit {
	int64_t hits = 0;
	std::string site;
	std::string line;
};

std::vector<PartialHit> readPartialHits(const ScenarioServers& servers) {
	std::vector<PartialHit> hits;
	for (const std::string& line : servers.readReportLines("partial_trace.txt")) {
		const size_t first = line.find('\t');
		if (first == std::string::npos)
			continue;
		const size_t second = line.find('\t', first + 1);
		PartialHit hit;
		hit.line = line;
		hit.site = line.substr(first + 1, second == std::string::npos ? std::string::npos : second - first - 1);
		try {
			hit.hits = std::stoll(line.substr(0, first));
		} catch (const std::exception&) {
			continue;
		}
		hits.push_back(hit);
	}
	return hits;
}

struct LiveCount {
	int64_t live = 0;
	int64_t created = 0;
	std::string line;
};

/** "<live>\t<created>\t<qualified class name>" rows of live_counts.txt, keyed by the unqualified name */
std::vector<std::pair<std::string, LiveCount>> readLiveCounts(const ScenarioServers& servers, std::string_view fileName) {
	std::vector<std::pair<std::string, LiveCount>> counts;
	for (const std::string& line : servers.readReportLines(fileName)) {
		const size_t firstTab = line.find('\t');
		const size_t lastTab = line.rfind('\t');
		if (firstTab == std::string::npos || lastTab == firstTab)
			continue;
		const std::string qualified = line.substr(lastTab + 1);
		const size_t colons = qualified.rfind("::");
		LiveCount count;
		count.line = line;
		try {
			count.live = std::stoll(line.substr(0, firstTab));
			count.created = std::stoll(line.substr(firstTab + 1, lastTab - firstTab - 1));
		} catch (const std::exception&) {
			continue;
		}
		counts.emplace_back(colons == std::string::npos ? qualified : qualified.substr(colons + 2), count);
	}
	return counts;
}

/** The end of a run: the two test schemas are dropped, and a failed run says where its evidence is (the M5a finishRun) */
void finishRun(ScenarioServers& servers, const std::filesystem::path& outputDir, std::string_view testName) {
	for (const std::string& problem : servers.stopProblemsReported())
		ADD_FAILURE() << testName << ": " << problem;
	const bool failed = ::testing::Test::HasFailure();
	if (failed)
		std::cout << testName << " failed.\n"
		          << "logs: " << (outputDir / "game_server.log") << ", " << (outputDir / "login_server.log") << "\n"
		          << "reports: " << servers.checkOutputDir() << std::endl;
	if (failed && ScenarioServers::keepSchemasOnFailure()) {
		std::cout << "the scenario schemas " << servers.gameSchema() << " and " << servers.loginSchema()
		          << " were kept for the post mortem (AION_SCENARIO_KEEP_SCHEMAS is set)" << std::endl;
		return;
	}
	try {
		servers.dropSchemas();
		if (failed)
			std::cout << "the scenario schemas " << servers.gameSchema() << " and " << servers.loginSchema()
			          << " were dropped; set AION_SCENARIO_KEEP_SCHEMAS=1 and run the gate again to keep them" << std::endl;
	} catch (const std::exception& exception) {
		std::cout << "the scenario schemas could not be dropped (" << exception.what() << ")" << std::endl;
	}
}

/** one player_quests row (quest_id, status, quest_vars, complete_count) as the database holds it after a quit */
struct QuestRow {
	int32_t questId = 0;
	std::string status;
	int64_t vars = 0;
	int64_t completeCount = 0;
	bool operator==(const QuestRow&) const = default;
};

/** GoogleTest prints a QuestRow of a failed comparison with this (it would print the object's bytes otherwise) */
void PrintTo(const QuestRow& row, std::ostream* out) {
	*out << row.questId << ":" << row.status << ":v" << row.vars << ":c" << row.completeCount;
}

std::string describe(const std::vector<QuestRow>& rows) {
	std::vector<std::string> texts;
	for (const QuestRow& row : rows)
		texts.push_back(std::to_string(row.questId) + ":" + row.status + ":v" + std::to_string(row.vars) + ":c" + std::to_string(row.completeCount));
	return "[" + join(texts) + "]";
}

std::vector<QuestRow> questRows(const ScenarioDatabase& database, const std::string& schema, int32_t playerId) {
	std::vector<QuestRow> rows;
	for (const auto& row : database.queryRows(schema,
	                                          "SELECT quest_id, status, quest_vars, complete_count FROM player_quests WHERE player_id = " +
	                                            std::to_string(playerId) + " ORDER BY quest_id",
	                                          4)) {
		QuestRow quest;
		quest.questId = std::stoi(row[0].value_or("0"));
		quest.status = row[1].value_or("");
		quest.vars = std::stoll(row[2].value_or("0"));
		quest.completeCount = std::stoll(row[3].value_or("0"));
		rows.push_back(quest);
	}
	return rows;
}

std::string describe(const decoders::QuestList& list) {
	std::vector<std::string> texts;
	for (const decoders::QuestEntry& entry : list.quests)
		texts.push_back(std::to_string(entry.questId) + ":s" + std::to_string(entry.status) + ":v" + std::to_string(entry.questVarsAndFlags) + ":c" +
		                std::to_string(entry.completeCount));
	return "[" + join(texts) + "]";
}

std::string describe(const decoders::QuestCompletedList& list) {
	std::vector<std::string> texts;
	for (const decoders::QuestCompletedEntry& entry : list.quests)
		texts.push_back(std::to_string(entry.questId) + ":c" + std::to_string(entry.completeCount) + ":r" + std::to_string(entry.repeatFlag));
	return "[" + join(texts) + "]";
}

/** the enter world's quest lists: SM_QUEST_LIST, and every SM_QUEST_COMPLETED_LIST of the burst merged (updateMode 1 inserts) */
struct EnterWorldQuests {
	std::optional<decoders::QuestList> list;
	std::vector<decoders::QuestCompletedEntry> completed;
};

EnterWorldQuests enterWorldQuests(const std::vector<Packet>& burst, std::string_view row) {
	EnterWorldQuests quests;
	for (const Packet& packet : burst) {
		try {
			if (packet.name == "SM_QUEST_LIST" && !quests.list)
				quests.list = decoders::decodeQuestList(packet.data);
			else if (packet.name == "SM_QUEST_COMPLETED_LIST")
				for (const decoders::QuestCompletedEntry& entry : decoders::decodeQuestCompletedList(packet.data).quests)
					quests.completed.push_back(entry);
		} catch (const DecodeError& error) {
			ADD_FAILURE() << row << ": " << packet.name << " does not decode: " << error.what();
		}
	}
	return quests;
}

/** the SM_INVENTORY_INFO stacks of an enter-world burst: the count of one item id over every stack */
int64_t enterWorldCount(const std::vector<Packet>& burst, int32_t itemId, std::string_view row) {
	int64_t count = 0;
	for (const Packet& packet : burst) {
		if (packet.name != "SM_INVENTORY_INFO")
			continue;
		try {
			for (const decoders::InventoryItem& item : decoders::decodeInventoryInfo(packet.data).items)
				if (item.templateId == itemId && item.general)
					count += item.general->count;
		} catch (const DecodeError& error) {
			ADD_FAILURE() << row << ": SM_INVENTORY_INFO does not decode: " << error.what();
		}
	}
	return count;
}

// ---- the gate ---------------------------------------------------------------------------------------------------------------------------

/** What separates gs.scenario.m5d from gs.scenario.m5d_geo */
struct GateVariant {
	bool geodata = false;
	std::string testName;     // gs.scenario.m5d
	std::string outputSubdir; // m5d
	std::string schemaPrefix; // m5d
	std::string accountPrefix;
};

/** One kill of the gate: the npc, its object and where its fight and its death are in the recording */
struct KillRecord {
	int32_t templateId = 0;
	int32_t objectId = 0;
	size_t fightFrom = 0, dieIndex = 0;
	/** the packets from the fight's start to one second after the death: the quest update of the kill arrives in that burst */
	std::vector<Packet> packets;
};

void runM5dGate(const GateVariant& variant) {
	// A skipped gate is NOT a passed gate (m5a-plan.md §5.10): AION_SCENARIO_REQUIRE=1 - the default of the CTest registration - turns every
	// skip reason into a failure that names the variable.
	const char* requireEnvironment = std::getenv("AION_SCENARIO_REQUIRE");
	const bool required = requireEnvironment != nullptr && *requireEnvironment != '\0' && std::string_view(requireEnvironment) != "0";
	const auto unavailable = [&](std::string_view reason) {
		if (required)
			ADD_FAILURE() << variant.testName << " was not configured and AION_SCENARIO_REQUIRE is set: " << reason;
		else
			GTEST_SKIP() << variant.testName << ": skipped (" << reason << ")";
	};

	std::optional<ScenarioEnvironment> environment = ScenarioEnvironment::fromEnvironment();
	if (!environment) {
		unavailable("set AION_TEST_GS_DATABASE_URL and AION_TEST_LS_DATABASE_URL");
		return;
	}
	const std::filesystem::path outputDir = std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / variant.outputSubdir;
	std::optional<Oracle> oracle = Oracle::fromEnvironment(outputDir / "oracle");
	if (!oracle) {
		unavailable("no Python interpreter for tools/oracle: set AION_TEST_PYTHON");
		return;
	}
	if (variant.geodata) {
		const std::filesystem::path geoDirectory = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data" / "geo";
		size_t geoFiles = 0;
		if (std::filesystem::is_directory(geoDirectory))
			for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(geoDirectory))
				if (entry.path().extension() == ".geo")
					geoFiles++;
		if (geoFiles == 0) {
			unavailable("the Java game server checkout has no data/geo/*.geo files, which this gate exists to run against");
			return;
		}
		std::cout << variant.testName << ": " << geoFiles << " .geo files in " << geoDirectory << std::endl;
	}

	CaseLog cases;
	struct ReportPrinter {
		const CaseLog& cases;
		const std::string& testName;
		~ReportPrinter() { std::cout << cases.report(testName) << std::flush; }
	} printer{cases, variant.testName};

	// ---- §10.1 processes, databases and profile ----
	// The M5a set comes from ScenarioServers::m5aProfile (with G-07's far-future wall-clock schedules); then the M5b-2 profile's keys (the
	// M5b-1 ones and the drop rate 0, so a kill leaves no loot) and what M5d adds: the quest rates written out at their defaults (membership
	// 0 -> 1.0, D7), the spawn analysis off (so QuestEngine.cpp:115 is a §B row) and the simple class window off (§18.4: the gate must not
	// depend on the owner's mygs.properties, which turns it on; with it off the four ascension handlers register, as in Java's default).
	// gameserver.character.creation.mode stays at its default 0 (D15: the Asmodian plays on account B). Geodata separates the variants.
	std::map<std::string, std::string> gateKeys;
	gateKeys["gameserver.geodata.enable"] = variant.geodata ? "true" : "false";
	gateKeys["gameserver.npcshouts.enable"] = "false";
	gateKeys["gameserver.rates.xp.solo"] = "1.0, 2.0";
	gateKeys["gameserver.soulsickness.disable"] = "10";
	gateKeys["gameserver.rates.drop"] = "0";
	gateKeys["gameserver.rates.xp.quest"] = "1.0, 2.0";
	gateKeys["gameserver.rates.kinah.quest"] = "1.0, 2.0";
	gateKeys["gameserver.analysis.quest_handlers"] = "false";
	gateKeys["gameserver.simple.secondclass.enable"] = "false";

	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.schemaPrefix = variant.schemaPrefix;
	config.gameServerProperties = gateKeys;
	config.startupTimeout = variant.geodata ? 25min : 10min;
	config.stopTimeout = 3min;
	ScenarioServers servers(config, *environment);
	const std::string schema = servers.gameSchema();
	const ScenarioDatabase& database = servers.gameDatabase();

	// The oracles' profile: the server's own -D keys (m5aProfile under the gate's keys) as the override file m5d-quest reads over
	// config/{administration,main,network} (Config.loadProperties' layering), so an expectation never reads the owner's mygs.properties
	std::filesystem::create_directories(outputDir);
	const std::filesystem::path profileFile = outputDir / "m5d_oracle_profile.properties";
	{
		std::map<std::string, std::string> keys = ScenarioServers::m5aProfile();
		for (const auto& [key, value] : gateKeys)
			keys[key] = value;
		std::ofstream out(profileFile, std::ios::binary | std::ios::trunc);
		for (const auto& [key, value] : keys)
			out << key << " = " << value << "\n";
	}
	const std::vector<std::string> profileArguments{"--profile", profileFile.string()};
	const auto questOracle = [&](std::vector<std::string> arguments) {
		arguments.insert(arguments.end(), profileArguments.begin(), profileArguments.end());
		return oracle->run(arguments);
	};

	bool ok = true;
	const auto runCase = [&](std::string_view id, std::string_view title, const std::function<void()>& body) {
		if (!ok)
			cases.skip(id, title, "an earlier case ended with an exception or a fatal failure");
		else
			ok = cases.run(id, title, body);
	};

	ok = cases.run("S-0", "the servers start (§10.1)", [&] {
		servers.createSchemas();
		servers.startLoginServer();
		try {
			servers.startGameServer();
		} catch (const std::exception& exception) {
			std::vector<std::string> diagnosis;
			diagnosis.push_back(exception.what());
			ChildProcess* gameServer = servers.gameServer();
			if (gameServer != nullptr) {
				const std::vector<std::string> steps = gameServer->findLogLines("startup step ", 1000);
				if (!steps.empty())
					diagnosis.push_back("last startup step: " + steps.back());
				for (const std::string& line : gameServer->findLogLines(" ERROR ", 5))
					diagnosis.push_back(line);
			}
			throw std::runtime_error(join(diagnosis, "\n  "));
		}
	});

	const std::string suffix = servers.gameSchema().substr(servers.gameSchema().size() - 8);
	ScenarioClient a;
	a.label = "A";
	a.account = variant.accountPrefix + "a" + suffix;
	a.name = "Questelyos";
	ScenarioClient b;
	b.label = "B";
	b.account = variant.accountPrefix + "b" + suffix;
	b.name = "Questasmo";
	b.asmodian = true;

	// ---- C0: the oracles answer (§10.2 C0, G-01), and the plan's premises are re-derived from them ----
	OracleCreation elyos, asmodian;
	OracleSpawns poetaSpawns, ishalgenSpawns;
	OracleSpot elpasSpot, miresSpot, asakSpot, vandarSpot;
	NearbyAnswer poetaNearby, ishalgenNearby;
	QuestAnswer quest1101, quest2101;
	std::vector<OracleMonster> kerubs, spriggs;
	std::vector<KillSpot> kerubSpots, spriggSpots;
	int64_t levelTwoExpNeed = 0, levelOneExpNeed = 0;
	runCase("C0", "the oracles answer and the plan's premises hold (m5d-quests, m5d-quest, m5a-creation, m5a-spawns, m5b-monster)", [&] {
		elyos = oracle->creation("ELYOS", "WARRIOR");
		asmodian = oracle->creation("ASMODIANS", "WARRIOR");
		ASSERT_EQ(elyos.mapId, ELYOS_START_MAP);
		ASSERT_EQ(asmodian.mapId, ASMODIAN_START_MAP);
		std::cout << "C0: m5a-creation's starters: Elyos Warrior " << starterCount(elyos, KINAH_ITEM) << " kinah; Asmodian Warrior "
		          << starterCount(asmodian, KINAH_ITEM) << " kinah, " << starterCount(asmodian, BANDAGE) << " x " << BANDAGE << std::endl;
		poetaSpawns =oracle->spawns(ELYOS_START_MAP, elyos.x, elyos.y, elyos.z, 12, 120.0);
		ishalgenSpawns = oracle->spawns(ASMODIAN_START_MAP, asmodian.x, asmodian.y, asmodian.z, 12, 120.0);
		const std::optional<OracleSpot> elpas = spotOf(poetaSpawns, ELPAS), mires = spotOf(poetaSpawns, MIRES);
		const std::optional<OracleSpot> asak = spotOf(ishalgenSpawns, ASAK), vandar = spotOf(ishalgenSpawns, VANDAR);
		ASSERT_TRUE(elpas && mires) << "m5a-spawns has no spot of elpas (203049) or mires (203057) within 120 m of the Elyos spawn";
		ASSERT_TRUE(asak && vandar) << "m5a-spawns has no spot of asak (203500) or vandar (203504) within 120 m of the Asmodian spawn";
		elpasSpot = *elpas;
		miresSpot = *mires;
		asakSpot = *asak;
		vandarSpot = *vandar;
		for (const OracleSpot* spot : {&elpasSpot, &miresSpot, &asakSpot, &vandarSpot})
			EXPECT_TRUE(spot->deterministic) << "npc " << spot->npcId << " does not stand at a fixed spot (the talks walk to it)";

		// §10.3 Y1 / Y13: both start maps at level 1, from the XML registry and the Java handlers the C++ tree registers
		poetaNearby = parseNearby(questOracle({"m5d-quests", "--map", std::to_string(ELYOS_START_MAP), "--race", "ELYOS", "--level", "1"}));
		ishalgenNearby = parseNearby(questOracle({"m5d-quests", "--map", std::to_string(ASMODIAN_START_MAP), "--race", "ASMODIANS", "--level", "1"}));
		EXPECT_TRUE(poetaNearby.exact && ishalgenNearby.exact) << "the oracle cannot model a start map's nearby set exactly";
		EXPECT_TRUE(poetaNearby.expected.contains(Q1101)) << "D6: 1101 is offered at the first enter world";
		for (const int32_t absent : {Q1102, Q1103, Q1104})
			EXPECT_FALSE(poetaNearby.expected.contains(absent)) << "Y1's precondition rows: " << absent << " must not be startable yet";
		EXPECT_TRUE(ishalgenNearby.expected.contains(Q2101) && ishalgenNearby.expected.contains(Q2102))
		  << "D6: 2102 has no precondition, so 2101 and 2102 are both offered";
		std::cout << "C0: Poeta level 1: " << joinNumbers(poetaNearby.expected) << " (grey " << joinNumbers(poetaNearby.expectedGrey)
		          << "; Java-handled " << joinNumbers(poetaNearby.javaAdded) << "; XML only " << joinNumbers(poetaNearby.xmlOnly)
		          << "); Ishalgen level 1: " << joinNumbers(ishalgenNearby.expected) << " (grey " << joinNumbers(ishalgenNearby.expectedGrey)
		          << "; Java-handled " << joinNumbers(ishalgenNearby.javaAdded) << ")" << std::endl;

		// D6's chain: 1101 report_to elpas -> mires, whose follow-up is 1102; 2101 asak -> vandar with no follow-up (page 10)
		quest1101 = parseQuest(questOracle({"m5d-quest", "--quest", std::to_string(Q1101)}));
		quest2101 = parseQuest(questOracle({"m5d-quest", "--quest", std::to_string(Q2101)}));
		EXPECT_EQ(quest1101.kind, "report_to");
		EXPECT_TRUE(quest1101.inCppRegistry);
		EXPECT_EQ(quest1101.startNpcs, std::vector<int32_t>{ELPAS});
		EXPECT_EQ(quest1101.endNpcs, std::vector<int32_t>{MIRES});
		EXPECT_EQ(quest1101.followUp, std::optional<int32_t>(Q1102));
		EXPECT_EQ(quest2101.kind, "report_to");
		EXPECT_EQ(quest2101.startNpcs, std::vector<int32_t>{ASAK});
		EXPECT_EQ(quest2101.endNpcs, std::vector<int32_t>{VANDAR});
		EXPECT_FALSE(quest2101.followUp.has_value()) << "D6: no startable quest at vandar names 2101 as its <finished>";
		EXPECT_EQ(quest2101.followUpWindow, std::optional<OracleWindow>(OracleWindow{PAGE_SELECT_QUEST, 0}));

		// the monsters: every kill at level 1 pays the capped 80 (m5b-monster), and the gate fights at the fixed plain spot nearest the end npc
		for (const int32_t npcId : KERUBS)
			kerubs.push_back(oracle->monster(ELYOS_START_MAP, npcId, 1));
		for (const int32_t npcId : SPRIGG_WORKERS)
			spriggs.push_back(oracle->monster(ASMODIAN_START_MAP, npcId, 1));
		kerubSpots = nearestPlainSpots(kerubs, miresSpot.x, miresSpot.y, KILL_SPOTS);
		spriggSpots = nearestPlainSpots(spriggs, vandarSpot.x, vandarSpot.y, KILL_SPOTS);
		ASSERT_FALSE(kerubSpots.empty() || spriggSpots.empty()) << "no fixed plain spot of the kerubs or of the sprigg workers";
		levelOneExpNeed = kerubs.front().expNeed;
		levelTwoExpNeed = oracle->monster(ELYOS_START_MAP, KERUBS.front(), 2).expNeed;
		EXPECT_GT(levelTwoExpNeed, 0);
		const auto describeSpots = [](const std::vector<KillSpot>& spots, const OracleSpot& anchor) {
			std::vector<std::string> texts;
			for (const KillSpot& spot : spots)
				texts.push_back(std::to_string(spot.templateId) + " at (" + std::to_string(spot.spot.x) + ", " + std::to_string(spot.spot.y) + ") " +
				                std::to_string(distance2d(spot.spot.x, spot.spot.y, anchor.x, anchor.y)) + " m");
			return join(texts);
		};
		std::cout << "C0: elpas " << elpasSpot.distance << " m, mires " << miresSpot.distance << " m from the Elyos spawn; asak " << asakSpot.distance
		          << " m, vandar " << vandarSpot.distance << " m from the Asmodian spawn; kerubs " << describeSpots(kerubSpots, miresSpot)
		          << " from mires; sprigg workers " << describeSpots(spriggSpots, vandarSpot) << " from vandar; exp need level 1 " << levelOneExpNeed
		          << ", level 2 " << levelTwoExpNeed << std::endl;
	});

	/**
	 * The window of one client packet: sends it and reads until a quiet second, recording everything. The quiet is the burst collector's -
	 * measured from the last packet the async set does not explain - and not GameSession::talk's collectUntilQuiet, which ends at the first
	 * quiet second of ALL traffic: beside a walker or a fight between npcs that second never comes, and every talk ran into its 10 s limit.
	 * DialogService answers inside the packet's runImpl, so the answer is one burst either way (GameSession.h, talk).
	 */
	const auto exchange = [&](ScenarioClient& client, int32_t opcode, const std::vector<uint8_t>& body) {
		const size_t from = client.mark();
		client.game->send(opcode, body);
		collectBurst(*client.game, client.async, QUIET, 10s);
		client.model.sync();
		return client.since(from);
	};
	const auto showDialog = [&](ScenarioClient& client, int32_t npcObject) {
		return exchange(client, GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(npcObject));
	};
	/** talk(npcObjectId, action, questId) of G-02 with the burst collector above: CM_DIALOG_SELECT(npc, action, 0, 0, questId) */
	const auto dialogSelect = [&](ScenarioClient& client, int32_t npcObject, uint16_t action, int32_t questId) {
		return exchange(client, GameSession::CM_DIALOG_SELECT, GameSession::buildCM_DIALOG_SELECT(npcObject, action, 0, 0, questId));
	};
	const auto walkToTalk = [&](ScenarioClient& client, const OracleSpot& npc, double distance) {
		const std::array<float, 3> at = pointNear(npc.x, npc.y, npc.z, distance, client.x, client.y);
		walkTo(client, at[0], at[1], at[2]);
	};
	/** the npc's object id, announced by an SM_NPC_INFO at its spot (read on while it has not arrived) */
	const auto npcAt = [&](ScenarioClient& client, const OracleSpot& spot, std::string_view name) -> int32_t {
		std::optional<int32_t> object = npcObject(client, spot.npcId, spot.x, spot.y);
		if (!object) {
			readUntil(
			  *client.game,
			  [&](const Packet& packet) {
				  if (packet.name != "SM_NPC_INFO")
					  return false;
				  try {
					  const decoders::NpcInfo npc = decoders::decodeNpcInfo(packet.data);
					  return npc.templateId == spot.npcId && distance2d(npc.x, npc.y, spot.x, spot.y) < 5.0;
				  } catch (const DecodeError&) {
					  return false;
				  }
			  },
			  10s);
			object = npcObject(client, spot.npcId, spot.x, spot.y);
		}
		if (!object)
			throw std::runtime_error(client.label + ": no SM_NPC_INFO of " + std::string(name) + " (" + std::to_string(spot.npcId) + ")");
		client.labels[*object] = std::string(name);
		return *object;
	};
	/** the quest events of a window, with a decode failure reported against the row */
	const auto eventsOf = [&](ScenarioClient& client, const std::vector<Packet>& packets, std::string_view row) {
		client.model.sync();
		QuestEvents events = questEvents(packets, client.playerId, client.labels, [&client](int32_t objectId) {
			const std::optional<ModelItem> item = client.model.byObjectId(objectId);
			return item ? item->itemId : 0;
		});
		EXPECT_TRUE(events.decodeFailures.empty()) << row << ": " << join(events.decodeFailures, "; ");
		return events;
	};
	/** a nearby-quest set compared with the oracle's as sets, with bit 17 exactly on the grey ones (§10.3 Y1) */
	const auto expectNearbySet = [&](const decoders::NearbyQuests& nearby, const NearbyAnswer& answer, std::string_view row) {
		const std::vector<int32_t> ids = nearby.ids();
		const std::vector<int32_t> grey = nearby.notYetAvailableIds();
		EXPECT_EQ(toSet(ids), answer.expected) << row << ": SM_NEARBY_QUESTS lists " << joinNumbers(ids) << ", the oracle's set is "
		                                       << joinNumbers(answer.expected) << " (XML " << joinNumbers(answer.xmlOnly) << " + Java "
		                                       << joinNumbers(answer.javaAdded) << ")";
		EXPECT_EQ(toSet(grey), answer.expectedGrey) << row << ": the grey markers (bit 17) are " << joinNumbers(grey) << ", the oracle's "
		                                            << joinNumbers(answer.expectedGrey);
		EXPECT_EQ(ids.size(), toSet(ids).size()) << row << ": a quest id twice";
	};

	/**
	 * Waits until `client`'s HP is at least `fraction` of its maximum, reading packets (the regeneration arrives as SM_STATUPDATE_HP), for at
	 * most `timeout`. @return the HP last seen
	 */
	const auto restUntil = [&](ScenarioClient& client, double fraction, std::chrono::milliseconds timeout) {
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		for (;;) {
			const std::optional<decoders::StatUpdateHp> hp = client.lastHp();
			if (!hp || hp->maxHp <= 0 || hp->currentHp >= fraction * hp->maxHp || std::chrono::steady_clock::now() >= deadline)
				return hp;
			collectFor(*client.game, 1s);
		}
	};

	/**
	 * One kill of the §10.2 script: takes the npc of the first spot (nearest the end npc first) at which an npc stands that the gate has not
	 * killed - the latest SM_NPC_INFO at the spot that names none of the objects `killed` names: a respawn is a new object, and the corpse of
	 * an earlier kill keeps its id, also across a relog, which a recording index could not say (FightSupport.h's waitForRespawnAt reasons
	 * the same way inside one session) - or, when every spot's npc is dead, waits at the nearest spot for its respawn. It walks to 2 m from
	 * that spot, selects the npc and fights it until it dies (M5b's fight through GameSession::fightUntil), then reads one and a half seconds
	 * more for the quest update of the kill. Any other npc that attacks the character is not fought: an extra kill would move the Y11
	 * level-up, and the recorded exp events make that visible.
	 */
	const auto killAt = [&](ScenarioClient& client, const std::vector<KillSpot>& spots, const std::set<int32_t>& killed,
	                        std::chrono::milliseconds respawnWait) -> KillRecord {
		if (spots.empty())
			throw std::runtime_error("no kill spot");
		const auto matches = [&](const decoders::NpcInfo& npc, const KillSpot& at) {
			return npc.templateId == at.templateId && !killed.contains(npc.objectId) && std::abs(npc.x - at.spot.x) <= 0.01f &&
			       std::abs(npc.y - at.spot.y) <= 0.01f && std::abs(npc.z - at.spot.z) <= 0.01f;
		};
		const auto standingAt = [&](const KillSpot& at) -> std::optional<int32_t> {
			std::optional<int32_t> found;
			for (const Packet& packet : client.game->recorded()) {
				if (packet.name != "SM_NPC_INFO")
					continue;
				try {
					const decoders::NpcInfo info = decoders::decodeNpcInfo(packet.data);
					if (matches(info, at))
						found = info.objectId;
				} catch (const DecodeError&) {
					// not this npc
				}
			}
			return found;
		};
		const KillSpot* chosen = &spots.front();
		std::optional<int32_t> npc;
		for (const KillSpot& at : spots) {
			npc = standingAt(at);
			if (npc) {
				chosen = &at;
				break;
			}
		}
		const int32_t templateId = chosen->templateId;
		const OracleMonsterSpot& spot = chosen->spot;
		const std::array<float, 3> melee = pointNear(spot.x, spot.y, spot.z, MELEE_DISTANCE, client.x, client.y);
		walkTo(client, melee[0], melee[1], melee[2]);
		if (!npc) {
			const std::optional<size_t> index = readUntil(
			  *client.game,
			  [&](const Packet& packet) {
				  if (packet.name != "SM_NPC_INFO")
					  return false;
				  try {
					  return matches(decoders::decodeNpcInfo(packet.data), *chosen);
				  } catch (const DecodeError&) {
					  return false;
				  }
			  },
			  respawnWait);
			if (index)
				npc = decoders::decodeNpcInfo(client.game->recorded()[*index].data).objectId;
		}
		if (!npc)
			throw std::runtime_error(client.label + ": no SM_NPC_INFO of npc " + std::to_string(templateId) + " at (" + std::to_string(spot.x) + ", " +
			                         std::to_string(spot.y) + ")");
		client.game->send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(*npc));
		waitFor(*client.game, "SM_TARGET_SELECTED", 10s);
		std::optional<decoders::StatsInfo> stats;
		for (const Packet& packet : client.game->recorded())
			if (packet.name == "SM_STATS_INFO")
				stats = decoders::decodeStatsInfo(packet.data);
		if (!stats)
			throw std::runtime_error("no SM_STATS_INFO recorded");
		KillRecord kill;
		kill.templateId = templateId;
		kill.objectId = *npc;
		bool died = false, characterDied = false;
		const GameSession::FightOutcome outcome = client.game->fightUntil(
		  *npc, std::chrono::milliseconds(stats->attackSpeed),
		  [&](const Packet& packet) {
			  if (packet.name != "SM_EMOTION")
				  return false;
			  try {
				  const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
				  if (emotion.emotionType != decoders::EMOTION_DIE)
					  return false;
				  died = died || emotion.senderObjectId == *npc;
				  characterDied = characterDied || emotion.senderObjectId == client.playerId;
				  return died || characterDied;
			  } catch (const DecodeError&) {
				  return false;
			  }
		  },
		  120s, 80);
		kill.fightFrom = outcome.firstPacket;
		kill.dieIndex = client.game->recorded().size() - 1;
		if (characterDied)
			throw std::runtime_error(client.label + ": npc " + std::to_string(templateId) + " killed the character");
		if (!died)
			throw std::runtime_error(client.label + ": npc " + std::to_string(templateId) + " did not die within " +
			                         std::to_string(outcome.elapsed.count()) + " ms");
		collectFor(*client.game, 1500ms);
		kill.packets = client.since(kill.fightFrom);
		client.scanExp();
		client.model.sync();
		const std::optional<decoders::StatUpdateHp> hp = client.lastHp();
		std::cout << client.label << ": killed npc " << templateId << " (" << *npc << ") in " << outcome.elapsed.count() << " ms, "
		          << outcome.attacksSent << " attacks; HP " << (hp ? std::to_string(hp->currentHp) + "/" + std::to_string(hp->maxHp) : "?")
		          << "; exp so far " << joinNumbers(client.expEvents) << std::endl;
		return kill;
	};
	/** the quest's SM_QUEST_ACTIONs of a kill's window, as tokens */
	const auto killTokens = [&](ScenarioClient& client, const KillRecord& kill, std::string_view row) {
		std::vector<std::string> tokens;
		const QuestEvents events = eventsOf(client, kill.packets, row);
		for (const std::string& token : events.tokens)
			if (token.starts_with("ADD ") || token.starts_with("UPDATE ") || token.starts_with("ABANDON ") || token == "EMPTY")
				tokens.push_back(token);
		return tokens;
	};

	// ======================================================================================================================================
	// The Elyos Warrior on account A (C1-C15)
	// ======================================================================================================================================

	int32_t elpasObject = 0, miresObject = 0;
	runCase("C1-C3", "account A: login, create the Elyos Warrior, enter world, level ready", [&] {
		const decoders::CharacterList list = logIn(servers, a);
		EXPECT_EQ(list.characterCount, 0) << "a fresh account must have no character";
		NewCharacter character;
		character.name = a.name;
		character.asmodian = false;
		character.playerClassId = NewCharacter::WARRIOR;
		a.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(a.key.accountId, a.account, character, 1));
		EXPECT_EQ(decoders::decodeCreateCharacter(expectNext(*a.game, "SM_CREATE_CHARACTER", a.async).data).responseCode, RESPONSE_OPEN_CREATION_WINDOW);
		a.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(a.key.accountId, a.account, character, 0));
		const decoders::CreateCharacter created = decoders::decodeCreateCharacter(expectNext(*a.game, "SM_CREATE_CHARACTER", a.async).data);
		if (created.responseCode != RESPONSE_OK || !created.player)
			throw std::runtime_error("creating the Elyos Warrior answered response code " + std::to_string(created.responseCode));
		a.playerId = created.player->playerId;
		enterWorld(a, true);
		levelReady(a, true);
		EXPECT_EQ(a.kinah(), starterCount(elyos, KINAH_ITEM)) << "m5a-creation's starter kinah (the Y12 ledger starts here)";
		elpasObject = npcAt(a, elpasSpot, "elpas");
		miresObject = npcAt(a, miresSpot, "mires");
	});

	// ---- C4 / Y1: both enter-world SM_NEARBY_QUESTS of a first enter ----
	runCase("C4", "markers at enter world: both SM_NEARBY_QUESTS are the oracle's set (Y1)", [&] {
		const std::vector<Packet> enterNearby = ofName(a.lastEnterWorld, "SM_NEARBY_QUESTS");
		const std::vector<Packet> readyNearby = ofName(a.lastLevelReady, "SM_NEARBY_QUESTS");
		ASSERT_GE(enterNearby.size(), 1u) << "Y1: no SM_NEARBY_QUESTS in the CM_ENTER_WORLD burst (the first-enter onLevelChange(0, 1))";
		ASSERT_GE(readyNearby.size(), 1u) << "Y1: no SM_NEARBY_QUESTS in the CM_LEVEL_READY burst (CM_LEVEL_READY.cpp:108)";
		expectNearbySet(decoders::decodeNearbyQuests(enterNearby.front().data), poetaNearby, "Y1 (the CM_ENTER_WORLD one)");
		expectNearbySet(decoders::decodeNearbyQuests(readyNearby.front().data), poetaNearby, "Y1 (the CM_LEVEL_READY one)");
		std::cout << "C4: SM_NEARBY_QUESTS in the first CM_ENTER_WORLD burst " << enterNearby.size() << ", in the CM_LEVEL_READY burst "
		          << readyNearby.size() << std::endl;
		// §10.6 (d): a later one (a quest-start npc's first spawn in the instance) must decode to the same set
		std::vector<Packet> later(enterNearby.begin() + 1, enterNearby.end());
		later.insert(later.end(), readyNearby.begin() + 1, readyNearby.end());
		for (const Packet& packet : later)
			expectNearbySet(decoders::decodeNearbyQuests(packet.data), poetaNearby, "Y1 (a later one)");
		// no quest was started at the first enter world: the Java handlers of the map's level-1 hooks start nothing (header comment)
		const QuestEvents enterEvents = eventsOf(a, a.lastEnterWorld, "Y1");
		EXPECT_TRUE(enterEvents.actions.empty()) << "Y1: the first enter world sent SM_QUEST_ACTION: " << enterEvents.describe();
	});

	// ---- C5 / Y2: talk to elpas ----
	runCase("C5", "talk to elpas: page 10, quest 0, nothing else from the quest engine (Y2)", [&] {
		walkToTalk(a, elpasSpot, TALK_DISTANCE);
		const std::vector<Packet> window = news(a, showDialog(a, elpasObject));
		const QuestEvents events = eventsOf(a, window, "Y2");
		EXPECT_EQ(events.tokens, std::vector<std::string>{windowToken("elpas", PAGE_SELECT_QUEST, 0)})
		  << "Y2: CM_SHOW_DIALOG(elpas) answered " << events.describe() << "; packets " << join(namesOf(window));
	});

	// ---- C6 / Y3: select and accept 1101, close ----
	runCase("C6", "select and accept 1101, close the dialog (Y3)", [&] {
		const std::optional<OracleWindow> selectPage = quest1101.window("start", QUEST_SELECT);
		const std::optional<OracleWindow> acceptPage = quest1101.window("start", QUEST_ACCEPT_1);
		ASSERT_TRUE(selectPage && acceptPage) << "the oracle has no start window of 1101";
		const QuestEvents select = eventsOf(a, news(a, dialogSelect(a, elpasObject, QUEST_SELECT, Q1101)), "Y3");
		EXPECT_EQ(select.tokens, std::vector<std::string>{windowToken("elpas", selectPage->page, Q1101)}) << "Y3: (31, 1101) answered " << select.describe();
		const QuestEvents accept = eventsOf(a, news(a, dialogSelect(a, elpasObject, QUEST_ACCEPT_1, Q1101)), "Y3");
		EXPECT_EQ(accept.tokens, (std::vector<std::string>{questToken("ADD", Q1101, decoders::QUEST_STATUS_START, 0), "NEARBY",
		                                                   windowToken("elpas", acceptPage->page, Q1101)}))
		  << "Y3: (1002, 1101) answered " << accept.describe();
		if (!accept.nearby.empty())
			EXPECT_FALSE(accept.nearby.front().contains(Q1101)) << "Y3: the SM_NEARBY_QUESTS after the start still offers 1101";
		// CM_CLOSE_DIALOG -> DialogService.onCloseDialog: the npc's target is cleared, and NpcController.onTargetChanged broadcasts
		// SM_LOOKATOBJECT(elpas, 0) (NpcController.java:78-93); nothing from the quest engine
		const std::vector<Packet> closed = exchange(a, GameSession::CM_CLOSE_DIALOG, GameSession::buildCM_CLOSE_DIALOG(elpasObject));
		EXPECT_TRUE(eventsOf(a, news(a, closed), "Y3").tokens.empty()) << "Y3: CM_CLOSE_DIALOG sent quest packets";
		bool turnedBack = false;
		for (const Packet& packet : ofName(closed, "SM_LOOKATOBJECT"))
			turnedBack = turnedBack || decoders::decodeLookAtObject(packet.data).objectId == elpasObject;
		EXPECT_TRUE(turnedBack) << "Y3: CM_CLOSE_DIALOG was not answered with elpas's SM_LOOKATOBJECT: " << join(namesOf(closed));
	});

	// ---- C7 / Y4: out of range ----
	runCase("C7", "from 15 m, CM_SHOW_DIALOG(mires): too far, no window (Y4)", [&] {
		walkToTalk(a, miresSpot, TOO_FAR_DISTANCE);
		const std::vector<Packet> window = news(a, showDialog(a, miresObject));
		const QuestEvents events = eventsOf(a, window, "Y4");
		EXPECT_EQ(events.tokens, std::vector<std::string>{"MSG " + std::to_string(STR_DIALOG_TOO_FAR_TO_TALK)})
		  << "Y4: CM_SHOW_DIALOG(mires) from " << distance2d(a.x, a.y, miresSpot.x, miresSpot.y) << " m answered " << events.describe();
	});

	// ---- C8 / Y5: report 1101 at mires ----
	runCase("C8", "report 1101 at mires (Y5)", [&] {
		const std::optional<OracleWindow> reportPage = quest1101.window("report", QUEST_SELECT);
		const std::optional<OracleWindow> rewardPage = quest1101.window("report", SELECT_QUEST_REWARD);
		ASSERT_TRUE(reportPage && rewardPage) << "the oracle has no report window of 1101";
		walkToTalk(a, miresSpot, TALK_DISTANCE);
		const QuestEvents talk = eventsOf(a, news(a, showDialog(a, miresObject)), "Y5");
		EXPECT_EQ(talk.tokens, std::vector<std::string>{windowToken("mires", PAGE_SELECT_QUEST, 0)}) << "Y5: CM_SHOW_DIALOG(mires) answered " << talk.describe();
		const QuestEvents select = eventsOf(a, news(a, dialogSelect(a, miresObject, QUEST_SELECT, Q1101)), "Y5");
		EXPECT_EQ(select.tokens, std::vector<std::string>{windowToken("mires", reportPage->page, Q1101)}) << "Y5: (31, 1101) answered " << select.describe();
		const QuestEvents report = eventsOf(a, news(a, dialogSelect(a, miresObject, SELECT_QUEST_REWARD, Q1101)), "Y5");
		EXPECT_EQ(report.tokens, (std::vector<std::string>{questToken("UPDATE", Q1101, decoders::QUEST_STATUS_REWARD, 1), "NEARBY",
		                                                   windowToken("mires", rewardPage->page, Q1101)}))
		  << "Y5: (1009, 1101) answered " << report.describe();
	});

	// ---- C9 / Y6, Y7: the reward, the follow-up, the replay ----
	runCase("C9", "take 1101's reward: kinah, exp, COMPLETE, the follow-up window of 1102; the replay pays nothing (Y6, Y7)", [&] {
		ASSERT_TRUE(quest1101.followUpWindow) << "the oracle has no follow-up window at mires";
		const int64_t kinahBefore = a.kinah();
		const QuestEvents reward = eventsOf(a, news(a, dialogSelect(a, miresObject, SELECTED_QUEST_NOREWARD, Q1101)), "Y6");
		EXPECT_EQ(reward.tokens, (std::vector<std::string>{"ITEM " + std::to_string(KINAH_ITEM) + "x" + std::to_string(kinahBefore + quest1101.kinah),
		                                                   "EXP", "GET_EXP " + std::to_string(quest1101.exp),
		                                                   questToken("UPDATE", Q1101, decoders::QUEST_STATUS_COMPLETE, 0), "NEARBY",
		                                                   windowToken("mires", quest1101.followUpWindow->page, quest1101.followUpWindow->questId)}))
		  << "Y6: (23, 1101) answered " << reward.describe() << "; the oracle pays " << quest1101.kinah << " kinah and " << quest1101.exp << " exp";
		if (!reward.exps.empty())
			EXPECT_EQ(reward.exps.front().currentExp, quest1101.exp) << "Y6: the exp shown after the reward (level 1 starts at 0)";
		if (!reward.nearby.empty()) {
			EXPECT_TRUE(reward.nearby.front().contains(Q1102)) << "Y6: the SM_NEARBY_QUESTS after 1101's reward does not offer 1102";
			EXPECT_FALSE(reward.nearby.front().contains(Q1101)) << "Y6: 1101 is still offered after its reward";
		}
		EXPECT_EQ(a.kinah(), kinahBefore + quest1101.kinah);
		const QuestEvents replay = eventsOf(a, news(a, dialogSelect(a, miresObject, SELECTED_QUEST_NOREWARD, Q1101)), "Y7");
		EXPECT_EQ(replay.tokens, std::vector<std::string>{windowToken("mires", SELECTED_QUEST_NOREWARD, Q1101)})
		  << "Y7: the replayed (23, 1101) must be answered by DialogService's next page alone: " << replay.describe();
	});

	// ---- C10 / Y8: accept 1102, one kill, report too early ----
	std::optional<QuestAnswer> quest1102Plan;
	std::set<int32_t> killedKerubs;
	runCase("C10", "accept 1102, kill one kerub, report too early (Y8)", [&] {
		quest1102Plan = parseQuest(questOracle({"m5d-quest", "--quest", std::to_string(Q1102), "--completed", std::to_string(Q1101)}));
		ASSERT_EQ(quest1102Plan->kind, "monster_hunt");
		EXPECT_EQ(toSet(quest1102Plan->killNpcIds), std::set<int32_t>(KERUBS.begin(), KERUBS.end())) << "§10.1: 1102 counts 210133 and 210134";
		EXPECT_EQ(quest1102Plan->killCount, 3) << "§10.2 C12: the oracle's count";
		const std::optional<OracleWindow> acceptPage = quest1102Plan->window("start", QUEST_ACCEPT_1);
		const std::optional<OracleWindow> reportPage = quest1102Plan->window("report", QUEST_SELECT);
		ASSERT_TRUE(acceptPage && reportPage);
		const QuestEvents accept = eventsOf(a, news(a, dialogSelect(a, miresObject, QUEST_ACCEPT_1, Q1102)), "Y8");
		EXPECT_EQ(accept.tokens, (std::vector<std::string>{questToken("ADD", Q1102, decoders::QUEST_STATUS_START, 0), "NEARBY",
		                                                   windowToken("mires", acceptPage->page, Q1102)}))
		  << "Y8: (1002, 1102) answered " << accept.describe();

		const KillRecord first = killAt(a, kerubSpots, killedKerubs, 30s);
		killedKerubs.insert(first.objectId);
		EXPECT_EQ(killTokens(a, first, "Y8"), std::vector<std::string>{questToken("UPDATE", Q1102, decoders::QUEST_STATUS_START, 1)})
		  << "Y8: the first kill's quest updates";

		walkToTalk(a, miresSpot, TALK_DISTANCE);
		const QuestEvents select = eventsOf(a, news(a, dialogSelect(a, miresObject, QUEST_SELECT, Q1102)), "Y8");
		EXPECT_EQ(select.tokens, std::vector<std::string>{windowToken("mires", reportPage->page, Q1102)}) << "Y8: (31, 1102) answered " << select.describe();
		const QuestEvents early = eventsOf(a, news(a, dialogSelect(a, miresObject, SELECT_QUEST_REWARD, Q1102)), "Y8");
		EXPECT_EQ(early.tokens, std::vector<std::string>{windowToken("mires", SELECT_QUEST_REWARD, Q1102)})
		  << "Y8: (1009, 1102) with 1 of 3 kills must change nothing and get DialogService's next page: " << early.describe();
	});

	// ---- C10b / Y7b: the journal's report on a START quest ----
	runCase("C10b", "the journal's auto reward on 1102 in START does nothing (Y7b)", [&] {
		const std::vector<Packet> journal = news(a, dialogSelect(a, 0, SELECTED_QUEST_AUTO_REWARD, Q1102));
		const std::vector<Packet> sentinel = news(a, showDialog(a, miresObject));
		std::vector<Packet> both = journal;
		both.insert(both.end(), sentinel.begin(), sentinel.end());
		const QuestEvents events = eventsOf(a, both, "Y7b");
		EXPECT_EQ(events.tokens, std::vector<std::string>{windowToken("mires", PAGE_SELECT_QUEST, 0)})
		  << "Y7b: (target 0, 108, 1102) on a START quest must send nothing before the sentinel's window: " << events.describe();
	});

	// ---- C11 / Y9: relog mid-quest ----
	runCase("C11", "relog mid-quest: player_quests and the enter-world quest lists (Y9)", [&] {
		disconnect(a);
		const std::vector<QuestRow> rows = questRows(database, schema, a.playerId);
		EXPECT_EQ(rows, (std::vector<QuestRow>{{Q1101, std::string(DB_COMPLETE), 0, 1}, {Q1102, std::string(DB_START), 1, 0}}))
		  << "Y9: player_quests holds " << describe(rows);
		const auto [list, burst] = relogIn(servers, a);
		EXPECT_EQ(list.characterCount, 1);
		const EnterWorldQuests quests = enterWorldQuests(burst, "Y9");
		ASSERT_TRUE(quests.list) << "Y9: no SM_QUEST_LIST after re-entry";
		EXPECT_EQ(quests.list->quests, (std::vector<decoders::QuestEntry>{{Q1102, decoders::QUEST_STATUS_START, 1, 0}}))
		  << "Y9: SM_QUEST_LIST " << describe(*quests.list);
		decoders::QuestCompletedList completed;
		completed.quests = quests.completed;
		EXPECT_EQ(quests.completed, (std::vector<decoders::QuestCompletedEntry>{{Q1101, 1, 1}})) << "Y9: SM_QUEST_COMPLETED_LIST " << describe(completed);
		elpasObject = npcAt(a, elpasSpot, "elpas");
		miresObject = npcAt(a, miresSpot, "mires");
	});

	// ---- C12 / Y10: kills 2 and 3 ----
	runCase("C12", "kills 2 and 3 of 1102: steps 2 and 3, nothing else (Y10)", [&] {
		for (int32_t kill = 2; kill <= quest1102Plan.value_or(QuestAnswer{}).killCount; kill++) {
			const std::optional<decoders::StatUpdateHp> hp = restUntil(a, REST_BELOW, 60s);
			if (hp)
				std::cout << "A: HP " << hp->currentHp << "/" << hp->maxHp << " before kill " << kill << std::endl;
			const KillRecord record = killAt(a, kerubSpots, killedKerubs, 45s);
			killedKerubs.insert(record.objectId);
			EXPECT_EQ(killTokens(a, record, "Y10"), std::vector<std::string>{questToken("UPDATE", Q1102, decoders::QUEST_STATUS_START, kill)})
			  << "Y10: kill " << kill << "'s quest updates";
		}
	});

	// ---- C13 / Y11: report and reward 1102, the level-up inside the reward ----
	runCase("C13", "report and reward 1102: the level-up inside finishQuest, the follow-up window of 1103 (Y11)", [&] {
		walkToTalk(a, miresSpot, TALK_DISTANCE);
		const int64_t expBefore = a.totalExp();
		const QuestAnswer plan = parseQuest(questOracle({"m5d-quest", "--quest", std::to_string(Q1102), "--completed", std::to_string(Q1101), "--exp",
		                                                 std::to_string(expBefore)}));
		ASSERT_TRUE(plan.followUpWindow) << "the oracle has no follow-up window after 1102";
		EXPECT_EQ(plan.levelsSinceEnterWorld, (std::vector<int32_t>{1, 2}))
		  << "Y11: the oracle places no level-up in 1102's reward at exp " << expBefore << " (" << joinNumbers(a.expEvents)
		  << "): the script went off its path (exactly 2 or 3 credited kills after 1101, §10.3 Y11)";
		const std::optional<OracleWindow> reportPage = plan.window("report", QUEST_SELECT);
		const std::optional<OracleWindow> rewardPage = plan.window("report", SELECT_QUEST_REWARD);
		ASSERT_TRUE(reportPage && rewardPage);
		const QuestEvents select = eventsOf(a, news(a, dialogSelect(a, miresObject, QUEST_SELECT, Q1102)), "Y11");
		EXPECT_EQ(select.tokens, std::vector<std::string>{windowToken("mires", reportPage->page, Q1102)}) << "Y11: (31, 1102) answered " << select.describe();
		const QuestEvents report = eventsOf(a, news(a, dialogSelect(a, miresObject, SELECT_QUEST_REWARD, Q1102)), "Y11");
		EXPECT_EQ(report.tokens, (std::vector<std::string>{questToken("UPDATE", Q1102, decoders::QUEST_STATUS_REWARD, quest1102Plan->killCount), "NEARBY",
		                                                   windowToken("mires", rewardPage->page, Q1102)}))
		  << "Y11: (1009, 1102) answered " << report.describe();

		const int64_t kinahBefore = a.kinah();
		const QuestEvents reward = eventsOf(a, news(a, dialogSelect(a, miresObject, SELECTED_QUEST_NOREWARD, Q1102)), "Y11");
		// Java's order (QuestService.java:101-116, PlayerCommonData.java:196-287, PlayerController.java:583-591, _1205ANewSkill.java:40-65): the
		// kinah; setExp's level change - the LEVEL_UP animation, QuestEngine.onLevelChanged (1205 starts: ADD, then REWARD with var 1 and its
		// nearby update), onLevelChange's own nearby update -, SM_STATUPDATE_EXP, STR_GET_EXP; COMPLETE, the nearby update, the follow-up window
		EXPECT_EQ(reward.tokens, (std::vector<std::string>{"ITEM " + std::to_string(KINAH_ITEM) + "x" + std::to_string(kinahBefore + plan.kinah), "LEVEL_UP 2",
		                                                   questToken("ADD", Q1205, decoders::QUEST_STATUS_START, 0), "NEARBY",
		                                                   questToken("UPDATE", Q1205, decoders::QUEST_STATUS_REWARD, 1), "NEARBY", "NEARBY", "EXP",
		                                                   "GET_EXP " + std::to_string(plan.exp),
		                                                   questToken("UPDATE", Q1102, decoders::QUEST_STATUS_COMPLETE, 0), "NEARBY",
		                                                   windowToken("mires", plan.followUpWindow->page, plan.followUpWindow->questId)}))
		  << "Y11: (23, 1102) answered " << reward.describe() << "; the oracle pays " << plan.kinah << " kinah and " << plan.exp << " exp";
		EXPECT_EQ(plan.followUpWindow, std::optional<OracleWindow>(OracleWindow{PAGE_SELECT1, Q1103})) << "D6: 1102's follow-up is 1103";
		if (!reward.exps.empty()) {
			EXPECT_EQ(reward.exps.front().currentExp, plan.expAfterReward - levelOneExpNeed)
			  << "Y11: the exp shown at level 2 (the total " << plan.expAfterReward << " less level 2's start exp " << levelOneExpNeed << ")";
			EXPECT_EQ(reward.exps.front().maxExp, levelTwoExpNeed) << "Y11: SM_STATUPDATE_EXP's need is level 2's (setExp sends it after onLevelChange)";
		}
		if (!reward.nearby.empty())
			EXPECT_TRUE(reward.nearby.back().contains(Q1103)) << "Y11: the last SM_NEARBY_QUESTS does not offer 1103";
		a.scanExp();
	});

	// ---- C14 / Y12: accept and abandon 1103 ----
	runCase("C14", "accept 1103 and abandon it (Y12)", [&] {
		const QuestAnswer plan = parseQuest(questOracle({"m5d-quest", "--quest", std::to_string(Q1103), "--completed", std::to_string(Q1101),
		                                                 std::to_string(Q1102), "--level", "2"}));
		const std::optional<OracleWindow> acceptPage = plan.window("start", QUEST_ACCEPT_1);
		ASSERT_TRUE(acceptPage);
		const QuestEvents accept = eventsOf(a, news(a, dialogSelect(a, miresObject, QUEST_ACCEPT_1, Q1103)), "Y12");
		EXPECT_EQ(accept.tokens, (std::vector<std::string>{questToken("ADD", Q1103, decoders::QUEST_STATUS_START, 0), "NEARBY",
		                                                   windowToken("mires", acceptPage->page, Q1103)}))
		  << "Y12: (1002, 1103) answered " << accept.describe();
		const QuestEvents abandon = eventsOf(a, news(a, exchange(a, GameSession::CM_DELETE_QUEST, GameSession::buildCM_DELETE_QUEST(Q1103))), "Y12");
		EXPECT_EQ(abandon.tokens, (std::vector<std::string>{"ABANDON " + std::to_string(Q1103), "NEARBY"}))
		  << "Y12: CM_DELETE_QUEST(1103) answered " << abandon.describe();
		if (!abandon.nearby.empty())
			EXPECT_TRUE(abandon.nearby.front().contains(Q1103)) << "Y12: the SM_NEARBY_QUESTS after the abandon does not offer 1103 again";
	});

	// ---- C15 / Y12: persistence of everything ----
	runCase("C15", "relog: player_quests, the quest lists, the kinah, elpas's page 1011 (Y12)", [&] {
		disconnect(a);
		const std::vector<QuestRow> rows = questRows(database, schema, a.playerId);
		EXPECT_EQ(rows, (std::vector<QuestRow>{{Q1101, std::string(DB_COMPLETE), 0, 1}, {Q1102, std::string(DB_COMPLETE), 0, 1},
		                                        {Q1205, std::string(DB_REWARD), 1, 0}}))
		  << "Y12: player_quests holds " << describe(rows) << " (1103 deleted, 1205 started by the level-up)";
		const auto [list, burst] = relogIn(servers, a);
		const EnterWorldQuests quests = enterWorldQuests(burst, "Y12");
		ASSERT_TRUE(quests.list);
		EXPECT_EQ(quests.list->quests, (std::vector<decoders::QuestEntry>{{Q1205, decoders::QUEST_STATUS_REWARD, 1, 0}}))
		  << "Y12: SM_QUEST_LIST " << describe(*quests.list) << " (no 1103; 1205 waits in REWARD)";
		std::set<int32_t> completedIds;
		for (const decoders::QuestCompletedEntry& entry : quests.completed)
			completedIds.insert(entry.questId);
		EXPECT_EQ(completedIds, (std::set<int32_t>{Q1101, Q1102})) << "Y12: SM_QUEST_COMPLETED_LIST";
		const int64_t kinah = enterWorldCount(burst, KINAH_ITEM, "Y12");
		const QuestAnswer plan1102 = parseQuest(questOracle({"m5d-quest", "--quest", std::to_string(Q1102), "--completed", std::to_string(Q1101)}));
		EXPECT_EQ(kinah, starterCount(elyos, KINAH_ITEM) + quest1101.kinah + plan1102.kinah)
		  << "Y12: SM_INVENTORY_INFO's kinah (m5a-creation's starter + 1101's + 1102's)";
		elpasObject = npcAt(a, elpasSpot, "elpas");
		walkToTalk(a, elpasSpot, TALK_DISTANCE);
		const QuestEvents talk = eventsOf(a, news(a, showDialog(a, elpasObject)), "Y12");
		EXPECT_EQ(talk.tokens, std::vector<std::string>{windowToken("elpas", PAGE_SELECT1, 0)})
		  << "Y12: CM_SHOW_DIALOG(elpas) with nothing left to offer answered " << talk.describe();
	});

	// the Elyos leaves; its database state is final
	cases.run("C15b", "account A disconnects", [&] { disconnect(a); });

	// ======================================================================================================================================
	// The Asmodian Warrior on account B (C16, Y13)
	// ======================================================================================================================================

	int32_t asakObject = 0, vandarObject = 0;
	QuestAnswer quest2102Plan;
	runCase("C16a", "account B: login, create the Asmodian Warrior, enter world; both SM_NEARBY_QUESTS are the Ishalgen set (Y13)", [&] {
		const decoders::CharacterList list = logIn(servers, b);
		EXPECT_EQ(list.characterCount, 0);
		NewCharacter character;
		character.name = b.name;
		character.asmodian = true;
		character.playerClassId = NewCharacter::WARRIOR;
		b.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(b.key.accountId, b.account, character, 1));
		EXPECT_EQ(decoders::decodeCreateCharacter(expectNext(*b.game, "SM_CREATE_CHARACTER", b.async).data).responseCode, RESPONSE_OPEN_CREATION_WINDOW);
		b.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(b.key.accountId, b.account, character, 0));
		const decoders::CreateCharacter created = decoders::decodeCreateCharacter(expectNext(*b.game, "SM_CREATE_CHARACTER", b.async).data);
		if (created.responseCode != RESPONSE_OK || !created.player)
			throw std::runtime_error("creating the Asmodian Warrior answered response code " + std::to_string(created.responseCode));
		b.playerId = created.player->playerId;
		enterWorld(b, true);
		levelReady(b, true);
		const std::vector<Packet> enterNearby = ofName(b.lastEnterWorld, "SM_NEARBY_QUESTS");
		const std::vector<Packet> readyNearby = ofName(b.lastLevelReady, "SM_NEARBY_QUESTS");
		ASSERT_GE(enterNearby.size(), 1u);
		ASSERT_GE(readyNearby.size(), 1u);
		expectNearbySet(decoders::decodeNearbyQuests(enterNearby.front().data), ishalgenNearby, "Y13 (the CM_ENTER_WORLD one)");
		expectNearbySet(decoders::decodeNearbyQuests(readyNearby.front().data), ishalgenNearby, "Y13 (the CM_LEVEL_READY one)");
		std::cout << "C16a: SM_NEARBY_QUESTS in the first CM_ENTER_WORLD burst " << enterNearby.size() << ", in the CM_LEVEL_READY burst "
		          << readyNearby.size() << std::endl;
		// as C4's for the Elyos: a later one (§10.6 (d)) must decode to the same set
		std::vector<Packet> later(enterNearby.begin() + 1, enterNearby.end());
		later.insert(later.end(), readyNearby.begin() + 1, readyNearby.end());
		for (const Packet& packet : later)
			expectNearbySet(decoders::decodeNearbyQuests(packet.data), ishalgenNearby, "Y13 (a later one)");
		// and no quest was started at the first enter world (2000 and 2100 are held back, and no registered handler of Ishalgen
		// starts anything at level 1: the header comment)
		const QuestEvents enterEvents = eventsOf(b, b.lastEnterWorld, "Y13");
		EXPECT_TRUE(enterEvents.actions.empty()) << "Y13: the first enter world sent SM_QUEST_ACTION: " << enterEvents.describe();
		asakObject = npcAt(b, asakSpot, "asak");
		vandarObject = npcAt(b, vandarSpot, "vandar");
		EXPECT_EQ(b.countOf(BANDAGE), starterCount(asmodian, BANDAGE))
		  << "m5a-creation's starter bandages (Y13's refresh: the reward adds to this stack)";
	});

	runCase("C16b", "2101 asak -> vandar: accept, report, reward; no follow-up (Y13)", [&] {
		const std::optional<OracleWindow> selectPage = quest2101.window("start", QUEST_SELECT);
		const std::optional<OracleWindow> acceptPage = quest2101.window("start", QUEST_ACCEPT_1);
		const std::optional<OracleWindow> reportPage = quest2101.window("report", QUEST_SELECT);
		const std::optional<OracleWindow> rewardPage = quest2101.window("report", SELECT_QUEST_REWARD);
		ASSERT_TRUE(selectPage && acceptPage && reportPage && rewardPage && quest2101.followUpWindow);
		walkToTalk(b, asakSpot, TALK_DISTANCE);
		const QuestEvents talk = eventsOf(b, news(b, showDialog(b, asakObject)), "Y13");
		EXPECT_EQ(talk.tokens, std::vector<std::string>{windowToken("asak", PAGE_SELECT_QUEST, 0)}) << "Y13: CM_SHOW_DIALOG(asak) answered " << talk.describe();
		const QuestEvents select = eventsOf(b, news(b, dialogSelect(b, asakObject, QUEST_SELECT, Q2101)), "Y13");
		EXPECT_EQ(select.tokens, std::vector<std::string>{windowToken("asak", selectPage->page, Q2101)}) << "Y13: (31, 2101) answered " << select.describe();
		const QuestEvents accept = eventsOf(b, news(b, dialogSelect(b, asakObject, QUEST_ACCEPT_1, Q2101)), "Y13");
		EXPECT_EQ(accept.tokens, (std::vector<std::string>{questToken("ADD", Q2101, decoders::QUEST_STATUS_START, 0), "NEARBY",
		                                                   windowToken("asak", acceptPage->page, Q2101)}))
		  << "Y13: (1002, 2101) answered " << accept.describe();
		if (!accept.nearby.empty())
			EXPECT_FALSE(accept.nearby.front().contains(Q2101));

		walkToTalk(b, vandarSpot, TALK_DISTANCE);
		const QuestEvents report = eventsOf(b, news(b, dialogSelect(b, vandarObject, QUEST_SELECT, Q2101)), "Y13");
		EXPECT_EQ(report.tokens, std::vector<std::string>{windowToken("vandar", reportPage->page, Q2101)}) << "Y13: (31, 2101) answered " << report.describe();
		const QuestEvents toReward = eventsOf(b, news(b, dialogSelect(b, vandarObject, SELECT_QUEST_REWARD, Q2101)), "Y13");
		EXPECT_EQ(toReward.tokens, (std::vector<std::string>{questToken("UPDATE", Q2101, decoders::QUEST_STATUS_REWARD, 1), "NEARBY",
		                                                     windowToken("vandar", rewardPage->page, Q2101)}))
		  << "Y13: (1009, 2101) answered " << toReward.describe();
		const int64_t kinahBefore = b.kinah();
		const QuestEvents reward = eventsOf(b, news(b, dialogSelect(b, vandarObject, SELECTED_QUEST_NOREWARD, Q2101)), "Y13");
		EXPECT_EQ(reward.tokens, (std::vector<std::string>{"ITEM " + std::to_string(KINAH_ITEM) + "x" + std::to_string(kinahBefore + quest2101.kinah),
		                                                   "EXP", "GET_EXP " + std::to_string(quest2101.exp),
		                                                   questToken("UPDATE", Q2101, decoders::QUEST_STATUS_COMPLETE, 0), "NEARBY",
		                                                   windowToken("vandar", quest2101.followUpWindow->page, quest2101.followUpWindow->questId)}))
		  << "Y13: (23, 2101) answered " << reward.describe() << " - no follow-up: the selection page, 2102 being startable";
		b.scanExp();
	});

	runCase("C16c", "accept 2102 at vandar, kill four sprigg workers and a fifth (Y13)", [&] {
		quest2102Plan = parseQuest(questOracle({"m5d-quest", "--quest", std::to_string(Q2102), "--completed", std::to_string(Q2101)}));
		ASSERT_EQ(quest2102Plan.kind, "monster_hunt");
		EXPECT_EQ(toSet(quest2102Plan.killNpcIds), std::set<int32_t>(SPRIGG_WORKERS.begin(), SPRIGG_WORKERS.end()));
		EXPECT_EQ(quest2102Plan.killCount, 4) << "§10.2 C16: the oracle's count";
		const std::optional<OracleWindow> selectPage = quest2102Plan.window("start", QUEST_SELECT);
		const std::optional<OracleWindow> acceptPage = quest2102Plan.window("start", QUEST_ACCEPT_1);
		ASSERT_TRUE(selectPage && acceptPage);
		const QuestEvents select = eventsOf(b, news(b, dialogSelect(b, vandarObject, QUEST_SELECT, Q2102)), "Y13");
		EXPECT_EQ(select.tokens, std::vector<std::string>{windowToken("vandar", selectPage->page, Q2102)}) << "Y13: (31, 2102) answered " << select.describe();
		const QuestEvents accept = eventsOf(b, news(b, dialogSelect(b, vandarObject, QUEST_ACCEPT_1, Q2102)), "Y13");
		EXPECT_EQ(accept.tokens, (std::vector<std::string>{questToken("ADD", Q2102, decoders::QUEST_STATUS_START, 0), "NEARBY",
		                                                   windowToken("vandar", acceptPage->page, Q2102)}))
		  << "Y13: (1002, 2102) answered " << accept.describe();

		std::set<int32_t> killed;
		for (int32_t kill = 1; kill <= quest2102Plan.killCount + 1; kill++) {
			const std::optional<decoders::StatUpdateHp> hp = restUntil(b, REST_BELOW, 60s);
			if (hp)
				std::cout << "B: HP " << hp->currentHp << "/" << hp->maxHp << " before kill " << kill << std::endl;
			const KillRecord record = killAt(b, spriggSpots, killed, 45s);
			killed.insert(record.objectId);
			const std::vector<std::string> tokens = killTokens(b, record, "Y13");
			if (kill <= quest2102Plan.killCount)
				EXPECT_EQ(tokens, std::vector<std::string>{questToken("UPDATE", Q2102, decoders::QUEST_STATUS_START, kill)}) << "Y13: kill " << kill;
			else
				EXPECT_TRUE(tokens.empty()) << "Y13: the kill past the count must not update 2102 (MonsterHunt's total <= endVar): " << join(tokens);
		}
	});

	runCase("C16d", "report and reward 2102: the bandages, the kinah, the exp, the follow-up window of 2103 (Y13)", [&] {
		walkToTalk(b, vandarSpot, TALK_DISTANCE);
		const QuestAnswer plan = parseQuest(questOracle({"m5d-quest", "--quest", std::to_string(Q2102), "--completed", std::to_string(Q2101), "--exp",
		                                                 std::to_string(b.totalExp())}));
		const std::optional<OracleWindow> reportPage = plan.window("report", QUEST_SELECT);
		const std::optional<OracleWindow> rewardPage = plan.window("report", SELECT_QUEST_REWARD);
		ASSERT_TRUE(reportPage && rewardPage && plan.followUpWindow);
		EXPECT_EQ(plan.followUpWindow, std::optional<OracleWindow>(OracleWindow{PAGE_SELECT1, Q2103})) << "D6: 2102's follow-up is 2103";
		ASSERT_EQ(plan.items.size(), 1u);
		EXPECT_EQ(plan.items.front().first, BANDAGE);
		const QuestEvents select = eventsOf(b, news(b, dialogSelect(b, vandarObject, QUEST_SELECT, Q2102)), "Y13");
		EXPECT_EQ(select.tokens, std::vector<std::string>{windowToken("vandar", reportPage->page, Q2102)}) << "Y13: (31, 2102) answered " << select.describe();
		const QuestEvents toReward = eventsOf(b, news(b, dialogSelect(b, vandarObject, SELECT_QUEST_REWARD, Q2102)), "Y13");
		EXPECT_EQ(toReward.tokens, (std::vector<std::string>{questToken("UPDATE", Q2102, decoders::QUEST_STATUS_REWARD, plan.killCount), "NEARBY",
		                                                     windowToken("vandar", rewardPage->page, Q2102)}))
		  << "Y13: (1009, 2102) answered " << toReward.describe();
		const int64_t kinahBefore = b.kinah();
		const int64_t bandagesBefore = b.countOf(BANDAGE);
		const QuestEvents reward = eventsOf(b, news(b, dialogSelect(b, vandarObject, SELECTED_QUEST_NOREWARD, Q2102)), "Y13");
		EXPECT_EQ(reward.tokens,
		          (std::vector<std::string>{"ITEM " + std::to_string(BANDAGE) + "x" + std::to_string(bandagesBefore + plan.items.front().second),
		                                    "ITEM " + std::to_string(KINAH_ITEM) + "x" + std::to_string(kinahBefore + plan.kinah), "EXP",
		                                    "GET_EXP " + std::to_string(plan.exp), questToken("UPDATE", Q2102, decoders::QUEST_STATUS_COMPLETE, 0),
		                                    "NEARBY", windowToken("vandar", plan.followUpWindow->page, plan.followUpWindow->questId)}))
		  << "Y13: (23, 2102) answered " << reward.describe() << "; the oracle pays " << plan.items.front().second << " x " << BANDAGE << ", "
		  << plan.kinah << " kinah and " << plan.exp << " exp";
		if (!reward.nearby.empty())
			EXPECT_TRUE(reward.nearby.back().contains(Q2103)) << "Y13: the last SM_NEARBY_QUESTS does not offer 2103";
	});

	runCase("C16e", "account B relogs: the bandages and the kinah were saved (Y13)", [&] {
		disconnect(b);
		const std::vector<QuestRow> rows = questRows(database, schema, b.playerId);
		EXPECT_EQ(rows, (std::vector<QuestRow>{{Q2101, std::string(DB_COMPLETE), 0, 1}, {Q2102, std::string(DB_COMPLETE), 0, 1}}))
		  << "Y13: player_quests holds " << describe(rows);
		const auto [list, burst] = relogIn(servers, b);
		EXPECT_EQ(enterWorldCount(burst, BANDAGE, "Y13"), starterCount(asmodian, BANDAGE) + quest2102Plan.items.front().second)
		  << "Y13: SM_INVENTORY_INFO's bandages (m5a-creation's starter + the reward)";
		EXPECT_EQ(enterWorldCount(burst, KINAH_ITEM, "Y13"), starterCount(asmodian, KINAH_ITEM) + quest2101.kinah + quest2102Plan.kinah)
		  << "Y13: SM_INVENTORY_INFO's kinah (m5a-creation's starter + 2101's + 2102's)";
		const EnterWorldQuests quests = enterWorldQuests(burst, "Y13");
		ASSERT_TRUE(quests.list);
		EXPECT_TRUE(quests.list->quests.empty()) << "Y13: SM_QUEST_LIST " << describe(*quests.list);
		std::set<int32_t> completedIds;
		for (const decoders::QuestCompletedEntry& entry : quests.completed)
			completedIds.insert(entry.questId);
		EXPECT_EQ(completedIds, (std::set<int32_t>{Q2101, Q2102})) << "Y13: SM_QUEST_COMPLETED_LIST";
	});
	cases.run("C16f", "account B disconnects", [&] { disconnect(b); });

	// ---- C17: the reports ----
	std::optional<int32_t> gameServerExit;
	if (servers.gameServer() != nullptr)
		gameServerExit = servers.stopGameServer();
	const std::optional<int32_t> loginServerExit = servers.loginServer() != nullptr ? servers.stopLoginServer() : std::nullopt;

	cases.run("C17", "reports: the Q8 bar, the allow-list, the quest classes released (Y14)", [&] {
		ASSERT_TRUE(gameServerExit) << "the game server did not exit after the stop file was written";
		EXPECT_EQ(*gameServerExit, 0) << "the game server exited with " << *gameServerExit;
		ASSERT_TRUE(loginServerExit) << "the login server did not exit on CTRL_BREAK";
		if (*loginServerExit == 98) {
			ASSERT_NE(servers.loginServer(), nullptr);
			EXPECT_FALSE(servers.loginServer()->findLogLines("ServerChannels closed.", 1).empty())
			  << "the login server was terminated (exit 98) without shutting down";
		} else {
			EXPECT_EQ(*loginServerExit, 0) << "the login server exited with " << *loginServerExit;
		}
		ASSERT_TRUE(std::filesystem::is_regular_file(servers.checkOutputDir() / "m5a_summary.txt"))
		  << "Y14: the game server wrote no check output in " << servers.checkOutputDir();

		EXPECT_TRUE(servers.readReportLines("unported_trace.txt").empty())
		  << "Y14: AION_UNPORTED sites were reached on the quest path:\n" << join(servers.readReportLines("unported_trace.txt"), "\n");
		const std::vector<AllowlistEntry> allowlist = readAllowlist();
		ASSERT_FALSE(allowlist.empty()) << "Y14: tests/scenario/m5d_partial_allowlist.txt is empty or missing";
		std::map<std::string, int64_t> hitsByEntry;
		for (const AllowlistEntry& entry : allowlist)
			hitsByEntry[entry.site] = 0;
		for (const PartialHit& hit : readPartialHits(servers)) {
			bool allowed = false;
			for (const AllowlistEntry& entry : allowlist)
				if (allowlistEntryMatches(entry.site, hit.site)) {
					allowed = true;
					hitsByEntry[entry.site] += hit.hits;
				}
			EXPECT_TRUE(allowed) << "Y14: the AION_PARTIAL site " << hit.site << " is not in tests/scenario/m5d_partial_allowlist.txt (" << hit.line << ")";
		}
		for (const AllowlistEntry& entry : allowlist) {
			if (entry.section == AllowlistSection::HitAtLeastOnce)
				EXPECT_GT(hitsByEntry[entry.site], 0) << "Y14: the section A row " << entry.site << " was never hit";
			else if (entry.section == AllowlistSection::HitNever)
				EXPECT_EQ(hitsByEntry[entry.site], 0) << "Y14: the section B row " << entry.site << " was hit " << hitsByEntry[entry.site] << " times";
		}
		std::cout << "Y14: AION_PARTIAL hits by allow-list row (hits, section, site):\n";
		for (const AllowlistEntry& entry : allowlist)
			std::cout << "  " << hitsByEntry[entry.site] << "\t" << sectionName(entry.section) << "\t" << entry.site << "\n";
		std::cout << std::flush;

		const std::vector<std::string> census = servers.readReportLines("census.txt");
		EXPECT_TRUE(census.empty()) << "Y14: the final census reports leaks:\n" << join(census, "\n");
		EXPECT_TRUE(servers.readReportLines("lockdep.txt").empty()) << "Y14: the lock order validator reported:\n"
		                                                            << join(servers.readReportLines("lockdep.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("watchdog.txt").empty()) << "Y14: the watchdog dumped:\n" << join(servers.readReportLines("watchdog.txt"), "\n");
		const std::map<std::string, std::vector<std::string>> summary = servers.readSummary();
		const auto value = [&](std::string_view key) -> std::string {
			const auto found = summary.find(std::string(key));
			return found == summary.end() || found->second.empty() ? std::string() : found->second[0];
		};
		EXPECT_EQ(value("started"), "true");
		EXPECT_EQ(value("exitCode"), "0");
		EXPECT_EQ(value("liveCountsEnabled"), "true") << "Y14: a release build counts nothing: build it checked";
		const auto notPorted = summary.find("notPortedClientPacket");
		if (notPorted != summary.end())
			ADD_FAILURE() << "Y14: the scripted path sent client packets that are not ported: " << join(notPorted->second);
		std::vector<std::string> errors;
		ASSERT_NE(servers.gameServer(), nullptr);
		for (const std::string& line : servers.gameServer()->findLogLines(" ERROR "))
			errors.push_back("game server: " + line);
		if (servers.loginServer() != nullptr)
			for (const std::string& line : servers.loginServer()->findLogLines(" ERROR "))
				errors.push_back("login server: " + line);
		EXPECT_TRUE(errors.empty()) << "Y14: ERROR lines in the server logs (QuestEngine's dispatchers and checkStartConditions swallow an exception "
		                               "into an ERROR line, m5d-plan.md §11 risk 1):\n"
		                            << join(errors, "\n");
		const std::filesystem::path errorLog = servers.logFolder() / "server_errors.log";
		if (std::filesystem::exists(errorLog)) {
			std::vector<std::string> fileErrors;
			std::ifstream in(errorLog, std::ios::binary);
			std::string line;
			while (std::getline(in, line) && fileErrors.size() < 20) {
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				if (!line.empty())
					fileErrors.push_back(line);
			}
			EXPECT_TRUE(fileErrors.empty()) << "Y14: " << errorLog << " is not empty:\n" << join(fileErrors, "\n");
		} else {
			ADD_FAILURE() << "Y14: the game server wrote no " << errorLog;
		}
		EXPECT_TRUE(servers.gameServer()->findLogLines("objects removed from the world are still alive", 5).empty())
		  << "Y14: " << join(servers.gameServer()->findLogLines("objects removed from the world are still alive", 5), "\n");

		// §10.3 Y14: no quest object leaked - QuestEnv live 0 with created > 0, QuestState (and QuestVars, QuestStateList) live 0 after both
		// characters logged out; Player 0 and the per-connection classes 0 (nobody is online at the stop)
		std::map<std::string, LiveCount> counts;
		for (const auto& [name, count] : readLiveCounts(servers, "live_counts.txt"))
			counts[name] = count;
		for (const std::string_view name : {"QuestEnv", "QuestState", "QuestVars", "QuestStateList", "Player"}) {
			const auto found = counts.find(std::string(name));
			if (found == counts.end()) {
				ADD_FAILURE() << "Y14: live_counts.txt has no row for " << name;
				continue;
			}
			EXPECT_EQ(found->second.live, 0) << "Y14: live instances left: " << found->second.line;
			EXPECT_GT(found->second.created, 0) << "Y14: " << found->second.line << " - the run created none";
		}
		const auto questEnv = counts.find("QuestEnv");
		const auto questState = counts.find("QuestState");
		if (questEnv != counts.end() && questState != counts.end())
			std::cout << "Y14: " << questEnv->second.line << "; " << questState->second.line << std::endl;
		for (const std::string& line : servers.gameServer()->findLogLines("quest handlers", 3))
			std::cout << "Y14: " << line << std::endl;
	});

	finishRun(servers, outputDir, variant.testName);
}

} // namespace

// ---- the gates ---------------------------------------------------------------------------------------------------------------------------

/** `gs.scenario.m5d` (G-03): the quest cases of §10.2 with `gameserver.geodata.enable=false` */
TEST(M5dScenario, Run) {
	runM5dGate({false, "gs.scenario.m5d", "m5d", "m5d", "m5d"});
}

/**
 * `gs.scenario.m5d_geo` (G-04, §10.5): the same script with `gameserver.geodata.enable=true`, its own output directory, schema pair and CTest
 * entry, in the same gate slot. **Geo changes nothing on the quest path itself** - `isInTalkRange` has no geo test and no zone-triggered
 * quest is on the start maps (the six `start_zone` quests are in instance 300610000) - so, as m5b-plan.md §6.4 did, this is a re-run whose
 * value is the approach and the fights under geo heights (mires stands 12 m lower than the Elyos spawn): the walks, the aggro of the kerubs
 * and sprigg workers (GeoService.canSee) and the kill counts. No row here can fail only with geo; none is invented.
 */
TEST(M5dScenarioGeo, Run) {
	runM5dGate({true, "gs.scenario.m5d_geo", "m5d_geo", "m5dgeo", "m5dg"});
}

} // namespace aion::gameserver::scenario
