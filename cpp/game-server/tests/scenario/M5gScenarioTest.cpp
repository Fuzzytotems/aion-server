// The M5g party gate (m5g-plan.md G-01, §10.2, §10.3): one login server and one game server as child processes on their own test schemas, and
// FOUR accounts online at once - A an Elyos Warrior (the leader), B a Mage, C a Priest, D a Scout (the outsider, later a member) - beside the
// juvenile sparkies of Poeta: an invite declined and accepted, a third member, the invite refusals, the party UI data exchange, a buff's member
// icons, the move updater, the loot rules, a target mark, the shared experience and the round-robin looter of two kills and a third with C out
// of range, the kinah split, a leader change, a kick, find group, a leader's disconnect and reconnect, a member's death by a monster, the
// offline timeout, the last leave's disband, and a live group at the shutdown; then the reports the server writes at shutdown.
//
// **What this gate covers of §10.2, and what not** (docs/deviations/P5-SC.md, "M5g gate"): the chat rows C6 / C6c (GP5, GP5c) are lane A's,
// which ports CM_CHAT_MESSAGE_PUBLIC; the mantra (C7's 1809, GP6b), the group buff (C8, GP7), the roll and its automatic pass (C13, GP13,
// GP14), the summon (C13b, GP13b), the quest credit and the quest share (C14's GP11b, C15b's GP15b) are not scripted in this first gate:
// their unit tests stand (tests/team, tests/cm_*, tests/playersvc), and the rows are the plan's open items. C7 keeps the member icons of a
// self-buff (GP6 without the CHANT entry), C12 runs the shared experience and the round robin (GP11, GP12) and the kinah split of the corpses
// (GP15), C19b the friendly death and the revive (GP17b).
//
// Every expectation is independent of the C++ server code, as in the earlier gates: server packets are read with the decoders of
// tests/scenario/decoders (TeamDecoders.h for the party packets, written from the Java writeImpl, m5a-plan.md D9), and every number comes from
// `tools/oracle/oracle.py` - m5g-team (the levels of D8, each member's share of each kill, the team constants and the message and question
// ids), m5a-creation and m5b-monster - or from the Java method an assertion is about, cited at the line.
//
// **Four clients, read in turn.** GameSession reads on demand, so a client that is not being read keeps its packets in the socket buffer, and
// the party updaters send every member a packet every 0.5-2 s. Every step therefore drains all four clients (drainAll) and reads a window "of
// X" as X's packets recorded between two marks.
//
// **This file deliberately does not share the other gates' helpers** (M5b3ScenarioTest.cpp gives the reason); the scaffolding below is
// M5eScenarioTest.cpp's, never an assertion.

#include <algorithm>
#include <array>
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
#include "decoders/ItemDecoders.h"
#include "decoders/PacketDecoders.h"
#include "decoders/ProgressionDecoders.h"
#include "decoders/QuestDecoders.h"
#include "decoders/SkillDecoders.h"
#include "decoders/TeamDecoders.h"

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
	std::string password = "m5gPassword1";
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

/** Reads tests/scenario/m5g_partial_allowlist.txt with its three sections ("# --- SECTION A/B/C" marker lines, as the earlier lists) */
std::vector<AllowlistEntry> readAllowlist() {
	std::vector<AllowlistEntry> entries;
	std::ifstream in(AION_SCENARIO_M5G_PARTIAL_ALLOWLIST, std::ios::binary);
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

// ---- the party gate --------------------------------------------------------------------------------------------------------------------

constexpr int32_t POETA = 210010000;
/** the juvenile sparkie of m5b-plan.md D11 (level 2, aggressive): the party's kills */
constexpr int32_t SPARKIE = 210663;
/** Scout's two-template self-buff (m5b2-plan.md X7), seeded into B's player_skills (D8, the m5b2 D3 technique) */
constexpr uint16_t FOCUSED_EVASION = 3195;
/** PlayerClass ids (PlayerClass.java): WARRIOR 0, SCOUT 3, MAGE 6, PRIEST 9 */
constexpr int32_t CLASS_SCOUT = 3;
constexpr int32_t CLASS_PRIEST = 9;
/** the stand of the party: this far from the sparkie spot (outside its 8 m aggro range), and the far spot of kill 3 (> 100 m) */
constexpr double STAND_DISTANCE = 14.0;
constexpr double FAR_DISTANCE = 125.0;
constexpr int32_t KINAH_ITEM = 182400001;
/** SM_QUESTION_WINDOW.STR_PARTY_DO_YOU_ACCEPT_INVITATION, confirmed by the oracle in C0 */
constexpr int32_t QUESTION_PARTY = 60000;
/** TeamCommand codes (TeamCommand.java), confirmed by the oracle in C0 */
constexpr uint8_t GROUP_BAN_MEMBER = 2;
constexpr uint8_t GROUP_SET_LEADER = 3;
constexpr uint8_t GROUP_REMOVE_MEMBER = 6;

/** One kill of the gate: the npc and where its fight is in the recording */
struct KillRecord {
	int32_t templateId = 0;
	int32_t objectId = 0;
	size_t fightFrom = 0;
	bool died = false;
};

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

std::vector<decoders::GroupMemberInfo> memberInfosOf(const std::vector<Packet>& packets) {
	std::vector<decoders::GroupMemberInfo> infos;
	for (const Packet& packet : ofName(packets, "SM_GROUP_MEMBER_INFO"))
		infos.push_back(decoders::decodeGroupMemberInfo(packet.data));
	return infos;
}

std::vector<decoders::GroupInfo> groupInfosOf(const std::vector<Packet>& packets) {
	std::vector<decoders::GroupInfo> infos;
	for (const Packet& packet : ofName(packets, "SM_GROUP_INFO"))
		infos.push_back(decoders::decodeGroupInfo(packet.data));
	return infos;
}

size_t countMemberInfo(const std::vector<Packet>& packets, int32_t objectId, uint8_t event) {
	size_t n = 0;
	for (const decoders::GroupMemberInfo& info : memberInfosOf(packets))
		n += info.objectId == objectId && info.event == event ? 1 : 0;
	return n;
}

/** the experience a window's STR_GET_EXP messages grant (their second parameter, Events' GET_EXP token) */
std::vector<int64_t> expGains(const std::vector<Packet>& packets) {
	std::vector<int64_t> gains;
	for (const SystemMessage& message : messagesOf(packets))
		if (message.messageId == STR_GET_EXP_ID && message.params.size() >= 2)
			gains.push_back(std::stoll(message.params[1]));
	return gains;
}

/** the gate's oracle answer of m5g-team */
struct TeamAnswer {
	std::vector<int32_t> levels;
	std::vector<int64_t> startExp;
	/** kills[k][member]: the awarded experience, std::nullopt for a member who is not counted */
	std::vector<std::vector<std::optional<int64_t>>> awarded;
	std::vector<int32_t> lootDefaults;
	int32_t groupType = 0, groupSubType = 0;
	std::map<std::string, int32_t> messages;
	std::map<std::string, int32_t> questions;
	std::map<std::string, int32_t> commands;
	int32_t groupMaxDistance = 0;

	int32_t message(const std::string& name) const {
		const auto found = messages.find(name);
		if (found == messages.end())
			throw std::runtime_error("m5g-team answered no id for " + name);
		return found->second;
	}
};

TeamAnswer parseTeam(const std::string& text) {
	const json root = json::parse(text);
	TeamAnswer answer;
	if (!root.at("problems").empty())
		throw std::runtime_error("m5g-team: the levels do not prove the formula: " + root.at("problems").dump());
	for (const json& level : root.at("levels"))
		answer.levels.push_back(level.get<int32_t>());
	for (const json& exp : root.at("startExp"))
		answer.startExp.push_back(exp.get<int64_t>());
	for (const json& kill : root.at("kills")) {
		std::vector<std::optional<int64_t>> row;
		for (const json& member : kill.at("members"))
			row.push_back(member.is_null() ? std::nullopt : std::optional<int64_t>(member.at("awarded").get<int64_t>()));
		answer.awarded.push_back(row);
	}
	const json& constants = root.at("constants");
	const json& loot = constants.at("lootGroupRulesDefault");
	for (const char* field : {"lootRule", "misc", "commonItemAbove", "superiorItemAbove", "heroicItemAbove", "fabledItemAbove", "eternalItemAbove",
	                          "mythicItemAbove"})
		answer.lootDefaults.push_back(loot.at(field).get<int32_t>());
	answer.groupType = constants.at("teamTypes").at("GROUP").at("type").get<int32_t>();
	answer.groupSubType = constants.at("teamTypes").at("GROUP").at("subType").get<int32_t>();
	for (const auto& [name, id] : constants.at("messages").items())
		answer.messages[name] = id.get<int32_t>();
	for (const auto& [name, id] : constants.at("questions").items())
		answer.questions[name] = id.get<int32_t>();
	for (const auto& [name, id] : constants.at("teamCommands").items())
		answer.commands[name] = id.get<int32_t>();
	answer.groupMaxDistance = constants.at("groupMaxDistance").get<int32_t>();
	return answer;
}

/** the corpse a kill leaves and who may loot it (SM_LOOT_STATUS LOOT_ENABLE) */
struct TeamKill {
	KillRecord kill;
	std::array<size_t, 4> from{};
	std::vector<std::string> enabledFor;
};

void runM5gGate() {
	const std::string testName = "gs.scenario.m5g";
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
	const std::filesystem::path outputDir = std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "m5g";
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

	// ---- §10.1: the M5d profile's keys as M5e passes them, and what M5g adds (m5g.properties.example, I-04): the drop rate of D9, the group
	// rate of D8 that tells XP_GROUP_HUNTING from XP_HUNTING, the removetime of D6(a); events, autogroup, rift and vortex stay off (the
	// M5a profile already has them off)
	std::map<std::string, std::string> gateKeys;
	gateKeys["gameserver.geodata.enable"] = "false";
	gateKeys["gameserver.npcshouts.enable"] = "false";
	gateKeys["gameserver.rates.xp.solo"] = "1.0, 2.0";
	gateKeys["gameserver.rates.xp.group"] = "1.5, 3.0";
	gateKeys["gameserver.soulsickness.disable"] = "10";
	gateKeys["gameserver.rates.drop"] = "1000000";
	gateKeys["gameserver.rates.xp.quest"] = "1.0, 2.0";
	gateKeys["gameserver.rates.kinah.quest"] = "1.0, 2.0";
	gateKeys["gameserver.analysis.quest_handlers"] = "false";
	gateKeys["gameserver.playergroup.removetime"] = "5";
	gateKeys["gameserver.playeralliance.removetime"] = "5";

	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.schemaPrefix = "m5g";
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
	ScenarioClient a, b, c, d;
	std::array<ScenarioClient*, 4> all{&a, &b, &c, &d};
	const std::array<std::string, 4> labels{"A", "B", "C", "D"};
	for (size_t i = 0; i < all.size(); i++) {
		all[i]->label = labels[i];
		all[i]->account = "m5g" + std::string(1, static_cast<char>('a' + i)) + suffix;
	}
	// D8's characters; names are letters only (NameRestrictionService)
	Character ca{"A", "Partylead", NewCharacter::WARRIOR, "WARRIOR", ""};
	Character cb{"B", "Partymage", NewCharacter::MAGE, "MAGE", ""};
	Character cc{"C", "Partypriest", CLASS_PRIEST, "PRIEST", ""};
	Character cd{"D", "Partyscout", CLASS_SCOUT, "SCOUT", ""};
	std::array<Character*, 4> characters{&ca, &cb, &cc, &cd};

	// ---- the four-client helpers ----
	/** reads every client for `window`, a few milliseconds at a time each, so no socket buffer fills while another client acts */
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
	/** drains until `done` answers true or `timeout` passed. @return whether it did */
	const auto until = [&](std::chrono::milliseconds timeout, const std::function<bool()>& done) {
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		while (!done()) {
			if (std::chrono::steady_clock::now() >= deadline)
				return false;
			drainAll(100ms);
		}
		return true;
	};
	const auto marks = [&] {
		std::array<size_t, 4> m{};
		for (size_t i = 0; i < all.size(); i++)
			m[i] = all[i]->mark();
		return m;
	};
	const auto windowOf = [&](const ScenarioClient& client, const std::array<size_t, 4>& from) {
		for (size_t i = 0; i < all.size(); i++)
			if (all[i] == &client)
				return client.since(from[i]);
		return std::vector<Packet>{};
	};
	const auto send = [&](ScenarioClient& client, int32_t opcode, const std::vector<uint8_t>& body) { client.game->send(opcode, body); };

	// ---- C0: the oracles ----
	TeamAnswer team;
	std::optional<OracleMonsterSpot> sparkieSpot;
	std::array<std::array<float, 3>, 4> stands{};
	std::array<float, 3> farSpot{};
	const std::vector<std::string> messageNames{"STR_PARTY_INVITED_HIM", "STR_PARTY_HE_REJECT_INVITATION", "STR_PARTY_ENTERED_PARTY",
	  "STR_PARTY_HE_ENTERED_PARTY", "STR_PARTY_ONLY_LEADER_CAN_INVITE", "STR_PARTY_CAN_NOT_INVITE_SELF", "STR_PARTY_HE_IS_ALREADY_MEMBER_OF_OUR_PARTY",
	  "STR_NO_SUCH_USER", "STR_PARTY_YOU_BECOME_NEW_LEADER", "STR_PARTY_HE_IS_NEW_LEADER", "STR_PARTY_HE_BECOME_OFFLINE", "STR_PARTY_YOU_ARE_BANISHED",
	  "STR_PARTY_HE_IS_BANISHED", "STR_PARTY_MATCH_OFFER_PARTY_POSTED", "STR_PARTY_HE_BECOME_OFFLINE_TIMEOUT", "STR_PARTY_HE_LEAVE_PARTY",
	  "STR_PARTY_IS_DISPERSED", "STR_MSG_SPLIT_ME_TO_B", "STR_MSG_SPLIT_B_TO_ME", "STR_MSG_COMBAT_FRIENDLY_DEATH", "STR_MSG_COMBAT_MY_DEATH",
	  "STR_MSG_GET_ITEM_PARTYNOTICE", "STR_MSG_DICE_RESULT_ME", "STR_MSG_DICE_RESULT_OTHER", "STR_MSG_DICE_GIVEUP_ME", "STR_MSG_PAY_ALL_GIVEUP"};
	runCase("C0", "the oracles answer: m5g-team (D8's levels, the shares, the constants), m5a-creation, m5b-monster", [&] {
		std::vector<std::string> arguments{"m5g-team", "--npc-id", std::to_string(SPARKIE), "--xp-group-rate", "1.5", "--xp-solo-rate", "1.0", "--question",
		                                   "STR_PARTY_DO_YOU_ACCEPT_INVITATION", "--message"};
		for (const std::string& name : messageNames)
			arguments.push_back(name);
		team = parseTeam(oracle->run(arguments));
		ASSERT_EQ(team.levels.size(), 3u);
		EXPECT_EQ(team.questions.at("STR_PARTY_DO_YOU_ACCEPT_INVITATION"), QUESTION_PARTY);
		EXPECT_EQ(team.commands.at("GROUP_BAN_MEMBER"), GROUP_BAN_MEMBER);
		EXPECT_EQ(team.commands.at("GROUP_SET_LEADER"), GROUP_SET_LEADER);
		EXPECT_EQ(team.commands.at("GROUP_REMOVE_MEMBER"), GROUP_REMOVE_MEMBER);
		EXPECT_EQ(team.groupMaxDistance, 100);
		EXPECT_GT(*team.awarded[0][2], *team.awarded[0][0]) << "C's share above A's (D8)";
		const OracleMonster sparkie = oracle->monster(POETA, SPARKIE, team.levels[0]);
		ASSERT_TRUE(sparkie.nearestPlainSpot) << "m5b-monster: no fixed spot of " << SPARKIE;
		sparkieSpot = sparkie.nearestPlainSpot;
		const OracleCreation warrior = oracle->creation("ELYOS", "WARRIOR");
		// the stands: A 14 m from the spot towards the Elyos spawn, B, C and D 4 m beside A (D within 20 m of B, §10.1 "Spots"); the far spot
		// 125 m from the sparkie spot on the same line
		const std::array<float, 3> lead = pointNear(sparkieSpot->x, sparkieSpot->y, sparkieSpot->z, STAND_DISTANCE, warrior.x, warrior.y);
		const double dx = lead[0] - sparkieSpot->x, dy = lead[1] - sparkieSpot->y;
		const double length = std::sqrt(dx * dx + dy * dy);
		const float px = static_cast<float>(-dy / length), py = static_cast<float>(dx / length);
		stands[0] = lead;
		stands[1] = {lead[0] + 4 * px, lead[1] + 4 * py, lead[2]};
		stands[2] = {lead[0] - 4 * px, lead[1] - 4 * py, lead[2]};
		stands[3] = {lead[0] + 8 * px, lead[1] + 8 * py, lead[2]};
		farSpot = pointNear(sparkieSpot->x, sparkieSpot->y, sparkieSpot->z, FAR_DISTANCE, warrior.x, warrior.y);
		std::cout << "C0: levels A = B = " << team.levels[0] << ", C = " << team.levels[2] << "; kill shares " << *team.awarded[0][0] << " / "
		          << *team.awarded[0][2] << ", kill 3 " << *team.awarded[2][0] << "; sparkie spot (" << sparkieSpot->x << ", " << sparkieSpot->y
		          << ")" << std::endl;
	});

	// ---- C1: the four characters ----
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
	runCase("C1", "four accounts: create A-D, the first enter world and the prologue, the seeds of D8 offline, then all four enter", [&] {
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
		// D8: A and B at level L, C at L', D at L; all at their stands with full HP; B knows Focused Evasion
		const std::array<int64_t, 4> exps{team.startExp[0], team.startExp[1], team.startExp[2], team.startExp[0]};
		for (size_t i = 0; i < all.size(); i++) {
			database.execute(schema, "UPDATE players SET exp = " + std::to_string(exps[i]) + ", world_id = " + std::to_string(POETA) + ", x = " +
			                           std::to_string(stands[i][0]) + ", y = " + std::to_string(stands[i][1]) + ", z = " + std::to_string(stands[i][2]) +
			                           ", heading = 0, npc_expands = 5 WHERE id = " + std::to_string(characters[i]->playerId));
			database.setLifeStatHp(schema, characters[i]->playerId, 1000000);
		}
		database.execute(schema, "INSERT INTO player_skills (player_id, skill_id, skill_level) VALUES (" + std::to_string(cb.playerId) + ", " +
		                           std::to_string(FOCUSED_EVASION) + ", 1)");
		for (size_t i = 0; i < all.size(); i++) {
			enterAs(servers, *all[i], *characters[i]);
			all[i]->labels[characters[i]->playerId] = all[i]->label;
			drainAll(500ms);
		}
		drainAll(2s);
		for (size_t i = 0; i < all.size(); i++)
			for (size_t j = 0; j < all.size(); j++)
				all[i]->labels[characters[j]->playerId] = labels[j];
	});

	const auto ids = [&](size_t i) { return characters[i]->playerId; };
	const auto msg = [&](const std::string& name) { return team.message(name); };
	int32_t groupId = 0;

	// ---- C2-C5: invites ----
	runCase("C2", "decline: A invites B, B answers 0 (GP1)", [&] {
		const auto from = marks();
		send(a, GameSession::CM_INVITE_TO_GROUP, GameSession::buildCM_INVITE_TO_GROUP(0, cb.name));
		ASSERT_TRUE(until(10s, [&] { return !ofName(windowOf(b, from), "SM_QUESTION_WINDOW").empty(); })) << "B was not asked";
		const decoders::QuestionWindow question = decoders::decodeQuestionWindow(ofName(windowOf(b, from), "SM_QUESTION_WINDOW")[0].data);
		EXPECT_EQ(question.code, QUESTION_PARTY);
		EXPECT_EQ(question.params[0], ca.name);
		EXPECT_EQ(countMessage(windowOf(a, from), msg("STR_PARTY_INVITED_HIM"), {cb.name}), 1u);
		send(b, GameSession::CM_QUESTION_RESPONSE, GameSession::buildCM_QUESTION_RESPONSE(QUESTION_PARTY, GameSession::ANSWER_NO));
		EXPECT_TRUE(until(10s, [&] { return countMessage(windowOf(a, from), msg("STR_PARTY_HE_REJECT_INVITATION"), {cb.name}) == 1; }))
		  << "GP1: A was not told B rejected";
		drainAll(1s);
		for (ScenarioClient* client : all)
			EXPECT_TRUE(ofName(windowOf(*client, from), "SM_GROUP_INFO").empty()) << "GP1: " << client->label << " got SM_GROUP_INFO after a decline";
	});

	runCase("C3", "accept: A invites B again, B answers 1 (GP2)", [&] {
		const auto from = marks();
		send(a, GameSession::CM_INVITE_TO_GROUP, GameSession::buildCM_INVITE_TO_GROUP(0, cb.name));
		ASSERT_TRUE(until(10s, [&] { return !ofName(windowOf(b, from), "SM_QUESTION_WINDOW").empty(); })) << "GP1: the second invite asked nothing";
		send(b, GameSession::CM_QUESTION_RESPONSE, GameSession::buildCM_QUESTION_RESPONSE(QUESTION_PARTY, GameSession::ANSWER_YES));
		ASSERT_TRUE(until(10s, [&] { return !groupInfosOf(windowOf(a, from)).empty() && !groupInfosOf(windowOf(b, from)).empty(); }))
		  << "GP2: no SM_GROUP_INFO for A and B";
		drainAll(1500ms);
		const std::vector<Packet> wa = windowOf(a, from), wb = windowOf(b, from), wd = windowOf(d, from);
		const decoders::GroupInfo infoA = groupInfosOf(wa).front();
		groupId = infoA.groupId;
		EXPECT_NE(groupId, 0);
		EXPECT_EQ(infoA.leaderId, ids(0));
		EXPECT_EQ(infoA.lootWords, team.lootDefaults) << "GP2: LootGroupRules() defaults";
		EXPECT_EQ(infoA.type, team.groupType);
		EXPECT_EQ(infoA.subType, team.groupSubType);
		EXPECT_EQ(infoA.mapId, POETA);
		EXPECT_EQ(countMessage(wa, msg("STR_PARTY_ENTERED_PARTY")), 1u);
		EXPECT_EQ(countMemberInfo(wa, ids(0), decoders::GROUP_EVENT_JOIN), 1u) << "GP2: A's JOIN";
		EXPECT_EQ(countMemberInfo(wa, ids(1), decoders::GROUP_EVENT_ENTER), 1u) << "GP2: A's ENTER for B";
		EXPECT_EQ(countMessage(wa, msg("STR_PARTY_HE_ENTERED_PARTY"), {cb.name}), 1u);
		EXPECT_EQ(countMemberInfo(wa, ids(0), decoders::GROUP_EVENT_ENTER), 0u) << "GP2: ENTER about oneself";
		for (const decoders::GroupMemberInfo& info : memberInfosOf(wa))
			if (info.objectId == ids(0) && info.event == decoders::GROUP_EVENT_JOIN) {
				EXPECT_EQ(info.name, std::optional<std::string>(ca.name));
				EXPECT_GT(info.maxHp, 0);
			}
		// A's order: SM_GROUP_INFO, STR_PARTY_ENTERED_PARTY, JOIN (PlayerGroupEnteredEvent.java:27-29)
		size_t infoAt = SIZE_MAX, enteredAt = SIZE_MAX, joinAt = SIZE_MAX;
		for (size_t i = 0; i < wa.size(); i++) {
			if (wa[i].name == "SM_GROUP_INFO" && infoAt == SIZE_MAX)
				infoAt = i;
			else if (wa[i].name == "SM_SYSTEM_MESSAGE" && enteredAt == SIZE_MAX && decodeSystemMessage(wa[i].data).messageId == msg("STR_PARTY_ENTERED_PARTY"))
				enteredAt = i;
			else if (wa[i].name == "SM_GROUP_MEMBER_INFO" && joinAt == SIZE_MAX && decoders::decodeGroupMemberInfo(wa[i].data).event == decoders::GROUP_EVENT_JOIN)
				joinAt = i;
		}
		EXPECT_TRUE(infoAt < enteredAt && enteredAt < joinAt) << "GP2: A's order " << infoAt << ", " << enteredAt << ", " << joinAt;
		ASSERT_FALSE(groupInfosOf(wb).empty());
		EXPECT_EQ(groupInfosOf(wb).front().groupId, groupId);
		EXPECT_EQ(groupInfosOf(wb).front().leaderId, ids(0));
		EXPECT_EQ(countMemberInfo(wb, ids(1), decoders::GROUP_EVENT_JOIN), 1u);
		EXPECT_EQ(countMemberInfo(wb, ids(0), decoders::GROUP_EVENT_ENTER), 1u);
		// E-21: the team id serializes - D sees both members' SM_ABYSS_RANK_UPDATE(1, groupId)
		std::set<int32_t> rankedForD;
		for (const Packet& packet : ofName(wd, "SM_ABYSS_RANK_UPDATE")) {
			const decoders::AbyssRankUpdate update = decoders::decodeAbyssRankUpdate(packet.data);
			if (update.action == 1 && update.value == groupId)
				rankedForD.insert(update.objectId);
		}
		EXPECT_EQ(rankedForD, (std::set<int32_t>{ids(0), ids(1)})) << "GP2: D's SM_ABYSS_RANK_UPDATE(1)";
		EXPECT_TRUE(ofName(wd, "SM_GROUP_INFO").empty()) << "GP2: D is no member";
	});

	runCase("C4", "a third member: A invites C, C accepts (GP3)", [&] {
		const auto from = marks();
		send(a, GameSession::CM_INVITE_TO_GROUP, GameSession::buildCM_INVITE_TO_GROUP(0, cc.name));
		ASSERT_TRUE(until(10s, [&] { return !ofName(windowOf(c, from), "SM_QUESTION_WINDOW").empty(); }));
		send(c, GameSession::CM_QUESTION_RESPONSE, GameSession::buildCM_QUESTION_RESPONSE(QUESTION_PARTY, GameSession::ANSWER_YES));
		ASSERT_TRUE(until(10s, [&] { return !groupInfosOf(windowOf(c, from)).empty(); }));
		drainAll(1500ms);
		const std::vector<Packet> wc = windowOf(c, from);
		EXPECT_EQ(groupInfosOf(wc).front().groupId, groupId) << "GP3: C joined another group";
		EXPECT_EQ(groupInfosOf(wc).front().leaderId, ids(0));
		EXPECT_EQ(countMemberInfo(wc, ids(2), decoders::GROUP_EVENT_JOIN), 1u);
		EXPECT_EQ(countMemberInfo(wc, ids(0), decoders::GROUP_EVENT_ENTER), 1u);
		EXPECT_EQ(countMemberInfo(wc, ids(1), decoders::GROUP_EVENT_ENTER), 1u);
		EXPECT_EQ(countMemberInfo(windowOf(a, from), ids(2), decoders::GROUP_EVENT_ENTER), 1u);
		EXPECT_EQ(countMemberInfo(windowOf(b, from), ids(2), decoders::GROUP_EVENT_ENTER), 1u);
	});

	runCase("C5", "refusals: B invites D and himself, A invites himself, B and a name nobody has (GP4)", [&] {
		auto from = marks();
		send(b, GameSession::CM_INVITE_TO_GROUP, GameSession::buildCM_INVITE_TO_GROUP(0, cd.name));
		send(b, GameSession::CM_INVITE_TO_GROUP, GameSession::buildCM_INVITE_TO_GROUP(0, cb.name));
		send(a, GameSession::CM_INVITE_TO_GROUP, GameSession::buildCM_INVITE_TO_GROUP(0, ca.name));
		send(a, GameSession::CM_INVITE_TO_GROUP, GameSession::buildCM_INVITE_TO_GROUP(0, cb.name));
		send(a, GameSession::CM_INVITE_TO_GROUP, GameSession::buildCM_INVITE_TO_GROUP(0, "Nosuchname"));
		drainAll(2s);
		EXPECT_EQ(countMessage(windowOf(b, from), msg("STR_PARTY_ONLY_LEADER_CAN_INVITE")), 2u) << "GP4: B's two invites (leader before self)";
		EXPECT_EQ(countMessage(windowOf(b, from), msg("STR_PARTY_CAN_NOT_INVITE_SELF")), 0u) << "GP4: the self check came before the leader check";
		EXPECT_TRUE(ofName(windowOf(d, from), "SM_QUESTION_WINDOW").empty()) << "GP4: D was asked by a member who is not the leader";
		EXPECT_EQ(countMessage(windowOf(a, from), msg("STR_PARTY_CAN_NOT_INVITE_SELF")), 1u);
		EXPECT_EQ(countMessage(windowOf(a, from), msg("STR_PARTY_HE_IS_ALREADY_MEMBER_OF_OUR_PARTY"), {cb.name}), 1u);
		EXPECT_EQ(countMessage(windowOf(a, from), msg("STR_NO_SUCH_USER"), {"Nosuchname"}), 1u);
	});

	runCase("C6b", "party UI data: action 0 to the other members, action 1 to the known list and the sender (GP5b)", [&] {
		const std::vector<uint8_t> data{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
		auto from = marks();
		send(a, GameSession::CM_GROUP_DATA_EXCHANGE, GameSession::buildCM_GROUP_DATA_EXCHANGE(0, 0, 0, data));
		drainAll(1500ms);
		const auto exchanges = [&](const ScenarioClient& client, const std::array<size_t, 4>& at) {
			std::vector<decoders::GroupDataExchange> found;
			for (const Packet& packet : ofName(windowOf(client, at), "SM_GROUP_DATA_EXCHANGE"))
				found.push_back(decoders::decodeGroupDataExchange(packet.data));
			return found;
		};
		for (ScenarioClient* member : {&b, &c}) {
			const auto found = exchanges(*member, from);
			ASSERT_EQ(found.size(), 1u) << "GP5b: " << member->label;
			EXPECT_EQ(found[0].action, 0);
			EXPECT_EQ(found[0].unk2, std::optional<uint8_t>(0));
			EXPECT_EQ(found[0].data, data);
		}
		EXPECT_TRUE(exchanges(a, from).empty()) << "GP5b: the sender";
		EXPECT_TRUE(exchanges(d, from).empty()) << "GP5b: the outsider";
		from = marks();
		send(a, GameSession::CM_GROUP_DATA_EXCHANGE, GameSession::buildCM_GROUP_DATA_EXCHANGE(1, 0, 0, data));
		drainAll(1500ms);
		for (ScenarioClient* client : all) {
			const auto found = exchanges(*client, from);
			EXPECT_EQ(found.size(), 1u) << "GP5b action 1: " << client->label;
			if (!found.empty())
				EXPECT_EQ(found[0].action, 1);
		}
	});

	runCase("C7", "member icons: B casts Focused Evasion on himself (GP6, the BUFF slot)", [&] {
		auto from = marks();
		GameSession::CastRequest request;
		request.spellId = FOCUSED_EVASION;
		request.level = 1;
		request.targetType = decoders::CAST_TARGET_OBJECT;
		request.targetObjectId = ids(1);
		const GameSession::CastOutcome outcome = b.game->castAndWait(ids(1), request, 6s);
		ASSERT_TRUE(outcome.castSpellResult) << "GP6: B's cast did not complete";
		drainAll(2s);
		const auto buffUpdate = [&](const std::vector<Packet>& window, bool withSkill) {
			for (const decoders::GroupMemberInfo& info : memberInfosOf(window)) {
				if (info.objectId != ids(1) || info.event != decoders::GROUP_EVENT_UPDATE_EFFECTS || info.slot != std::optional<uint8_t>(1))
					continue;
				const bool has = std::ranges::any_of(info.effects, [](const decoders::GroupMemberEffect& e) { return e.skillId == FOCUSED_EVASION; });
				if (has == withSkill)
					return true;
			}
			return false;
		};
		for (ScenarioClient* member : {&a, &c}) {
			const std::vector<Packet> window = windowOf(*member, from);
			EXPECT_TRUE(buffUpdate(window, true)) << "GP6: " << member->label << " got no BUFF-slot UPDATE_EFFECTS of B with 3195";
			for (const decoders::GroupMemberInfo& info : memberInfosOf(window))
				if (info.objectId == ids(1) && info.event == decoders::GROUP_EVENT_UPDATE_EFFECTS && info.slot == std::optional<uint8_t>(1))
					for (const decoders::GroupMemberEffect& effect : info.effects)
						EXPECT_EQ(effect.targetSlotOrdinal, 0) << "GP6: a BUFF update lists an effect of another slot (skill " << effect.skillId << ")";
			EXPECT_GE(countMemberInfo(window, ids(1), decoders::GROUP_EVENT_MOVEMENT), 1u) << "GP6: updatePlayerIconsAndGroup's MOVEMENT";
		}
		EXPECT_TRUE(std::ranges::none_of(memberInfosOf(windowOf(b, from)), [&](const decoders::GroupMemberInfo& i) { return i.objectId == ids(1); }))
		  << "GP6: B received member info about himself";
		EXPECT_TRUE(until(30s, [&] { return buffUpdate(windowOf(a, from), false); })) << "GP6: the buff's end sent no BUFF update without 3195";
	});

	runCase("C9", "the move updater: C walks 10 m (GP8's move half)", [&] {
		const std::array<float, 3> to{c.x + 10.0f, c.y, c.z};
		auto from = marks();
		walkTo(c, to[0], to[1], to[2]);
		const auto near = [&](const ScenarioClient& member) {
			for (const decoders::GroupMemberInfo& info : memberInfosOf(windowOf(member, from)))
				if (info.objectId == ids(2) && info.event == decoders::GROUP_EVENT_MOVEMENT && distance2d(info.x, info.y, to[0], to[1]) <= 0.5)
					return true;
			return false;
		};
		EXPECT_TRUE(until(3s, [&] { return near(a) && near(b); })) << "GP8: no MOVEMENT of C at its new position within 2.5 s";
		walkTo(c, stands[2][0], stands[2][1], stands[2][2]);
		drainAll(1s);
	});

	runCase("C10", "loot rules: A sets FREEFORALL with every quality word 0 (GP9)", [&] {
		auto from = marks();
		send(a, GameSession::CM_DISTRIBUTION_SETTINGS, GameSession::buildCM_DISTRIBUTION_SETTINGS(0, 0, {0, 0, 0, 0, 0, 0}));
		drainAll(1500ms);
		for (ScenarioClient* member : {&a, &b, &c}) {
			const std::vector<decoders::GroupInfo> infos = groupInfosOf(windowOf(*member, from));
			ASSERT_EQ(infos.size(), 1u) << "GP9: " << member->label;
			EXPECT_EQ(infos[0].lootWords, (std::vector<int32_t>(8, 0))) << "GP9: " << member->label;
		}
		EXPECT_TRUE(groupInfosOf(windowOf(d, from)).empty());
	});

	int32_t brandTarget = 0;
	runCase("C11", "target marks: the leader's brand reaches every member, B's nobody (GP10)", [&] {
		brandTarget = ids(3); // any object id: D's (the brand names an object, the server does not check it)
		auto from = marks();
		send(a, GameSession::CM_SHOW_BRAND, GameSession::buildCM_SHOW_BRAND(0, 1, brandTarget));
		drainAll(1500ms);
		for (ScenarioClient* member : {&a, &b, &c}) {
			const std::vector<Packet> brands = ofName(windowOf(*member, from), "SM_SHOW_BRAND");
			ASSERT_EQ(brands.size(), 1u) << "GP10: " << member->label;
			EXPECT_EQ(decoders::decodeShowBrand(brands[0].data).brands, (std::vector<std::pair<int32_t, int32_t>>{{1, brandTarget}}));
		}
		from = marks();
		send(b, GameSession::CM_SHOW_BRAND, GameSession::buildCM_SHOW_BRAND(0, 2, brandTarget));
		drainAll(1500ms);
		for (ScenarioClient* client : all)
			EXPECT_TRUE(ofName(windowOf(*client, from), "SM_SHOW_BRAND").empty()) << "GP10: B is not the leader: " << client->label;
	});

	/**
	 * One kill: the nearest announced npc of `templates` that is not dead (an SM_NPC_INFO, its last SM_MOVE, no DIE and no SM_DELETE since),
	 * waiting up to `respawnWait` for one; the character walks to 2 m of it, selects it and fights it with auto-attacks until it dies
	 * (GameSession::fightUntil, M5b's fight). Other npcs that attack the character are not fought.
	 */
	const auto killNearest = [&](ScenarioClient& client, const std::vector<int32_t>& templates, std::chrono::milliseconds respawnWait,
	                             double maxDistance = 45.0) -> KillRecord {
		const auto choose = [&]() -> std::optional<std::pair<int32_t, KnownNpcs::Npc>> {
			std::optional<std::pair<int32_t, KnownNpcs::Npc>> best;
			double bestDistance = maxDistance;
			for (const auto& [objectId, npc] : client.npcs.all()) {
				if (npc.dead || npc.deleted || std::ranges::find(templates, npc.templateId) == templates.end())
					continue;
				const double distance = distance2d(npc.x, npc.y, client.x, client.y);
				if (distance < bestDistance) {
					bestDistance = distance;
					best = {objectId, npc};
				}
			}
			return best;
		};
		// a character below half its HP rests first: the fights are real (M5dScenarioTest.cpp's REST_BELOW), and a death would be the gate's
		const auto restDeadline = std::chrono::steady_clock::now() + 60s;
		for (std::optional<decoders::StatUpdateHp> hp = client.lastHp();
		     hp && hp->maxHp > 0 && hp->currentHp < hp->maxHp / 2 && std::chrono::steady_clock::now() < restDeadline; hp = client.lastHp())
			collectFor(*client.game, 1s);
		std::optional<std::pair<int32_t, KnownNpcs::Npc>> target = choose();
		const auto deadline = std::chrono::steady_clock::now() + respawnWait;
		while (!target && std::chrono::steady_clock::now() < deadline) {
			collectFor(*client.game, 1s);
			target = choose();
		}
		if (!target)
			throw std::runtime_error(client.label + ": no npc of " + joinNumbers(templates) + " within " + std::to_string(maxDistance) + " m");
		const std::array<float, 3> melee = pointNear(target->second.x, target->second.y, target->second.z, MELEE_DISTANCE, client.x, client.y);
		walkTo(client, melee[0], melee[1], melee[2]);
		client.game->send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(target->first));
		waitFor(*client.game, "SM_TARGET_SELECTED", 10s);
		const std::optional<decoders::StatsInfo> stats = client.lastStats();
		if (!stats)
			throw std::runtime_error("no SM_STATS_INFO recorded");
		KillRecord kill;
		kill.templateId = target->second.templateId;
		kill.objectId = target->first;
		const int32_t npc = target->first;
		bool characterDied = false;
		const GameSession::FightOutcome outcome = client.game->fightUntil(
		  npc, std::chrono::milliseconds(stats->attackSpeed),
		  [&](const Packet& packet) {
			  if (packet.name != "SM_EMOTION")
				  return false;
			  try {
				  const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
				  if (emotion.emotionType != decoders::EMOTION_DIE)
					  return false;
				  kill.died = kill.died || emotion.senderObjectId == npc;
				  characterDied = characterDied || emotion.senderObjectId == client.playerId();
				  return kill.died || characterDied;
			  } catch (const DecodeError&) {
				  return false;
			  }
		  },
		  120s, 80);
		kill.fightFrom = outcome.firstPacket;
		if (characterDied || !kill.died) {
			// the fight's evidence: who attacked whom how often, and the attack responses (refusals) the character got
			std::map<std::string, int32_t> attacks;
			int32_t responses = 0;
			for (const Packet& packet : client.since(kill.fightFrom)) {
				try {
					if (packet.name == "SM_ATTACK") {
						const decoders::AttackParties parties = decoders::decodeAttackParties(packet.data);
						const auto who = [&](int32_t id) {
							if (id == client.playerId())
								return std::string("self");
							const std::optional<KnownNpcs::Npc> n = client.npcs.get(id);
							return n ? std::to_string(n->templateId) + "#" + std::to_string(id) : std::to_string(id);
						};
						attacks[who(parties.attackerObjectId) + "->" + who(parties.targetObjectId)]++;
					} else if (packet.name == "SM_ATTACK_RESPONSE") {
						responses++;
					}
				} catch (const DecodeError&) {
				}
			}
			std::vector<std::string> summary;
			for (const auto& [pair, count] : attacks)
				summary.push_back(pair + " x" + std::to_string(count));
			std::cout << client.label << ": the fight against " << npc << ": " << join(summary) << "; SM_ATTACK_RESPONSE x" << responses << "; "
			          << outcome.attacksSent << " CM_ATTACK sent" << std::endl;
		}
		if (characterDied)
			throw std::runtime_error(client.label + ": npc " + std::to_string(kill.templateId) + " killed the character");
		if (!kill.died)
			throw std::runtime_error(client.label + ": npc " + std::to_string(kill.templateId) + " did not die within " +
			                         std::to_string(outcome.elapsed.count()) + " ms");
		collectFor(*client.game, 1500ms);
		client.model.sync();
		std::cout << client.label << ": killed npc " << kill.templateId << " (" << npc << ") in " << outcome.elapsed.count() << " ms, "
		          << outcome.attacksSent << " attacks" << std::endl;
		return kill;
	};

	// ---- C12: the kills ----
	const auto teamKill = [&](int32_t killIndex, std::array<bool, 3> counted) -> TeamKill {
		TeamKill record;
		record.from = marks();
		record.kill = killNearest(a, {SPARKIE}, 90s);
		const int32_t corpse = record.kill.objectId;
		const auto enabled = [&](const ScenarioClient& client) {
			for (const Packet& packet : ofName(windowOf(client, record.from), "SM_LOOT_STATUS")) {
				const decoders::LootStatus status = decoders::decodeLootStatus(packet.data);
				if (status.targetObjectId == corpse && status.status == decoders::LOOT_STATUS_LOOT_ENABLE)
					return true;
			}
			return false;
		};
		until(5s, [&] { return std::ranges::any_of(all, [&](ScenarioClient* client) { return enabled(*client); }); });
		drainAll(1500ms);
		for (ScenarioClient* client : all)
			if (enabled(*client))
				record.enabledFor.push_back(client->label);
		// GP11: every counted member exactly the oracle's share, D nothing, an uncounted member nothing
		for (size_t m = 0; m < 3 && killIndex >= 0; m++) {
			const std::vector<int64_t> gains = expGains(windowOf(*all[m], record.from));
			const std::optional<int64_t> expected = team.awarded[static_cast<size_t>(killIndex)][m];
			if (counted[m]) {
				if (!expected)
					throw std::runtime_error("m5g-team counts no share of member " + std::to_string(m) + " in kill " + std::to_string(killIndex + 1));
				EXPECT_EQ(gains, std::vector<int64_t>{*expected}) << "GP11 kill " << killIndex + 1 << ": " << all[m]->label << "'s experience";
			} else {
				EXPECT_TRUE(gains.empty()) << "GP11 kill " << killIndex + 1 << ": " << all[m]->label << " is out of range and got " << joinNumbers(gains);
			}
		}
		EXPECT_TRUE(expGains(windowOf(d, record.from)).empty()) << "GP11: D is no member";
		return record;
	};
	/** the looter walks to the corpse, takes every entry and walks back (D9: every corpse looted empty, m5b3-plan.md Y14) */
	const auto lootEmpty = [&](ScenarioClient& looter, const TeamKill& record, size_t stand) {
		const std::optional<KnownNpcs::Npc> npc = looter.npcs.get(record.kill.objectId);
		const std::array<float, 3> corpseAt = npc ? std::array<float, 3>{npc->x, npc->y, npc->z} : stands[0];
		const std::array<float, 3> at = pointNear(corpseAt[0], corpseAt[1], corpseAt[2], 1.5, looter.x, looter.y);
		walkTo(looter, at[0], at[1], at[2]);
		const auto from = marks();
		const std::array<int64_t, 4> kinahBefore{a.model.kinah(), b.model.kinah(), c.model.kinah(), d.model.kinah()};
		send(looter, GameSession::CM_START_LOOT, GameSession::buildCM_START_LOOT(record.kill.objectId, GameSession::LOOT_OPEN));
		std::optional<decoders::LootItemList> list;
		until(5s, [&] {
			const std::vector<Packet> lists = ofName(windowOf(looter, from), "SM_LOOT_ITEMLIST");
			if (!lists.empty())
				list = decoders::decodeLootItemList(lists.front().data);
			return list.has_value();
		});
		if (!list)
			throw std::runtime_error(looter.label + ": the corpse " + std::to_string(record.kill.objectId) + " sent no SM_LOOT_ITEMLIST");
		int64_t kinahListed = 0;
		size_t items = 0;
		for (const decoders::LootItem& entry : list->items) {
			const auto before = marks();
			send(looter, GameSession::CM_LOOT_ITEM, GameSession::buildCM_LOOT_ITEM(record.kill.objectId, static_cast<uint8_t>(entry.index)));
			until(5s, [&] {
				const std::vector<Packet> window = windowOf(looter, before);
				return !ofName(window, "SM_LOOT_ITEMLIST").empty() || !ofName(window, "SM_DELETE").empty();
			});
			if (entry.itemId == KINAH_ITEM)
				kinahListed += entry.count;
			else
				items++;
		}
		drainAll(1500ms);
		bool deleted = false;
		for (const Packet& packet : ofName(windowOf(looter, from), "SM_DELETE"))
			deleted = deleted || decoders::decodeDeleteObjectId(packet.data) == record.kill.objectId;
		EXPECT_TRUE(deleted) << "GP12: the looted corpse " << record.kill.objectId << " was not deleted";
		// GP12: the other in-range members are told of every item the looter took
		for (ScenarioClient* member : {&a, &b, &c})
			if (member != &looter && distance2d(member->x, member->y, corpseAt[0], corpseAt[1]) < team.groupMaxDistance)
				EXPECT_EQ(countMessage(windowOf(*member, from), msg("STR_MSG_GET_ITEM_PARTYNOTICE")), items)
				  << "GP12: " << member->label << " for " << looter.label << "'s " << items << " items";
		// GP15: the corpse's kinah is split over the in-range members, nothing lost (distributeEqually)
		if (kinahListed > 0) {
			int64_t gained = 0;
			for (size_t i = 0; i < 3; i++)
				gained += all[i]->model.kinah() - kinahBefore[i];
			EXPECT_EQ(gained, kinahListed) << "GP15: the kinah of the corpse (" << kinahListed << ") is not what the members gained";
		}
		std::cout << looter.label << " looted " << list->items.size() << " entries (" << items << " items, " << kinahListed << " kinah)" << std::endl;
		walkTo(looter, stands[stand][0], stands[stand][1], stands[stand][2]);
	};
	const auto looterOf = [&](const TeamKill& record) -> ScenarioClient* {
		if (record.enabledFor.size() != 1)
			return nullptr;
		for (size_t i = 0; i < all.size(); i++)
			if (labels[i] == record.enabledFor[0])
				return all[i];
		return nullptr;
	};
	const auto indexOf = [&](const ScenarioClient* client) {
		for (size_t i = 0; i < all.size(); i++)
			if (all[i] == client)
				return i;
		return size_t{0};
	};
	runCase("C12", "shared experience and the round robin: two kills with A, B, C in range, a third with C beyond 100 m (GP11, GP12, GP15)", [&] {
		auto from = marks();
		send(a, GameSession::CM_DISTRIBUTION_SETTINGS, GameSession::buildCM_DISTRIBUTION_SETTINGS(1, 0, {0, 0, 0, 0, 0, 0}));
		drainAll(1500ms);
		const TeamKill first = teamKill(0, {true, true, true});
		ScenarioClient* firstLooter = looterOf(first);
		EXPECT_EQ(first.enabledFor.size(), 1u) << "GP11: kill 1's LOOT_ENABLE went to " << join(first.enabledFor);
		if (firstLooter)
			lootEmpty(*firstLooter, first, indexOf(firstLooter));
		walkTo(a, stands[0][0], stands[0][1], stands[0][2]);
		const TeamKill second = teamKill(1, {true, true, true});
		ScenarioClient* secondLooter = looterOf(second);
		EXPECT_EQ(second.enabledFor.size(), 1u) << "GP11: kill 2's LOOT_ENABLE went to " << join(second.enabledFor);
		EXPECT_NE(firstLooter, secondLooter) << "GP11: the round robin gave both corpses to " << (firstLooter ? firstLooter->label : "-");
		if (secondLooter)
			lootEmpty(*secondLooter, second, indexOf(secondLooter));
		walkTo(a, stands[0][0], stands[0][1], stands[0][2]);
		walkTo(c, farSpot[0], farSpot[1], farSpot[2]);
		const TeamKill third = teamKill(2, {true, true, false});
		ScenarioClient* thirdLooter = looterOf(third);
		if (thirdLooter)
			lootEmpty(*thirdLooter, third, indexOf(thirdLooter));
		walkTo(a, stands[0][0], stands[0][1], stands[0][2]);
	});


	runCase("C13", "roll: the default quality rules, A kills a sparkie with C beyond 100 m; the first roll item rolled by A and B, the second "
	               "left to the automatic pass (GP13, GP14)", [&] {
		send(a, GameSession::CM_DISTRIBUTION_SETTINGS, GameSession::buildCM_DISTRIBUTION_SETTINGS(1, 0, {0, 2, 2, 2, 2, 2}));
		drainAll(1500ms);
		const TeamKill kill = teamKill(-1, {true, true, false});
		ScenarioClient* looter = looterOf(kill);
		ASSERT_NE(looter, nullptr) << "C13: LOOT_ENABLE went to " << join(kill.enabledFor);
		const int32_t corpse = kill.kill.objectId;
		const std::optional<KnownNpcs::Npc> npc = looter->npcs.get(corpse);
		ASSERT_TRUE(npc);
		const std::array<float, 3> at = pointNear(npc->x, npc->y, npc->z, 1.5, looter->x, looter->y);
		walkTo(*looter, at[0], at[1], at[2]);
		auto from = marks();
		send(*looter, GameSession::CM_START_LOOT, GameSession::buildCM_START_LOOT(corpse, GameSession::LOOT_OPEN));
		std::optional<decoders::LootItemList> list;
		until(5s, [&] {
			const std::vector<Packet> lists = ofName(windowOf(*looter, from), "SM_LOOT_ITEMLIST");
			if (!lists.empty())
				list = decoders::decodeLootItemList(lists.front().data);
			return list.has_value();
		});
		ASSERT_TRUE(list) << "C13: no SM_LOOT_ITEMLIST";
		/** the SM_GROUP_LOOTs of a window */
		const auto groupLoots = [&](const ScenarioClient& client, const std::array<size_t, 4>& at) {
			std::vector<decoders::GroupLoot> found;
			for (const Packet& packet : ofName(windowOf(client, at), "SM_GROUP_LOOT"))
				found.push_back(decoders::decodeGroupLoot(packet.data));
			return found;
		};
		const auto finalOf = [&](const ScenarioClient& client, const std::array<size_t, 4>& at, int32_t index) -> std::optional<decoders::GroupLoot> {
			for (const decoders::GroupLoot& loot : groupLoots(client, at))
				if (loot.index == index && loot.luck == -1)
					return loot;
			return std::nullopt;
		};
		int32_t rolled = 0;
		for (const decoders::LootItem& entry : list->items) {
			auto before = marks();
			const auto requestedAt = std::chrono::steady_clock::now();
			send(*looter, GameSession::CM_LOOT_ITEM, GameSession::buildCM_LOOT_ITEM(corpse, static_cast<uint8_t>(entry.index)));
			const auto prompted = [&](const ScenarioClient& client) {
				return std::ranges::any_of(groupLoots(client, before), [&](const decoders::GroupLoot& l) { return l.index == entry.index && l.playerId == 0; });
			};
			until(5s, [&] {
				const std::vector<Packet> window = windowOf(*looter, before);
				return prompted(*looter) || !ofName(window, "SM_LOOT_ITEMLIST").empty() || !ofName(window, "SM_DELETE").empty();
			});
			drainAll(500ms);
			if (!prompted(*looter))
				continue;
			// GP13: the prompt reached A and B (in range), not C
			EXPECT_TRUE(prompted(a) && prompted(b)) << "GP13: the roll prompt for entry " << entry.index << " did not reach A and B";
			EXPECT_FALSE(prompted(c)) << "GP13: C, beyond 100 m, was prompted";
			for (const decoders::GroupLoot& prompt : groupLoots(a, before))
				if (prompt.index == entry.index && prompt.playerId == 0)
					EXPECT_EQ(prompt.distributionId, 2) << "GP13: the roll (getAutodistributionId 2)";
			if (rolled == 0) {
				send(a, GameSession::CM_GROUP_LOOT, GameSession::buildCM_GROUP_LOOT(groupId, entry.index, entry.itemId, corpse, 2, 1, 0));
				drainAll(300ms);
				send(b, GameSession::CM_GROUP_LOOT, GameSession::buildCM_GROUP_LOOT(groupId, entry.index, entry.itemId, corpse, 2, 1, 0));
				ASSERT_TRUE(until(5s, [&] { return finalOf(a, before, entry.index) && finalOf(b, before, entry.index); }))
				  << "GP13: no final SM_GROUP_LOOT after both rolls";
				drainAll(1500ms);
				const auto luckOf = [&](const ScenarioClient& client) -> std::optional<int64_t> {
					for (const SystemMessage& message : messagesOf(windowOf(client, before)))
						if (message.messageId == msg("STR_MSG_DICE_RESULT_ME") && !message.params.empty())
							return std::stoll(message.params[0]);
					return std::nullopt;
				};
				const std::optional<int64_t> luckA = luckOf(a), luckB = luckOf(b);
				ASSERT_TRUE(luckA && luckB) << "GP13: STR_MSG_DICE_RESULT_ME missing";
				EXPECT_EQ(countMessage(windowOf(a, before), msg("STR_MSG_DICE_RESULT_OTHER"), {cb.name}), 1u);
				EXPECT_EQ(countMessage(windowOf(b, before), msg("STR_MSG_DICE_RESULT_OTHER"), {ca.name}), 1u);
				const int32_t winner = *luckB > *luckA ? ids(1) : ids(0); // strictly greater wins, A rolled first and keeps a tie
				EXPECT_EQ(finalOf(a, before, entry.index)->playerId, winner) << "GP13: rolls A " << *luckA << ", B " << *luckB;
				EXPECT_EQ(finalOf(b, before, entry.index)->playerId, winner);
				ScenarioClient& won = winner == ids(0) ? a : b;
				EXPECT_FALSE(won.model.byItemId(entry.itemId).empty()) << "GP13: the winner holds no " << entry.itemId;
				EXPECT_TRUE(ofName(windowOf(c, before), "SM_GROUP_LOOT").empty()) << "GP13: C";
				std::cout << "C13: rolls A " << *luckA << ", B " << *luckB << " for " << entry.itemId << std::endl;
			} else if (rolled == 1) {
				// GP14: nobody answers; LootGroupRules.setPlayersInRoll passes for both after 17 s
				const bool passed = until(25s, [&] {
					return countMessage(windowOf(a, before), msg("STR_MSG_PAY_ALL_GIVEUP")) == 1 && countMessage(windowOf(b, before), msg("STR_MSG_PAY_ALL_GIVEUP")) == 1;
				});
				const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - requestedAt);
				EXPECT_TRUE(passed) << "GP14: no automatic pass within 25 s";
				EXPECT_GE(elapsed, 16900ms) << "GP14: the pass came early";
				EXPECT_LE(elapsed, 19500ms) << "GP14: the pass came late";
				for (ScenarioClient* member : {&a, &b})
					EXPECT_EQ(countMessage(windowOf(*member, before), msg("STR_MSG_DICE_GIVEUP_ME")), 1u) << "GP14: " << member->label;
			} else {
				for (ScenarioClient* member : {&a, &b}) {
					send(*member, GameSession::CM_GROUP_LOOT, GameSession::buildCM_GROUP_LOOT(groupId, entry.index, entry.itemId, corpse, 2, 0, 0));
					drainAll(300ms);
				}
				until(5s, [&] { return finalOf(a, before, entry.index).has_value(); });
			}
			rolled++;
			// an item nobody won is free for all: the looter takes it
			if (rolled >= 2) {
				auto again = marks();
				send(*looter, GameSession::CM_LOOT_ITEM, GameSession::buildCM_LOOT_ITEM(corpse, static_cast<uint8_t>(entry.index)));
				until(5s, [&] {
					const std::vector<Packet> window = windowOf(*looter, again);
					return !ofName(window, "SM_LOOT_ITEMLIST").empty() || !ofName(window, "SM_DELETE").empty();
				});
			}
		}
		EXPECT_GE(rolled, 2) << "C13: the corpse had fewer than two roll items (m5b3-plan.md §2.4: Potions (Rare) and an illusion godstone)";
		bool deleted = false;
		until(5s, [&] {
			for (const Packet& packet : ofName(windowOf(*looter, from), "SM_DELETE"))
				deleted = deleted || decoders::decodeDeleteObjectId(packet.data) == corpse;
			return deleted;
		});
		EXPECT_TRUE(deleted) << "C13: the corpse was not looted empty";
		if (!deleted) {
			auto again = marks();
			send(*looter, GameSession::CM_START_LOOT, GameSession::buildCM_START_LOOT(corpse, GameSession::LOOT_OPEN));
			drainAll(1500ms);
			for (const Packet& packet : windowOf(*looter, again)) {
				std::cout << "C13: after the loop " << packet.name;
				if (packet.name == "SM_LOOT_ITEMLIST")
					for (const decoders::LootItem& left : decoders::decodeLootItemList(packet.data).items)
						std::cout << " [" << left.index << "] " << left.itemId << " x" << left.count;
				std::cout << std::endl;
			}
			for (const SystemMessage& message : messagesOf(windowOf(*looter, from)))
				std::cout << "C13: message " << message.messageId << " " << join(message.params, ",") << std::endl;
		}
		walkTo(*looter, stands[indexOf(looter)][0], stands[indexOf(looter)][1], stands[indexOf(looter)][2]);
		walkTo(a, stands[0][0], stands[0][1], stands[0][2]);
	});

	runCase("C15", "the kinah split: A splits 100 over three online members, 1 kinah vanishes (GP16)", [&] {
		const std::array<int64_t, 3> before{a.model.kinah(), b.model.kinah(), c.model.kinah()};
		auto from = marks();
		send(a, GameSession::CM_GROUP_DISTRIBUTION, GameSession::buildCM_GROUP_DISTRIBUTION(100, 1));
		drainAll(2s);
		EXPECT_EQ(a.model.kinah() - before[0], -100 + 33) << "GP16: A";
		EXPECT_EQ(b.model.kinah() - before[1], 33) << "GP16: B";
		EXPECT_EQ(c.model.kinah() - before[2], 33) << "GP16: C (online, no range check)";
		EXPECT_EQ(countMessage(windowOf(a, from), msg("STR_MSG_SPLIT_ME_TO_B"), {"100", "3", "33"}), 1u);
		EXPECT_EQ(countMessage(windowOf(b, from), msg("STR_MSG_SPLIT_B_TO_ME"), {ca.name, "100", "3", "33"}), 1u);
		EXPECT_EQ(countMessage(windowOf(c, from), msg("STR_MSG_SPLIT_B_TO_ME"), {ca.name, "100", "3", "33"}), 1u);
	});

	runCase("C16", "leader change: A makes B the leader (GP17, first half)", [&] {
		auto from = marks();
		send(a, GameSession::CM_PLAYER_STATUS_INFO, GameSession::buildCM_PLAYER_STATUS_INFO(GROUP_SET_LEADER, ids(1)));
		drainAll(1500ms);
		for (ScenarioClient* member : {&a, &b, &c}) {
			const std::vector<decoders::GroupInfo> infos = groupInfosOf(windowOf(*member, from));
			ASSERT_EQ(infos.size(), 1u) << member->label;
			EXPECT_EQ(infos[0].leaderId, ids(1));
		}
		EXPECT_EQ(countMessage(windowOf(b, from), msg("STR_PARTY_YOU_BECOME_NEW_LEADER")), 1u);
		EXPECT_EQ(countMessage(windowOf(a, from), msg("STR_PARTY_HE_IS_NEW_LEADER"), {cb.name}), 1u);
		EXPECT_EQ(countMessage(windowOf(c, from), msg("STR_PARTY_HE_IS_NEW_LEADER"), {cb.name}), 1u);
	});

	runCase("C17", "kick: the leader B bans C (GP18)", [&] {
		auto from = marks();
		send(b, GameSession::CM_PLAYER_STATUS_INFO, GameSession::buildCM_PLAYER_STATUS_INFO(GROUP_BAN_MEMBER, ids(2)));
		drainAll(1500ms);
		ASSERT_EQ(ofName(windowOf(c, from), "SM_LEAVE_GROUP_MEMBER").size(), 1u);
		decoders::decodeLeaveGroupMember(ofName(windowOf(c, from), "SM_LEAVE_GROUP_MEMBER")[0].data);
		EXPECT_EQ(countMessage(windowOf(c, from), msg("STR_PARTY_YOU_ARE_BANISHED")), 1u);
		for (ScenarioClient* member : {&a, &b}) {
			EXPECT_EQ(countMemberInfo(windowOf(*member, from), ids(2), decoders::GROUP_EVENT_LEAVE), 1u) << member->label;
			EXPECT_EQ(countMessage(windowOf(*member, from), msg("STR_PARTY_HE_IS_BANISHED"), {cc.name}), 1u) << member->label;
		}
	});

	runCase("C18", "find group: D posts a recruitment, C lists it, D joins the group and C's list loses it (GP19)", [&] {
		auto from = marks();
		send(d, GameSession::CM_FIND_GROUP, GameSession::buildCM_FIND_GROUP_OFFER(ids(3), "m5g lfg", 0));
		drainAll(1500ms);
		EXPECT_EQ(countMessage(windowOf(d, from), msg("STR_PARTY_MATCH_OFFER_PARTY_POSTED")), 1u);
		const auto listed = [&](const ScenarioClient& client, const std::array<size_t, 4>& at) {
			std::optional<bool> found;
			for (const Packet& packet : ofName(windowOf(client, at), "SM_FIND_GROUP")) {
				const decoders::FindGroup list = decoders::decodeFindGroup(packet.data);
				if (list.action == 0)
					found = std::ranges::any_of(list.recruitments, [&](const decoders::FindGroupRecruitment& r) { return r.objectId == ids(3); });
			}
			return found;
		};
		EXPECT_EQ(listed(d, from), std::optional<bool>(true)) << "GP19: D's list after posting";
		from = marks();
		send(c, GameSession::CM_FIND_GROUP, GameSession::buildCM_FIND_GROUP_LIST());
		drainAll(1500ms);
		EXPECT_EQ(listed(c, from), std::optional<bool>(true)) << "GP19: C's list";
		from = marks();
		send(b, GameSession::CM_INVITE_TO_GROUP, GameSession::buildCM_INVITE_TO_GROUP(0, cd.name));
		ASSERT_TRUE(until(10s, [&] { return !ofName(windowOf(d, from), "SM_QUESTION_WINDOW").empty(); }));
		send(d, GameSession::CM_QUESTION_RESPONSE, GameSession::buildCM_QUESTION_RESPONSE(QUESTION_PARTY, GameSession::ANSWER_YES));
		ASSERT_TRUE(until(10s, [&] { return !groupInfosOf(windowOf(d, from)).empty(); }));
		drainAll(1500ms);
		bool removed = false;
		for (const Packet& packet : ofName(windowOf(c, from), "SM_FIND_GROUP")) {
			const decoders::FindGroup removal = decoders::decodeFindGroup(packet.data);
			removed = removed || (removal.action == 1 && removal.removedId == ids(3));
		}
		EXPECT_TRUE(removed) << "GP19: C was not told D's recruitment is gone (onJoinedTeam)";
		from = marks();
		send(c, GameSession::CM_FIND_GROUP, GameSession::buildCM_FIND_GROUP_LIST());
		drainAll(1500ms);
		EXPECT_EQ(listed(c, from), std::optional<bool>(false)) << "GP19: C's list still holds D";
	});

	runCase("C19", "the leader B disconnects and returns (B seeded with 1 HP while offline) (GP17)", [&] {
		auto from = marks();
		disconnect(b);
		drainAll(2s);
		int32_t newLeader = 0;
		for (ScenarioClient* member : {&a, &d}) {
			const std::vector<Packet> window = windowOf(*member, from);
			EXPECT_EQ(countMessage(window, msg("STR_PARTY_HE_BECOME_OFFLINE"), {cb.name}), 1u) << member->label;
			EXPECT_EQ(countMemberInfo(window, ids(1), decoders::GROUP_EVENT_DISCONNECTED), 1u) << member->label;
			const std::vector<decoders::GroupInfo> infos = groupInfosOf(window);
			ASSERT_FALSE(infos.empty()) << "GP17: " << member->label << " got no SM_GROUP_INFO with the new leader";
			newLeader = infos.back().leaderId;
			EXPECT_TRUE(newLeader == ids(0) || newLeader == ids(3)) << "GP17: the leader passed to " << newLeader;
		}
		database.setLifeStatHp(schema, cb.playerId, 1); // m5b-plan.md D12: after the logout store
		std::this_thread::sleep_for(1500ms);
		from = marks();
		enterAs(servers, b, cb);
		drainAll(2s);
		const std::vector<Packet> wb = windowOf(b, from);
		ASSERT_FALSE(groupInfosOf(wb).empty()) << "GP17: B's return got no SM_GROUP_INFO";
		EXPECT_EQ(groupInfosOf(wb).back().leaderId, newLeader);
		EXPECT_EQ(countMemberInfo(wb, ids(1), decoders::GROUP_EVENT_JOIN), 1u) << "GP17: B's JOIN";
		EXPECT_GE(countMemberInfo(wb, ids(0), decoders::GROUP_EVENT_ENTER), 1u);
		EXPECT_GE(countMemberInfo(wb, ids(3), decoders::GROUP_EVENT_ENTER), 1u);
		bool brand = false;
		for (const Packet& packet : ofName(wb, "SM_SHOW_BRAND"))
			for (const auto& [icon, target] : decoders::decodeShowBrand(packet.data).brands)
				brand = brand || (icon == 1 && target == brandTarget);
		EXPECT_TRUE(brand) << "GP17: CM_LEVEL_READY's sendBrands did not resend the brand";
		for (ScenarioClient* member : {&a, &d})
			EXPECT_GE(countMemberInfo(windowOf(*member, from), ids(1), decoders::GROUP_EVENT_ENTER), 1u) << member->label;
	});

	runCase("C19b", "B (1 HP) dies to a sparkie, then revives at the bind point (GP17b)", [&] {
		auto from = marks();
		// B walks into the nearest sparkie's aggro range and attacks it: one hit at 1 HP ends him
		std::optional<std::pair<int32_t, KnownNpcs::Npc>> target;
		for (const auto& [objectId, npc] : b.npcs.all())
			if (npc.templateId == SPARKIE && !npc.dead && !npc.deleted && (!target || distance2d(npc.x, npc.y, b.x, b.y) <
			                                                                             distance2d(target->second.x, target->second.y, b.x, b.y)))
				target = std::make_pair(objectId, npc);
		ASSERT_TRUE(target) << "no living sparkie known to B";
		if (const std::optional<decoders::StatUpdateHp> hp = b.lastHp())
			std::cout << "C19b: B's HP " << hp->currentHp << " / " << hp->maxHp << std::endl;
		const auto isDeathOfB = [&](const Packet& packet) {
			if (packet.name != "SM_EMOTION")
				return false;
			try {
				const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
				return emotion.emotionType == decoders::EMOTION_DIE && emotion.senderObjectId == ids(1);
			} catch (const DecodeError&) {
				return false;
			}
		};
		const auto died = [&] { return std::ranges::any_of(windowOf(b, from), isDeathOfB); };
		// the M5e X13 fight: B auto-attacks a sparkie until he or it dies (an aggressive one may kill B on the way); B, a Mage with a few HP
		// regenerated since the seed, may win a fight, then the next sparkie
		for (int32_t fight = 0; fight < 4 && !died(); fight++) {
			if (fight > 0) {
				target.reset();
				for (const auto& [objectId, npc] : b.npcs.all())
					if (npc.templateId == SPARKIE && !npc.dead && !npc.deleted &&
					    (!target || distance2d(npc.x, npc.y, b.x, b.y) < distance2d(target->second.x, target->second.y, b.x, b.y)))
						target = std::make_pair(objectId, npc);
				if (!target) {
					collectFor(*b.game, 5s);
					continue;
				}
			}
			const std::array<float, 3> melee = pointNear(target->second.x, target->second.y, target->second.z, MELEE_DISTANCE, b.x, b.y);
			walkTo(b, melee[0], melee[1], melee[2]);
			if (died())
				break;
			const int32_t npc = target->first;
			send(b, GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(npc));
			waitFor(*b.game, "SM_TARGET_SELECTED", 10s);
			const std::optional<decoders::StatsInfo> stats = b.lastStats();
			ASSERT_TRUE(stats);
			if (died())
				break;
			b.game->fightUntil(npc, std::chrono::milliseconds(stats->attackSpeed),
			                   [&](const Packet& packet) {
				                   if (isDeathOfB(packet))
					                   return true;
				                   if (packet.name != "SM_EMOTION")
					                   return false;
				                   try {
					                   const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
					                   return emotion.emotionType == decoders::EMOTION_DIE && emotion.senderObjectId == npc;
				                   } catch (const DecodeError&) {
					                   return false;
				                   }
			                   },
			                   90s, 60);
			if (!died())
				std::cout << "C19b: B survived fight " << fight + 1 << " against " << npc << std::endl;
		}
		if (!died()) {
			std::map<std::string, int32_t> names;
			for (const Packet& packet : windowOf(b, from))
				names[packet.name]++;
			for (const auto& [name, count] : names)
				std::cout << "C19b: B got " << name << " x" << count << std::endl;
			if (const std::optional<decoders::StatUpdateHp> hp = b.lastHp())
				std::cout << "C19b: B's HP now " << hp->currentHp << " / " << hp->maxHp << std::endl;
		}
		ASSERT_TRUE(until(5s, died)) << "B did not die";
		drainAll(2s);
		EXPECT_EQ(countMessage(windowOf(b, from), msg("STR_MSG_COMBAT_MY_DEATH")), 1u);
		EXPECT_EQ(countMessage(windowOf(b, from), msg("STR_MSG_COMBAT_FRIENDLY_DEATH")), 0u) << "GP17b: allExcept(victim)";
		for (ScenarioClient* member : {&a, &d})
			EXPECT_EQ(countMessage(windowOf(*member, from), msg("STR_MSG_COMBAT_FRIENDLY_DEATH"), {cb.name}), 1u) << "GP17b: " << member->label;
		EXPECT_EQ(countMessage(windowOf(c, from), msg("STR_MSG_COMBAT_FRIENDLY_DEATH")), 0u) << "GP17b: C was kicked";
		from = marks();
		send(b, GameSession::CM_REVIVE, GameSession::buildCM_REVIVE(GameSession::BIND_REVIVE));
		const auto revived = [&](const ScenarioClient& member) {
			for (const decoders::GroupMemberInfo& info : memberInfosOf(windowOf(member, from)))
				if (info.objectId == ids(1) && info.event == decoders::GROUP_EVENT_MOVEMENT && info.currentHp > 0)
					return true;
			return false;
		};
		EXPECT_TRUE(until(5s, [&] { return revived(a) && revived(d); })) << "GP17b: no MOVEMENT of the revived B";
		drainAll(3s);
	});

	runCase("C20", "the offline timeout: D quits and stays out (GP20)", [&] {
		auto from = marks();
		disconnect(d);
		const auto timedOut = [&](const ScenarioClient& member) {
			return countMessage(windowOf(member, from), msg("STR_PARTY_HE_BECOME_OFFLINE_TIMEOUT"), {cd.name}) == 1 &&
			       countMemberInfo(windowOf(member, from), ids(3), decoders::GROUP_EVENT_LEAVE) == 1;
		};
		EXPECT_TRUE(until(40s, [&] { return timedOut(a) && timedOut(b); })) << "GP20: no LEAVE_TIMEOUT for D within 40 s";
	});

	runCase("C21", "leave and disband: B leaves the group of two (GP21)", [&] {
		auto from = marks();
		send(b, GameSession::CM_PLAYER_STATUS_INFO, GameSession::buildCM_PLAYER_STATUS_INFO(GROUP_REMOVE_MEMBER, 0));
		drainAll(2s);
		EXPECT_EQ(ofName(windowOf(b, from), "SM_LEAVE_GROUP_MEMBER").size(), 1u);
		const std::vector<Packet> wa = windowOf(a, from);
		EXPECT_EQ(countMemberInfo(wa, ids(1), decoders::GROUP_EVENT_LEAVE), 1u);
		EXPECT_EQ(countMessage(wa, msg("STR_PARTY_HE_LEAVE_PARTY"), {cb.name}), 1u);
		EXPECT_EQ(countMessage(wa, msg("STR_PARTY_IS_DISPERSED")), 1u);
		EXPECT_EQ(ofName(wa, "SM_LEAVE_GROUP_MEMBER").size(), 1u);
	});

	bool liveGroupAtStop = false;
	cases.run("C22a", "a live group at the stop: A invites C, C accepts; B quits; A and C stay online", [&] {
		auto from = marks();
		send(a, GameSession::CM_INVITE_TO_GROUP, GameSession::buildCM_INVITE_TO_GROUP(0, cc.name));
		ASSERT_TRUE(until(10s, [&] { return !ofName(windowOf(c, from), "SM_QUESTION_WINDOW").empty(); }));
		send(c, GameSession::CM_QUESTION_RESPONSE, GameSession::buildCM_QUESTION_RESPONSE(QUESTION_PARTY, GameSession::ANSWER_YES));
		ASSERT_TRUE(until(10s, [&] { return !groupInfosOf(windowOf(c, from)).empty(); }));
		disconnect(b);
		liveGroupAtStop = a.game && c.game;
	});
	std::optional<int32_t> gameServerExit;
	if (servers.gameServer() != nullptr)
		gameServerExit = servers.stopGameServer();
	for (ScenarioClient* client : all)
		if (client->game)
			client->game->waitClosed(60s);
	const std::optional<int32_t> loginServerExit = servers.loginServer() != nullptr ? servers.stopLoginServer() : std::nullopt;

	cases.run("C22", "reports: the Q8 bar, the allow-list, the live counts (GP22, GP23)", [&] {
		ASSERT_TRUE(gameServerExit) << "the game server did not exit after the stop file was written";
		EXPECT_EQ(*gameServerExit, 0);
		ASSERT_TRUE(loginServerExit);
		EXPECT_TRUE(*loginServerExit == 0 || *loginServerExit == 98) << "the login server exited with " << *loginServerExit;
		EXPECT_TRUE(liveGroupAtStop) << "C22: the group of A and C was not formed for the stop";
		EXPECT_TRUE(servers.readReportLines("unported_trace.txt").empty()) << "GP22: AION_UNPORTED sites were reached:\n"
		                                                                   << join(servers.readReportLines("unported_trace.txt"), "\n");
		const std::vector<AllowlistEntry> allowlist = readAllowlist();
		ASSERT_FALSE(allowlist.empty()) << "GP22: tests/scenario/m5g_partial_allowlist.txt is empty or missing";
		std::map<std::string, int64_t> hitsByEntry;
		for (const PartialHit& hit : readPartialHits(servers)) {
			bool allowed = false;
			for (const AllowlistEntry& entry : allowlist)
				if (allowlistEntryMatches(entry.site, hit.site)) {
					allowed = true;
					hitsByEntry[entry.site] += hit.hits;
				}
			EXPECT_TRUE(allowed) << "GP22: the AION_PARTIAL site " << hit.site << " is not in tests/scenario/m5g_partial_allowlist.txt (" << hit.line << ")";
		}
		for (const AllowlistEntry& entry : allowlist) {
			if (entry.section == AllowlistSection::HitAtLeastOnce)
				EXPECT_GT(hitsByEntry[entry.site], 0) << "GP22: the section A row " << entry.site << " was never hit";
			else if (entry.section == AllowlistSection::HitNever)
				EXPECT_EQ(hitsByEntry[entry.site], 0) << "GP22: the section B row " << entry.site << " was hit";
		}
		const std::vector<std::string> census = servers.readReportLines("census.txt");
		EXPECT_TRUE(census.empty()) << "GP22: the final census reports leaks:\n" << join(census, "\n");
		EXPECT_TRUE(servers.readReportLines("lockdep.txt").empty()) << "GP22: the lock order validator reported:\n"
		                                                            << join(servers.readReportLines("lockdep.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("watchdog.txt").empty()) << "GP22: the watchdog dumped:\n"
		                                                             << join(servers.readReportLines("watchdog.txt"), "\n");
		const std::map<std::string, std::vector<std::string>> summary = servers.readSummary();
		const auto notPorted = summary.find("notPortedClientPacket");
		if (notPorted != summary.end())
			ADD_FAILURE() << "GP22: the scripted path sent client packets that are not ported: " << join(notPorted->second);
		std::vector<std::string> errors;
		ASSERT_NE(servers.gameServer(), nullptr);
		for (const std::string& line : servers.gameServer()->findLogLines(" ERROR "))
			errors.push_back("game server: " + line);
		if (servers.loginServer() != nullptr)
			for (const std::string& line : servers.loginServer()->findLogLines(" ERROR "))
				errors.push_back("login server: " + line);
		EXPECT_TRUE(errors.empty()) << "GP22: ERROR lines in the server logs:\n" << join(errors, "\n");
		EXPECT_TRUE(servers.gameServer()->findLogLines("objects removed from the world are still alive", 5).empty())
		  << "GP22: " << join(servers.gameServer()->findLogLines("objects removed from the world are still alive", 5), "\n");

		// GP23: every cut of §2.11 item 4 ran, and the group alive at the stop was disbanded by its last member's shutdown logout
		std::map<std::string, LiveCount> counts;
		for (const auto& [name, count] : readLiveCounts(servers, "live_counts.txt"))
			counts[name] = count;
		const auto row = [&](std::string_view name) -> std::optional<LiveCount> {
			const auto found = counts.find(std::string(name));
			return found == counts.end() ? std::nullopt : std::optional<LiveCount>(found->second);
		};
		for (const char* name : {"PlayerGroup", "PlayerGroupMember", "PlayerGroupInvite", "GroupRecruitment", "Player", "DropNpc"}) {
			const std::optional<LiveCount> count = row(name);
			if (!count) {
				ADD_FAILURE() << "GP23: live_counts.txt has no " << name << " row";
				continue;
			}
			EXPECT_EQ(count->live, 0) << "GP23: " << count->line;
			std::cout << "GP23: " << count->line << std::endl;
		}
		const std::optional<LiveCount> groups = row("PlayerGroup");
		if (groups)
			EXPECT_GE(groups->created, 2) << "GP23: " << groups->line;
	});

	finishRun(servers, outputDir, testName);
}

} // namespace

/** `gs.scenario.m5g` (m5g-plan.md G-01): the party cases of §10.2 this gate scripts (the header comment says which are not) */
TEST(M5gScenario, Run) {
	runM5gGate();
}

} // namespace aion::gameserver::scenario
