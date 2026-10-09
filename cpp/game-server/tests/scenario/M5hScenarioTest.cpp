// The M5h legion gate (m5h-plan.md G-03, §10.2, §10.3): one login server and one game server as child processes on their own test schemas, and
// THREE Elyos accounts online at once - A a Warrior (the founder), B a Mage (the member), C a Priest (the brigade general of a seeded legion of
// level 3): create and its refusals, invite (accepted, the own-member and other-legion refusals), the announcement (the volunteer's refusal,
// the 256-character cut, the notice), ranks, permissions, the self introduction and the nickname, the level-up refused for money, a member's
// relog (the logout's store, the login's notice), the history pages, the seeded legion's emblem (a predefined one, a 9,000-byte upload in two
// chunks, B reading it with and without its data), leave and kick; the studio (by quest and by fee at Parrine, entered through the use bar and the beam,
// decorated, used, configured, left, destroyed and saved, re-entered, quit inside and logged into again, seen in the member list); then the
// reports the server writes at shutdown.
//
// **What this gate covers of §10.2, and what not** (docs/design/m5h-plan.md §14): the legion cases C0-C8, C10, C11, C12 and
// C21 without their dialog arms, and the studio cases C13-C20 (HS-4; C13 needs lane C's quest 18802). Not scripted: C8's kill (a member's
// level-up is LegionService.updateMemberInfo, which C8's relog and C13's quest experience drive), C9's warehouse and C22's disband (both open
// through DialogService's npc dialogs, m5h-plan.md A-06, and the warehouse's items through CM_MOVE_ITEM, A-03). Their unit tests stand
// (tests/legionhouse/LegionServiceTest.cpp: the lazy disband, the warehouse rules) and the rows are the plan's open items.
//
// Every expectation is independent of the C++ server code, as in the earlier gates: server packets are read by name and SM_SYSTEM_MESSAGE /
// SM_QUESTION_WINDOW with the decoders of tests/scenario/decoders, and every id and Java constant comes from `tools/oracle/oracle.py
// m5h-legion` (the message and question ids, the legion enums, LegionService's emblem chunk and notice limit), `oracle.py m5h-housing` (the
// studio path's npc spots and talk delays, the Elyos studio, the furniture templates, PartType, HouseDoorState) or from the Java method an
// assertion is about, cited at the line.
//
// **Three clients, read in turn**, as the M5g gate reads four: every step drains every client (drainAll) and then reads each one to an empty
// socket (catchUp, the M5g gate's C12 lesson), and a window "of X" is X's packets recorded between two marks.
//
// **This file deliberately does not share the other gates' helpers** (M5b3ScenarioTest.cpp gives the reason); the scaffolding below is
// M5gScenarioTest.cpp's, never an assertion.

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
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

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

#include "AsyncAllowed.h"
#include "FakeLoginClient.h"
#include "GameSession.h"
#include "InventoryModel.h"
#include "Oracle.h"
#include "PacketSequence.h"
#include "PrologueSupport.h"
#include "ScenarioDatabase.h"
#include "ScenarioServers.h"
#include "decoders/CombatDecoders.h"
#include "decoders/EconomyDecoders.h"
#include "decoders/HousingDecoders.h"
#include "decoders/ItemDecoders.h"
#include "decoders/PacketDecoders.h"
#include "decoders/ProgressionDecoders.h"
#include "decoders/QuestDecoders.h"
#include "decoders/SkillDecoders.h"
#include "decoders/TeamDecoders.h"
#include "decoders/TravelDecoders.h"

namespace aion::gameserver::scenario {
namespace {

using namespace std::chrono_literals;
using decoders::DecodeError;
using nlohmann::json;
using Packet = GameSession::Packet;

/** the quiet period that ends a burst of server packets (m5a-plan.md §5.4) */
constexpr std::chrono::milliseconds QUIET = 1000ms;
/** How long collectBurst waits for the FIRST packet of an answer (M5dScenarioTest.cpp: a loaded machine delays a re-entry) */
constexpr std::chrono::milliseconds FIRST_REPLY_WAIT = 5000ms;
constexpr std::chrono::milliseconds BURST_LIMIT = 90s;

/** SM_CREATE_CHARACTER response codes (SM_CREATE_CHARACTER.java) */
constexpr int32_t RESPONSE_OK = 0;
constexpr int32_t RESPONSE_OPEN_CREATION_WINDOW = 22;
/** SM_ENTER_WORLD_CHECK's first byte for a character that may enter */
constexpr uint8_t ENTER_WORLD_OK = 0;



/** where a character stands to talk (inside the talk distance of 5 + 1, PositionUtil.isInTalkRange) */
constexpr double TALK_DISTANCE = 3.0;
/** the melee distance of m5b-plan.md K4b: inside the Warrior's attack range */
constexpr double MELEE_DISTANCE = 2.0;

/** STR_GET_EXP (SM_SYSTEM_MESSAGE.java), the experience message Events decodes */
constexpr int32_t STR_GET_EXP_ID = 1370000;

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

/** SM_SYSTEM_MESSAGE (SM_SYSTEM_MESSAGE.java:28940-28953) with its parameter lists (M5dScenarioTest.cpp's decodeSystemMessage) */
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

/** SM_ENTER_WORLD_CHECK's first byte (SM_ENTER_WORLD_CHECK.java: writeC(msg), then the rest) */
std::optional<uint8_t> enterWorldCheck(const std::vector<Packet>& burst) {
	const Packet* check = firstOfName(burst, "SM_ENTER_WORLD_CHECK");
	if (check == nullptr || check->data.empty())
		return std::nullopt;
	return check->data[0];
}

// ---- the progression events of a window --------------------------------------------------------------------------------------------------

/**
 * One decoded pass over a window's progression packets. `tokens` lists them in arrival order in a compact notation the §10.3 rows compare
 * with; every other packet is not a token:
 *   "LEVEL_UP <level>" / "CLASS_CHANGE <level>" - the character's own SM_ACTION_ANIMATION (0 / 4), "ANIM@<object> <id> <level>" another's;
 *   "SKILL <id> <messageId>" - an SM_SKILL_LIST of one skill (the learn), "SKILLS <n>" - a list of n; "SKILL_REMOVE <id>";
 *   "ADD <quest> s<status> v<vars>" / "UPDATE ..." / "EMPTY" - SM_QUEST_ACTION; "NEARBY" - SM_NEARBY_QUESTS;
 *   "DW <npc> <page> <quest>" - SM_DIALOG_WINDOW (the npc as its label, or its object id, 0 for the window without a target);
 *   "EXP" - SM_STATUPDATE_EXP; "GET_EXP <n>" - STR_GET_EXP's gained exp; "MSG <id>" - any other system message;
 *   "TEXT <message>" - SM_MESSAGE; "STATS" - SM_STATS_INFO; "PLAYER_INFO <object> <class id>"; "DP <n>" - SM_STATUPDATE_DP;
 *   "FLY <current>" - SM_FLY_TIME; "RESURRECT <name> <skill>"; "MANTRA <effector> <skill>"; "ROBOT <object> <robot id>";
 *   "SUMMON_PANEL <object>", "SUMMON_PANEL_REMOVE <skill>", "SUMMON_OWNER_REMOVE <object>"; "DIE <object>" - SM_EMOTION DIE.
 */
struct Events {
	std::vector<std::string> tokens;
	std::vector<decoders::SkillEntry> learned;
	std::vector<int32_t> learnMessages;
	std::vector<decoders::QuestAction> actions;
	std::vector<decoders::DialogWindow> windows;
	std::vector<decoders::StatUpdateExp> exps;
	std::vector<decoders::StatsInfo> stats;
	std::vector<decoders::PlayerInfo> playerInfos;
	std::vector<decoders::FlyTime> flyTimes;
	std::vector<int32_t> messages;
	std::vector<std::string> texts;
	std::vector<std::string> decodeFailures;

	std::string describe() const { return "[" + join(tokens, " | ") + "]"; }
	bool has(std::string_view token) const { return std::ranges::find(tokens, token) != tokens.end(); }
	size_t count(std::string_view prefix) const {
		return static_cast<size_t>(std::ranges::count_if(tokens, [&](const std::string& t) { return t.starts_with(prefix); }));
	}
	std::optional<size_t> indexOf(std::string_view token) const {
		const auto found = std::ranges::find(tokens, token);
		return found == tokens.end() ? std::nullopt : std::optional<size_t>(static_cast<size_t>(found - tokens.begin()));
	}
	/** whether `expected` occurs in `tokens` in this order (other tokens between them allowed) */
	bool inOrder(const std::vector<std::string>& expected) const {
		size_t next = 0;
		for (const std::string& token : tokens)
			if (next < expected.size() && token == expected[next])
				next++;
		return next == expected.size();
	}
};

Events eventsOf(const std::vector<Packet>& packets, int32_t playerId, const std::map<int32_t, std::string>& labels) {
	Events events;
	const auto label = [&](int32_t objectId) {
		const auto found = labels.find(objectId);
		return found == labels.end() ? std::to_string(objectId) : found->second;
	};
	for (size_t i = 0; i < packets.size(); i++) {
		const Packet& packet = packets[i];
		try {
			if (packet.name == "SM_ACTION_ANIMATION") {
				const decoders::ActionAnimation animation = decoders::decodeActionAnimation(packet.data);
				if (animation.objectId == playerId && animation.animation == decoders::ACTION_ANIMATION_LEVEL_UP)
					events.tokens.push_back("LEVEL_UP " + std::to_string(animation.levelOrObjectId));
				else if (animation.objectId == playerId && animation.animation == decoders::ACTION_ANIMATION_CLASS_CHANGE)
					events.tokens.push_back("CLASS_CHANGE " + std::to_string(animation.levelOrObjectId));
				else
					events.tokens.push_back("ANIM@" + label(animation.objectId) + " " + std::to_string(animation.animation) + " " +
					                        std::to_string(animation.levelOrObjectId));
			} else if (packet.name == "SM_SKILL_LIST") {
				const decoders::SkillList list = decoders::decodeSkillList(packet.data);
				if (list.skills.size() == 1 && !list.silentUpdate) {
					events.learned.push_back(list.skills.front());
					events.learnMessages.push_back(list.messageId);
					events.tokens.push_back("SKILL " + std::to_string(list.skills.front().skillId) + " " + std::to_string(list.messageId));
				} else {
					events.tokens.push_back("SKILLS " + std::to_string(list.skills.size()));
				}
			} else if (packet.name == "SM_SKILL_REMOVE") {
				events.tokens.push_back("SKILL_REMOVE " + std::to_string(decoders::decodeSkillRemove(packet.data).skillId));
			} else if (packet.name == "SM_QUEST_ACTION") {
				const decoders::QuestAction action = decoders::decodeQuestAction(packet.data);
				events.actions.push_back(action);
				if (action.empty)
					events.tokens.push_back("EMPTY");
				else if (action.actionType == decoders::QUEST_ACTION_ADD || action.actionType == decoders::QUEST_ACTION_UPDATE)
					events.tokens.push_back(std::string(action.actionType == decoders::QUEST_ACTION_ADD ? "ADD " : "UPDATE ") + std::to_string(action.questId) +
					                        " s" + std::to_string(action.status) + " v" + std::to_string(action.questVarsAndFlags));
				else
					events.tokens.push_back("QUEST_ACTION " + std::to_string(action.actionType) + " " + std::to_string(action.questId));
			} else if (packet.name == "SM_NEARBY_QUESTS") {
				events.tokens.push_back("NEARBY");
			} else if (packet.name == "SM_DIALOG_WINDOW") {
				const decoders::DialogWindow window = decoders::decodeDialogWindow(packet.data);
				events.windows.push_back(window);
				events.tokens.push_back("DW " + (window.targetObjectId == 0 ? std::string("0") : label(window.targetObjectId)) + " " +
				                        std::to_string(window.dialogPageId) + " " + std::to_string(window.questId));
			} else if (packet.name == "SM_STATUPDATE_EXP") {
				events.exps.push_back(decoders::decodeStatUpdateExp(packet.data));
				events.tokens.push_back("EXP");
			} else if (packet.name == "SM_SYSTEM_MESSAGE") {
				const SystemMessage message = decodeSystemMessage(packet.data);
				if (message.messageId == STR_GET_EXP_ID && message.params.size() >= 2) {
					events.tokens.push_back("GET_EXP " + message.params[1]);
				} else {
					events.messages.push_back(message.messageId);
					events.tokens.push_back("MSG " + std::to_string(message.messageId));
				}
			} else if (packet.name == "SM_MESSAGE") {
				const decoders::Message message = decoders::decodeMessage(packet.data);
				events.texts.push_back(message.message);
				events.tokens.push_back("TEXT " + message.message);
			} else if (packet.name == "SM_STATS_INFO") {
				events.stats.push_back(decoders::decodeStatsInfo(packet.data));
				events.tokens.push_back("STATS");
			} else if (packet.name == "SM_PLAYER_INFO") {
				const decoders::PlayerInfo info = decoders::decodePlayerInfo(packet.data);
				events.playerInfos.push_back(info);
				events.tokens.push_back("PLAYER_INFO " + label(info.objectId) + " " + std::to_string(info.classId));
			} else if (packet.name == "SM_STATUPDATE_DP") {
				events.tokens.push_back("DP " + std::to_string(decoders::decodeStatUpdateDp(packet.data)));
			} else if (packet.name == "SM_FLY_TIME") {
				const decoders::FlyTime fly = decoders::decodeFlyTime(packet.data);
				events.flyTimes.push_back(fly);
				events.tokens.push_back("FLY " + std::to_string(fly.currentFp));
			} else if (packet.name == "SM_RESURRECT") {
				const decoders::Resurrect resurrect = decoders::decodeResurrect(packet.data);
				events.tokens.push_back("RESURRECT " + resurrect.name + " " + std::to_string(resurrect.skillId));
			} else if (packet.name == "SM_MANTRA_EFFECT") {
				const decoders::MantraEffect mantra = decoders::decodeMantraEffect(packet.data);
				events.tokens.push_back("MANTRA " + label(mantra.effectorObjectId) + " " + std::to_string(mantra.subEffectId));
			} else if (packet.name == "SM_RIDE_ROBOT") {
				const decoders::RideRobot robot = decoders::decodeRideRobot(packet.data);
				events.tokens.push_back("ROBOT " + label(robot.objectId) + " " + std::to_string(robot.robotId));
			} else if (packet.name == "SM_SUMMON_PANEL") {
				events.tokens.push_back("SUMMON_PANEL " + std::to_string(decoders::decodeSummonPanel(packet.data).objectId));
			} else if (packet.name == "SM_SUMMON_PANEL_REMOVE") {
				events.tokens.push_back("SUMMON_PANEL_REMOVE " + std::to_string(decoders::decodeSummonPanelRemove(packet.data)));
			} else if (packet.name == "SM_SUMMON_OWNER_REMOVE") {
				events.tokens.push_back("SUMMON_OWNER_REMOVE " + std::to_string(decoders::decodeSummonOwnerRemove(packet.data)));
			} else if (packet.name == "SM_EMOTION") {
				const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
				if (emotion.emotionType == decoders::EMOTION_DIE)
					events.tokens.push_back("DIE " + label(emotion.senderObjectId));
			}
		} catch (const std::exception& error) {
			events.decodeFailures.push_back(packet.name + " #" + std::to_string(i) + ": " + error.what());
		}
	}
	return events;
}

std::string questToken(std::string_view type, int32_t questId, uint8_t status, int32_t vars) {
	return std::string(type) + " " + std::to_string(questId) + " s" + std::to_string(status) + " v" + std::to_string(vars);
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

int32_t failedAssertions(bool fatalOnly = false) {
	const ::testing::TestResult* result = ::testing::UnitTest::GetInstance()->current_test_info()->result();
	int32_t failed = 0;
	for (int i = 0; i < result->total_part_count(); i++)
		if (fatalOnly ? result->GetTestPartResult(i).fatally_failed() : result->GetTestPartResult(i).failed())
			failed++;
	return failed;
}

/** Runs the cases in order (M5dScenarioTest.cpp's CaseLog): false after an exception or a fatal failure, true after EXPECT failures only */
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
		std::cout << "---- " << id << " " << title << std::endl;
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

/** The object ids the server announced as npcs (SM_NPC_INFO), with their last known position and whether they died */
class KnownNpcs {
public:
	struct Npc {
		int32_t templateId = 0;
		float x = 0, y = 0, z = 0;
		bool dead = false;
		bool deleted = false;
		/** when its last SM_MOVE arrived (default: never) */
		std::chrono::steady_clock::time_point lastMoveAt{};
		/**
		 * When the npc reaches (x, y, z) at the latest: the last SM_MOVE's time plus the way from the position it reported to its target at
		 * `slowestSpeed` (a random walk names only its destination, and a walk of a few metres takes many seconds); an exactly known
		 * position (SM_NPC_INFO, a stumble's result) is reached at once
		 */
		std::chrono::steady_clock::time_point arrivalAt{};
	};

	/** the slowest speed an npc's SM_MOVE may be walked at, in m/s (KnownNpcs::Npc::arrivalAt); 0 waits for no walk */
	void setSlowestSpeed(float metresPerSecond) { slowestSpeed = metresPerSecond; }

	/** a position the server reported exactly (a stumble's target position, SM_CASTSPELL_RESULT.java:145-152) */
	void place(int32_t objectId, float x, float y, float z, std::chrono::steady_clock::time_point at) {
		scan();
		const auto found = npcs.find(objectId);
		if (found == npcs.end())
			return;
		found->second.x = x;
		found->second.y = y;
		found->second.z = z;
		found->second.arrivalAt = at;
	}

	void follow(const GameSession* next) {
		session = next;
		scanned = 0;
		npcs.clear();
	}

	bool contains(int32_t objectId) {
		scan();
		return npcs.contains(objectId);
	}

	std::function<bool(int32_t)> predicate() {
		return [this](int32_t objectId) { return contains(objectId); };
	}

	const std::map<int32_t, Npc>& all() {
		scan();
		return npcs;
	}

	std::optional<Npc> get(int32_t objectId) {
		scan();
		const auto found = npcs.find(objectId);
		return found == npcs.end() ? std::nullopt : std::optional<Npc>(found->second);
	}

private:
	void scan() {
		if (session == nullptr)
			return;
		const std::vector<Packet>& packets = session->recorded();
		for (; scanned < packets.size(); scanned++) {
			const Packet& packet = packets[scanned];
			try {
				if (packet.name == "SM_NPC_INFO") {
					const decoders::NpcInfo info = decoders::decodeNpcInfo(packet.data);
					Npc& npc = npcs[info.objectId];
					npc.templateId = info.templateId;
					npc.x = info.x;
					npc.y = info.y;
					npc.z = info.z;
					npc.dead = false;
					npc.deleted = false;
					npc.arrivalAt = packet.receivedAt;
					// an npc seen while it walks: SM_NPC_INFO carries the move controller's target, which is its own position when it stands
					// (NpcMoveController.getTargetX2, SM_NPC_INFO.java:110-112), so it is where the npc goes, as for an SM_MOVE
					const double way = std::hypot(double(info.targetX) - info.x, double(info.targetY) - info.y);
					if (way > 0.01 && (info.targetX != 0 || info.targetY != 0)) {
						npc.x = info.targetX;
						npc.y = info.targetY;
						npc.z = info.targetZ;
						npc.lastMoveAt = packet.receivedAt;
						npc.arrivalAt += arrivalDelay(way);
					}
				} else if (packet.name == "SM_MOVE") {
					const decoders::NpcMove move = decoders::decodeNpcMove(packet.data);
					const auto found = npcs.find(move.objectId);
					if (found != npcs.end()) {
						// a move with a target goes there (POSITION | MANUAL: the move controller's target); else the reported position
						const std::array<float, 3> at = move.target.value_or(std::array<float, 3>{move.x, move.y, move.z});
						found->second.x = at[0];
						found->second.y = at[1];
						found->second.z = at[2];
						found->second.lastMoveAt = packet.receivedAt;
						found->second.arrivalAt = packet.receivedAt + arrivalDelay(std::hypot(double(at[0]) - move.x, double(at[1]) - move.y));
					}
				} else if (packet.name == "SM_EMOTION") {
					const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
					const auto found = npcs.find(emotion.senderObjectId);
					if (found != npcs.end() && emotion.emotionType == decoders::EMOTION_DIE)
						found->second.dead = true;
				} else if (packet.name == "SM_DELETE") {
					const auto found = npcs.find(decoders::decodeDeleteObjectId(packet.data));
					if (found != npcs.end())
						found->second.deleted = true;
				}
			} catch (const DecodeError&) {
				// a packet that does not decode is not an npc's
			}
		}
	}

	/** how long a way of `metres` takes at the slowest speed (0 when no speed is set) */
	std::chrono::milliseconds arrivalDelay(double metres) const {
		return slowestSpeed > 0 ? std::chrono::milliseconds(int64_t(std::ceil(metres / slowestSpeed * 1000))) : 0ms;
	}

	const GameSession* session = nullptr;
	size_t scanned = 0;
	std::map<int32_t, Npc> npcs;
	float slowestSpeed = 0;
};

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
		const auto window = collected.empty() ? std::max(quiet, FIRST_REPLY_WAIT) : quiet;
		const auto quietLeft = std::chrono::duration_cast<std::chrono::milliseconds>(lastAwaited + window - now);
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

/** Reads until `accept` answers true for a packet, recording everything on the way, or until `timeout`. @return the index in recorded() */
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

Packet waitFor(GameSession& session, std::string_view name, std::chrono::milliseconds timeout = 15s) {
	const std::optional<size_t> index = readUntil(session, [name](const Packet& packet) { return packet.name == name; }, timeout);
	if (!index)
		throw std::runtime_error("timeout waiting for " + std::string(name) + (session.client.socket.isClosed() ? " (the connection closed)" : ""));
	return session.recorded()[*index];
}

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

void expectSequence(const std::vector<Packet>& packets, std::string_view pattern, const AsyncAllowed& async, std::string_view row) {
	const PacketSequence sequence = PacketSequence::parse(pattern);
	const std::vector<std::string> names = namesOf(packets);
	const PacketSequence::Result result = sequence.match(names, async.predicate(packets));
	EXPECT_TRUE(result.matched) << row << ": " << result.message << "\n  expected: " << sequence.toString() << "\n  got (" << names.size()
	                            << "): " << join(names);
}

/**
 * The CM_ENTER_WORLD part of m5a-plan.md §5.8 for a FIRST enter world (M5dScenarioTest.cpp's enterWorldPattern(true)): the first-enter level
 * change 0 -> 1 with the prologue's mission in front. Only a first enter is matched against it: a later one carries an offline level change,
 * the class window (S1) or a Daeva's packets at positions the rows below read token by token instead.
 */
std::string firstEnterWorldPattern() {
	std::string pattern(PROLOGUE_FIRST_ENTER_LEVEL_CHANGE);
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


// ---- the scenario clients ---------------------------------------------------------------------------------------------------------------

/** one character of an account */
struct Character {
	std::string label; // "A1", "B1" ...
	std::string name;
	int32_t classId = 0; // the creation class (NewCharacter's playerClassId, PlayerClass.getClassId)
	std::string startingClass, daevaClass;
	int32_t playerId = 0;
};

struct ScenarioClient {
	std::string label; // "A" or "B"
	std::string account;
	std::string password = "m5hPassword1";
	std::unique_ptr<FakeLoginClient> login;
	std::unique_ptr<GameSession> game;
	FakeLoginClient::SessionKey key;
	Character* character = nullptr;
	InventoryModel model;
	KnownNpcs npcs;
	AsyncAllowed async = AsyncAllowed::m5aDefault();
	/** the walk cursor: where the server has the character after its last CM_MOVE (or its spawn) */
	float x = 0, y = 0, z = 0;
	std::vector<Packet> lastEnterWorld, lastLevelReady;
	std::map<int32_t, std::string> labels;

	int32_t playerId() const { return character ? character->playerId : 0; }
	size_t mark() const { return game ? game->recorded().size() : 0; }
	std::vector<Packet> since(size_t from) const { return game ? slice(*game, from) : std::vector<Packet>{}; }
	Events events(size_t from) const { return eventsOf(since(from), playerId(), labels); }
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
	std::optional<decoders::StatsInfo> lastStats() const {
		if (!game)
			return std::nullopt;
		const std::vector<Packet>& packets = game->recorded();
		for (size_t i = packets.size(); i-- > 0;)
			if (packets[i].name == "SM_STATS_INFO") {
				try {
					return decoders::decodeStatsInfo(packets[i].data);
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

/** CM_ENTER_WORLD and its burst; the character must be let in (SM_ENTER_WORLD_CHECK 0) and spawned. @return the burst */
std::vector<Packet> enterWorld(ScenarioClient& client, bool firstEnter) {
	client.async = AsyncAllowed::m5aDefault();
	client.async.selfPlayerState(client.playerId());
	client.async.npcActivity(client.npcs.predicate());
	client.game->send(GameSession::CM_ENTER_WORLD, GameSession::buildCM_ENTER_WORLD(client.playerId()));
	std::vector<Packet> burst = collectAnswer(*client.game, client.async, 30s);
	if (burst.empty())
		throw std::runtime_error(client.character->label + ": no packet after CM_ENTER_WORLD");
	const std::optional<uint8_t> check = enterWorldCheck(burst);
	if (!check || *check != ENTER_WORLD_OK)
		throw std::runtime_error(client.character->label + ": the enter world was refused (SM_ENTER_WORLD_CHECK " +
		                         (check ? std::to_string(*check) : std::string("missing")) + "): " + join(namesOf(burst)));
	const Packet* spawn = firstOfName(burst, "SM_PLAYER_SPAWN");
	if (spawn == nullptr)
		throw std::runtime_error(client.character->label + ": no SM_PLAYER_SPAWN after CM_ENTER_WORLD: " + join(namesOf(burst)));
	const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(spawn->data);
	client.x = spawned.x;
	client.y = spawned.y;
	client.z = spawned.z;
	if (firstEnter)
		expectSequence(burst, firstEnterWorldPattern(), client.async, client.character->label + " first enter world");
	client.model.sync();
	client.lastEnterWorld = burst;
	return burst;
}

std::vector<Packet> levelReady(ScenarioClient& client) {
	client.game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
	std::vector<Packet> burst = collectAnswer(*client.game, client.async, 30s);
	if (burst.empty())
		throw std::runtime_error(client.character->label + ": no packet after CM_LEVEL_READY");
	client.model.sync();
	client.lastLevelReady = burst;
	return burst;
}

/** CM_QUIT(0): the character leaves the world and the connection ends - the only state in which a database read sees its last save */
void disconnect(ScenarioClient& client) {
	if (!client.game)
		return;
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

/** a new login and `character` into the world. @return the enter-world burst (the level-ready burst is client.lastLevelReady) */
std::vector<Packet> enterAs(ScenarioServers& servers, ScenarioClient& client, Character& character) {
	logIn(servers, client);
	client.character = &character;
	client.game->send(GameSession::CM_MAY_LOGIN_INTO_GAME, GameSession::buildCM_MAY_LOGIN_INTO_GAME());
	expectNext(*client.game, "SM_MAY_LOGIN_INTO_GAME", client.async);
	std::this_thread::sleep_for(1500ms); // gameserver.character.reentry.time is 1 s in the scenario profile
	std::vector<Packet> burst = enterWorld(client, false);
	levelReady(client);
	return burst;
}

/** the walk of the M5b gates: 5 m steps, each followed by a short read, then a stop */
void walkTo(ScenarioClient& client, float toX, float toY, float toZ) {
	const double total = distance2d(client.x, client.y, toX, toY);
	const int32_t steps = std::max(1, static_cast<int32_t>(total / 5.0));
	const float fromX = client.x, fromY = client.y, fromZ = client.z;
	for (int32_t step = 1; step <= steps; step++) {
		const float t = static_cast<float>(step) / static_cast<float>(steps);
		client.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(fromX + (toX - fromX) * t, fromY + (toY - fromY) * t, fromZ + (toZ - fromZ) * t,
		                                                                  0, static_cast<int8_t>(0xE0), toX, toY, toZ));
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

/** the object id of the npc of `templateId` nearest to (x, y) within 5 m, from every SM_NPC_INFO this session recorded */
std::optional<int32_t> npcObject(ScenarioClient& client, int32_t templateId, float x, float y) {
	std::optional<int32_t> found;
	double best = 5.0;
	for (const auto& [objectId, npc] : client.npcs.all()) {
		if (npc.templateId != templateId || npc.deleted)
			continue;
		const double distance = distance2d(npc.x, npc.y, x, y);
		if (distance < best) {
			best = distance;
			found = objectId;
		}
	}
	return found;
}

// ---- the check output reports (X19, X20) ------------------------------------------------------------------------------------------------

enum class AllowlistSection { HitAtLeastOnce, HitNever, NotPinned };

struct AllowlistEntry {
	std::string site;
	AllowlistSection section = AllowlistSection::NotPinned;
};

/** Reads tests/scenario/m5h_partial_allowlist.txt with its three sections ("# --- SECTION A/B/C" marker lines, as the earlier lists) */
std::vector<AllowlistEntry> readAllowlist() {
	std::vector<AllowlistEntry> entries;
	std::ifstream in(AION_SCENARIO_M5H_PARTIAL_ALLOWLIST, std::ios::binary);
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
	} catch (const std::exception& exception) {
		std::cout << "the scenario schemas could not be dropped (" << exception.what() << ")" << std::endl;
	}
}

/** the decoded SM_SYSTEM_MESSAGEs of a window */
std::vector<SystemMessage> messagesOf(const std::vector<Packet>& packets) {
	std::vector<SystemMessage> messages;
	for (const Packet& packet : ofName(packets, "SM_SYSTEM_MESSAGE"))
		messages.push_back(decodeSystemMessage(packet.data));
	return messages;
}

/** how many messages of `id` a window holds, whose parameters start with `params` */
size_t countMessage(const std::vector<Packet>& packets, int32_t id, const std::vector<std::string>& params = {}) {
	size_t n = 0;
	for (const SystemMessage& message : messagesOf(packets)) {
		if (message.messageId != id || message.params.size() < params.size())
			continue;
		if (std::equal(params.begin(), params.end(), message.params.begin()))
			n++;
	}
	return n;
}

// ---- the studio cases (m5h-plan.md §10.2 C14-C20, HS-4) ----------------------------------------------------------------------------------

/** the client packet ids of ClientPacketInfo.gen.inc for the studio cases */
constexpr int32_t CM_HOUSE_SCRIPT = 30;
constexpr int32_t CM_HOUSE_KICK = 72;
constexpr int32_t CM_HOUSE_SETTINGS = 73;
constexpr int32_t CM_HOUSE_DECORATE = 75;
constexpr int32_t CM_HOUSE_EDIT = 82;
constexpr int32_t CM_USE_HOUSE_OBJECT = 224;
constexpr int32_t CM_RELEASE_OBJECT = 225;
/** the npcs of the studio path (m5h-plan.md §2.7 rows 2-4, 9): Parrine (the paid studio), the studio entrance in Oriel, the studio exit */
constexpr int32_t PARRINE = 830069;
constexpr int32_t STUDIO_ENTRANCE = 730517;
constexpr int32_t STUDIO_EXIT = 830229;
constexpr int32_t ORIEL = 700010000;
constexpr int32_t STUDIO_MAP = 720010000;
/** DialogAction.HOUSING_RECREATE_PERSONAL_INS (DialogAction.java:111), DialogService's paid studio arm (DialogService.java:270-272) */
constexpr int32_t HOUSING_RECREATE_PERSONAL_INS = 96;
/** DialogAction.SELECTED_QUEST_NOREWARD (DialogAction.java:38): the finish of a quest without a selectable reward */
constexpr int32_t SELECTED_QUEST_NOREWARD = 23;
/** [Housing] And A Home for Every Daeva, the Elyos studio quest (_18802AndAHomeforEveryDaeva.java; HousingService.canOwnHouse) */
constexpr int32_t STUDIO_QUEST = 18802;
/** the furniture (m5h-plan.md §10.1 "Seeds"): a bed (a chair template), the cake (a use_item with the COOKING limit) and a wallpaper */
constexpr int32_t BED_ITEM = 170120000;
constexpr int32_t CAKE_ITEM = 170190034;
constexpr int32_t WALLPAPER_ITEM = 171110000;
/** SM_USE_OBJECT's action types of UseableItemObject.onUse (UseableItemObject.java: 8 the use bar, 9 its end or cancel) */
constexpr uint8_t USE_OBJECT_HOUSE_USE = 8;
constexpr uint8_t USE_OBJECT_HOUSE_END = 9;
/** EmotionType START_QUESTLOOT (42) and END_QUESTLOOT (43), EmotionType.java:50-51: the use bar of ActionItemNpcAI */
constexpr uint8_t EMOTION_START_QUESTLOOT = 42;
constexpr uint8_t EMOTION_END_QUESTLOOT = 43;

/** the gate's oracle answer of m5h-housing */
struct HousingAnswer {
	struct Spot {
		int32_t map = 0;
		float x = 0, y = 0, z = 0;
		int32_t talkDelayMs = 0;
	};
	struct Furniture {
		std::optional<int32_t> houseObject, decoration;
		std::string kind;
		int32_t useDays = 0, delayMs = 0, cooldownSeconds = 0;
		std::optional<int32_t> rewardId;
	};
	std::map<std::string, int32_t> messages;
	std::map<int32_t, Spot> npcs;
	int32_t address = 0, studioMap = 0, building = 0, managerNpc = 0, teleportNpc = 0;
	int64_t goldPrice = 0;
	float x = 0, y = 0, z = 0;
	int32_t exitMap = 0;
	float exitX = 0, exitY = 0, exitZ = 0;
	std::map<std::string, std::array<float, 3>> houseNpcs;
	std::map<int32_t, Furniture> items;
	/** the decor slot of a PartType's room in SM_HOUSE_RENDER (writeCommonInfo's order) and the CM_HOUSE_DECORATE line of its first room */
	std::map<std::string, std::pair<size_t, int32_t>> partSlots;
	std::map<std::string, int32_t> doorStates;
	std::map<std::string, int32_t> houseOwnerStates;
	size_t scriptPadding = 0;

	int32_t message(const std::string& name) const {
		const auto found = messages.find(name);
		if (found == messages.end())
			throw std::runtime_error("m5h-housing: no message " + name);
		return found->second;
	}
	const Spot& npc(int32_t id) const {
		const auto found = npcs.find(id);
		if (found == npcs.end())
			throw std::runtime_error("m5h-housing: no npc " + std::to_string(id));
		return found->second;
	}
	const Furniture& item(int32_t id) const {
		const auto found = items.find(id);
		if (found == items.end())
			throw std::runtime_error("m5h-housing: no item " + std::to_string(id));
		return found->second;
	}
};

HousingAnswer parseHousing(const std::string& text) {
	const json answer = json::parse(text);
	HousingAnswer housing;
	for (const auto& [name, id] : answer.at("messages").items())
		housing.messages[name] = id.get<int32_t>();
	for (const auto& [id, npc] : answer.at("npcs").items())
		housing.npcs[std::stoi(id)] = {npc.at("map").get<int32_t>(), npc.at("x").get<float>(), npc.at("y").get<float>(), npc.at("z").get<float>(),
		                               npc.at("talkDelayMs").get<int32_t>()};
	const json& studio = answer.at("studio");
	housing.address = studio.at("address").get<int32_t>();
	housing.studioMap = studio.at("map").get<int32_t>();
	housing.x = studio.at("x").get<float>();
	housing.y = studio.at("y").get<float>();
	housing.z = studio.at("z").get<float>();
	housing.exitMap = studio.at("exitMap").get<int32_t>();
	housing.exitX = studio.at("exitX").get<float>();
	housing.exitY = studio.at("exitY").get<float>();
	housing.exitZ = studio.at("exitZ").get<float>();
	housing.building = studio.at("building").get<int32_t>();
	housing.goldPrice = studio.at("goldPrice").get<int64_t>();
	housing.managerNpc = studio.at("managerNpc").get<int32_t>();
	housing.teleportNpc = studio.at("teleportNpc").get<int32_t>();
	for (const auto& [type, spot] : studio.at("houseNpcs").items())
		housing.houseNpcs[type] = {spot.at("x").get<float>(), spot.at("y").get<float>(), spot.at("z").get<float>()};
	for (const auto& [id, item] : answer.at("items").items()) {
		HousingAnswer::Furniture furniture;
		if (!item.at("houseObject").is_null())
			furniture.houseObject = item.at("houseObject").get<int32_t>();
		if (!item.at("decoration").is_null())
			furniture.decoration = item.at("decoration").get<int32_t>();
		if (item.contains("template")) {
			const json& t = item.at("template");
			furniture.kind = t.at("kind").get<std::string>();
			furniture.useDays = t.at("useDays").get<int32_t>();
			furniture.delayMs = t.value("delayMs", 0);
			furniture.cooldownSeconds = t.value("cooldownSeconds", 0);
			if (t.contains("rewardId") && !t.at("rewardId").is_null())
				furniture.rewardId = t.at("rewardId").get<int32_t>();
		}
		housing.items[std::stoi(id)] = furniture;
	}
	size_t slot = 0;
	for (const json& part : answer.at("partTypes")) {
		housing.partSlots[part.at("name").get<std::string>()] = {slot, part.at("startLine").get<int32_t>()};
		slot += part.at("rooms").get<size_t>();
	}
	for (const auto& [name, id] : answer.at("doorStates").items())
		housing.doorStates[name] = id.get<int32_t>();
	for (const auto& [name, id] : answer.at("houseOwnerStates").items())
		housing.houseOwnerStates[name] = id.get<int32_t>();
	housing.scriptPadding = answer.at("scripts").at("padding").size();
	return housing;
}

/** RFC 1950's Adler-32 of `data` */
uint32_t adler32(const std::vector<uint8_t>& data) {
	uint32_t a = 1, b = 0;
	for (uint8_t byte : data) {
		a = (a + byte) % 65521;
		b = (b + a) % 65521;
	}
	return (b << 16) | a;
}

/**
 * A house script as the client sends it (PlayerScripts.decompressAndValidate: CompressUtil.decompress, java.util.zip.Inflater, then a UTF-16LE
 * string of exactly the announced size): the XML's UTF-16LE bytes in a zlib stream of one stored deflate block (RFC 1950/1951 - the gate
 * needs no compressor for Inflater to accept it). @return {compressed, uncompressed size}
 */
std::pair<std::vector<uint8_t>, int32_t> houseScript(std::string_view xml) {
	std::vector<uint8_t> raw;
	for (char c : xml) {
		raw.push_back(static_cast<uint8_t>(c));
		raw.push_back(0);
	}
	std::vector<uint8_t> zlib{0x78, 0x01, 0x01}; // CMF/FLG (deflate, 32K window, check bits), then BFINAL=1 BTYPE=00 (stored)
	const uint16_t length = static_cast<uint16_t>(raw.size());
	zlib.push_back(static_cast<uint8_t>(length));
	zlib.push_back(static_cast<uint8_t>(length >> 8));
	zlib.push_back(static_cast<uint8_t>(~length));
	zlib.push_back(static_cast<uint8_t>(static_cast<uint16_t>(~length) >> 8));
	zlib.insert(zlib.end(), raw.begin(), raw.end());
	const uint32_t check = adler32(raw);
	for (int shift = 24; shift >= 0; shift -= 8)
		zlib.push_back(static_cast<uint8_t>(check >> shift));
	return {zlib, static_cast<int32_t>(raw.size())};
}

/** PositionUtil.getHeadingTowards(x, y, x2, y2) (PositionUtil.java:104-107, 129-131, 137-139): the angle in degrees normalized to [0, 360), / 3 */
int8_t headingTowards(float x, float y, float x2, float y2) {
	float angle = static_cast<float>(std::atan2(static_cast<double>(y2 - y), static_cast<double>(x2 - x)) * 180.0 / 3.14159265358979323846);
	if (angle < 0)
		angle += 360;
	return static_cast<int8_t>(angle / 3);
}

/** the instance id of a personal world's SM_PLAYER_SPAWN: worldChannel = -(worldId + instanceId - 1) (PacketDecoders.h PlayerSpawn) */
int32_t instanceOf(const decoders::PlayerSpawn& spawn) {
	const int32_t channel = spawn.worldChannel < 0 ? -spawn.worldChannel : spawn.worldChannel;
	return channel - spawn.worldId + 1;
}

// ---- the legion gate --------------------------------------------------------------------------------------------------------------------

/** PlayerClass.PRIEST's id (PlayerClass.java) */
constexpr int32_t CLASS_PRIEST = 9;
constexpr int32_t KINAH_ITEM = 182400001;
/** the client packet ids of ClientPacketInfo.gen.inc (the internal opcodes GameSession sends) */
constexpr int32_t CM_LEGION_SEND_EMBLEM_INFO = 16;
constexpr int32_t CM_CHAT_MESSAGE_PUBLIC = 27;
constexpr int32_t CM_LEGION = 45;
constexpr int32_t CM_LEGION_SEND_EMBLEM = 47;
constexpr int32_t CM_LEGION_HISTORY = 55;
constexpr int32_t CM_LEGION_MODIFY_EMBLEM = 59;
constexpr int32_t CM_LEGION_UPLOAD_INFO = 160;
constexpr int32_t CM_LEGION_UPLOAD_EMBLEM = 161;
/** the seeded legion of C (m5h-plan.md §10.1 "Seeds"): an id at the top of the seeded object id range, which IDFactory does not reach in a run */
constexpr int32_t SEEDED_LEGION_ID = ScenarioDatabase::SEEDED_OBJECT_ID_END - 0x100;
/** the custom emblem C uploads: 9,000 bytes in two chunks of 4,500 (§10.2 C12) */
constexpr int32_t EMBLEM_SIZE = 9000;

/** Little endian client packet bodies, written as the Java readImpl reads them (readC/H/D/Q, readS: UTF-16LE with a terminating 0) */
struct Body {
	std::vector<uint8_t> data;
	Body& C(int32_t value) {
		data.push_back(static_cast<uint8_t>(value));
		return *this;
	}
	Body& H(int32_t value) {
		for (int i = 0; i < 2; i++)
			data.push_back(static_cast<uint8_t>(static_cast<uint32_t>(value) >> (8 * i)));
		return *this;
	}
	Body& D(int32_t value) {
		for (int i = 0; i < 4; i++)
			data.push_back(static_cast<uint8_t>(static_cast<uint32_t>(value) >> (8 * i)));
		return *this;
	}
	Body& S(std::string_view ascii) {
		for (char c : ascii)
			H(static_cast<uint8_t>(c));
		return H(0);
	}
	Body& B(const std::vector<uint8_t>& bytes) {
		data.insert(data.end(), bytes.begin(), bytes.end());
		return *this;
	}
};

/** the gate's oracle answer of m5h-legion */
struct LegionAnswer {
	std::map<std::string, int32_t> messages;
	std::map<std::string, int32_t> questions;
	std::map<std::string, int32_t> ranks;
	std::map<std::string, int32_t> historyTypes; // the type ordinal of CM_LEGION_HISTORY: LEGION 0, REWARD 1, WAREHOUSE 2
	std::map<std::string, int32_t> chatTypes;
	int32_t emblemChunkSize = 0;
	int32_t announcementLimit = 0;

	int32_t message(const std::string& name) const {
		const auto found = messages.find(name);
		if (found == messages.end())
			throw std::runtime_error("m5h-legion: no message " + name);
		return found->second;
	}
};

LegionAnswer parseLegion(const std::string& text) {
	const json answer = json::parse(text);
	LegionAnswer legion;
	for (const auto& [name, id] : answer.at("messages").items())
		legion.messages[name] = id.get<int32_t>();
	for (const auto& [name, id] : answer.at("questions").items())
		legion.questions[name] = id.get<int32_t>();
	for (const auto& [name, id] : answer.at("ranks").items())
		legion.ranks[name] = id.get<int32_t>();
	legion.historyTypes = {{"LEGION", 0}, {"REWARD", 1}, {"WAREHOUSE", 2}}; // LegionHistoryAction.Type's ordinals (LegionHistoryAction.java:39)
	for (const auto& [name, id] : answer.at("chatTypes").items())
		legion.chatTypes[name] = id.get<int32_t>();
	legion.emblemChunkSize = answer.at("emblemChunkSize").get<int32_t>();
	legion.announcementLimit = answer.at("announcementLimit").get<int32_t>();
	return legion;
}

void runM5hGate() {
	const std::string testName = "gs.scenario.m5h";
	const char* requireEnvironment = std::getenv("AION_SCENARIO_REQUIRE");
	const bool required = requireEnvironment != nullptr && *requireEnvironment != '\0' && std::string_view(requireEnvironment) != "0";
	const auto unavailable = [&](std::string_view reason) {
		if (required)
			ADD_FAILURE() << testName << " was not configured and AION_SCENARIO_REQUIRE is set: " << reason;
		else
			GTEST_SKIP() << testName << ": skipped (" << reason << ")";
	};
	std::optional<ScenarioEnvironment> environment = ScenarioEnvironment::fromEnvironment();
	if (!environment) {
		unavailable("set AION_TEST_GS_DATABASE_URL and AION_TEST_LS_DATABASE_URL");
		return;
	}
	const std::filesystem::path outputDir = std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "m5h";
	std::optional<Oracle> oracle = Oracle::fromEnvironment(outputDir / "oracle");
	if (!oracle) {
		unavailable("no Python interpreter for tools/oracle: set AION_TEST_PYTHON");
		return;
	}

	CaseLog cases;
	struct ReportPrinter {
		const CaseLog& cases;
		const std::string& testName;
		~ReportPrinter() { std::cout << cases.report(testName) << std::flush; }
	} printer{cases, testName};

	// ---- §10.1: the M5g profile's keys and the legion keys of m5h.properties.example (I-04), written out so the expectations do not depend on
	// the shipped config/main/legion.properties: the creation price of A's 10,000 kinah, a cheap emblem, the level-2 price A cannot pay
	std::map<std::string, std::string> gateKeys;
	gateKeys["gameserver.geodata.enable"] = "false";
	gateKeys["gameserver.npcshouts.enable"] = "false";
	gateKeys["gameserver.analysis.quest_handlers"] = "false";
	gateKeys["gameserver.legion.disbandtime"] = "5";
	gateKeys["gameserver.legion.creationrequiredkinah"] = "10000";
	gateKeys["gameserver.legion.emblemrequiredkinah"] = "1000";
	gateKeys["gameserver.legion.level2requiredkinah"] = "100000";
	gateKeys["gameserver.legion.inviteotherfaction"] = "false";
	gateKeys["gameserver.legion.pattern"] = "[a-zA-Z ]{2,32}";

	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.schemaPrefix = "m5h";
	config.gameServerProperties = gateKeys;
	config.startupTimeout = 10min;
	config.stopTimeout = 3min;
	ScenarioServers servers(config, *environment);
	const std::string schema = servers.gameSchema();
	const ScenarioDatabase& database = servers.gameDatabase();
	std::filesystem::create_directories(outputDir);

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
		servers.startGameServer();
	});

	const std::string suffix = servers.gameSchema().substr(servers.gameSchema().size() - 8);
	ScenarioClient a, b, c;
	std::array<ScenarioClient*, 3> all{&a, &b, &c};
	const std::array<std::string, 3> labels{"A", "B", "C"};
	for (size_t i = 0; i < all.size(); i++) {
		all[i]->label = labels[i];
		all[i]->account = "m5h" + std::string(1, static_cast<char>('a' + i)) + suffix;
		all[i]->password = "m5hPassword1";
	}
	// §10.1's characters, all Elyos; names are letters only (NameRestrictionService)
	Character ca{"A", "Legionfounder", NewCharacter::WARRIOR, "WARRIOR", ""};
	Character cb{"B", "Legionmage", NewCharacter::MAGE, "MAGE", ""};
	Character cc{"C", "Legionpriest", CLASS_PRIEST, "PRIEST", ""};
	std::array<Character*, 3> characters{&ca, &cb, &cc};
	const std::string legionName = "Lanecrafters";
	const std::string seededLegionName = "Seededbanner";

	// ---- the three-client helpers (M5gScenarioTest.cpp's) ----
	const auto drainAll = [&](std::chrono::milliseconds window) {
		const auto deadline = std::chrono::steady_clock::now() + window;
		while (std::chrono::steady_clock::now() < deadline) {
			for (ScenarioClient* client : all)
				if (client->game && !client->game->client.socket.isClosed())
					client->game->next(5ms);
		}
		for (ScenarioClient* client : all)
			if (client->game)
				client->model.sync();
	};
	const auto until = [&](std::chrono::milliseconds timeout, const std::function<bool()>& done) {
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		while (!done()) {
			if (std::chrono::steady_clock::now() >= deadline)
				return false;
			drainAll(100ms);
		}
		return true;
	};
	/** reads each client until one 20 ms read comes back empty (the C12 lesson of the M5g gate: drainAll reads one packet per client a turn) */
	const auto catchUp = [&] {
		for (ScenarioClient* client : all) {
			if (!client->game || client->game->client.socket.isClosed())
				continue;
			const auto deadline = std::chrono::steady_clock::now() + 10s;
			while (std::chrono::steady_clock::now() < deadline && client->game->next(20ms)) {
			}
			client->model.sync();
		}
	};
	/** drains 1.5 s and then every backlog: the end of every step's window */
	const auto settle = [&] {
		drainAll(1500ms);
		catchUp();
	};
	const auto marks = [&] {
		std::array<size_t, 3> m{};
		for (size_t i = 0; i < all.size(); i++)
			m[i] = all[i]->mark();
		return m;
	};
	const auto windowOf = [&](const ScenarioClient& client, const std::array<size_t, 3>& from) {
		for (size_t i = 0; i < all.size(); i++)
			if (all[i] == &client)
				return client.since(from[i]);
		return std::vector<Packet>{};
	};
	const auto legionPacket = [&](ScenarioClient& client, const Body& body) { client.game->send(CM_LEGION, body.data); };
	const auto count = [&](std::string_view sql) { return database.queryLong(schema, sql).value_or(-1); };

	// ---- C0: the oracle ----
	LegionAnswer legion;
	const std::vector<std::string> messageNames{"STR_GUILD_CREATED", "STR_GUILD_CREATE_SAME_GUILD_EXIST", "STR_GUILD_CREATE_INVALID_GUILD_NAME",
		"STR_GUILD_CREATE_NOT_ENOUGH_MONEY", "STR_GUILD_INVITE_SENT_INVITE_MSG_TO_HIM", "STR_GUILD_INVITE_HE_IS_MY_GUILD_MEMBER",
		"STR_GUILD_INVITE_HE_IS_OTHER_GUILD_MEMBER", "STR_GUILD_WRITE_NOTICE_DONT_HAVE_RIGHT", "STR_GUILD_WRITE_NOTICE_DONE", "STR_GUILD_NOTICE",
		"STR_GUILD_CHANGE_RIGHT_DONT_HAVE_RIGHT", "STR_GUILD_WRITE_INTRO_DONE", "STR_GUILD_CHANGE_LEVEL_NOT_ENOUGH_MONEY",
		"STR_MSG_NOTIFY_LOGIN_GUILD", "STR_GUILD_CHANGE_EMBLEM", "STR_GUILD_WARN_SUCCESS_UPLOAD_EMBLEM"};
	runCase("C0", "the oracle answers: m5h-legion (the message and question ids, the legion enums, the emblem chunk and the notice limit)", [&] {
		std::vector<std::string> arguments{"m5h-legion", "--question", "STR_GUILD_INVITE_DO_YOU_ACCEPT_INVITATION", "--message"};
		for (const std::string& name : messageNames)
			arguments.push_back(name);
		legion = parseLegion(oracle->run(arguments));
		EXPECT_EQ(legion.questions.at("STR_GUILD_INVITE_DO_YOU_ACCEPT_INVITATION"), 80001);
		EXPECT_EQ(legion.emblemChunkSize, 7993);
		EXPECT_EQ(legion.announcementLimit, 256);
		EXPECT_EQ(legion.ranks.at("CENTURION"), 2);
		EXPECT_EQ(legion.chatTypes.at("LEGION"), 10);
	});
	const auto msg = [&](const std::string& name) { return legion.message(name); };

	// ---- C1: the three characters and the seeds ----
	const auto endFirstPrologue = [&](ScenarioClient& client, std::string_view label) {
		AsyncAllowed async = client.async;
		async.temporarySpawnUpdates(client.npcs.predicate());
		endPrologue(*client.game, client.lastLevelReady, decoders::ELYOS_PROLOGUE, 0, async, [&] { return collectBurst(*client.game, async); }, label);
		client.model.sync();
	};
	const auto createCharacter = [&](ScenarioClient& client, Character& character) {
		NewCharacter newCharacter;
		newCharacter.name = character.name;
		newCharacter.playerClassId = character.classId;
		client.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(client.key.accountId, client.account, newCharacter, 1));
		EXPECT_EQ(decoders::decodeCreateCharacter(expectNext(*client.game, "SM_CREATE_CHARACTER", client.async).data).responseCode,
		          RESPONSE_OPEN_CREATION_WINDOW);
		client.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(client.key.accountId, client.account, newCharacter, 0));
		const decoders::CreateCharacter created = decoders::decodeCreateCharacter(expectNext(*client.game, "SM_CREATE_CHARACTER", client.async).data);
		if (created.responseCode != RESPONSE_OK || !created.player)
			throw std::runtime_error("creating " + character.label + " answered response code " + std::to_string(created.responseCode));
		character.playerId = created.player->playerId;
	};
	const auto seedKinah = [&](int32_t playerId, int64_t kinah) {
		database.execute(schema, "DELETE FROM inventory WHERE item_owner = " + std::to_string(playerId) + " AND item_id = " + std::to_string(KINAH_ITEM));
		database.seedInventoryItem(schema, {playerId, KINAH_ITEM, kinah, 0, 65535});
	};
	runCase("C1", "three accounts: create A-C, the prologue, the seeds (kinah; C's legion of level 3), then all three enter", [&] {
		for (size_t i = 0; i < all.size(); i++) {
			ScenarioClient& client = *all[i];
			logIn(servers, client);
			createCharacter(client, *characters[i]);
			client.character = characters[i];
			enterWorld(client, true);
			levelReady(client);
			endFirstPrologue(client, client.label + "'s prologue (1000)");
			disconnect(client);
		}
		// §10.1: A's kinah exactly the creation price, B one short, C enough for two emblem changes; C's seeded legion (C its brigade general)
		seedKinah(ca.playerId, 10000);
		seedKinah(cb.playerId, 9999);
		seedKinah(cc.playerId, 100000);
		ASSERT_FALSE(ScenarioDatabase::isInvalidObjectId(SEEDED_LEGION_ID));
		database.execute(schema, "INSERT INTO legions (id, name, level, contribution_points) VALUES (" + std::to_string(SEEDED_LEGION_ID) + ", '" +
		                           seededLegionName + "', 3, 12345)");
		database.execute(schema, "INSERT INTO legion_members (legion_id, player_id, `rank`) VALUES (" + std::to_string(SEEDED_LEGION_ID) + ", " +
		                           std::to_string(cc.playerId) + ", 'BRIGADE_GENERAL')");
		for (size_t i = 0; i < all.size(); i++) {
			enterAs(servers, *all[i], *characters[i]);
			settle();
		}
		// C's legion is loaded at the character list of the new login and C's enter world runs LegionService.onLogin (LegionService.java:762-791)
		EXPECT_EQ(ofName(c.lastEnterWorld, "SM_LEGION_INFO").size() + ofName(c.since(0), "SM_LEGION_INFO").size() > 0, true)
		  << "C1: C's enter world sent no SM_LEGION_INFO";
		EXPECT_FALSE(ofName(c.since(0), "SM_LEGION_MEMBERLIST").empty()) << "C1: no member list for C";
		EXPECT_TRUE(ofName(a.since(0), "SM_LEGION_INFO").empty()) << "C1: A is in no legion";
		EXPECT_EQ(a.model.kinah(), 10000);
		EXPECT_EQ(b.model.kinah(), 9999);
	});

	// ---- C2-C3: create and its refusals ----
	runCase("C2", "create: A CM_LEGION(0x00, name) with exactly the creation price", [&] {
		const auto from = marks();
		legionPacket(a, Body().C(0x00).D(0).S(legionName));
		ASSERT_TRUE(until(10s, [&] { return countMessage(windowOf(a, from), msg("STR_GUILD_CREATED"), {legionName}) == 1; }))
		  << "C2: no STR_GUILD_CREATED";
		settle();
		const std::vector<Packet> wa = windowOf(a, from);
		EXPECT_EQ(ofName(wa, "SM_LEGION_INFO").size(), 1u);
		EXPECT_EQ(ofName(wa, "SM_LEGION_MEMBERLIST").size(), 1u)
		  << "the creator is excluded from his own first member list: one empty part (FixedElementCountSplitList with oneTimeSplitOnEmptyData)";
		EXPECT_EQ(ofName(wa, "SM_LEGION_ADD_MEMBER").size(), 1u);
		EXPECT_EQ(ofName(wa, "SM_LEGION_UPDATE_TITLE").size(), 1u);
		EXPECT_EQ(ofName(wa, "SM_LEGION_HISTORY").size(), 2u) << "CREATE and JOIN";
		EXPECT_EQ(a.model.kinah(), 0) << "the creation took the 10,000";
		EXPECT_EQ(count("SELECT COUNT(*) FROM legions WHERE name = '" + legionName + "'"), 1);
		EXPECT_EQ(count("SELECT COUNT(*) FROM legion_members WHERE `rank` = 'BRIGADE_GENERAL' AND player_id = " + std::to_string(ca.playerId)), 1);
		EXPECT_EQ(count("SELECT COUNT(*) FROM legion_history h JOIN legions l ON l.id = h.legion_id WHERE l.name = '" + legionName + "'"), 2);
	});

	runCase("C3", "refusals: B the same name, an invalid name, a valid name with 9,999 kinah - in Java's order", [&] {
		auto from = marks();
		legionPacket(b, Body().C(0x00).D(0).S(legionName));
		legionPacket(b, Body().C(0x00).D(0).S("A1!"));
		legionPacket(b, Body().C(0x00).D(0).S("Brandnewone"));
		ASSERT_TRUE(until(10s, [&] { return countMessage(windowOf(b, from), msg("STR_GUILD_CREATE_NOT_ENOUGH_MONEY")) == 1; }));
		settle();
		const std::vector<Packet> wb = windowOf(b, from);
		EXPECT_EQ(countMessage(wb, msg("STR_GUILD_CREATE_SAME_GUILD_EXIST")), 1u);
		EXPECT_EQ(countMessage(wb, msg("STR_GUILD_CREATE_INVALID_GUILD_NAME")), 1u);
		EXPECT_TRUE(ofName(wb, "SM_LEGION_INFO").empty());
		EXPECT_EQ(b.model.kinah(), 9999);
		EXPECT_EQ(count("SELECT COUNT(*) FROM legions"), 2) << "A's and C's seeded legion";
	});

	// ---- C4: invite ----
	runCase("C4", "invite: A invites B, B accepts; A invites B again and C (another legion's brigade general)", [&] {
		auto from = marks();
		legionPacket(a, Body().C(0x01).D(0).S(cb.name));
		ASSERT_TRUE(until(10s, [&] { return !ofName(windowOf(b, from), "SM_QUESTION_WINDOW").empty(); })) << "B was not asked";
		const decoders::QuestionWindow question = decoders::decodeQuestionWindow(ofName(windowOf(b, from), "SM_QUESTION_WINDOW")[0].data);
		EXPECT_EQ(question.code, legion.questions.at("STR_GUILD_INVITE_DO_YOU_ACCEPT_INVITATION"));
		EXPECT_EQ(question.params[0], legionName);
		EXPECT_EQ(question.params[1], "1") << "the legion level";
		EXPECT_EQ(question.params[2], ca.name);
		EXPECT_EQ(countMessage(windowOf(a, from), msg("STR_GUILD_INVITE_SENT_INVITE_MSG_TO_HIM"), {cb.name}), 1u);
		b.game->send(GameSession::CM_QUESTION_RESPONSE,
			GameSession::buildCM_QUESTION_RESPONSE(legion.questions.at("STR_GUILD_INVITE_DO_YOU_ACCEPT_INVITATION"), GameSession::ANSWER_YES));
		ASSERT_TRUE(until(10s, [&] { return !ofName(windowOf(b, from), "SM_LEGION_INFO").empty(); })) << "B did not join";
		settle();
		EXPECT_EQ(ofName(windowOf(b, from), "SM_LEGION_MEMBERLIST").size(), 1u) << "the list without B himself";
		EXPECT_EQ(ofName(windowOf(a, from), "SM_LEGION_ADD_MEMBER").size(), 1u);
		EXPECT_FALSE(ofName(windowOf(a, from), "SM_LEGION_HISTORY").empty()) << "the JOIN history";
		EXPECT_EQ(count("SELECT COUNT(*) FROM legion_members WHERE `rank` = 'VOLUNTEER' AND player_id = " + std::to_string(cb.playerId)), 1);

		from = marks();
		legionPacket(a, Body().C(0x01).D(0).S(cb.name));
		legionPacket(a, Body().C(0x01).D(0).S(cc.name));
		ASSERT_TRUE(until(10s, [&] { return countMessage(windowOf(a, from), msg("STR_GUILD_INVITE_HE_IS_OTHER_GUILD_MEMBER"), {cc.name}) == 1; }));
		settle();
		EXPECT_EQ(countMessage(windowOf(a, from), msg("STR_GUILD_INVITE_HE_IS_MY_GUILD_MEMBER"), {cb.name}), 1u);
		EXPECT_TRUE(ofName(windowOf(b, from), "SM_QUESTION_WINDOW").empty());
		EXPECT_TRUE(ofName(windowOf(c, from), "SM_QUESTION_WINDOW").empty());
	});

	// ---- C5: the announcement ----
	runCase("C5", "announcement: B (a volunteer) refused, A writes 300 characters, B reads the notice (0x07)", [&] {
		auto from = marks();
		legionPacket(b, Body().C(0x09).D(0).S("hi"));
		ASSERT_TRUE(until(10s, [&] { return countMessage(windowOf(b, from), msg("STR_GUILD_WRITE_NOTICE_DONT_HAVE_RIGHT")) == 1; }));
		from = marks();
		legionPacket(a, Body().C(0x09).D(0).S(std::string(300, 'x')));
		ASSERT_TRUE(until(10s, [&] { return countMessage(windowOf(a, from), msg("STR_GUILD_WRITE_NOTICE_DONE")) == 1; }));
		settle();
		EXPECT_EQ(ofName(windowOf(b, from), "SM_LEGION_EDIT").size(), 1u) << "the new notice to every member";
		EXPECT_EQ(count("SELECT CHAR_LENGTH(announcement) FROM legion_announcement_list a JOIN legions l ON l.id = a.legion_id WHERE l.name = '" +
		                legionName + "'"),
			legion.announcementLimit);
		from = marks();
		legionPacket(b, Body().C(0x07).D(0).H(0));
		ASSERT_TRUE(until(10s, [&] { return countMessage(windowOf(b, from), msg("STR_GUILD_NOTICE")) == 1; }));
		const std::vector<SystemMessage> notices = messagesOf(windowOf(b, from));
		for (const SystemMessage& notice : notices)
			if (notice.messageId == msg("STR_GUILD_NOTICE")) {
				ASSERT_FALSE(notice.params.empty());
				EXPECT_EQ(notice.params[0], std::string(static_cast<size_t>(legion.announcementLimit), 'x')) << "truncated to 256";
			}
	});

	// ---- C6-C7: ranks, permissions, self intro, nickname; the level-up refusal ----
	runCase("C6", "ranks and permissions: A makes B a centurion, changes the permissions; B refused there; B's intro, A names B", [&] {
		auto from = marks();
		legionPacket(a, Body().C(0x06).D(legion.ranks.at("CENTURION")).S(cb.name));
		ASSERT_TRUE(until(10s, [&] { return !ofName(windowOf(b, from), "SM_LEGION_UPDATE_MEMBER").empty(); }));
		settle();
		EXPECT_EQ(ofName(windowOf(a, from), "SM_LEGION_UPDATE_MEMBER").size(), 1u);
		from = marks();
		legionPacket(a, Body().C(0x0D).H(0x1E0C).H(0x1E08).H(0x1800).H(0x800));
		legionPacket(b, Body().C(0x0D).H(0).H(0).H(0).H(0));
		ASSERT_TRUE(until(10s, [&] { return countMessage(windowOf(b, from), msg("STR_GUILD_CHANGE_RIGHT_DONT_HAVE_RIGHT")) == 1; }));
		settle();
		EXPECT_EQ(ofName(windowOf(a, from), "SM_LEGION_EDIT").size(), 1u) << "one permission change, A's";
		EXPECT_EQ(ofName(windowOf(b, from), "SM_LEGION_EDIT").size(), 1u);
		from = marks();
		legionPacket(b, Body().C(0x0A).D(0).S("hello all"));
		legionPacket(a, Body().C(0x0F).S(cb.name).S("nick"));
		ASSERT_TRUE(until(10s, [&] { return !ofName(windowOf(b, from), "SM_LEGION_UPDATE_NICKNAME").empty(); }));
		settle();
		EXPECT_EQ(countMessage(windowOf(b, from), msg("STR_GUILD_WRITE_INTRO_DONE")), 1u);
		EXPECT_EQ(ofName(windowOf(a, from), "SM_LEGION_UPDATE_SELF_INTRO").size(), 1u);
		EXPECT_EQ(ofName(windowOf(a, from), "SM_LEGION_UPDATE_NICKNAME").size(), 1u);
	});

	runCase("C7", "level-up refused: A (no kinah left) CM_LEGION(0x0E)", [&] {
		const auto from = marks();
		legionPacket(a, Body().C(0x0E).D(0).H(0));
		ASSERT_TRUE(until(10s, [&] { return countMessage(windowOf(a, from), msg("STR_GUILD_CHANGE_LEVEL_NOT_ENOUGH_MONEY")) == 1; }));
		EXPECT_EQ(count("SELECT level FROM legions WHERE name = '" + legionName + "'"), 1);
	});

	// ---- C8: a member's relog ----
	runCase("C8", "B relogs: the logout stores B's member row, the login tells A and gives B the legion packets", [&] {
		auto from = marks();
		disconnect(b);
		settle();
		EXPECT_FALSE(ofName(windowOf(a, from), "SM_LEGION_UPDATE_MEMBER").empty()) << "onLogout's updateMemberInfo";
		const auto rows = database.queryRows(schema, "SELECT `rank`, nickname, selfintro FROM legion_members WHERE player_id = " + std::to_string(cb.playerId), 3);
		ASSERT_EQ(rows.size(), 1u);
		EXPECT_EQ(rows[0][0].value_or(""), "CENTURION");
		EXPECT_EQ(rows[0][1].value_or(""), "nick");
		EXPECT_EQ(rows[0][2].value_or(""), "hello all");
		from = marks();
		const std::vector<Packet> burst = enterAs(servers, b, cb);
		settle();
		EXPECT_FALSE(ofName(b.since(0), "SM_LEGION_INFO").empty() && ofName(burst, "SM_LEGION_INFO").empty()) << "C8: B's login sent no SM_LEGION_INFO";
		EXPECT_EQ(countMessage(windowOf(a, from), msg("STR_MSG_NOTIFY_LOGIN_GUILD"), {cb.name}), 1u);
	});

	// ---- C10: legion chat ----
	runCase("C10", "legion chat: B CM_CHAT_MESSAGE_PUBLIC(LEGION, text); A and B read it, C (another legion) does not", [&] {
		const std::string text = "legion business";
		const auto from = marks();
		b.game->send(CM_CHAT_MESSAGE_PUBLIC, Body().C(legion.chatTypes.at("LEGION")).S(text).data);
		const auto legionMessages = [&](const ScenarioClient& client) {
			std::vector<decoders::Message> found;
			for (const Packet& packet : ofName(windowOf(client, from), "SM_MESSAGE")) {
				const decoders::Message message = decoders::decodeMessage(packet.data);
				if (message.chatType == legion.chatTypes.at("LEGION"))
					found.push_back(message);
			}
			return found;
		};
		ASSERT_TRUE(until(10s, [&] { return !legionMessages(a).empty(); })) << "C10: A was not told B's legion message";
		settle();
		// CM_CHAT_MESSAGE_PUBLIC.broadcastToLegionMembers: PacketSendUtility.broadcastToLegion(legion, new SM_MESSAGE(player, message, type)) - every
		// online member, the sender included
		for (ScenarioClient* member : {&a, &b}) {
			const std::vector<decoders::Message> messages = legionMessages(*member);
			ASSERT_EQ(messages.size(), 1u) << member->label;
			EXPECT_EQ(messages[0].senderObjectId, cb.playerId) << member->label;
			EXPECT_EQ(messages[0].senderName, cb.name) << member->label;
			EXPECT_EQ(messages[0].message, text) << member->label;
		}
		EXPECT_TRUE(legionMessages(c).empty()) << "C10: C is in another legion";
	});

	// ---- C11: history ----
	runCase("C11", "history: A reads the LEGION page; B (a centurion) asks for the REWARD page and gets nothing", [&] {
		auto from = marks();
		a.game->send(CM_LEGION_HISTORY, Body().D(0).C(legion.historyTypes.at("LEGION")).data);
		ASSERT_TRUE(until(10s, [&] { return !ofName(windowOf(a, from), "SM_LEGION_HISTORY").empty(); }));
		from = marks();
		b.game->send(CM_LEGION_HISTORY, Body().D(0).C(legion.historyTypes.at("REWARD")).data);
		settle();
		EXPECT_TRUE(ofName(windowOf(b, from), "SM_LEGION_HISTORY").empty()) << "the reward history is the brigade general's";
	});

	// ---- C12: the seeded legion's emblem ----
	runCase("C12", "emblem: C picks emblem 5, uploads 9,000 bytes in two chunks; B asks for C's emblem with and without its data", [&] {
		auto from = marks();
		const int64_t kinahBefore = c.model.kinah();
		c.game->send(CM_LEGION_MODIFY_EMBLEM, Body().D(SEEDED_LEGION_ID).C(5).C(0).C(255).C(1).C(2).C(3).data);
		ASSERT_TRUE(until(10s, [&] { return countMessage(windowOf(c, from), msg("STR_GUILD_CHANGE_EMBLEM")) == 1; }));
		settle();
		EXPECT_EQ(ofName(windowOf(c, from), "SM_LEGION_UPDATE_EMBLEM").size(), 1u);
		const int64_t price = kinahBefore - c.model.kinah();
		EXPECT_GT(price, 0) << "PricesService.getPriceForService(LEGION_EMBLEM_REQUIRED_KINAH)";
		// storeLegionEmblem does not write the database (LegionService.java:469-478): the row comes with the upload below or the logout's store
		EXPECT_EQ(count("SELECT COUNT(*) FROM legion_emblems WHERE legion_id = " + std::to_string(SEEDED_LEGION_ID)), 0);

		from = marks();
		std::vector<uint8_t> image(static_cast<size_t>(EMBLEM_SIZE));
		for (size_t i = 0; i < image.size(); i++)
			image[i] = static_cast<uint8_t>(i * 7);
		c.game->send(CM_LEGION_UPLOAD_INFO, Body().D(EMBLEM_SIZE).C(255).C(10).C(20).C(30).data);
		const std::vector<uint8_t> first(image.begin(), image.begin() + EMBLEM_SIZE / 2), second(image.begin() + EMBLEM_SIZE / 2, image.end());
		c.game->send(CM_LEGION_UPLOAD_EMBLEM, Body().D(EMBLEM_SIZE / 2).B(first).data);
		c.game->send(CM_LEGION_UPLOAD_EMBLEM, Body().D(EMBLEM_SIZE / 2).B(second).data);
		ASSERT_TRUE(until(10s, [&] { return countMessage(windowOf(c, from), msg("STR_GUILD_WARN_SUCCESS_UPLOAD_EMBLEM")) == 1; }));
		settle();
		const int32_t chunks = (EMBLEM_SIZE + legion.emblemChunkSize - 1) / legion.emblemChunkSize;
		EXPECT_EQ(ofName(windowOf(c, from), "SM_LEGION_SEND_EMBLEM_DATA").size(), static_cast<size_t>(chunks)) << "updateMembersEmblem to C";
		EXPECT_EQ(kinahBefore - c.model.kinah(), 2 * price) << "the upload costs the same price";
		EXPECT_EQ(count("SELECT LENGTH(emblem_data) FROM legion_emblems WHERE legion_id = " + std::to_string(SEEDED_LEGION_ID)), EMBLEM_SIZE);
		EXPECT_EQ(count("SELECT COUNT(*) FROM legion_emblems WHERE emblem_type = 'CUSTOM' AND legion_id = " + std::to_string(SEEDED_LEGION_ID)), 1);
		EXPECT_EQ(count("SELECT emblem_id FROM legion_emblems WHERE legion_id = " + std::to_string(SEEDED_LEGION_ID)), 5) << "the upload keeps emblem 5";

		from = marks();
		b.game->send(CM_LEGION_SEND_EMBLEM, Body().D(SEEDED_LEGION_ID).data);
		ASSERT_TRUE(until(10s, [&] { return ofName(windowOf(b, from), "SM_LEGION_SEND_EMBLEM_DATA").size() == static_cast<size_t>(chunks); }));
		settle();
		EXPECT_EQ(ofName(windowOf(b, from), "SM_LEGION_SEND_EMBLEM").size(), 1u);
		from = marks();
		b.game->send(CM_LEGION_SEND_EMBLEM_INFO, Body().D(SEEDED_LEGION_ID).data);
		ASSERT_TRUE(until(10s, [&] { return !ofName(windowOf(b, from), "SM_LEGION_SEND_EMBLEM").empty(); }));
		settle();
		EXPECT_TRUE(ofName(windowOf(b, from), "SM_LEGION_SEND_EMBLEM_DATA").empty()) << "the info only";
	});

	// ======================================================================================================================================
	// The studio cases C13-C20 (m5h-plan.md §10.2, §10.3 Y12-Y19; HS-4): A's studio by quest 18802 (lane C's generated handler), C's by fee.
	// ======================================================================================================================================

	HousingAnswer housing;
	runCase("C0h", "the oracle answers: m5h-housing (the studio npcs, the Elyos studio, the furniture, PartType, the door states, the messages)", [&] {
		std::vector<std::string> arguments{"m5h-housing", "--npc", std::to_string(ORIEL) + ":" + std::to_string(PARRINE),
			std::to_string(ORIEL) + ":" + std::to_string(STUDIO_ENTRANCE), std::to_string(STUDIO_MAP) + ":" + std::to_string(STUDIO_EXIT), "--item",
			std::to_string(BED_ITEM), std::to_string(CAKE_ITEM), std::to_string(WALLPAPER_ITEM), "--message"};
		for (const char* name : {"STR_MSG_HOUSING_INS_OWN_SUCCESS", "STR_MSG_HOUSING_INS_CANT_OWN_MORE_HOUSE", "STR_MSG_HOUSING_OBJECT_USE",
		         "STR_MSG_HOUSING_OBJECT_REWARD_ITEM", "STR_MSG_HOUSING_CANNOT_USE_FLOWERPOT_COOLTIME", "STR_MSG_CANNOT_USE_ALREADY_HAVE_REWARD_ITEM",
		         "STR_MSG_HOUSING_OBJECT_CANCEL_USE", "STR_MSG_HOUSING_ORDER_OUT_ALL", "STR_MSG_HOUSING_ORDER_CLOSE_DOOR_ALL",
		         "STR_MSG_HOUSING_ORDER_OUT_WITHOUT_FRIENDS", "STR_MSG_INSTANCE_DUNGEON_OPENED_FOR_SELF", "STR_MSG_LEAVE_INSTANCE"})
			arguments.push_back(name);
		housing = parseHousing(oracle->run(arguments));
		EXPECT_EQ(housing.address, 2001) << "HouseData.getStudioAddress(ELYOS)";
		EXPECT_EQ(housing.studioMap, STUDIO_MAP);
		EXPECT_EQ(housing.exitMap, ORIEL);
		EXPECT_EQ(housing.npc(STUDIO_ENTRANCE).talkDelayMs, 2000);
		EXPECT_EQ(housing.item(CAKE_ITEM).kind, "use_item");
		EXPECT_EQ(housing.item(CAKE_ITEM).rewardId, 160010196);
		EXPECT_EQ(housing.item(WALLPAPER_ITEM).decoration, 3554000);
		EXPECT_EQ(housing.doorStates.at("CLOSED"), 3);
	});
	const auto hmsg = [&](const std::string& name) { return housing.message(name); };

	// ---- the studio helpers ----
	const auto seedPosition = [&](const Character& character, int32_t map, float x, float y, float z) {
		database.execute(schema, "UPDATE players SET world_id = " + std::to_string(map) + ", x = " + std::to_string(x) + ", y = " + std::to_string(y) +
		                           ", z = " + std::to_string(z) + " WHERE id = " + std::to_string(character.playerId));
	};
	/** the object id of the npc of `templateId` this client was shown nearest to (x, y) */
	const auto requireNpc = [&](ScenarioClient& client, int32_t templateId, float x, float y) {
		std::optional<int32_t> found;
		until(10s, [&] { return (found = npcObject(client, templateId, x, y)).has_value(); });
		if (!found)
			throw std::runtime_error(client.label + ": no SM_NPC_INFO of " + std::to_string(templateId) + " near " + std::to_string(x) + ", " +
			                         std::to_string(y));
		return *found;
	};
	const auto houseEdits = [&](const std::vector<Packet>& packets) {
		std::vector<decoders::HouseEdit> edits;
		for (const Packet& packet : ofName(packets, "SM_HOUSE_EDIT"))
			edits.push_back(decoders::decodeHouseEdit(packet.data));
		return edits;
	};
	const auto useObjects = [&](const std::vector<Packet>& packets) {
		std::vector<decoders::UseObject> uses;
		for (const Packet& packet : ofName(packets, "SM_USE_OBJECT"))
			uses.push_back(decoders::decodeUseObject(packet.data));
		return uses;
	};
	const auto emotionsOf = [&](const std::vector<Packet>& packets, int32_t sender, uint8_t type) {
		size_t n = 0;
		for (const Packet& packet : ofName(packets, "SM_EMOTION")) {
			try {
				const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
				n += emotion.senderObjectId == sender && emotion.emotionType == type;
			} catch (const DecodeError&) {
			}
		}
		return n;
	};
	const auto studioScripts = [&](const std::vector<Packet>& packets) {
		std::vector<decoders::HouseScripts> found;
		for (const Packet& packet : ofName(packets, "SM_HOUSE_SCRIPTS")) {
			const decoders::HouseScripts scripts = decoders::decodeHouseScripts(packet.data, housing.scriptPadding);
			if (scripts.address == housing.address)
				found.push_back(scripts);
		}
		return found;
	};
	const auto legionUpdatesOf = [&](const std::vector<Packet>& packets, int32_t memberId) {
		std::vector<decoders::LegionUpdateMember> found;
		for (const Packet& packet : ofName(packets, "SM_LEGION_UPDATE_MEMBER")) {
			const decoders::LegionUpdateMember update = decoders::decodeLegionUpdateMember(packet.data);
			if (update.objectId == memberId)
				found.push_back(update);
		}
		return found;
	};
	/**
	 * A talk to a studio portal (StudioPortalAI over ActionItemNpcAI, ActionItemNpcAI.java:36-80): CM_SHOW_DIALOG, the use bar with its two
	 * SM_USE_OBJECT and SM_EMOTION, then SM_TELEPORT_LOC (FADE_OUT_BEAM) and nothing more until CM_TELEPORT_ANIMATION_DONE; then the arrival's
	 * SM_CHANNEL_INFO + SM_PLAYER_SPAWN and CM_LEVEL_READY. @return {the teleport, the spawn, the mark before the animation done}
	 */
	struct PortalTrip {
		decoders::TeleportLoc loc;
		decoders::PlayerSpawn spawn;
		size_t from = 0, done = 0;
	};
	const auto takePortal = [&](ScenarioClient& client, int32_t portalObject, int32_t talkDelayMs, std::string_view label) {
		PortalTrip trip;
		trip.from = client.mark();
		client.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(portalObject));
		const Packet start = waitFor(*client.game, "SM_USE_OBJECT", 10s);
		const decoders::UseObject startBar = decoders::decodeUseObject(start.data);
		EXPECT_EQ(startBar.playerObjectId, client.playerId()) << label;
		EXPECT_EQ(startBar.targetObjectId, portalObject) << label;
		EXPECT_EQ(startBar.time, talkDelayMs) << label << ": the npc's talk delay in ms";
		EXPECT_EQ(startBar.actionType, decoders::USE_OBJECT_START_BAR) << label;
		const Packet end = waitFor(*client.game, "SM_USE_OBJECT", std::chrono::milliseconds(talkDelayMs) + 5s);
		const decoders::UseObject endBar = decoders::decodeUseObject(end.data);
		EXPECT_EQ(endBar.actionType, decoders::USE_OBJECT_CANCEL_BAR) << label << ": the task's own SM_USE_OBJECT(..., 2)";
		const double barMs = std::chrono::duration<double, std::milli>(end.receivedAt - start.receivedAt).count();
		EXPECT_NEAR(barMs, talkDelayMs, 500.0) << label;
		const Packet locPacket = waitFor(*client.game, "SM_TELEPORT_LOC", 10s);
		trip.loc = decoders::decodeTeleportLoc(locPacket.data);
		EXPECT_EQ(trip.loc.animation, decoders::TELEPORT_ANIMATION_FADE_OUT_BEAM) << label << ": TeleportAnimation.FADE_OUT_BEAM";
		const std::vector<Packet> waiting = collectFor(*client.game, 1s);
		EXPECT_TRUE(ofName(waiting, "SM_PLAYER_SPAWN").empty()) << label << ": the arrival waits for CM_TELEPORT_ANIMATION_DONE";
		const std::vector<Packet> bar = client.since(trip.from);
		EXPECT_EQ(emotionsOf(bar, client.playerId(), EMOTION_START_QUESTLOOT), 1u) << label << ": SM_EMOTION(START_QUESTLOOT)";
		EXPECT_EQ(emotionsOf(bar, client.playerId(), EMOTION_END_QUESTLOOT), 1u) << label << ": SM_EMOTION(END_QUESTLOOT)";
		trip.done = client.mark();
		client.game->send(GameSession::CM_TELEPORT_ANIMATION_DONE, GameSession::buildCM_TELEPORT_ANIMATION_DONE());
		trip.spawn = decoders::decodePlayerSpawn(waitFor(*client.game, "SM_PLAYER_SPAWN", 15s).data);
		client.x = trip.spawn.x;
		client.y = trip.spawn.y;
		client.z = trip.spawn.z;
		levelReady(client);
		return trip;
	};
	/** decoration mode's add + spawn of one inventory item (CM_HOUSE_EDIT 3 then 5). @return {the SM_HOUSE_EDIT(3), the SM_HOUSE_EDIT(5)} */
	const auto placeItem = [&](ScenarioClient& client, int32_t itemObjectId, float x, float y, float z, int32_t rotation, std::string_view label) {
		auto from = client.mark();
		client.game->send(CM_HOUSE_EDIT, Body().C(3).D(itemObjectId).data);
		if (!until(10s, [&] { return !houseEdits(client.since(from)).empty(); }))
			throw std::runtime_error(std::string(label) + ": no SM_HOUSE_EDIT(3)");
		const decoders::HouseEdit added = houseEdits(client.since(from))[0];
		EXPECT_EQ(added.action, 3) << label;
		from = client.mark();
		const auto bits = [](float value) { return static_cast<int32_t>(std::bit_cast<uint32_t>(value)); };
		client.game->send(CM_HOUSE_EDIT, Body().C(5).D(added.objectId).D(bits(x)).D(bits(y)).D(bits(z)).H(rotation).data);
		if (!until(10s, [&] { return houseEdits(client.since(from)).size() >= 2 && !ofName(client.since(from), "SM_HOUSE_OBJECT").empty(); }))
			throw std::runtime_error(std::string(label) + ": the spawn's SM_HOUSE_EDIT(5) + SM_HOUSE_OBJECT + SM_HOUSE_EDIT(4) did not arrive");
		const std::vector<decoders::HouseEdit> edits = houseEdits(client.since(from));
		EXPECT_EQ(edits[0].action, 5) << label;
		EXPECT_EQ(edits[1].action, 4) << label << ": CM_HOUSE_EDIT.java:59, SM_HOUSE_EDIT(4, 1, objId) after the spawn";
		EXPECT_EQ(edits[1].objectId, added.objectId) << label;
		return std::make_pair(added, edits[0]);
	};

	const auto itemCount = [&](ScenarioClient& client, int32_t itemId) {
		client.model.sync();
		int64_t n = 0;
		for (const ModelItem& item : client.model.byItemId(itemId))
			n += item.count;
		return n;
	};
	int32_t aBed = 0, aCake = 0, aWallpaper = 0, cCake = 0;
	const std::string studioAddress = std::to_string(2001);

	// ---- C13: the studio by quest ----
	const HousingAnswer::Spot& parrine = housing.npc(PARRINE);
	const std::array<float, 3> parrineSpot = pointNear(parrine.x, parrine.y, parrine.z, TALK_DISTANCE, housing.npc(STUDIO_ENTRANCE).x,
		housing.npc(STUDIO_ENTRANCE).y);
	const auto ownerInfos = [&](const std::vector<Packet>& packets) {
		std::vector<decoders::HouseOwnerInfo> infos;
		for (const Packet& packet : ofName(packets, "SM_HOUSE_OWNER_INFO"))
			infos.push_back(decoders::decodeHouseOwnerInfo(packet.data));
		return infos;
	};
	runCase("C13", "studio by quest: A, with 18802 at REWARD, selects the no-reward finish at Parrine (registerPlayerStudio, free)", [&] {
		disconnect(a);
		seedPosition(ca, ORIEL, parrineSpot[0], parrineSpot[1], parrineSpot[2]);
		database.execute(schema, "INSERT INTO player_quests (player_id, quest_id, status) VALUES (" + std::to_string(ca.playerId) + ", " +
		                           std::to_string(STUDIO_QUEST) + ", 'REWARD')");
		aBed = database.seedInventoryItem(schema, {ca.playerId, BED_ITEM, 1, 0, 65535});
		aCake = database.seedInventoryItem(schema, {ca.playerId, CAKE_ITEM, 1, 0, 65535});
		aWallpaper = database.seedInventoryItem(schema, {ca.playerId, WALLPAPER_ITEM, 1, 0, 65535});
		const std::vector<Packet> burst = enterAs(servers, a, ca);
		settle();
		// before: no house, 18802 not COMPLETE - SINGLE_HOUSE only (SM_HOUSE_OWNER_INFO.java:24-38, HousingService.canOwnHouse)
		const std::vector<decoders::HouseOwnerInfo> before = ownerInfos(burst);
		ASSERT_FALSE(before.empty()) << "the enter world's SM_HOUSE_OWNER_INFO";
		EXPECT_EQ(before.back().address, 0);
		EXPECT_EQ(before.back().buildingId, 0);
		EXPECT_EQ(before.back().ownerState, housing.houseOwnerStates.at("SINGLE_HOUSE"));
		const int64_t kinahBefore = a.model.kinah();
		const int32_t parrineObject = requireNpc(a, PARRINE, parrine.x, parrine.y);
		a.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(parrineObject));
		settle();
		const auto from = marks();
		a.game->send(GameSession::CM_DIALOG_SELECT, GameSession::buildCM_DIALOG_SELECT(parrineObject, SELECTED_QUEST_NOREWARD, 0, 0, STUDIO_QUEST));
		ASSERT_TRUE(until(10s, [&] { return countMessage(windowOf(a, from), hmsg("STR_MSG_HOUSING_INS_OWN_SUCCESS")) == 1; }))
		  << "C13: no STR_MSG_HOUSING_INS_OWN_SUCCESS (_18802AndAHomeforEveryDaeva: registerPlayerStudio)";
		settle();
		const std::vector<Packet> window = windowOf(a, from);
		// notifyAboutOwnerChange's order for a new owner (HousingService.java:137-145): SM_HOUSE_OWNER_INFO, then SM_HOUSE_ACQUIRE(true)
		const std::vector<decoders::HouseOwnerInfo> after = ownerInfos(window);
		ASSERT_FALSE(after.empty()) << "notifyAboutOwnerChange's SM_HOUSE_OWNER_INFO";
		EXPECT_EQ(after.front().address, housing.address);
		EXPECT_EQ(after.front().buildingId, housing.building);
		EXPECT_EQ(after.front().ownerState, housing.houseOwnerStates.at("HAS_OWNER") | housing.houseOwnerStates.at("BIDDING_ALLOWED"));
		const std::vector<Packet> acquires = ofName(window, "SM_HOUSE_ACQUIRE");
		ASSERT_EQ(acquires.size(), 1u);
		const decoders::HouseAcquire acquire = decoders::decodeHouseAcquire(acquires[0].data);
		EXPECT_EQ(acquire.playerId, ca.playerId);
		EXPECT_EQ(acquire.address, housing.address);
		EXPECT_EQ(acquire.acquire, 1);
		size_t ownerInfoAt = 0, acquireAt = 0;
		for (size_t i = 0; i < window.size(); i++) {
			if (window[i].name == "SM_HOUSE_OWNER_INFO" && ownerInfoAt == 0)
				ownerInfoAt = i + 1;
			if (window[i].name == "SM_HOUSE_ACQUIRE")
				acquireAt = i + 1;
		}
		EXPECT_LT(ownerInfoAt, acquireAt) << "SM_HOUSE_OWNER_INFO before SM_HOUSE_ACQUIRE";
		EXPECT_EQ(a.model.kinah(), kinahBefore) << "registerPlayerStudio is free (createStudio(player, false))";
		EXPECT_EQ(count("SELECT COUNT(*) FROM houses WHERE address = " + studioAddress + " AND building_id = " + std::to_string(housing.building) +
		                " AND player_id = " + std::to_string(ca.playerId)),
			1);
		// the quest's experience levels A up: B, A's legion mate, is told the new level (LegionService.updateMemberInfo)
		std::optional<int32_t> levelToB;
		until(5s, [&] {
			for (const decoders::LegionUpdateMember& update : legionUpdatesOf(windowOf(b, from), ca.playerId))
				if (update.level > 1)
					levelToB = update.level;
			return levelToB.has_value();
		});
		EXPECT_TRUE(levelToB) << "B: SM_LEGION_UPDATE_MEMBER(A) with A's new level";
	});

	// ---- C14: the studio by fee ----
	runCase("C14", "studio by fee: C buys the studio at Parrine (dialog 96) with exactly the price; A, who owns one, is refused before the charge", [&] {
		disconnect(c);
		seedPosition(cc, ORIEL, parrineSpot[0] + 1.0f, parrineSpot[1], parrineSpot[2]);
		seedKinah(cc.playerId, housing.goldPrice);
		cCake = database.seedInventoryItem(schema, {cc.playerId, CAKE_ITEM, 1, 0, 65535});
		enterAs(servers, c, cc);
		settle();
		ASSERT_EQ(c.model.kinah(), housing.goldPrice);
		const int32_t parrineObject = requireNpc(c, PARRINE, parrine.x, parrine.y);
		c.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(parrineObject));
		settle();
		auto from = marks();
		c.game->send(GameSession::CM_DIALOG_SELECT, GameSession::buildCM_DIALOG_SELECT(parrineObject, HOUSING_RECREATE_PERSONAL_INS));
		ASSERT_TRUE(until(10s, [&] { return countMessage(windowOf(c, from), hmsg("STR_MSG_HOUSING_INS_OWN_SUCCESS")) == 1; }))
		  << "C: no STR_MSG_HOUSING_INS_OWN_SUCCESS";
		settle();
		const std::vector<Packet> acquires = ofName(windowOf(c, from), "SM_HOUSE_ACQUIRE");
		ASSERT_EQ(acquires.size(), 1u) << "notifyAboutOwnerChange's SM_HOUSE_ACQUIRE (HousingService.java:137-145)";
		const decoders::HouseAcquire acquire = decoders::decodeHouseAcquire(acquires[0].data);
		EXPECT_EQ(acquire.playerId, cc.playerId);
		EXPECT_EQ(acquire.address, housing.address);
		EXPECT_EQ(acquire.acquire, 1);
		EXPECT_EQ(c.model.kinah(), 0) << "the land's gold price, exactly (HousingService.createStudio)";
		EXPECT_EQ(count("SELECT COUNT(*) FROM houses WHERE address = " + studioAddress + " AND player_id = " + std::to_string(cc.playerId)), 1);

		const int64_t aKinah = a.model.kinah();
		from = marks();
		a.game->send(GameSession::CM_DIALOG_SELECT,
			GameSession::buildCM_DIALOG_SELECT(requireNpc(a, PARRINE, parrine.x, parrine.y), HOUSING_RECREATE_PERSONAL_INS));
		ASSERT_TRUE(until(10s, [&] { return countMessage(windowOf(a, from), hmsg("STR_MSG_HOUSING_INS_CANT_OWN_MORE_HOUSE")) == 1; }));
		settle();
		EXPECT_TRUE(ofName(windowOf(a, from), "SM_HOUSE_ACQUIRE").empty()) << "the refusal before the charge (HousingService.java:281-284)";
		EXPECT_EQ(a.model.kinah(), aKinah) << "A's kinah unchanged";
		EXPECT_EQ(count("SELECT COUNT(*) FROM houses WHERE address = " + studioAddress), 2) << "A's and C's studio";
	});

	// ---- C15: enter ----
	std::map<int32_t, int32_t> studioInstance; // player id -> instance id
	runCase("C15", "enter: A and C walk to the studio entrance, take the 2-s bar and the beam into their own studios", [&] {
		const HousingAnswer::Spot& entrance = housing.npc(STUDIO_ENTRANCE);
		const std::array<float, 3> crystal = housing.houseNpcs.at("TELEPORT");
		const int8_t heading = headingTowards(housing.x, housing.y, crystal[0], crystal[1]);
		for (ScenarioClient* owner : {&a, &c}) {
			const std::string label = "C15 " + owner->label;
			const std::array<float, 3> at = pointNear(entrance.x, entrance.y, entrance.z, TALK_DISTANCE, owner->x, owner->y);
			walkTo(*owner, at[0], at[1], at[2]);
			const int32_t portal = requireNpc(*owner, STUDIO_ENTRANCE, entrance.x, entrance.y);
			const auto from = marks();
			const PortalTrip trip = takePortal(*owner, portal, entrance.talkDelayMs, label);
			EXPECT_EQ(trip.loc.mapId, STUDIO_MAP) << label;
			EXPECT_GE(trip.loc.mapOrInstanceId, 2) << label << ": a personal instance, not the main one";
			EXPECT_FLOAT_EQ(trip.loc.x, housing.x) << label << ": the address point (StudioPortalAI.java:52-55)";
			EXPECT_FLOAT_EQ(trip.loc.y, housing.y) << label;
			EXPECT_FLOAT_EQ(trip.loc.z, housing.z) << label;
			EXPECT_EQ(static_cast<int8_t>(trip.loc.heading), heading) << label << ": House.getTeleportHeading, towards the relationship crystal";
			EXPECT_EQ(trip.spawn.worldId, STUDIO_MAP) << label;
			EXPECT_TRUE(trip.spawn.personal) << label;
			studioInstance[owner->playerId()] = trip.loc.mapOrInstanceId;
			EXPECT_FALSE(servers.gameServer()->findLogLines("Created new instance: " + std::to_string(STUDIO_MAP) + " [" +
			                                                 std::to_string(trip.loc.mapOrInstanceId) + "] owner:" + std::to_string(owner->playerId()), 5).empty())
			  << label << ": InstanceService.getOrCreatePersonalInstance's log line";
			// the studio, its two npcs and the butler's scripts (ButlerAI.handleCreatureSee -> House.sendScripts)
			ASSERT_TRUE(until(15s, [&] { return !studioScripts(owner->since(trip.done)).empty(); })) << label << ": the butler's SM_HOUSE_SCRIPTS(2001)";
			settle();
			const std::vector<Packet> arrival = owner->since(trip.done);
			EXPECT_EQ(countMessage(arrival, hmsg("STR_MSG_INSTANCE_DUNGEON_OPENED_FOR_SELF")), 0u) << label << ": a personal map (TeleportService.java:531-532)";
			size_t renders = 0;
			for (const Packet& packet : ofName(arrival, "SM_HOUSE_RENDER")) {
				const decoders::HouseInfo info = decoders::decodeHouseRender(packet.data);
				if (info.address != housing.address)
					continue;
				renders++;
				EXPECT_EQ(info.ownerId, owner->playerId()) << label;
				EXPECT_EQ(info.buildingId, housing.building) << label;
			}
			EXPECT_EQ(renders, 1u) << label << ": exactly one SM_HOUSE_RENDER of the studio";
			const std::array<float, 3> manager = housing.houseNpcs.at("MANAGER");
			EXPECT_TRUE(npcObject(*owner, housing.managerNpc, manager[0], manager[1])) << label << ": the butler at house_npcs.xml's MANAGER spot";
			EXPECT_TRUE(npcObject(*owner, housing.teleportNpc, crystal[0], crystal[1])) << label << ": the crystal at the TELEPORT spot";
			const std::vector<decoders::HouseScripts> scripts = studioScripts(arrival);
			ASSERT_FALSE(scripts.empty());
			EXPECT_EQ(scripts[0].scripts.size(), 8u) << label << ": PlayerScripts.SCRIPT_LIMIT slots";
			for (const decoders::HouseScript& script : scripts[0].scripts)
				EXPECT_FALSE(script.hasData) << label << ": a new studio has no scripts";
			owner->model.sync();
		}
		EXPECT_NE(studioInstance[ca.playerId], studioInstance[cc.playerId]) << "one personal instance per owner";
		// the map change's LegionService.updateMemberInfo: A's legion mate B is told A's new map; C's legion is C alone
		std::vector<decoders::LegionUpdateMember> toB;
		until(5s, [&] {
			toB.clear();
			for (const decoders::LegionUpdateMember& update : legionUpdatesOf(b.since(0), ca.playerId))
				if (update.worldId == STUDIO_MAP)
					toB.push_back(update);
			return !toB.empty();
		});
		EXPECT_FALSE(toB.empty()) << "B: SM_LEGION_UPDATE_MEMBER(A) with map 720010000";
	});

	// ---- C16: decorate ----
	int32_t bedObject = 0, cakeObject = 0, decorObject = 0;
	std::array<float, 4> bedMoved{363.0f, 297.5f, 0, 90};
	runCase("C16", "decorate: A enters the decoration mode, places the bed and the cake, puts up the wallpaper and moves the bed", [&] {
		auto from = a.mark();
		a.game->send(CM_HOUSE_EDIT, Body().C(1).data);
		ASSERT_TRUE(until(10s, [&] { return ofName(a.since(from), "SM_HOUSE_REGISTRY").size() == 2; })) << "SM_HOUSE_REGISTRY(1) and (2)";
		ASSERT_EQ(houseEdits(a.since(from)).size(), 1u);
		EXPECT_EQ(houseEdits(a.since(from))[0].action, 1);
		const float z = a.z;
		bedMoved[2] = z;
		// the bed: a chair template (type 5), no use_days
		const auto [bedAdded, bedSpawned] = placeItem(a, aBed, 364.0f, 293.0f, z, 0, "C16 bed");
		bedObject = bedAdded.objectId;
		EXPECT_EQ(bedAdded.storeId, 1);
		EXPECT_EQ(bedAdded.templateId, *housing.item(BED_ITEM).houseObject);
		EXPECT_EQ(bedAdded.secondsUntilExpiration, 0) << "no use_days";
		EXPECT_FALSE(bedAdded.usage);
		EXPECT_EQ(bedSpawned.x, 364.0f) << "the floats as sent";
		EXPECT_EQ(bedSpawned.y, 293.0f);
		EXPECT_EQ(bedSpawned.address, housing.address);
		EXPECT_EQ(bedSpawned.playerId, ca.playerId);
		// the cake: a use_item (type 1, the usage block), 30 use_days; at A's own position, inside its 3.25-m use range
		const auto [cakeAdded, cakeSpawned] = placeItem(a, aCake, a.x + 0.5f, a.y, z, 0, "C16 cake");
		cakeObject = cakeAdded.objectId;
		EXPECT_EQ(cakeAdded.templateId, *housing.item(CAKE_ITEM).houseObject);
		EXPECT_EQ(cakeAdded.typeId, decoders::HOUSE_OBJECT_TYPE_USE_ITEM);
		EXPECT_EQ(cakeAdded.userId, ca.playerId) << "SM_HOUSE_EDIT.java:74-78";
		EXPECT_TRUE(cakeAdded.usage);
		EXPECT_NEAR(cakeAdded.secondsUntilExpiration, housing.item(CAKE_ITEM).useDays * 86400, 120) << "HouseObjectFactory: now + use_days";
		EXPECT_TRUE(cakeSpawned.usage) << "SM_HOUSE_EDIT.java:99-102";
		bool cakeShown = false;
		for (const Packet& packet : ofName(a.since(from), "SM_HOUSE_OBJECT")) {
			const decoders::HouseObjectInfo info = decoders::decodeHouseObject(packet.data);
			if (info.objectId == cakeObject) {
				cakeShown = true;
				EXPECT_EQ(info.typeId, decoders::HOUSE_OBJECT_TYPE_USE_ITEM);
				EXPECT_TRUE(info.usage) << "SM_HOUSE_OBJECT.java:50-56";
				EXPECT_EQ(info.ownerId, ca.playerId);
			}
		}
		EXPECT_TRUE(cakeShown) << "obj.spawn: the cake's SM_HOUSE_OBJECT";
		// the wallpaper: a decoration (store 2), applied to the first inner-wall room
		from = a.mark();
		a.game->send(CM_HOUSE_EDIT, Body().C(3).D(aWallpaper).data);
		ASSERT_TRUE(until(10s, [&] { return !houseEdits(a.since(from)).empty(); }));
		const decoders::HouseEdit decor = houseEdits(a.since(from))[0];
		decorObject = decor.objectId;
		EXPECT_EQ(decor.storeId, 2);
		EXPECT_EQ(decor.templateId, *housing.item(WALLPAPER_ITEM).decoration) << "DecorateAction.getTemplateId";
		from = a.mark();
		const auto [inwallSlot, inwallLine] = housing.partSlots.at("INWALL_ANY");
		a.game->send(CM_HOUSE_DECORATE, Body().D(decorObject).D(*housing.item(WALLPAPER_ITEM).decoration).H(inwallLine).data);
		ASSERT_TRUE(until(10s, [&] { return !ofName(a.since(from), "SM_HOUSE_UPDATE").empty(); })) << "updateAppearance's SM_HOUSE_UPDATE";
		settle();
		const std::vector<decoders::HouseEdit> applied = houseEdits(a.since(from));
		EXPECT_EQ(applied.size(), 2u) << "SM_HOUSE_EDIT(4, 2, decor) twice (CM_HOUSE_DECORATE.java: 'yes, in retail it's sent twice')";
		const decoders::HouseInfo updated = decoders::decodeHouseUpdate(ofName(a.since(from), "SM_HOUSE_UPDATE")[0].data);
		EXPECT_EQ(updated.decorIds.at(inwallSlot), *housing.item(WALLPAPER_ITEM).decoration) << "the first inner-wall room";
		// the move: despawn notice, SM_DELETE_HOUSE_OBJECT, the new place, the new SM_HOUSE_OBJECT
		from = a.mark();
		const auto bits = [](float value) { return static_cast<int32_t>(std::bit_cast<uint32_t>(value)); };
		a.game->send(CM_HOUSE_EDIT, Body().C(6).D(bedObject).D(bits(bedMoved[0])).D(bits(bedMoved[1])).D(bits(bedMoved[2])).H(90).data);
		ASSERT_TRUE(until(10s, [&] { return !ofName(a.since(from), "SM_HOUSE_OBJECT").empty(); }));
		settle();
		const std::vector<decoders::HouseEdit> moved = houseEdits(a.since(from));
		ASSERT_EQ(moved.size(), 2u);
		EXPECT_EQ(moved[0].action, 7);
		EXPECT_EQ(moved[1].action, 5);
		EXPECT_EQ(moved[1].x, bedMoved[0]);
		EXPECT_EQ(moved[1].y, bedMoved[1]);
		EXPECT_EQ(moved[1].rotation, 90);
		ASSERT_EQ(ofName(a.since(from), "SM_DELETE_HOUSE_OBJECT").size(), 1u);
		EXPECT_EQ(decoders::decodeDeleteHouseObject(ofName(a.since(from), "SM_DELETE_HOUSE_OBJECT")[0].data), bedObject);
		from = a.mark();
		a.game->send(CM_HOUSE_EDIT, Body().C(2).data);
		ASSERT_TRUE(until(10s, [&] { return !houseEdits(a.since(from)).empty(); }));
		EXPECT_EQ(houseEdits(a.since(from))[0].action, 2);
	});

	// ---- C17: use ----
	runCase("C17", "use: A uses the cake (reward), again at once (cooldown), after 10 s (COOKING); C cancels its cake's use after 1 s", [&] {
		const int32_t reward = *housing.item(CAKE_ITEM).rewardId;
		auto from = a.mark();
		a.game->send(CM_USE_HOUSE_OBJECT, Body().D(cakeObject).data);
		ASSERT_TRUE(until(10s, [&] { return countMessage(a.since(from), hmsg("STR_MSG_HOUSING_OBJECT_USE")) == 1; }));
		ASSERT_TRUE(until(10s, [&] { return countMessage(a.since(from), hmsg("STR_MSG_HOUSING_OBJECT_REWARD_ITEM")) == 1; }));
		settle();
		std::vector<decoders::UseObject> uses = useObjects(a.since(from));
		ASSERT_EQ(uses.size(), 2u);
		EXPECT_EQ(uses[0].targetObjectId, cakeObject);
		EXPECT_EQ(uses[0].time, housing.item(CAKE_ITEM).delayMs);
		EXPECT_EQ(uses[0].actionType, USE_OBJECT_HOUSE_USE);
		EXPECT_EQ(uses[1].time, 0);
		EXPECT_EQ(uses[1].actionType, USE_OBJECT_HOUSE_END);
		const std::vector<Packet> useWindow = a.since(from);
		const auto useStart = ofName(useWindow, "SM_USE_OBJECT")[0].receivedAt, useEnd = ofName(useWindow, "SM_USE_OBJECT")[1].receivedAt;
		EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(useEnd - useStart).count(), housing.item(CAKE_ITEM).delayMs - 100)
		  << "the HOUSE_OBJECT_USE task after the template's delay";
		EXPECT_EQ(itemCount(a, reward), 1) << "ItemService.addItem(reward)";
		ASSERT_EQ(ofName(useWindow, "SM_OBJECT_USE_UPDATE").size(), 1u);
		const decoders::ObjectUseUpdate update = decoders::decodeObjectUseUpdate(ofName(useWindow, "SM_OBJECT_USE_UPDATE")[0].data);
		EXPECT_EQ(update.userId, ca.playerId);
		EXPECT_EQ(update.ownerId, ca.playerId);
		EXPECT_EQ(update.objectId, cakeObject);
		EXPECT_EQ(update.useCount, 1);
		from = a.mark();
		a.game->send(CM_USE_HOUSE_OBJECT, Body().D(cakeObject).data);
		ASSERT_TRUE(until(10s, [&] { return countMessage(a.since(from), hmsg("STR_MSG_HOUSING_CANNOT_USE_FLOWERPOT_COOLTIME")) == 1; }))
		  << "the 10-s cooldown (cd)";
		drainAll(std::chrono::milliseconds(housing.item(CAKE_ITEM).cooldownSeconds * 1000 + 500));
		from = a.mark();
		a.game->send(CM_USE_HOUSE_OBJECT, Body().D(cakeObject).data);
		ASSERT_TRUE(until(10s, [&] { return countMessage(a.since(from), hmsg("STR_MSG_CANNOT_USE_ALREADY_HAVE_REWARD_ITEM")) == 1; }))
		  << "the COOKING limit (placementLimitOf)";
		settle();
		EXPECT_EQ(itemCount(a, reward), 1) << "no second reward";

		// C places its cake at its own position and cancels the use after 1 s
		from = c.mark();
		c.game->send(CM_HOUSE_EDIT, Body().C(1).data);
		ASSERT_TRUE(until(10s, [&] { return ofName(c.since(from), "SM_HOUSE_REGISTRY").size() == 2; }));
		const auto [cakeAdded, cakeSpawned] = placeItem(c, cCake, c.x + 0.5f, c.y, c.z, 0, "C17 C's cake");
		static_cast<void>(cakeSpawned);
		c.game->send(CM_HOUSE_EDIT, Body().C(2).data);
		settle();
		from = c.mark();
		c.game->send(CM_USE_HOUSE_OBJECT, Body().D(cakeAdded.objectId).data);
		ASSERT_TRUE(until(5s, [&] { return countMessage(c.since(from), hmsg("STR_MSG_HOUSING_OBJECT_USE")) == 1; }));
		collectFor(*c.game, 1s);
		c.game->send(CM_RELEASE_OBJECT, Body().D(cakeAdded.objectId).data);
		ASSERT_TRUE(until(5s, [&] { return countMessage(c.since(from), hmsg("STR_MSG_HOUSING_OBJECT_CANCEL_USE")) == 1; }));
		drainAll(3500ms);
		catchUp();
		uses = useObjects(c.since(from));
		ASSERT_EQ(uses.size(), 2u) << "the bar and its cancel";
		EXPECT_EQ(uses[1].playerObjectId, cc.playerId);
		EXPECT_EQ(uses[1].time, 0);
		EXPECT_EQ(uses[1].actionType, USE_OBJECT_HOUSE_END);
		EXPECT_EQ(countMessage(c.since(from), hmsg("STR_MSG_HOUSING_OBJECT_REWARD_ITEM")), 0u) << "the cancelled task gives nothing";
		EXPECT_EQ(itemCount(c, reward), 0);
	});

	// ---- C18: configure ----
	const std::string notice = "keep out";
	const std::string script1 = "<scripts><script id=\"0\">hello</script></scripts>";
	const auto [script1Bytes, script1Size] = houseScript(script1);
	runCase("C18", "configure: A closes the door with a notice, stores script 0, sends a foreign address's script, kicks the visitors", [&] {
		auto from = a.mark();
		a.game->send(CM_HOUSE_SETTINGS, Body().C(housing.doorStates.at("CLOSED")).C(0).S(notice).data);
		ASSERT_TRUE(until(10s, [&] { return countMessage(a.since(from), hmsg("STR_MSG_HOUSING_ORDER_CLOSE_DOOR_ALL")) == 1; }));
		settle();
		// Java's order (CM_HOUSE_SETTINGS.java:57-67; HouseController.kickVisitors): acquire, update, OUT_ALL, CLOSE_DOOR_ALL
		std::vector<std::string> order;
		for (const Packet& packet : a.since(from)) {
			if (packet.name == "SM_HOUSE_ACQUIRE" || packet.name == "SM_HOUSE_UPDATE")
				order.push_back(packet.name);
			else if (packet.name == "SM_SYSTEM_MESSAGE") {
				const int32_t id = decodeSystemMessage(packet.data).messageId;
				if (id == hmsg("STR_MSG_HOUSING_ORDER_OUT_ALL"))
					order.push_back("OUT_ALL");
				else if (id == hmsg("STR_MSG_HOUSING_ORDER_CLOSE_DOOR_ALL"))
					order.push_back("CLOSE_DOOR_ALL");
			}
		}
		EXPECT_EQ(join(order), "SM_HOUSE_ACQUIRE, SM_HOUSE_UPDATE, OUT_ALL, CLOSE_DOOR_ALL");
		const std::vector<Packet> updates = ofName(a.since(from), "SM_HOUSE_UPDATE");
		ASSERT_FALSE(updates.empty());
		const decoders::HouseInfo info = decoders::decodeHouseUpdate(updates[0].data);
		EXPECT_EQ(info.doorState, housing.doorStates.at("CLOSED"));
		EXPECT_EQ(info.signNotice, notice);
		EXPECT_EQ(info.showOwnerName, 0);

		from = a.mark();
		a.game->send(CM_HOUSE_SCRIPT, Body()
		                                  .D(housing.address)
		                                  .C(0)
		                                  .H(8 + static_cast<int32_t>(script1Bytes.size()))
		                                  .D(static_cast<int32_t>(script1Bytes.size()))
		                                  .D(script1Size)
		                                  .B(script1Bytes)
		                                  .data);
		const auto [script2Bytes, script2Size] = houseScript("<scripts><script id=\"1\">foreign</script></scripts>");
		a.game->send(CM_HOUSE_SCRIPT, Body()
		                                  .D(3001)
		                                  .C(1)
		                                  .H(8 + static_cast<int32_t>(script2Bytes.size()))
		                                  .D(static_cast<int32_t>(script2Bytes.size()))
		                                  .D(script2Size)
		                                  .B(script2Bytes)
		                                  .data);
		settle();
		EXPECT_TRUE(studioScripts(a.since(from)).empty()) << "the broadcast goes to the players who know A, not to A (CM_HOUSE_SCRIPT.java:62)";
		const std::string houseOfA = "(SELECT id FROM houses WHERE address = " + studioAddress + " AND player_id = " + std::to_string(ca.playerId) + ")";
		const auto rows = database.queryRows(schema, "SELECT script_id, script FROM house_scripts WHERE house_id = " + houseOfA, 2);
		ASSERT_EQ(rows.size(), 1u) << "PlayerScripts.set stores at once (PlayerScripts.java:50-55); the foreign address is refused";
		EXPECT_EQ(rows[0][0].value_or(""), "0");
		EXPECT_EQ(rows[0][1].value_or(""), script1);
		EXPECT_EQ(count("SELECT COUNT(*) FROM house_scripts WHERE script_id = 1"), 0) << "the audit guard (CM_HOUSE_SCRIPT.java)";

		from = a.mark();
		a.game->send(CM_HOUSE_KICK, Body().C(1).H(0).data);
		ASSERT_TRUE(until(10s, [&] { return countMessage(a.since(from), hmsg("STR_MSG_HOUSING_ORDER_OUT_WITHOUT_FRIENDS")) == 1; }));
	});

	// ---- C19: leave, destroy, re-enter, persist, re-login ----
	runCase("C19", "leave, destroy, re-enter, persist, re-login: A leaves its studio, the instance is destroyed and saved, A returns and quits inside", [&] {
		const HousingAnswer::Spot& exit = housing.npc(STUDIO_EXIT);
		const HousingAnswer::Spot& entrance = housing.npc(STUDIO_ENTRANCE);
		auto from = a.mark();
		const std::array<float, 3> exitSpot = pointNear(exit.x, exit.y, exit.z, TALK_DISTANCE, a.x, a.y);
		walkTo(a, exitSpot[0], exitSpot[1], a.z);
		EXPECT_TRUE(useObjects(a.since(from)).empty()) << "the entrance's use-bar observer was removed when its task ran (ActionItemNpcAI.java:68)";
		EXPECT_EQ(emotionsOf(a.since(from), ca.playerId, EMOTION_END_QUESTLOOT), 0u);
		const int32_t exitObject = requireNpc(a, STUDIO_EXIT, exit.x, exit.y);
		const auto legionFrom = marks();
		const PortalTrip out = takePortal(a, exitObject, exit.talkDelayMs, "C19 exit");
		EXPECT_EQ(out.loc.mapId, housing.exitMap);
		EXPECT_FLOAT_EQ(out.loc.x, housing.exitX);
		EXPECT_FLOAT_EQ(out.loc.y, housing.exitY);
		EXPECT_FLOAT_EQ(out.loc.z, housing.exitZ);
		EXPECT_EQ(out.spawn.worldId, housing.exitMap);
		EXPECT_EQ(countMessage(a.since(out.from), hmsg("STR_MSG_LEAVE_INSTANCE")), 0u) << "no registration (InstanceService.java:213)";
		bool told = false;
		until(5s, [&] {
			for (const decoders::LegionUpdateMember& update : legionUpdatesOf(windowOf(b, legionFrom), ca.playerId))
				told |= update.worldId == housing.exitMap;
			return told;
		});
		EXPECT_TRUE(told) << "B: SM_LEGION_UPDATE_MEMBER(A) with map 700010000";

		// the destroy: EmptyInstanceCheckerTask runs every 60 s
		const std::string destroying = "Destroying WorldMapInstance " + std::to_string(STUDIO_MAP) + " [" + std::to_string(studioInstance[ca.playerId]) + "]";
		ASSERT_TRUE(until(75s, [&] { return !servers.gameServer()->findLogLines(destroying, 1).empty(); })) << "C19: no '" << destroying << "' within 75 s";
		std::vector<std::vector<std::optional<std::string>>> registered;
		until(5s, [&] {
			registered = database.queryRows(schema,
				"SELECT item_unique_id, x, y, area FROM player_registered_items WHERE player_id = " + std::to_string(ca.playerId) +
					" ORDER BY item_unique_id",
				4);
			return registered.size() == 3;
		});
		ASSERT_EQ(registered.size(), 3u) << "HouseController.onDespawn saved the registry: the bed, the cake and the wallpaper";
		for (const auto& row : registered) {
			const int32_t id = std::stoi(row[0].value_or("0"));
			if (id == bedObject) {
				EXPECT_NEAR(std::stod(row[1].value_or("0")), bedMoved[0], 0.01) << "the bed where it was moved";
				EXPECT_NEAR(std::stod(row[2].value_or("0")), bedMoved[1], 0.01);
			} else if (id == decorObject) {
				EXPECT_EQ(row[3].value_or(""), "DECOR");
			} else {
				EXPECT_EQ(id, cakeObject);
			}
		}

		// re-entry: a new instance, the bed at its new place, the cake, the wallpaper and script 0 (the butler)
		const std::array<float, 3> at = pointNear(entrance.x, entrance.y, entrance.z, TALK_DISTANCE, a.x, a.y);
		walkTo(a, at[0], at[1], at[2]);
		const PortalTrip back = takePortal(a, requireNpc(a, STUDIO_ENTRANCE, entrance.x, entrance.y), entrance.talkDelayMs, "C19 re-entry");
		EXPECT_EQ(back.loc.mapId, STUDIO_MAP);
		EXPECT_NE(back.loc.mapOrInstanceId, studioInstance[ca.playerId]) << "the destroyed instance is gone";
		EXPECT_GE(back.loc.mapOrInstanceId, 2);
		ASSERT_TRUE(until(15s, [&] { return !studioScripts(a.since(back.done)).empty(); }));
		settle();
		const std::vector<Packet> arrival = a.since(back.done);
		bool bedSeen = false, cakeSeen = false;
		for (const Packet& packet : ofName(arrival, "SM_HOUSE_OBJECT")) {
			const decoders::HouseObjectInfo info = decoders::decodeHouseObject(packet.data);
			if (info.objectId == bedObject) {
				bedSeen = true;
				EXPECT_EQ(info.x, bedMoved[0]);
				EXPECT_EQ(info.y, bedMoved[1]);
			}
			cakeSeen |= info.objectId == cakeObject;
		}
		EXPECT_TRUE(bedSeen) << "the bed re-spawned from the in-memory registry";
		EXPECT_TRUE(cakeSeen);
		bool wallpaper = false;
		for (const Packet& packet : ofName(arrival, "SM_HOUSE_RENDER")) {
			const decoders::HouseInfo info = decoders::decodeHouseRender(packet.data);
			if (info.address == housing.address)
				wallpaper |= info.decorIds.at(housing.partSlots.at("INWALL_ANY").first) == *housing.item(WALLPAPER_ITEM).decoration;
		}
		EXPECT_TRUE(wallpaper) << "the wallpaper in SM_HOUSE_RENDER";
		const decoders::HouseScripts scripts = studioScripts(arrival)[0];
		ASSERT_EQ(scripts.scripts.size(), 8u);
		EXPECT_TRUE(scripts.scripts[0].hasData) << "script 0 reaches its owner through the butler";
		EXPECT_EQ(scripts.scripts[0].compressed, script1Bytes) << "the compressed bytes as A sent them";
		EXPECT_EQ(scripts.scripts[0].uncompressedSize, script1Size);
		EXPECT_EQ(scripts.scripts[0].padding.size(), housing.scriptPadding);
		EXPECT_FALSE(scripts.scripts[1].hasData);

		// A quits inside the studio: the logout's store
		disconnect(a);
		settle();
		const std::string houseOfA = "address = " + studioAddress + " AND player_id = " + std::to_string(ca.playerId);
		EXPECT_EQ(database.queryRows(schema, "SELECT sign_notice FROM houses WHERE " + houseOfA, 1).at(0).at(0).value_or(""), notice);
		EXPECT_EQ(count("SELECT COUNT(*) FROM house_scripts WHERE house_id = (SELECT id FROM houses WHERE " + houseOfA + ")"), 1);
		EXPECT_EQ(count("SELECT COUNT(*) FROM house_object_cooldowns WHERE player_id = " + std::to_string(ca.playerId)), 0)
		  << "the cake's 10-s cooldown ran out long before the quit: HouseObjectCooldownsDAO.storeHouseObjectCooldowns skips an expired reuse time";
		for (int32_t item : {aBed, aCake, aWallpaper})
			EXPECT_EQ(count("SELECT COUNT(*) FROM inventory WHERE item_unique_id = " + std::to_string(item)), 0)
			  << "C16's ItemDeleteType.REGISTER deletes, stored with the inventory at the logout";
		EXPECT_EQ(count("SELECT world_id FROM players WHERE id = " + std::to_string(ca.playerId)), STUDIO_MAP);
		EXPECT_EQ(count("SELECT COUNT(*) FROM player_quests WHERE status = 'COMPLETE' AND quest_id = " + std::to_string(STUDIO_QUEST) +
		                " AND player_id = " + std::to_string(ca.playerId)),
			1)
		  << "C13's sendQuestEndDialog finished 18802, stored with the quest list at the logout";

		// re-login (A-15): A lands in its studio
		const std::vector<Packet> burst = enterAs(servers, a, ca);
		const Packet* spawn = firstOfName(burst, "SM_PLAYER_SPAWN");
		ASSERT_NE(spawn, nullptr);
		const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(spawn->data);
		EXPECT_EQ(spawned.worldId, STUDIO_MAP) << "PlayerEnterWorldService: the personal arm of HousingService.onPlayerLogin";
		EXPECT_NE(instanceOf(spawned), 1) << "not the main instance";
		ASSERT_TRUE(until(15s, [&] { return !ofName(a.since(0), "SM_HOUSE_OBJECT").empty(); })) << "the studio re-rendered after the login";
		settle();
	});

	// ---- C20: the member list sees the studio ----
	runCase("C20", "member list: B relogs; its SM_LEGION_MEMBERLIST entry for A carries the studio address and the closed door", [&] {
		disconnect(b);
		enterAs(servers, b, cb);
		settle();
		std::optional<decoders::LegionMemberEntry> entry;
		for (const Packet& packet : ofName(b.since(0), "SM_LEGION_MEMBERLIST"))
			for (const decoders::LegionMemberEntry& member : decoders::decodeLegionMemberList(packet.data).members)
				if (member.objectId == ca.playerId)
					entry = member;
		ASSERT_TRUE(entry) << "B's member list names A";
		EXPECT_EQ(entry->houseAddress, housing.address) << "SM_LEGION_MEMBERLIST.java:46-48, HousingService.findActiveHouse";
		EXPECT_EQ(entry->houseDoorState, housing.doorStates.at("CLOSED"));
		EXPECT_EQ(entry->worldId, STUDIO_MAP);
	});

	// ---- C21: leave and kick ----
	runCase("C21", "leave and kick: B leaves; A invites B again, B accepts; A kicks B by name", [&] {
		auto from = marks();
		legionPacket(b, Body().C(0x02).D(0).H(0));
		ASSERT_TRUE(until(10s, [&] { return !ofName(windowOf(b, from), "SM_LEGION_LEAVE_MEMBER").empty(); }));
		settle();
		EXPECT_EQ(ofName(windowOf(a, from), "SM_LEGION_LEAVE_MEMBER").size(), 1u);
		EXPECT_EQ(count("SELECT COUNT(*) FROM legion_members WHERE player_id = " + std::to_string(cb.playerId)), 0);
		from = marks();
		legionPacket(a, Body().C(0x01).D(0).S(cb.name));
		ASSERT_TRUE(until(10s, [&] { return !ofName(windowOf(b, from), "SM_QUESTION_WINDOW").empty(); }));
		b.game->send(GameSession::CM_QUESTION_RESPONSE,
			GameSession::buildCM_QUESTION_RESPONSE(legion.questions.at("STR_GUILD_INVITE_DO_YOU_ACCEPT_INVITATION"), GameSession::ANSWER_YES));
		ASSERT_TRUE(until(10s, [&] { return !ofName(windowOf(b, from), "SM_LEGION_INFO").empty(); }));
		settle();
		from = marks();
		legionPacket(a, Body().C(0x04).D(0).S(cb.name));
		ASSERT_TRUE(until(10s, [&] { return !ofName(windowOf(b, from), "SM_LEGION_LEAVE_MEMBER").empty(); }));
		settle();
		EXPECT_EQ(count("SELECT COUNT(*) FROM legion_members WHERE player_id = " + std::to_string(cb.playerId)), 0);
		EXPECT_EQ(count("SELECT COUNT(*) FROM legion_history h JOIN legions l ON l.id = h.legion_id WHERE h.history_type = 'KICK' AND l.name = '" +
		                legionName + "'"),
			2) << "the leave and the kick each write a KICK";
	});

	std::optional<int32_t> gameServerExit;
	if (servers.gameServer() != nullptr)
		gameServerExit = servers.stopGameServer();
	for (ScenarioClient* client : all)
		if (client->game)
			client->game->waitClosed(60s);
	const std::optional<int32_t> loginServerExit = servers.loginServer() != nullptr ? servers.stopLoginServer() : std::nullopt;

	cases.run("C23", "reports: the Q8 bar, the allow-list, the live counts", [&] {
		ASSERT_TRUE(gameServerExit) << "the game server did not exit after the stop file was written";
		EXPECT_EQ(*gameServerExit, 0);
		ASSERT_TRUE(loginServerExit);
		EXPECT_TRUE(*loginServerExit == 0 || *loginServerExit == 98) << "the login server exited with " << *loginServerExit;
		EXPECT_TRUE(servers.readReportLines("unported_trace.txt").empty()) << "AION_UNPORTED sites were reached:\n"
		                                                                   << join(servers.readReportLines("unported_trace.txt"), "\n");
		const std::vector<AllowlistEntry> allowlist = readAllowlist();
		ASSERT_FALSE(allowlist.empty()) << "tests/scenario/m5h_partial_allowlist.txt is empty or missing";
		std::map<std::string, int64_t> hitsByEntry;
		for (const PartialHit& hit : readPartialHits(servers)) {
			bool allowed = false;
			for (const AllowlistEntry& entry : allowlist)
				if (allowlistEntryMatches(entry.site, hit.site)) {
					allowed = true;
					hitsByEntry[entry.site] += hit.hits;
				}
			EXPECT_TRUE(allowed) << "the AION_PARTIAL site " << hit.site << " is not in tests/scenario/m5h_partial_allowlist.txt (" << hit.line << ")";
		}
		for (const AllowlistEntry& entry : allowlist) {
			if (entry.section == AllowlistSection::HitAtLeastOnce)
				EXPECT_GT(hitsByEntry[entry.site], 0) << "the section A row " << entry.site << " was never hit";
			else if (entry.section == AllowlistSection::HitNever)
				EXPECT_EQ(hitsByEntry[entry.site], 0) << "the section B row " << entry.site << " was hit";
		}
		const std::vector<std::string> census = servers.readReportLines("census.txt");
		EXPECT_TRUE(census.empty()) << "the final census reports leaks:\n" << join(census, "\n");
		EXPECT_TRUE(servers.readReportLines("lockdep.txt").empty()) << "the lock order validator reported:\n"
		                                                            << join(servers.readReportLines("lockdep.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("watchdog.txt").empty()) << "the watchdog dumped:\n" << join(servers.readReportLines("watchdog.txt"), "\n");
		const std::map<std::string, std::vector<std::string>> summary = servers.readSummary();
		const auto notPorted = summary.find("notPortedClientPacket");
		if (notPorted != summary.end())
			ADD_FAILURE() << "the scripted path sent client packets that are not ported: " << join(notPorted->second);
		std::vector<std::string> errors;
		ASSERT_NE(servers.gameServer(), nullptr);
		for (const std::string& line : servers.gameServer()->findLogLines(" ERROR "))
			errors.push_back("game server: " + line);
		if (servers.loginServer() != nullptr)
			for (const std::string& line : servers.loginServer()->findLogLines(" ERROR "))
				errors.push_back("login server: " + line);
		EXPECT_TRUE(errors.empty()) << "ERROR lines in the server logs:\n" << join(errors, "\n");
		EXPECT_TRUE(servers.gameServer()->findLogLines("objects removed from the world are still alive", 5).empty())
		  << join(servers.gameServer()->findLogLines("objects removed from the world are still alive", 5), "\n");
		for (const auto& [name, count] : readLiveCounts(servers, "live_counts.txt"))
			if (name == "Legion" || name == "LegionMember" || name == "Player")
				std::cout << "live counts: " << count.line << std::endl;
	});

	finishRun(servers, outputDir, testName);
}

} // namespace

/** `gs.scenario.m5h` (m5h-plan.md G-03): the legion cases of §10.2 this gate scripts (the header comment says which are not) */
TEST(M5hScenario, Run) {
	runM5hGate();
}

} // namespace aion::gameserver::scenario
