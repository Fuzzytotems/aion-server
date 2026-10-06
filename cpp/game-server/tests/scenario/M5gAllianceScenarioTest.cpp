// gs.scenario.m5g_alliance (m5g-plan.md §16.3 item 7, the alliance part of §10.5): the four party accounts of gs.scenario.m5g (A Warrior,
// B Mage, C Priest, D Scout at D8's levels beside Poeta's sparkies) form and run an alliance:
//   GA1  A and B form a group; A invites C (solo) to an alliance: the group is dissolved, all three join group 1000 of a new alliance
//   GA2  A moves B to alliance group 1001 (CM_PLAYER_STATUS_INFO 27): MEMBER_GROUP_CHANGE to everyone
//   GA3  A makes C a vice captain (25); C invites D, who joins (a vice captain may invite)
//   GA4  the ready check: A starts (21), B ready (23), C not ready (24), D ready: the SM_ALLIANCE_READY_CHECK sequence to every member
//   GA5  B casts Focused Evasion: UPDATE_EFFECTS to the others, never to B; A's group data (groupType 1) reaches A's alliance group only
//        (the alliance chat row of §10.5 GA5 waits for CM_CHAT_MESSAGE_PUBLIC, lane A's)
//   GA6  the round robin without quality rolls; A kills a sparkie with all four in range: each gets the oracle's alliance share
//   GA7  C and D leave; C forms alliance 2 with D; A invites C to a league (CM_INVITE_TO_GROUP 28): both alliances see the league of two,
//        positions 0 and 1, and the league's loot rules FREEFORALL 0 0 2 2 2 2 2
//   GA8  A moves the alliances (31) and back: the position messages; A makes B his alliance's leader (17): the other alliance is told
//   GA9  B, now the league captain, expels alliance 2 (30): LEAGUE_EXPELLED to it, LEAGUE_EXPEL and the league of one's LEAGUE_DISPERSED to his
//   GA10 B quits and stays out: LEAVE_TIMEOUT within 40 s disbands alliance 1; C leaves alliance 2, which disbands
//   GA11 A invites D (a live alliance at the stop); the reports: the Q8 bar, the allow-list, the alliance and league classes at 0 live
// The group-instance cases GA12-GA14 are stage 3's (m5g-plan.md §16.3).
//
// Built from M5gScenarioTest.cpp's scaffolding (the helpers, the oracle case C0, the characters of C1) and its kill and loot helpers; every
// expectation is derived from the Java sources named at each case or from the oracle (m5g-team), never from the C++ port.

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
#include "decoders/AllianceDecoders.h"
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

void runM5gAllianceGate() {
	const std::string testName = "gs.scenario.m5g_alliance";
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
	const std::filesystem::path outputDir = std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "m5g_alliance";
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
	config.schemaPrefix = "m5ga";
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
	  "STR_MSG_GET_ITEM_PARTYNOTICE", "STR_MSG_DICE_RESULT_ME", "STR_MSG_DICE_RESULT_OTHER", "STR_MSG_DICE_GIVEUP_ME", "STR_MSG_PAY_ALL_GIVEUP",
	  "STR_FORCE_INVITED_HIM", "STR_FORCE_ENTERED_FORCE", "STR_FORCE_HE_ENTERED_FORCE", "STR_FORCE_LEAVE_HIM", "STR_FORCE_HE_BECOME_OFFLINE",
	  "STR_PARTY_ALLIANCE_HE_LEAVED_PARTY_OFFLINE_TIMEOUT", "STR_PARTY_ALLIANCE_DISPERSED", "STR_UNION_INVITE_HIM", "STR_UNION_CHANGE_FORCE_NUMBER_ME",
	  "STR_UNION_CHANGE_FORCE_NUMBER_HIM", "STR_UNION_CHANGE_LEADER_TIMEOUT", "STR_UNION_YOU_BECOME_NEW_LEADER_TIMEOUT"};
	runCase("C0", "the oracles answer: m5g-team (D8's levels, the shares, the constants), m5a-creation, m5b-monster", [&] {
		std::vector<std::string> arguments{"m5g-team", "--npc-id", std::to_string(SPARKIE), "--xp-group-rate", "1.5", "--xp-solo-rate", "1.0", "--question",
		                                   "STR_PARTY_DO_YOU_ACCEPT_INVITATION", "STR_PARTY_ALLIANCE_DO_YOU_ACCEPT_HIS_INVITATION", "STR_MSGBOX_UNION_INVITE_ME", "--message"};
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
		for (ScenarioClient* member : all)
			if (member != &looter && distance2d(member->x, member->y, corpseAt[0], corpseAt[1]) < team.groupMaxDistance)
				EXPECT_EQ(countMessage(windowOf(*member, from), msg("STR_MSG_GET_ITEM_PARTYNOTICE")), items)
				  << "GP12: " << member->label << " for " << looter.label << "'s " << items << " items";
		// GP15: the corpse's kinah is split over the in-range members, nothing lost (distributeEqually)
		if (kinahListed > 0) {
			int64_t gained = 0;
			for (size_t i = 0; i < all.size(); i++)
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

	// ---- the alliance cases ----
	int32_t questionAlliance = 0;
	int32_t allianceId = 0;
	const auto command = [&](const char* name) { return static_cast<uint8_t>(team.commands.at(name)); };
	const auto allianceInfosOf = [&](const std::vector<Packet>& window) {
		std::vector<decoders::AllianceInfo> found;
		for (const Packet& packet : ofName(window, "SM_ALLIANCE_INFO"))
			found.push_back(decoders::decodeAllianceInfo(packet.data));
		return found;
	};
	const auto allianceMembersOf = [&](const std::vector<Packet>& window) {
		std::vector<decoders::AllianceMemberInfo> found;
		for (const Packet& packet : ofName(window, "SM_ALLIANCE_MEMBER_INFO"))
			found.push_back(decoders::decodeAllianceMemberInfo(packet.data));
		return found;
	};
	const auto readyChecksOf = [&](const std::vector<Packet>& window) {
		std::vector<std::pair<int32_t, int32_t>> found;
		for (const Packet& packet : ofName(window, "SM_ALLIANCE_READY_CHECK")) {
			const decoders::AllianceReadyCheck check = decoders::decodeAllianceReadyCheck(packet.data);
			found.emplace_back(check.playerObjectId, check.statusCode);
		}
		return found;
	};
	/** whether `client` was told (JOIN, ENTER, ENTER_OFFLINE or the group change) that member `objectId` is in alliance group `groupId` */
	const auto knowsMemberIn = [&](const ScenarioClient& client, const std::array<size_t, 4>& from, int32_t objectId, int32_t groupId) {
		for (const decoders::AllianceMemberInfo& info : allianceMembersOf(windowOf(client, from)))
			if (info.objectId == objectId && info.allianceGroupId == groupId)
				return true;
		return false;
	};
	/** `inviter` invites `invited` to the alliance (CM_INVITE_TO_GROUP 12, CM_INVITE_TO_GROUP.java:61-63) and the invited answers yes */
	const auto inviteToAlliance = [&](ScenarioClient& inviter, ScenarioClient& invited, const Character& invitedCharacter) {
		auto from = marks();
		send(inviter, GameSession::CM_INVITE_TO_GROUP, GameSession::buildCM_INVITE_TO_GROUP(12, invitedCharacter.name));
		if (!until(10s, [&] { return !ofName(windowOf(invited, from), "SM_QUESTION_WINDOW").empty(); }))
			throw std::runtime_error(invited.label + " was not asked to join the alliance");
		const decoders::QuestionWindow question = decoders::decodeQuestionWindow(ofName(windowOf(invited, from), "SM_QUESTION_WINDOW")[0].data);
		EXPECT_EQ(question.code, questionAlliance);
		send(invited, GameSession::CM_QUESTION_RESPONSE, GameSession::buildCM_QUESTION_RESPONSE(questionAlliance, GameSession::ANSWER_YES));
		if (!until(10s, [&] { return !allianceInfosOf(windowOf(invited, from)).empty(); }))
			throw std::runtime_error(invited.label + " got no SM_ALLIANCE_INFO after accepting");
		drainAll(1500ms);
		return from;
	};

	runCase("GA0", "A and B form a group (the parties gate's C3)", [&] {
		questionAlliance = team.questions.at("STR_PARTY_ALLIANCE_DO_YOU_ACCEPT_HIS_INVITATION");
		const auto from = marks();
		send(a, GameSession::CM_INVITE_TO_GROUP, GameSession::buildCM_INVITE_TO_GROUP(0, cb.name));
		ASSERT_TRUE(until(10s, [&] { return !ofName(windowOf(b, from), "SM_QUESTION_WINDOW").empty(); }));
		send(b, GameSession::CM_QUESTION_RESPONSE, GameSession::buildCM_QUESTION_RESPONSE(QUESTION_PARTY, GameSession::ANSWER_YES));
		ASSERT_TRUE(until(10s, [&] { return !groupInfosOf(windowOf(b, from)).empty(); }));
		drainAll(1s);
	});

	runCase("GA1", "A (in a group with B) invites C to an alliance: the group dissolves, all three join group 1000 (PlayerAllianceInvite.java:28-66)",
	  [&] {
		  const auto from = inviteToAlliance(a, c, cc);
		  EXPECT_EQ(countMessage(windowOf(a, from), msg("STR_FORCE_INVITED_HIM"), {cc.name}), 1u);
		  for (ScenarioClient* member : {&a, &b})
			  EXPECT_GE(ofName(windowOf(*member, from), "SM_LEAVE_GROUP_MEMBER").size(), 1u) << "GA1: " << member->label << "'s group was not dissolved";
		  for (ScenarioClient* member : {&a, &b, &c}) {
			  const std::vector<decoders::AllianceInfo> infos = allianceInfosOf(windowOf(*member, from));
			  ASSERT_FALSE(infos.empty()) << "GA1: " << member->label << " got no SM_ALLIANCE_INFO";
			  const decoders::AllianceInfo& last = infos.back();
			  EXPECT_EQ(last.groupSize, 4) << member->label;
			  EXPECT_EQ(last.leaderId, ids(0)) << member->label;
			  EXPECT_TRUE(last.viceCaptains.empty()) << member->label;
			  EXPECT_EQ(last.lootWords, team.lootDefaults) << member->label;
			  EXPECT_EQ(last.leagueAlliances, 0) << member->label;
			  if (allianceId == 0)
				  allianceId = last.allianceId;
			  EXPECT_EQ(last.allianceId, allianceId) << member->label;
			  for (size_t m : {0u, 1u, 2u})
				  EXPECT_TRUE(knowsMemberIn(*member, from, ids(m), 1000)) << "GA1: " << member->label << " was not told " << labels[m] << " is in group 1000";
		  }
		  EXPECT_EQ(countMessage(windowOf(c, from), msg("STR_FORCE_ENTERED_FORCE")), 1u);
		  EXPECT_TRUE(allianceInfosOf(windowOf(d, from)).empty()) << "GA1: D is no member";
	  });

	runCase("GA2", "A moves B to alliance group 1001: MEMBER_GROUP_CHANGE to every member (ChangeMemberGroupEvent.java:46-53)", [&] {
		const auto from = marks();
		send(a, GameSession::CM_PLAYER_STATUS_INFO, GameSession::buildCM_PLAYER_STATUS_INFO(command("ALLIANCE_CHANGE_GROUP"), ids(1), 1001, 0));
		drainAll(2s);
		for (ScenarioClient* member : {&a, &b, &c}) {
			bool changed = false;
			for (const decoders::AllianceMemberInfo& info : allianceMembersOf(windowOf(*member, from)))
				changed = changed || (info.objectId == ids(1) && info.groupChange && info.allianceGroupId == 1001);
			EXPECT_TRUE(changed) << "GA2: " << member->label << " got no MEMBER_GROUP_CHANGE of B to 1001";
		}
	});

	runCase("GA3", "A makes C a vice captain; C invites D, who joins (AssignViceCaptainEvent.java:34-68, canInviteToTeam's isViceCaptain)", [&] {
		auto from = marks();
		send(a, GameSession::CM_PLAYER_STATUS_INFO, GameSession::buildCM_PLAYER_STATUS_INFO(command("ALLIANCE_SET_VICECAPTAIN"), ids(2)));
		drainAll(2s);
		for (ScenarioClient* member : {&a, &b, &c}) {
			const std::vector<decoders::AllianceInfo> infos = allianceInfosOf(windowOf(*member, from));
			ASSERT_FALSE(infos.empty()) << "GA3: " << member->label;
			EXPECT_EQ(infos.back().viceCaptains, std::vector<int32_t>{ids(2)}) << member->label;
			EXPECT_EQ(infos.back().messageId, 1300984) << "SM_ALLIANCE_INFO.VICECAPTAIN_PROMOTE"; // SM_ALLIANCE_INFO.java's constant
			EXPECT_EQ(infos.back().message, cc.name);
		}
		from = inviteToAlliance(c, d, cd);
		for (ScenarioClient* member : {&a, &b, &c, &d})
			EXPECT_TRUE(knowsMemberIn(*member, from, ids(3), 1000)) << "GA3: " << member->label << " was not told D joined group 1000";
		for (ScenarioClient* member : {&a, &b, &c})
			EXPECT_EQ(countMessage(windowOf(*member, from), msg("STR_FORCE_HE_ENTERED_FORCE"), {cd.name}), 1u) << member->label;
	});

	runCase("GA4", "the ready check: A starts, B ready, C not ready, D ready (CheckAllianceReadyEvent.java:27-70)", [&] {
		const auto from = marks();
		send(a, GameSession::CM_PLAYER_STATUS_INFO, GameSession::buildCM_PLAYER_STATUS_INFO(command("ALLIANCE_CHECKREADY_START"), 0));
		drainAll(1s);
		send(b, GameSession::CM_PLAYER_STATUS_INFO, GameSession::buildCM_PLAYER_STATUS_INFO(command("ALLIANCE_CHECKREADY_READY"), 0));
		drainAll(1s);
		send(c, GameSession::CM_PLAYER_STATUS_INFO, GameSession::buildCM_PLAYER_STATUS_INFO(command("ALLIANCE_CHECKREADY_NOTREADY"), 0));
		drainAll(1s);
		send(d, GameSession::CM_PLAYER_STATUS_INFO, GameSession::buildCM_PLAYER_STATUS_INFO(command("ALLIANCE_CHECKREADY_READY"), 0));
		drainAll(1500ms);
		// START: readyStatus = 4 online - 1 = 3; each answer counts down; the third answer reaches 0 and adds (0, 3)
		const std::vector<std::pair<int32_t, int32_t>> expected{{ids(0), 5}, {ids(0), 1}, {ids(1), 5}, {ids(2), 4}, {ids(3), 5}, {0, 3}};
		for (ScenarioClient* member : all)
			EXPECT_EQ(readyChecksOf(windowOf(*member, from)), expected) << "GA4: " << member->label;
	});

	runCase("GA5", "B casts Focused Evasion: UPDATE_EFFECTS to the others only; A's group data reaches A's alliance group only", [&] {
		auto from = marks();
		GameSession::CastRequest request;
		request.spellId = FOCUSED_EVASION;
		request.level = 1;
		request.targetType = decoders::CAST_TARGET_OBJECT;
		request.targetObjectId = ids(1);
		const GameSession::CastOutcome outcome = b.game->castAndWait(ids(1), request, 6s);
		ASSERT_TRUE(outcome.castSpellResult) << "GA5: B's cast did not complete";
		drainAll(2s);
		const auto buffed = [&](const ScenarioClient& member) {
			for (const decoders::AllianceMemberInfo& info : allianceMembersOf(windowOf(member, from)))
				if (info.objectId == ids(1) && info.event == decoders::ALLIANCE_EVENT_UPDATE_EFFECTS &&
				    std::ranges::any_of(info.effects, [](const decoders::GroupMemberEffect& e) { return e.skillId == FOCUSED_EVASION; }))
					return true;
			return false;
		};
		for (ScenarioClient* member : {&a, &c, &d})
			EXPECT_TRUE(buffed(*member)) << "GA5: " << member->label << " got no UPDATE_EFFECTS of B with " << FOCUSED_EVASION;
		EXPECT_TRUE(std::ranges::none_of(allianceMembersOf(windowOf(b, from)), [&](const decoders::AllianceMemberInfo& i) { return i.objectId == ids(1); }))
		  << "GA5: B received member info about himself (PlayerAllianceUpdateEvent's allExcept)";
		// CM_GROUP_DATA_EXCHANGE action 0, groupType 1: the other online members of A's alliance group (1000: A, C, D; B is in 1001)
		const std::vector<uint8_t> data{1, 2, 3, 4, 5, 6, 7, 8};
		from = marks();
		send(a, GameSession::CM_GROUP_DATA_EXCHANGE, GameSession::buildCM_GROUP_DATA_EXCHANGE(0, 1, 0, data));
		drainAll(1500ms);
		for (ScenarioClient* member : {&c, &d})
			EXPECT_EQ(ofName(windowOf(*member, from), "SM_GROUP_DATA_EXCHANGE").size(), 1u) << "GA5: " << member->label;
		for (ScenarioClient* member : {&a, &b})
			EXPECT_TRUE(ofName(windowOf(*member, from), "SM_GROUP_DATA_EXCHANGE").empty()) << "GA5: " << member->label;
	});

	runCase("GA6", "the round robin without quality rolls; A kills a sparkie with all four in range: the oracle's alliance shares", [&] {
		std::vector<std::string> arguments{"m5g-team", "--npc-id", std::to_string(SPARKIE), "--xp-group-rate", "1.5", "--xp-solo-rate", "1.0", "--levels",
		                                   std::to_string(team.levels[0]), std::to_string(team.levels[1]), std::to_string(team.levels[2]),
		                                   std::to_string(team.levels[0]), "--kill", "1111"};
		const TeamAnswer four = parseTeam(oracle->run(arguments));
		ASSERT_EQ(four.awarded.size(), 1u);
		send(a, GameSession::CM_DISTRIBUTION_SETTINGS, GameSession::buildCM_DISTRIBUTION_SETTINGS(1, 0, {0, 0, 0, 0, 0, 0}));
		drainAll(1500ms);
		const TeamKill kill = teamKill(-1, {true, true, true});
		for (size_t m = 0; m < all.size(); m++) {
			const std::optional<int64_t> expected = four.awarded[0][m];
			ASSERT_TRUE(expected) << "m5g-team counts no share of member " << m;
			EXPECT_EQ(expGains(windowOf(*all[m], kill.from)), std::vector<int64_t>{*expected}) << "GA6: " << all[m]->label << "'s experience";
		}
		ScenarioClient* looter = looterOf(kill);
		ASSERT_NE(looter, nullptr) << "GA6: LOOT_ENABLE went to " << join(kill.enabledFor);
		lootEmpty(*looter, kill, indexOf(looter));
		walkTo(a, stands[0][0], stands[0][1], stands[0][2]);
	});

	// SM_ALLIANCE_INFO's league message ids (SM_ALLIANCE_INFO.java's constants)
	constexpr int32_t LEAGUE_ALLIANCE_ENTERED = 1400560, LEAGUE_JOINED_ALLIANCE = 1400561, LEAGUE_EXPEL = 1400574, LEAGUE_EXPELLED = 1400576,
	                  LEAGUE_DISPERSED = 1400579;
	int32_t allianceOne = 0, allianceTwo = 0;
	const auto lastAllianceInfo = [&](const ScenarioClient& client, const std::array<size_t, 4>& from) -> std::optional<decoders::AllianceInfo> {
		const std::vector<decoders::AllianceInfo> infos = allianceInfosOf(windowOf(client, from));
		return infos.empty() ? std::nullopt : std::optional<decoders::AllianceInfo>(infos.back());
	};
	const auto hasInfoMessage = [&](const ScenarioClient& client, const std::array<size_t, 4>& from, int32_t messageId, const std::string& text) {
		for (const decoders::AllianceInfo& info : allianceInfosOf(windowOf(client, from)))
			if (info.messageId == messageId && info.message == text)
				return true;
		return false;
	};

	runCase("GA7", "C and D leave; C forms alliance 2 with D; A invites C to a league (LeagueService.java:30-93, LeagueJoinEvent)", [&] {
		auto from = marks();
		send(c, GameSession::CM_PLAYER_STATUS_INFO, GameSession::buildCM_PLAYER_STATUS_INFO(command("ALLIANCE_LEAVE"), 0));
		drainAll(1500ms);
		send(d, GameSession::CM_PLAYER_STATUS_INFO, GameSession::buildCM_PLAYER_STATUS_INFO(command("ALLIANCE_LEAVE"), 0));
		drainAll(1500ms);
		EXPECT_EQ(countMessage(windowOf(a, from), msg("STR_FORCE_LEAVE_HIM"), {cc.name}), 1u);
		EXPECT_EQ(countMessage(windowOf(a, from), msg("STR_FORCE_LEAVE_HIM"), {cd.name}), 1u);
		inviteToAlliance(c, d, cd);
		from = marks();
		send(a, GameSession::CM_INVITE_TO_GROUP, GameSession::buildCM_INVITE_TO_GROUP(28, cc.name));
		ASSERT_TRUE(until(10s, [&] { return !ofName(windowOf(c, from), "SM_QUESTION_WINDOW").empty(); })) << "GA7: C was not asked";
		const int32_t questionLeague = team.questions.at("STR_MSGBOX_UNION_INVITE_ME");
		EXPECT_EQ(decoders::decodeQuestionWindow(ofName(windowOf(c, from), "SM_QUESTION_WINDOW")[0].data).code, questionLeague);
		EXPECT_EQ(countMessage(windowOf(a, from), msg("STR_UNION_INVITE_HIM"), {cc.name, "2"}), 1u);
		send(c, GameSession::CM_QUESTION_RESPONSE, GameSession::buildCM_QUESTION_RESPONSE(questionLeague, GameSession::ANSWER_YES));
		drainAll(2s);
		const std::optional<decoders::AllianceInfo> infoA = lastAllianceInfo(a, from), infoC = lastAllianceInfo(c, from);
		ASSERT_TRUE(infoA && infoC) << "GA7: no SM_ALLIANCE_INFO after the league was formed";
		allianceOne = infoA->allianceId;
		allianceTwo = infoC->allianceId;
		for (ScenarioClient* member : all) {
			const std::optional<decoders::AllianceInfo> info = lastAllianceInfo(*member, from);
			ASSERT_TRUE(info) << "GA7: " << member->label;
			EXPECT_EQ(info->leagueAlliances, 2) << "GA7: " << member->label;
			EXPECT_NE(info->leagueId, 0);
			EXPECT_EQ(info->leagueId, infoA->leagueId) << member->label;
			EXPECT_EQ(info->lootWords, (std::vector<int32_t>{0, 0, 0, 2, 2, 2, 2, 2})) << "GA7: the league's rules (LeagueService.java:88), " << member->label;
			for (const decoders::LeagueAllianceInfo& alliance : info->league)
				EXPECT_EQ(alliance.position, alliance.allianceObjectId == allianceOne ? 0 : 1) << member->label;
		}
		for (ScenarioClient* member : {&a, &b})
			EXPECT_TRUE(hasInfoMessage(*member, from, LEAGUE_JOINED_ALLIANCE, cc.name)) << "GA7: " << member->label;
		for (ScenarioClient* member : {&c, &d})
			EXPECT_TRUE(hasInfoMessage(*member, from, LEAGUE_ALLIANCE_ENTERED, ca.name)) << "GA7: " << member->label;
	});

	runCase("GA8", "A moves the alliances and back (LeagueMoveEvent); A makes B his alliance's leader: the league is told (ChangeAllianceLeaderEvent:51-75)",
	  [&] {
		  auto from = marks();
		  send(a, GameSession::CM_PLAYER_STATUS_INFO, GameSession::buildCM_PLAYER_STATUS_INFO(command("LEAGUE_ALLIANCE_MOVE"), allianceOne, allianceTwo));
		  drainAll(2s);
		  for (ScenarioClient* member : {&a, &b}) {
			  EXPECT_EQ(countMessage(windowOf(*member, from), msg("STR_UNION_CHANGE_FORCE_NUMBER_ME"), {"1"}), 1u) << "GA8: " << member->label;
			  EXPECT_EQ(countMessage(windowOf(*member, from), msg("STR_UNION_CHANGE_FORCE_NUMBER_HIM"), {cc.name, "0"}), 1u) << member->label;
		  }
		  for (ScenarioClient* member : {&c, &d}) {
			  EXPECT_EQ(countMessage(windowOf(*member, from), msg("STR_UNION_CHANGE_FORCE_NUMBER_ME"), {"0"}), 1u) << "GA8: " << member->label;
			  EXPECT_EQ(countMessage(windowOf(*member, from), msg("STR_UNION_CHANGE_FORCE_NUMBER_HIM"), {ca.name, "1"}), 1u) << member->label;
		  }
		  // back: a leader's alliance off position 0 makes a later reorganize call changeLeader on the leader, which throws (Java too)
		  send(a, GameSession::CM_PLAYER_STATUS_INFO, GameSession::buildCM_PLAYER_STATUS_INFO(command("LEAGUE_ALLIANCE_MOVE"), allianceOne, allianceTwo));
		  drainAll(1500ms);
		  from = marks();
		  send(a, GameSession::CM_PLAYER_STATUS_INFO, GameSession::buildCM_PLAYER_STATUS_INFO(command("ALLIANCE_SET_CAPTAIN"), ids(1)));
		  drainAll(2s);
		  for (ScenarioClient* member : {&a, &c, &d})
			  EXPECT_EQ(countMessage(windowOf(*member, from), msg("STR_UNION_CHANGE_LEADER_TIMEOUT"), {cb.name}), 1u) << "GA8: " << member->label;
		  EXPECT_EQ(countMessage(windowOf(b, from), msg("STR_UNION_CHANGE_LEADER_TIMEOUT")), 0u) << "GA8: allExcept(B)";
		  EXPECT_EQ(countMessage(windowOf(b, from), msg("STR_UNION_YOU_BECOME_NEW_LEADER_TIMEOUT")), 1u);
	  });

	runCase("GA9", "B, the league captain, expels alliance 2: LEAGUE_EXPELLED; the league of one disbands (LeagueLeftEvent.java)", [&] {
		const auto from = marks();
		send(b, GameSession::CM_PLAYER_STATUS_INFO, GameSession::buildCM_PLAYER_STATUS_INFO(command("LEAGUE_EXPEL"), allianceTwo));
		drainAll(2s);
		for (ScenarioClient* member : {&c, &d})
			EXPECT_TRUE(hasInfoMessage(*member, from, LEAGUE_EXPELLED, cb.name)) << "GA9: " << member->label;
		for (ScenarioClient* member : {&a, &b}) {
			EXPECT_TRUE(hasInfoMessage(*member, from, LEAGUE_EXPEL, cc.name)) << "GA9: " << member->label;
			EXPECT_TRUE(hasInfoMessage(*member, from, LEAGUE_DISPERSED, "")) << "GA9: the league of one, " << member->label;
		}
		for (ScenarioClient* member : all) {
			const std::optional<decoders::AllianceInfo> info = lastAllianceInfo(*member, from);
			ASSERT_TRUE(info) << member->label;
			EXPECT_EQ(info->leagueAlliances, 0) << "GA9: " << member->label << " still sees a league";
		}
	});

	runCase("GA10", "B quits and stays out: LEAVE_TIMEOUT disbands alliance 1; C leaves alliance 2, which disbands (PlayerAllianceLeavedEvent.java)", [&] {
		auto from = marks();
		disconnect(b);
		EXPECT_TRUE(until(40s, [&] {
			return countMessage(windowOf(a, from), msg("STR_PARTY_ALLIANCE_HE_LEAVED_PARTY_OFFLINE_TIMEOUT"), {cb.name}) == 1;
		})) << "GA10: no LEAVE_TIMEOUT for B within 40 s";
		drainAll(1s);
		EXPECT_EQ(countMessage(windowOf(a, from), msg("STR_FORCE_HE_BECOME_OFFLINE"), {cb.name}), 1u);
		EXPECT_EQ(countMessage(windowOf(a, from), msg("STR_PARTY_ALLIANCE_DISPERSED")), 1u) << "GA10: alliance 1 of one was not disbanded";
		from = marks();
		send(c, GameSession::CM_PLAYER_STATUS_INFO, GameSession::buildCM_PLAYER_STATUS_INFO(command("ALLIANCE_LEAVE"), 0));
		drainAll(2s);
		EXPECT_EQ(countMessage(windowOf(d, from), msg("STR_FORCE_LEAVE_HIM"), {cc.name}), 1u);
		EXPECT_EQ(countMessage(windowOf(d, from), msg("STR_PARTY_ALLIANCE_DISPERSED")), 1u) << "GA10: alliance 2 of one was not disbanded";
	});

	bool liveAllianceAtStop = false;
	cases.run("GA11a", "a live alliance at the stop: A invites D again; A and D stay online", [&] {
		inviteToAlliance(a, d, cd);
		liveAllianceAtStop = a.game && d.game;
	});

	std::optional<int32_t> gameServerExit;
	if (servers.gameServer() != nullptr)
		gameServerExit = servers.stopGameServer();
	for (ScenarioClient* client : all)
		if (client->game)
			client->game->waitClosed(60s);
	const std::optional<int32_t> loginServerExit = servers.loginServer() != nullptr ? servers.stopLoginServer() : std::nullopt;

	cases.run("GA11", "reports: the Q8 bar, the allow-list, the live counts", [&] {
		ASSERT_TRUE(gameServerExit) << "the game server did not exit after the stop file was written";
		EXPECT_EQ(*gameServerExit, 0);
		ASSERT_TRUE(loginServerExit);
		EXPECT_TRUE(*loginServerExit == 0 || *loginServerExit == 98) << "the login server exited with " << *loginServerExit;
		EXPECT_TRUE(liveAllianceAtStop) << "GA11: the alliance of A and D was not formed for the stop";
		EXPECT_TRUE(servers.readReportLines("unported_trace.txt").empty()) << "GA11: AION_UNPORTED sites were reached:\n"
		                                                                   << join(servers.readReportLines("unported_trace.txt"), "\n");
		const std::vector<AllowlistEntry> allowlist = readAllowlist();
		ASSERT_FALSE(allowlist.empty()) << "GA11: tests/scenario/m5g_partial_allowlist.txt is empty or missing";
		std::map<std::string, int64_t> hitsByEntry;
		for (const PartialHit& hit : readPartialHits(servers)) {
			bool allowed = false;
			for (const AllowlistEntry& entry : allowlist)
				if (allowlistEntryMatches(entry.site, hit.site)) {
					allowed = true;
					hitsByEntry[entry.site] += hit.hits;
				}
			EXPECT_TRUE(allowed) << "GA11: the AION_PARTIAL site " << hit.site << " is not in tests/scenario/m5g_partial_allowlist.txt (" << hit.line << ")";
		}
		for (const AllowlistEntry& entry : allowlist) {
			if (entry.section == AllowlistSection::HitAtLeastOnce)
				EXPECT_GT(hitsByEntry[entry.site], 0) << "GA11: the section A row " << entry.site << " was never hit";
			else if (entry.section == AllowlistSection::HitNever)
				EXPECT_EQ(hitsByEntry[entry.site], 0) << "GA11: the section B row " << entry.site << " was hit";
		}
		const std::vector<std::string> census = servers.readReportLines("census.txt");
		EXPECT_TRUE(census.empty()) << "GA11: the final census reports leaks:\n" << join(census, "\n");
		EXPECT_TRUE(servers.readReportLines("lockdep.txt").empty()) << "GA11: the lock order validator reported:\n"
		                                                            << join(servers.readReportLines("lockdep.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("watchdog.txt").empty()) << "GA11: the watchdog dumped:\n"
		                                                             << join(servers.readReportLines("watchdog.txt"), "\n");
		const std::map<std::string, std::vector<std::string>> summary = servers.readSummary();
		const auto notPorted = summary.find("notPortedClientPacket");
		if (notPorted != summary.end())
			ADD_FAILURE() << "GA11: the scripted path sent client packets that are not ported: " << join(notPorted->second);
		std::vector<std::string> errors;
		ASSERT_NE(servers.gameServer(), nullptr);
		for (const std::string& line : servers.gameServer()->findLogLines(" ERROR "))
			errors.push_back("game server: " + line);
		if (servers.loginServer() != nullptr)
			for (const std::string& line : servers.loginServer()->findLogLines(" ERROR "))
				errors.push_back("login server: " + line);
		EXPECT_TRUE(errors.empty()) << "GA11: ERROR lines in the server logs:\n" << join(errors, "\n");
		EXPECT_TRUE(servers.gameServer()->findLogLines("objects removed from the world are still alive", 5).empty())
		  << "GA11: " << join(servers.gameServer()->findLogLines("objects removed from the world are still alive", 5), "\n");

		// GA11: the disband breaker and the last-leave breaker ran, and the alliance alive at the stop was disbanded by its last member's logout
		std::map<std::string, LiveCount> counts;
		for (const auto& [name, count] : readLiveCounts(servers, "live_counts.txt"))
			counts[name] = count;
		const auto row = [&](std::string_view name) -> std::optional<LiveCount> {
			const auto found = counts.find(std::string(name));
			return found == counts.end() ? std::nullopt : std::optional<LiveCount>(found->second);
		};
		for (const char* name : {"PlayerAlliance", "PlayerAllianceGroup", "PlayerAllianceMember", "PlayerAllianceInvite", "League", "LeagueMember",
		                         "LeagueInviteEvent", "PlayerGroup", "PlayerGroupMember", "Player", "DropNpc"}) {
			const std::optional<LiveCount> count = row(name);
			if (!count) {
				ADD_FAILURE() << "GA11: live_counts.txt has no " << name << " row";
				continue;
			}
			EXPECT_EQ(count->live, 0) << "GA11: " << count->line;
			std::cout << "GA11: " << count->line << std::endl;
		}
		const std::optional<LiveCount> alliances = row("PlayerAlliance");
		if (alliances)
			EXPECT_GE(alliances->created, 2) << "GA11: " << alliances->line;
		const std::optional<LiveCount> leagues = row("League");
		if (leagues)
			EXPECT_GE(leagues->created, 1) << "GA11: " << leagues->line;
		const std::optional<LiveCount> allianceGroups = row("PlayerAllianceGroup");
		if (allianceGroups)
			EXPECT_GE(allianceGroups->created, 8) << "GA11: four groups per alliance: " << allianceGroups->line;
	});

	finishRun(servers, outputDir, testName);
}

} // namespace

/** `gs.scenario.m5g_alliance` (m5g-plan.md §16.3 item 7): the alliance cases of §10.5 this gate scripts (the header comment says which) */
TEST(M5gAllianceScenario, Run) {
	runM5gAllianceGate();
}

} // namespace aion::gameserver::scenario
