// The M5i siege gate gs.scenario.m5i (m5i-plan.md 10, checkpoint I-6, the siege half): one login server and one game server as child processes
// on their own test schemas, sieges on (M5iGateProfile.h), and two accounts online at once in Reshanta - W, an Asmodian watcher at Primum
// Landing, and G, an Elyos GM (access level 9) at Teminon Landing, who drives the sieges with //siege (the admin-command clock of D2):
//   C0 the oracle (m5i-sieges: the 70 locations of SiegeLocationData; the artifact message ids); S-0 the servers start (initSieges); C1 the
//   characters; C2 the login state (X1-X3: SM_INFLUENCE_RATIO, SM_SIEGE_LOCATION_INFO with every location, SM_AFTER_SIEGE_LOCINFO_475,
//   SM_RIFT_ANNOUNCE) and the siege world (X4: SM_SHIELD_EFFECT, SM_ABYSS_ARTIFACT_INFO3); C3 captures without a siege (X5); C4 a siege start
//   with its shield (X6); C7 a defended siege (X10); C8 an artifact captured twice, the siege restarted in between (X11); C14 persistence across
//   a game server restart (X18); C15 the reports.
// Not gated here (m5i-plan.md 10.2, later checkpoints): the positions of S1-S7 and the cases that need them (the relocation at login C5b, the
// shield crossing C5, a capture during a siege C6, the artifact cooldown C9), the world raid C10 and the rifts C11 (I-11), the bases C12 and the
// Balaur assault C13 (stage 2), the event buff X13 (I-11).
//
// Every expectation is independent of the C++ server code: server packets are read by name with the decoders of tests/scenario/decoders, and
// every id comes from `tools/oracle/oracle.py` (m5i-sieges, m5h-housing --message) or from the Java method an assertion is about. The scaffolding
// is M5h2ScenarioTest.cpp's (scratch assembly), never an assertion.

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
#include "M5iGateProfile.h"
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
#include "decoders/SiegeDecoders.h"
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
	Body& Q(int64_t value) {
		for (int i = 0; i < 8; i++)
			data.push_back(static_cast<uint8_t>(static_cast<uint64_t>(value) >> (8 * i)));
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


// ---- the siege gate (m5i-plan.md 10, I-6) -------------------------------------------------------------------------------------------------

constexpr int32_t CM_CHAT_MESSAGE_PUBLIC = 27;
/** Reshanta (WorldMapType ABYSS) */
constexpr int32_t RESHANTA = 400010000;
constexpr int32_t ELTNEN = 210020000;
/** the fortress and the artifact of the cases (siege_locations.xml): Siel's Western Fortress and one of its Reshanta artifacts */
constexpr int32_t FORTRESS = 1131;
constexpr int32_t ARTIFACT = 1135;
/** SiegeRace ids (SiegeRace.java) */
constexpr uint8_t RACE_ELYOS = 0, RACE_ASMODIANS = 1, RACE_BALAUR = 2;

/** the oracle's siege locations (m5i-sieges) */
struct SiegeAnswer {
	struct Location {
		int32_t id = 0;
		std::string type;
		int32_t world = 0;
		uint8_t raceId = 0;
		bool vulnerable = false;
		uint8_t nextState = 0;
	};
	std::vector<Location> locations;
	std::vector<int32_t> artifactsOf(int32_t world) const {
		std::vector<int32_t> ids;
		for (const auto& [id, w] : artifacts)
			if (w == world)
				ids.push_back(id);
		return ids;
	}
	std::vector<std::pair<int32_t, int32_t>> artifacts; // id, world, in Java's insertion order
};

SiegeAnswer parseSieges(const std::string& text) {
	const json parsed = json::parse(text);
	SiegeAnswer answer;
	for (const json& location : parsed.at("locations")) {
		SiegeAnswer::Location l;
		l.id = location.at("id").get<int32_t>();
		l.type = location.at("type").get<std::string>();
		l.world = location.at("world").get<int32_t>();
		l.raceId = static_cast<uint8_t>(location.at("raceId").get<int32_t>());
		l.vulnerable = location.at("vulnerable").get<bool>();
		l.nextState = static_cast<uint8_t>(location.at("nextState").get<int32_t>());
		answer.locations.push_back(l);
	}
	for (const json& artifact : parsed.at("artifacts"))
		answer.artifacts.emplace_back(artifact.at("id").get<int32_t>(), artifact.at("world").get<int32_t>());
	return answer;
}

std::map<std::string, int32_t> parseMessages(const std::string& text) {
	std::map<std::string, int32_t> ids;
	for (const auto& [name, id] : json::parse(text).at("messages").items())
		ids[name] = id.get<int32_t>();
	return ids;
}

void runM5iGate(bool geo) {
	const std::string testName = geo ? "gs.scenario.m5i_geo" : "gs.scenario.m5i";
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
	const std::filesystem::path outputDir = std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / (geo ? "m5i_geo" : "m5i");
	std::optional<Oracle> oracle = Oracle::fromEnvironment(outputDir / "oracle");
	if (!oracle) {
		unavailable("no Python interpreter for tools/oracle: set AION_TEST_PYTHON");
		return;
	}
	{
		// rifts open by cron at every hh:00 (rift_schedule.xml): the run (about 10 minutes) must not cross one;
		// checked after the availability checks, so an unconfigured run (CI) skips at any time of day
		const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
		const auto untilHour = std::chrono::ceil<std::chrono::hours>(now + std::chrono::seconds(1)) - now;
		if (untilHour < std::chrono::minutes(13)) {
			// fail at once instead of waiting: the run holds the test-database lock, and the next start window is minutes away
			ADD_FAILURE() << testName << ": started " << std::chrono::duration_cast<std::chrono::seconds>(untilHour).count()
			              << " s before an hh:00 rift cron; start it at least 13 minutes before the hour";
			return;
		}
	}

	CaseLog cases;
	struct ReportPrinter {
		const CaseLog& cases;
		const std::string& testName;
		~ReportPrinter() { std::cout << cases.report(testName) << std::flush; }
	} printer{cases, testName};

	std::map<std::string, std::string> gateKeys;
	gateKeys["gameserver.geodata.enable"] = geo ? "true" : "false";
	gateKeys["gameserver.npcshouts.enable"] = "false";
	gateKeys["gameserver.analysis.quest_handlers"] = "false";
	for (const auto& [key, value] : m5iSiegeKeys())
		gateKeys[key] = value;

	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.schemaPrefix = geo ? "m5ig" : "m5i";
	config.gameServerProperties = gateKeys;
	config.startupTimeout = 10min;
	config.stopTimeout = 3min;
	ScenarioServers servers(config, *environment);
	const std::string schema = servers.gameSchema();
	const ScenarioDatabase& database = servers.gameDatabase();
	std::filesystem::create_directories(outputDir);

	bool ok = true;
	std::string skipReason = "an earlier case ended with an exception or a fatal failure";
	const auto runCase = [&](std::string_view id, std::string_view title, const std::function<void()>& body) {
		if (!ok)
			cases.skip(id, title, skipReason);
		else
			ok = cases.run(id, title, body);
	};

	// ---- C0: the oracle ----
	SiegeAnswer sieges;
	std::map<std::string, int32_t> messages;
	const std::vector<std::string> messageNames{"STR_GUILD_EVENT_WIN_ARTIFACT", "STR_GUILD_EVENT_LOSE_ARTIFACT", "STR_MSG_WORLDRAID_MESSAGE_01",
		"STR_ABYSS_CARRIER_SPAWN", "STR_ABYSS_WARP_DRAGON", "STR_CANNOT_USE_ARTIFACT_OUT_OF_ORDER"};
	json spots;
	ok = cases.run("C0", "the oracle answers: m5i-sieges (the 70 locations of SiegeLocationData), the artifact message ids", [&] {
		sieges = parseSieges(oracle->run({"m5i-sieges"}));
		EXPECT_EQ(sieges.locations.size(), 70u) << "18 fortresses, 49 artifacts, 2 outposts, the agent (m5i-plan.md X1)";
		std::vector<std::string> arguments{"m5h-housing", "--message"};
		for (const std::string& name : messageNames)
			arguments.push_back(name);
		messages = parseMessages(oracle->run(arguments));
		EXPECT_EQ(messages.size(), messageNames.size());
		spots = json::parse(oracle->run({"m5i-spots"})); // G-01: S1, S4, S6, S7 and the sets they see
		EXPECT_GT(spots.at("checks").at("S1").at("eligibleElyosSiegeInView").get<int32_t>(), 11) << "risk 15: n >= 12 eligible npcs in view of S1";
	});

	runCase("S-0", "the servers start with sieges on (M5iGateProfile.h): initSieges spawns the siege npcs and starts the 49 artifact sieges", [&] {
		servers.createSchemas();
		servers.startLoginServer();
		servers.startGameServer();
	});

	const std::string suffix = servers.gameSchema().substr(servers.gameSchema().size() - 8);
	ScenarioClient w, g;
	std::array<ScenarioClient*, 2> all{&w, &g};
	w.label = "W";
	w.account = "m5iw" + suffix;
	g.label = "G";
	g.account = "m5ig" + suffix;
	Character cw{"W", "Siegewatcher", NewCharacter::WARRIOR, "WARRIOR", ""}; // Asmodian
	Character cg{"G", "Siegegm", NewCharacter::WARRIOR, "WARRIOR", ""};      // Elyos, access level 9

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
	const auto settle = [&] { drainAll(2000ms); };
	const auto text = [&](std::string_view sql) { return database.queryString(schema, sql).value_or(""); };
	const auto number = [&](std::string_view sql) { return database.queryLong(schema, sql).value_or(-1); };

	const auto createCharacter = [&](ScenarioClient& client, Character& character, bool asmodian) {
		NewCharacter newCharacter;
		newCharacter.name = character.name;
		newCharacter.playerClassId = character.classId;
		newCharacter.asmodian = asmodian;
		client.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(client.key.accountId, client.account, newCharacter, 1));
		EXPECT_EQ(decoders::decodeCreateCharacter(expectNext(*client.game, "SM_CREATE_CHARACTER", client.async).data).responseCode,
		          RESPONSE_OPEN_CREATION_WINDOW);
		client.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(client.key.accountId, client.account, newCharacter, 0));
		const decoders::CreateCharacter created = decoders::decodeCreateCharacter(expectNext(*client.game, "SM_CREATE_CHARACTER", client.async).data);
		if (created.responseCode != RESPONSE_OK || !created.player)
			throw std::runtime_error("creating " + character.label + " answered response code " + std::to_string(created.responseCode));
		character.playerId = created.player->playerId;
	};
	const auto endFirstPrologue = [&](ScenarioClient& client, const decoders::Prologue& prologue, std::string_view label) {
		AsyncAllowed async = client.async;
		async.temporarySpawnUpdates(client.npcs.predicate());
		endPrologue(*client.game, client.lastLevelReady, prologue, 0, async, [&] { return collectBurst(*client.game, async); }, label);
		client.model.sync();
	};
	/** Primum Landing for the Asmodian, Teminon Landing for the Elyos (teleport_location.xml 38 and 35): Reshanta, outside every fortress */
	const auto seedReshanta = [&](const Character& character, float x, float y, float z) {
		database.execute(schema, "UPDATE players SET world_id = " + std::to_string(RESHANTA) + ", x = " + std::to_string(x) + ", y = " + std::to_string(y) +
		                           ", z = " + std::to_string(z) + ", heading = 0 WHERE id = " + std::to_string(character.playerId));
	};

	// ---- the GM line (M5h2ScenarioTest.cpp's) ----
	const auto gm = [&](const std::string& line) {
		const size_t from = g.mark();
		g.game->send(CM_CHAT_MESSAGE_PUBLIC, Body().C(0).S(line).data);
		until(10s, [&] { return !ofName(g.since(from), "SM_MESSAGE").empty(); });
		settle();
		std::vector<std::string> texts;
		for (const Packet& packet : ofName(g.since(from), "SM_MESSAGE")) {
			const decoders::Message message = decoders::decodeMessage(packet.data);
			if (message.chatType == decoders::CHAT_GOLDEN_YELLOW)
				texts.push_back(message.message);
		}
		return texts;
	};
	const auto said = [](const std::vector<std::string>& texts, std::string_view t) {
		return std::count_if(texts.begin(), texts.end(), [&](const std::string& s) { return s.find(t) != std::string::npos; });
	};

	// ---- decoders over a slice ----
	const auto locationInfos = [](const std::vector<Packet>& packets) {
		std::vector<decoders::SiegeLocationInfo> infos;
		for (const Packet& packet : ofName(packets, "SM_SIEGE_LOCATION_INFO"))
			infos.push_back(decoders::decodeSiegeLocationInfo(packet.data));
		return infos;
	};
	const auto states = [](const std::vector<Packet>& packets, int32_t id) {
		std::vector<uint8_t> values;
		for (const Packet& packet : ofName(packets, "SM_SIEGE_LOCATION_STATE")) {
			const decoders::SiegeLocationState state = decoders::decodeSiegeLocationState(packet.data);
			if (state.locationId == id)
				values.push_back(state.state);
		}
		return values;
	};
	const auto shields = [](const std::vector<Packet>& packets, int32_t id) {
		std::vector<bool> values;
		for (const Packet& packet : ofName(packets, "SM_SHIELD_EFFECT"))
			for (const decoders::ShieldEffect::Location& location : decoders::decodeShieldEffect(packet.data).locations)
				if (location.locationId == id)
					values.push_back(location.underShield);
		return values;
	};

	/** X1-X3: the login burst of a character with sieges on (SiegeService.onPlayerLogin's enabled branch, SiegeService.java:579-586) */
	const auto checkLoginState = [&](const ScenarioClient& client, const std::vector<Packet>& burst, uint8_t ownRace,
	                                 const std::map<int32_t, std::pair<uint8_t, int32_t>>& changed, std::string_view label) {
		const std::vector<std::string> names = namesOf(burst);
		const auto at = [&](std::string_view name) {
			const auto found = std::find(names.begin(), names.end(), name);
			return found == names.end() ? -1 : static_cast<int32_t>(found - names.begin());
		};
		const int32_t influence = at("SM_INFLUENCE_RATIO"), info = at("SM_SIEGE_LOCATION_INFO"), after = at("SM_AFTER_SIEGE_LOCINFO_475"),
		              rift = at("SM_RIFT_ANNOUNCE");
		ASSERT_GE(influence, 0) << label << ": no SM_INFLUENCE_RATIO in the login burst: " << join(names);
		ASSERT_GE(info, 0) << label << ": no SM_SIEGE_LOCATION_INFO in the login burst";
		ASSERT_GE(after, 0) << label << ": no SM_AFTER_SIEGE_LOCINFO_475 in the login burst";
		ASSERT_GE(rift, 0) << label << ": no SM_RIFT_ANNOUNCE in the login burst";
		EXPECT_TRUE(influence < info && info < after && after < rift) << label << ": SiegeService.onPlayerLogin's order (influence, info, 475, rift)";
		const decoders::RiftAnnounce silentera = decoders::decodeRiftAnnounce(burst[static_cast<size_t>(rift)].data);
		EXPECT_EQ(silentera.actionId, 1);
		EXPECT_EQ(silentera.gelkmaros, 1) << label << ": OutpostLocation.isSilenteraAllowed is the constant true (X3)";
		EXPECT_EQ(silentera.inggison, 1);
		const decoders::SiegeLocationInfo all = decoders::decodeSiegeLocationInfo(burst[static_cast<size_t>(info)].data);
		EXPECT_EQ(all.infoType, 0);
		ASSERT_EQ(all.locations.size(), sieges.locations.size()) << label << ": the 70 locations of SiegeLocationData (X1)";
		for (size_t i = 0; i < all.locations.size(); i++) {
			const SiegeAnswer::Location& expected = sieges.locations[i];
			const decoders::SiegeLocationEntry& entry = all.locations[i];
			EXPECT_EQ(entry.locationId, expected.id) << label << ": location " << i << " - the XML order (header request 5a-pre-10b)";
			const auto override = changed.find(expected.id);
			const uint8_t race = override != changed.end() ? override->second.first : expected.raceId;
			EXPECT_EQ(entry.raceId, race) << label << ": the race of " << expected.id;
			EXPECT_EQ(entry.vulnerable, expected.vulnerable) << label << ": vulnerable of " << expected.id;
			EXPECT_EQ(entry.canTeleport, race == ownRace) << label << ": SiegeLocation.isCanTeleport of " << expected.id;
			if (expected.type != "FORTRESS")
				EXPECT_EQ(entry.nextState, expected.nextState) << label << ": the getNextState override of " << expected.id;
			EXPECT_EQ(entry.occupiedCount, override != changed.end() ? override->second.second : 0) << label << ": the occupy count of " << expected.id;
			EXPECT_EQ(entry.legionId, 0);
		}
		(void)client;
	};
	/** X4: CM_LEVEL_READY in Reshanta (SiegeService.onEnterSiegeWorld, SiegeService.java:590-605) */
	const auto checkSiegeWorld = [&](const std::vector<Packet>& levelReadyBurst, std::string_view label) {
		const Packet* shield = firstOfName(levelReadyBurst, "SM_SHIELD_EFFECT");
		const Packet* artifacts = firstOfName(levelReadyBurst, "SM_ABYSS_ARTIFACT_INFO3");
		ASSERT_NE(shield, nullptr) << label << ": no SM_SHIELD_EFFECT after CM_LEVEL_READY in Reshanta";
		ASSERT_NE(artifacts, nullptr) << label << ": no SM_ABYSS_ARTIFACT_INFO3 after CM_LEVEL_READY in Reshanta";
		std::vector<int32_t> expectedLocations;
		for (const SiegeAnswer::Location& location : sieges.locations)
			if (location.world == RESHANTA)
				expectedLocations.push_back(location.id);
		std::vector<int32_t> shieldIds;
		for (const decoders::ShieldEffect::Location& location : decoders::decodeShieldEffect(shield->data).locations) {
			shieldIds.push_back(location.locationId);
			EXPECT_FALSE(location.underShield) << label << ": " << location.locationId << " shielded at peace";
		}
		EXPECT_EQ(shieldIds, expectedLocations) << label << ": Reshanta's siege locations in the service's order";
		std::vector<int32_t> artifactIds;
		for (const decoders::ArtifactInfo3::Artifact& artifact : decoders::decodeArtifactInfo3(artifacts->data).artifacts) {
			artifactIds.push_back(artifact.locationId);
			EXPECT_EQ(artifact.status, 0) << label << ": artifact " << artifact.locationId << " not IDLE";
		}
		EXPECT_EQ(artifactIds, sieges.artifactsOf(RESHANTA)) << label << ": Reshanta's artifact locations, the fortresses' own included";
	};

	std::vector<Packet> wLogin, gLogin;
	runCase("C1", "seed: W (Asmodian) and G (Elyos, access level 9) created, through their prologues, then moved to Reshanta's landings", [&] {
		logIn(servers, w);
		createCharacter(w, cw, true);
		w.character = &cw;
		enterWorld(w, false);
		levelReady(w);
		endFirstPrologue(w, decoders::ASMODIAN_PROLOGUE, "W's prologue (2000)");
		disconnect(w);
		logIn(servers, g);
		createCharacter(g, cg, false);
		g.character = &cg;
		enterWorld(g, false);
		levelReady(g);
		endFirstPrologue(g, decoders::ELYOS_PROLOGUE, "G's prologue (1000)");
		disconnect(g);
		servers.loginDatabase().execute(servers.loginSchema(), "UPDATE account_data SET access_level = 9 WHERE name = '" + g.account + "'");
		seedReshanta(cw, 1078.71f, 2838.54f, 1636.58f);
		seedReshanta(cg, 2865.86f, 1036.57f, 1528.12f);
		EXPECT_EQ(text("SELECT race FROM siege_locations WHERE id = " + std::to_string(FORTRESS)), "BALAUR")
		  << "SiegeDAO.loadSiegeLocations inserts the default rows";
	});

	runCase("C2", "both enter Reshanta: the login state (X1-X3) and the siege world's shields and artifacts (X4)", [&] {
		wLogin = enterAs(servers, w, cw);
		checkLoginState(w, wLogin, RACE_ASMODIANS, {}, "W");
		checkSiegeWorld(w.lastLevelReady, "W");
		gLogin = enterAs(servers, g, cg);
		checkLoginState(g, gLogin, RACE_ELYOS, {}, "G");
		checkSiegeWorld(g.lastLevelReady, "G");
		settle();
	});

	const auto infoAfter = [&](const ScenarioClient& client, size_t from, int32_t id) {
		size_t n = 0;
		for (const decoders::SiegeLocationInfo& info : locationInfos(client.since(from)))
			for (const decoders::SiegeLocationEntry& entry : info.locations)
				if (entry.locationId == id)
					n++;
		return n;
	};
	const auto lastRaceOf = [&](const ScenarioClient& client, size_t from, int32_t id) -> int32_t {
		int32_t race = -1;
		for (const decoders::SiegeLocationInfo& info : locationInfos(client.since(from)))
			for (const decoders::SiegeLocationEntry& entry : info.locations)
				if (entry.locationId == id)
					race = entry.raceId;
		return race;
	};

	runCase("C3", "a capture without a siege: //siege capture 1131 asmodians, then elyos (X5: the location info to both, the row)", [&] {
		for (const auto& [race, raceId, dbRace] : std::vector<std::tuple<std::string, uint8_t, std::string>>{
			     {"asmodians", RACE_ASMODIANS, "ASMODIANS"}, {"elyos", RACE_ELYOS, "ELYOS"}}) {
			const size_t wFrom = w.mark(), gFrom = g.mark();
			gm("//siege capture " + std::to_string(FORTRESS) + " " + race);
			until(10s, [&] { return lastRaceOf(w, wFrom, FORTRESS) == raceId && lastRaceOf(g, gFrom, FORTRESS) == raceId; });
			EXPECT_EQ(infoAfter(w, wFrom, FORTRESS), 1u) << "captureSiege's no-siege branch: one broadcastUpdate (" << race << ")";
			EXPECT_EQ(lastRaceOf(w, wFrom, FORTRESS), raceId);
			EXPECT_EQ(lastRaceOf(g, gFrom, FORTRESS), raceId);
			EXPECT_FALSE(ofName(w.since(wFrom), "SM_INFLUENCE_RATIO").empty()) << "broadcastUpdate sends the influence after the info";
			EXPECT_EQ(text("SELECT race FROM siege_locations WHERE id = " + std::to_string(FORTRESS)), dbRace);
			EXPECT_EQ(number("SELECT occupy_count FROM siege_locations WHERE id = " + std::to_string(FORTRESS)), 0);
		}
	});

	runCase("C4", "a siege start: //siege start 1131 (X6: the state to both, the shield raised on the map)", [&] {
		const size_t wFrom = w.mark(), gFrom = g.mark();
		const std::vector<std::string> texts = gm("//siege start " + std::to_string(FORTRESS));
		EXPECT_EQ(said(texts, "Started siege at "), 1);
		until(15s, [&] { return !shields(w.since(wFrom), FORTRESS).empty() && !states(g.since(gFrom), FORTRESS).empty(); });
		EXPECT_EQ(states(w.since(wFrom), FORTRESS), std::vector<uint8_t>{1}) << "Siege.broadcastState: SM_SIEGE_LOCATION_STATE(1131, 1) world-wide";
		EXPECT_EQ(states(g.since(gFrom), FORTRESS), std::vector<uint8_t>{1});
		const std::vector<bool> shieldW = shields(w.since(wFrom), FORTRESS);
		EXPECT_FALSE(shieldW.empty()) << "ShieldNpcAI.handleSpawned: SM_SHIELD_EFFECT(1131, 1) to the map";
		if (!shieldW.empty())
			EXPECT_TRUE(shieldW.back());
		EXPECT_EQ(said(gm("//siege start " + std::to_string(FORTRESS)), "This location is already under siege."), 1);
	});

	runCase("C7", "a defended siege: //siege stop 1131 (X10: the state back to 0, the shield down, the row occupy 1)", [&] {
		const size_t wFrom = w.mark(), gFrom = g.mark();
		const std::vector<std::string> texts = gm("//siege stop " + std::to_string(FORTRESS));
		EXPECT_EQ(said(texts, "Stopped siege at "), 1);
		until(15s, [&] { return !states(w.since(wFrom), FORTRESS).empty() && !states(g.since(gFrom), FORTRESS).empty(); });
		EXPECT_EQ(states(w.since(wFrom), FORTRESS), std::vector<uint8_t>{0}) << "onSiegeFinish broadcastState(0)";
		EXPECT_EQ(states(g.since(gFrom), FORTRESS), std::vector<uint8_t>{0});
		const std::vector<bool> shieldW = shields(w.since(wFrom), FORTRESS);
		if (!shieldW.empty())
			EXPECT_FALSE(shieldW.back()) << "the shield npc despawned: SM_SHIELD_EFFECT(1131, 0)";
		EXPECT_TRUE(until(10s, [&] { return number("SELECT occupy_count FROM siege_locations WHERE id = " + std::to_string(FORTRESS)) == 1; }))
		  << "onDefended: increaseOccupiedCount, then SiegeDAO.updateSiegeLocation";
		EXPECT_EQ(text("SELECT race FROM siege_locations WHERE id = " + std::to_string(FORTRESS)), "ELYOS");
		std::cout << "C7: faction_balance of 1131 after the defence: "
		          << number("SELECT faction_balance FROM siege_locations WHERE id = " + std::to_string(FORTRESS)) << std::endl;
		EXPECT_EQ(said(gm("//siege stop " + std::to_string(FORTRESS)), "This location is not under siege."), 1);
	});

	runCase("C8", "an artifact: //siege capture 1135 elyos, then asmodians (X11: the artifact messages both times - the siege restarts)", [&] {
		const int32_t win = messages.at("STR_GUILD_EVENT_WIN_ARTIFACT"), lose = messages.at("STR_GUILD_EVENT_LOSE_ARTIFACT");
		for (const auto& [race, winner, loser] : std::vector<std::tuple<std::string, ScenarioClient*, ScenarioClient*>>{
			     {"elyos", &g, &w}, {"asmodians", &w, &g}}) {
			const size_t wFrom = winner->mark(), lFrom = loser->mark();
			gm("//siege capture " + std::to_string(ARTIFACT) + " " + race);
			until(10s, [&] { return countMessage(winner->since(wFrom), win) > 0 && countMessage(loser->since(lFrom), lose) > 0; });
			EXPECT_EQ(countMessage(winner->since(wFrom), win), 1u) << race << ": ArtifactSiege.onCapture's STR_GUILD_EVENT_WIN_ARTIFACT to the winners";
			EXPECT_EQ(countMessage(loser->since(lFrom), lose), 1u) << race << ": STR_GUILD_EVENT_LOSE_ARTIFACT to the others";
		}
		EXPECT_TRUE(until(10s, [&] { return text("SELECT race FROM siege_locations WHERE id = " + std::to_string(ARTIFACT)) == "ASMODIANS"; }))
		  << "ArtifactSiege.onSiegeFinish: SiegeDAO.updateSiegeLocation";
	});

	std::optional<int32_t> gameServerExit;
	bool gameServerRestarted = false;
	std::optional<int32_t> orphanBoss; // C13's siege boss, kept by the D4 orphan
	const auto checkRunReports = [&](std::string_view label) {
		EXPECT_TRUE(servers.readReportLines("unported_trace.txt").empty()) << label << ": AION_UNPORTED sites were reached:\n"
		                                                                   << join(servers.readReportLines("unported_trace.txt"), "\n");
		// D4 (m5i-plan.md, Java does this): with gameserver.siege.assault.enable=false the FortressAssault of C13's //siege assault stays in
		// BalaurAssaultService.fortressAssaults and keeps its Assault.boss - the census of the first shutdown names that boss, and nothing else
		std::vector<std::string> census;
		for (const std::string& line : servers.readReportLines("census.txt"))
			if (!(orphanBoss && !gameServerRestarted && line.find("SiegeNpc\t" + std::to_string(*orphanBoss) + "\t") != std::string::npos))
				census.push_back(line);
		EXPECT_TRUE(census.empty()) << label << ": " << join(census, "\n");
		EXPECT_TRUE(servers.readReportLines("lockdep.txt").empty()) << label << ": " << join(servers.readReportLines("lockdep.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("watchdog.txt").empty()) << label << ": " << join(servers.readReportLines("watchdog.txt"), "\n");
		for (const PartialHit& hit : readPartialHits(servers))
			std::cout << label << ": AION_PARTIAL " << hit.line << std::endl;
		ASSERT_NE(servers.gameServer(), nullptr);
		EXPECT_TRUE(servers.gameServer()->findLogLines(" ERROR ").empty()) << label << ": " << join(servers.gameServer()->findLogLines(" ERROR "), "\n");
	};

	// ---- M5i I-11: the world-event half (m5i-plan.md 10.2 C10-C13 and the vortex) over the oracle's spots (m5i-spots, G-01) ----
	const auto spot = [&](std::string_view name) {
		const json& p = spots.at("spots").at(std::string(name));
		return std::array<float, 3>{p.at(0).get<float>(), p.at(1).get<float>(), p.at(2).get<float>()};
	};
	const auto seedAt = [&](const Character& character, int32_t worldId, const std::array<float, 3>& p) {
		database.execute(schema, "UPDATE players SET world_id = " + std::to_string(worldId) + ", x = " + std::to_string(p[0]) + ", y = " +
		                           std::to_string(p[1]) + ", z = " + std::to_string(p[2]) + ", heading = 0 WHERE id = " + std::to_string(character.playerId));
	};
	/** quit, seed the character at the spot, enter: the level-ready burst */
	const auto relocate = [&](ScenarioClient& client, Character& character, int32_t worldId, const std::array<float, 3>& p) {
		disconnect(client);
		seedAt(character, worldId, p);
		std::vector<Packet> burst = enterAs(servers, client, character);
		settle();
		return burst;
	};
	const auto npcInfos = [](const std::vector<Packet>& packets) {
		std::vector<decoders::NpcInfo> infos;
		for (const Packet& packet : ofName(packets, "SM_NPC_INFO"))
			infos.push_back(decoders::decodeNpcInfo(packet.data));
		return infos;
	};
	const auto deletedIds = [](const std::vector<Packet>& packets) {
		std::set<int32_t> ids;
		for (const Packet& packet : ofName(packets, "SM_DELETE"))
			ids.insert(decoders::decodeDeleteObjectId(packet.data));
		return ids;
	};
	const auto templateIds = [&](const std::vector<Packet>& packets) {
		std::multiset<int32_t> ids;
		for (const decoders::NpcInfo& info : npcInfos(packets))
			ids.insert(info.templateId);
		return ids;
	};
	const auto npcIdsOf = [&](const json& list) {
		std::set<int32_t> ids;
		for (const json& npc : list)
			ids.insert(npc.at("npcId").get<int32_t>());
		return ids;
	};

	runCase("C9", "the artifact cooldown (X12): W (the owner since C8) at artifact 1135's aegis, both questions accepted: out of order, no activation", [&] {
		const std::array<float, 3> aegis{2917.352f, 2841.743f, 1451.369f}; // 276730, the ASMODIANS artifact npc of 1135 (spawns/Sieges/400010000_Reshanta.xml)
		relocate(w, cw, RESHANTA, {aegis[0] + 2.0f, aegis[1], aegis[2]});
		int32_t aegisObject = 0;
		for (const decoders::NpcInfo& info : npcInfos(w.since(0)))
			if (info.templateId == 276730)
				aegisObject = info.objectId;
		ASSERT_NE(aegisObject, 0) << "SM_NPC_INFO of the aegis 276730";
		size_t wFrom = w.mark();
		w.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(aegisObject));
		std::vector<int32_t> codes;
		for (int32_t question = 1; question <= 2; question++) {
			until(5s, [&] { return ofName(w.since(wFrom), "SM_QUESTION_WINDOW").size() >= static_cast<size_t>(question); });
			const std::vector<Packet> windows = ofName(w.since(wFrom), "SM_QUESTION_WINDOW");
			ASSERT_GE(windows.size(), static_cast<size_t>(question)) << "ArtifactAI's question " << question;
			const decoders::QuestionWindow window = decoders::decodeQuestionWindow(windows[static_cast<size_t>(question) - 1].data);
			codes.push_back(window.code);
			w.game->send(GameSession::CM_QUESTION_RESPONSE, GameSession::buildCM_QUESTION_RESPONSE(window.code, 1, aegisObject));
		}
		const int32_t outOfOrder = messages.at("STR_CANNOT_USE_ARTIFACT_OUT_OF_ORDER");
		until(5s, [&] { return countMessage(w.since(wFrom), outOfOrder) > 0; });
		EXPECT_EQ(countMessage(w.since(wFrom), outOfOrder), 1u) << "the 900 s cooldown of a captured artifact (setInitialDelay)";
		EXPECT_TRUE(ofName(w.since(wFrom), "SM_USE_OBJECT").empty()) << "no activation";
		EXPECT_EQ(codes.size(), 2u);
		if (codes.size() == 2)
			EXPECT_NE(codes[0], codes[1]) << "STR_ASK_ARTIFACT_POPUPDIALOG, then STR_ASK_USE_ARTIFACT";
	});

	runCase("C10", "a world raid's first stage: G at S4 in Reshanta, //worldraid start 31 (X14: the message to the map, the flag at the spot)", [&] {
		relocate(g, cg, RESHANTA, spot("S4"));
		const size_t wFrom = w.mark(), gFrom = g.mark();
		const std::vector<std::string> texts = gm("//worldraid start 31");
		const int32_t raidMessage = messages.at("STR_MSG_WORLDRAID_MESSAGE_01");
		until(5s, [&] { return countMessage(g.since(gFrom), raidMessage) > 0 && countMessage(w.since(wFrom), raidMessage) > 0; });
		EXPECT_EQ(countMessage(g.since(gFrom), raidMessage), 1u) << "minute 0 of the raid's fixed-rate task, to the map";
		EXPECT_EQ(countMessage(w.since(wFrom), raidMessage), 1u) << "W at Primum Landing is on 400010000 too";
		const json& flagPos = spots.at("worldRaid31").at("pos");
		bool flagSeen = false;
		for (const decoders::NpcInfo& info : npcInfos(g.since(gFrom)))
			if (info.templateId == spots.at("worldRaid31").at("flagNpcId").get<int32_t>()) {
				flagSeen = true;
				EXPECT_NEAR(info.x, flagPos.at(0).get<float>(), 0.5f);
				EXPECT_NEAR(info.y, flagPos.at(1).get<float>(), 0.5f);
			}
		EXPECT_TRUE(flagSeen) << "the raid's flag 832819 at location 31";
		std::cout << "C10: //worldraid answered: " << join(texts, " | ") << std::endl;
	});

	runCase("C11", "rifts: G at S6 in Eltnen, //rift open 2120 then //rift close 2120 (X15: the master rift announced and spawned, then removed)", [&] {
		relocate(g, cg, ELTNEN, spot("S6"));
		const json& rift = spots.at("rift2120");
		size_t gFrom = g.mark();
		EXPECT_EQ(said(gm("//rift open 2120"), "Opened rifts at location 2120."), 1);
		until(5s, [&] { return templateIds(g.since(gFrom)).contains(rift.at("masterNpcId").get<int32_t>()); });
		int32_t masterObject = 0;
		for (const decoders::NpcInfo& info : npcInfos(g.since(gFrom)))
			if (info.templateId == rift.at("masterNpcId").get<int32_t>())
				masterObject = info.objectId;
		ASSERT_NE(masterObject, 0) << "SM_NPC_INFO of the master rift 700137 at ELTNEN_AM";
		bool announced = false;
		for (const Packet& packet : ofName(g.since(gFrom), "SM_RIFT_ANNOUNCE")) {
			const decoders::RiftAnnounce announce = decoders::decodeRiftAnnounce(packet.data);
			if (announce.actionId == 2 && announce.objectId == masterObject) {
				announced = true;
				EXPECT_EQ(announce.maxEntries, rift.at("entries").get<int32_t>());
				EXPECT_EQ(announce.minLevel, rift.at("minLevel").get<int32_t>());
				EXPECT_EQ(announce.maxLevel, rift.at("maxLevel").get<int32_t>());
			}
		}
		EXPECT_TRUE(announced) << "RiftInformer: SM_RIFT_ANNOUNCE action 2 for the master";
		gFrom = g.mark();
		EXPECT_EQ(said(gm("//rift close 2120"), "Closed rifts at location 2120."), 1);
		until(5s, [&] { return deletedIds(g.since(gFrom)).contains(masterObject); });
		EXPECT_TRUE(deletedIds(g.since(gFrom)).contains(masterObject)) << "SM_DELETE of the master rift";
		bool despawnAnnounced = false;
		for (const Packet& packet : ofName(g.since(gFrom), "SM_RIFT_ANNOUNCE"))
			despawnAnnounced = despawnAnnounced || (decoders::decodeRiftAnnounce(packet.data).actionId == 4);
		EXPECT_TRUE(despawnAnnounced) << "SM_RIFT_ANNOUNCE action 4";
	});

	runCase("C12", "a base: G at S7, //base capture 2120 elyos and the //base assault refusals (X16)", [&] {
		relocate(g, cg, ELTNEN, spot("S7"));
		const std::vector<Packet> burst = g.since(0); // the level-ready burst and the settle after it (a flag can come later)
		const json& base = spots.at("base2120");
		std::set<int32_t> balaur;
		for (const char* set : {"BALAUR/FLAG", "BALAUR/SENTINEL"})
			for (int32_t id : npcIdsOf(base.at(set)))
				balaur.insert(id);
		std::map<int32_t, int32_t> seenBalaur; // object id -> template id
		for (const decoders::NpcInfo& info : npcInfos(burst))
			if (balaur.contains(info.templateId))
				seenBalaur[info.objectId] = info.templateId;
		EXPECT_TRUE(std::any_of(seenBalaur.begin(), seenBalaur.end(), [&](const auto& e) { return npcIdsOf(base.at("BALAUR/FLAG")).contains(e.second); }))
			<< "the BALAUR flag of base 2120 (the default occupier, started by initBases)";

		size_t gFrom = g.mark();
		gm("//base capture 2120 elyos");
		std::set<int32_t> elyos;
		for (const char* set : {"ELYOS/FLAG", "ELYOS/MERCHANT", "ELYOS/SENTINEL"})
			for (int32_t id : npcIdsOf(base.at(set)))
				elyos.insert(id);
		until(5s, [&] {
			const std::multiset<int32_t> spawned = templateIds(g.since(gFrom));
			return std::any_of(spawned.begin(), spawned.end(), [&](int32_t id) { return npcIdsOf(base.at("ELYOS/FLAG")).contains(id); });
		});
		const std::set<int32_t> deleted = deletedIds(g.since(gFrom));
		for (const auto& [objectId, templateId] : seenBalaur)
			EXPECT_TRUE(deleted.contains(objectId)) << "SM_DELETE of the BALAUR npc " << templateId;
		const std::multiset<int32_t> spawned = templateIds(g.since(gFrom));
		for (const char* set : {"ELYOS/FLAG", "ELYOS/MERCHANT", "ELYOS/SENTINEL"})
			for (int32_t id : npcIdsOf(base.at(set)))
				EXPECT_TRUE(spawned.contains(id)) << set << " " << id;

		EXPECT_EQ(said(gm("//base assault 2120"), "Not enough parameters."), 1);
		gFrom = g.mark();
		EXPECT_EQ(said(gm("//base assault 2120 elyos"), "Base cannot be assaulted by the same occupier"), 1);
		EXPECT_TRUE(npcInfos(g.since(gFrom)).empty()) << "a refused assault spawns nothing";
		gFrom = g.mark();
		gm("//base assault 2120 asmodians");
		until(5s, [&] { return templateIds(g.since(gFrom)).contains(231617); });
		EXPECT_TRUE(templateIds(g.since(gFrom)).contains(231617)) << "the ASMODIANS ATTACKER set";
	});

	runCase("C13", "a Balaur assault: G at S1 inside his fortress, //siege start 1131, //siege assault 1131 0, //siege stop 1131 (X17)", [&] {
		relocate(g, cg, RESHANTA, spot("S1"));
		size_t gFrom = g.mark();
		gm("//siege start " + std::to_string(FORTRESS));
		until(15s, [&] { return !states(g.since(gFrom), FORTRESS).empty(); });
		EXPECT_EQ(states(g.since(gFrom), FORTRESS), std::vector<uint8_t>{1}) << "the owner inside his fortress stays (clearLocation moves enemies)";
		settle();
		for (const decoders::NpcInfo& info : npcInfos(g.since(gFrom)))
			if (info.templateId == 263001) // the ELYOS SIEGE boss of 1131
				orphanBoss = info.objectId;
		EXPECT_TRUE(orphanBoss) << "the siege boss 263001 in view of S1";
		gFrom = g.mark();
		gm("//siege assault " + std::to_string(FORTRESS) + " 0");
		const int32_t carrierSpawn = messages.at("STR_ABYSS_CARRIER_SPAWN"), warp = messages.at("STR_ABYSS_WARP_DRAGON");
		until(5s, [&] { return countMessage(g.since(gFrom), carrierSpawn) > 0; });
		EXPECT_FALSE(ofName(g.since(gFrom), "SM_NPC_ASSEMBLER").empty()) << "BalaurAssaultService.spawnDredgion";
		EXPECT_EQ(countMessage(g.since(gFrom), carrierSpawn), 1u);
		const int32_t baseDelay = spots.at("assault1131").at("baseDelay").get<int32_t>();
		until(std::chrono::seconds(baseDelay + 5), [&] { return countMessage(g.since(gFrom), warp) > 0; });
		EXPECT_EQ(countMessage(g.since(gFrom), warp), 1u) << "the first wave (a teleport wave) within base_delay + 5 s";
		settle();
		std::set<int32_t> teleport;
		for (const json& id : spots.at("assault1131").at("teleport"))
			teleport.insert(id.get<int32_t>());
		std::set<int32_t> assaulters;
		for (const decoders::NpcInfo& info : npcInfos(g.since(gFrom)))
			if (teleport.contains(info.templateId))
				assaulters.insert(info.objectId);
		EXPECT_FALSE(assaulters.empty()) << "at least one ASSAULT npc of the TELEPORT set in view (" << spots.at("checks").at("S1").at("eligibleElyosSiegeInView")
		                                 << " eligible npcs in view of S1)";
		gFrom = g.mark();
		gm("//siege stop " + std::to_string(FORTRESS));
		until(15s, [&] { return !states(g.since(gFrom), FORTRESS).empty(); });
		const std::set<int32_t> deleted = deletedIds(g.since(gFrom));
		for (int32_t objectId : assaulters)
			EXPECT_TRUE(deleted.contains(objectId)) << "deSpawnNpcs removes the assault npcs (siege spawns of 1131)";
		EXPECT_EQ(text("SELECT race FROM siege_locations WHERE id = " + std::to_string(FORTRESS)), "ELYOS");
		EXPECT_EQ(number("SELECT occupy_count FROM siege_locations WHERE id = " + std::to_string(FORTRESS)), 2) << "C7 and C13: two defences";
		std::cout << "C13: faction_balance of 1131 after the second defence: "
		          << number("SELECT faction_balance FROM siege_locations WHERE id = " + std::to_string(FORTRESS)) << std::endl;
	});

	runCase("C17", "relocation at login (X8a/X8b): W quits, is seeded at S1, the siege of 1131 starts; W enters at his bind point; quits, is seeded again, enters at S1; the stop defends 1131 a third time", [&] {
		const auto spawnOf = [](const std::vector<Packet>& packets) {
			const Packet* spawn = firstOfName(packets, "SM_PLAYER_SPAWN");
			if (spawn == nullptr)
				throw std::runtime_error("no SM_PLAYER_SPAWN");
			return decoders::decodePlayerSpawn(spawn->data);
		};
		const std::array<float, 3> s1 = spot("S1");
		disconnect(w); // lastOnline t1
		seedAt(cw, RESHANTA, s1);
		std::this_thread::sleep_for(2s);
		size_t gFrom = g.mark();
		gm("//siege start " + std::to_string(FORTRESS)); // start t2 > t1
		until(15s, [&] { return !states(g.since(gFrom), FORTRESS).empty(); });
		const decoders::PlayerSpawn x8a = spawnOf(enterAs(servers, w, cw));
		settle();
		EXPECT_TRUE(x8a.worldId != RESHANTA || std::hypot(x8a.x - s1[0], x8a.y - s1[1]) > 100.0f)
			<< "X8a: logged out before the siege start, an enemy of the owner enters at his bind point (validateFortressZone), not at S1: world "
			<< x8a.worldId << " (" << x8a.x << ", " << x8a.y << ")";
		std::this_thread::sleep_for(2s);
		disconnect(w); // lastOnline t3 > t2
		seedAt(cw, RESHANTA, s1);
		const decoders::PlayerSpawn x8b = spawnOf(enterAs(servers, w, cw));
		settle();
		EXPECT_EQ(x8b.worldId, RESHANTA) << "X8b: logged out after the start, he enters where he left";
		EXPECT_NEAR(x8b.x, s1[0], 1.0f);
		EXPECT_NEAR(x8b.y, s1[1], 1.0f);
		gFrom = g.mark();
		gm("//siege stop " + std::to_string(FORTRESS));
		until(15s, [&] { return !states(g.since(gFrom), FORTRESS).empty(); });
		EXPECT_EQ(number("SELECT occupy_count FROM siege_locations WHERE id = " + std::to_string(FORTRESS)), 3) << "C7, C13, C17: three defences, uncapped";
	});

	runCase("C16", "the vortex: //vortexraid start theobomos, a second start refused, //vortexraid stop theobomos, twice (VortexService, Invasion)", [&] {
		// twice: the second invasion starts from what the first one's stop left (VortexService.despawn unsets the vortex controller)
		for (int32_t run = 1; run <= 2; run++) {
			SCOPED_TRACE("invasion " + std::to_string(run));
			EXPECT_EQ(said(gm("//vortexraid start theobomos"), " raid started."), 1);
			settle();
			EXPECT_EQ(said(gm("//vortexraid start theobomos"), " is already under siege."), 1);
			EXPECT_EQ(said(gm("//vortexraid stop theobomos"), " raid stopped."), 1);
			settle();
			EXPECT_EQ(said(gm("//vortexraid stop theobomos"), " is not under siege."), 1);
		}
		EXPECT_TRUE(servers.gameServer()->findLogLines(" ERROR ").empty()) << "both invasions ran without an error: "
		                                                                     << join(servers.gameServer()->findLogLines(" ERROR "), "\n");
	});

	runCase("C14", "persistence: both servers' game server restarts; the login state shows 1131 ELYOS occupy 3 and 1135 ASMODIANS (X18)", [&] {
		for (ScenarioClient* client : all)
			disconnect(*client);
		gameServerExit = servers.stopGameServer();
		ASSERT_TRUE(gameServerExit);
		EXPECT_EQ(*gameServerExit, 0);
		checkRunReports("the first run");
		gameServerExit.reset();
		servers.startGameServer();
		gameServerRestarted = true;
		const std::map<int32_t, std::pair<uint8_t, int32_t>> changed{{FORTRESS, {RACE_ELYOS, 3}}, {ARTIFACT, {RACE_ASMODIANS, 0}}}; // C7, C13, C17
		wLogin = enterAs(servers, w, cw);
		checkLoginState(w, wLogin, RACE_ASMODIANS, changed, "W after the restart");
		gLogin = enterAs(servers, g, cg);
		checkLoginState(g, gLogin, RACE_ELYOS, changed, "G after the restart");
		// GMService.onPlayerLogin schedules G's login announcement 15 s on (GMService.java:84-99); the server stops only after it ran, so no
		// task still holds G at the shutdown census
		const std::optional<size_t> announced = readUntil(*g.game, [](const Packet& packet) {
			return packet.name == "SM_MESSAGE" && decoders::decodeMessage(packet.data).message.starts_with("Your login has");
		}, 30s);
		EXPECT_TRUE(announced) << "G: the login announcement task never ran";
		settle();
	});

	for (ScenarioClient* client : all)
		if (client->game)
			try {
				disconnect(*client);
			} catch (const std::exception& e) {
				std::cout << "disconnect: " << e.what() << std::endl;
			}
	if (servers.gameServer() != nullptr && !gameServerExit)
		gameServerExit = servers.stopGameServer();
	const std::optional<int32_t> loginServerExit = servers.loginServer() != nullptr ? servers.stopLoginServer() : std::nullopt;

	cases.run("C15", "reports: exits, unported sites, census, lock order, watchdog, ERROR lines", [&] {
		ASSERT_TRUE(gameServerExit) << "the game server did not exit after the stop file was written";
		EXPECT_EQ(*gameServerExit, 0);
		ASSERT_TRUE(loginServerExit);
		EXPECT_TRUE(*loginServerExit == 0 || *loginServerExit == 98) << "the login server exited with " << *loginServerExit;
		checkRunReports(gameServerRestarted ? "the restarted run" : "the run");
	});

	finishRun(servers, outputDir, testName);
}

// ==================================================================================================================== the soak (D17, G-09)

/**
 * The window of the soak in UTC (the profile's server time zone): it must cross an hh:00 and stay clear of the shipped schedules - the siege
 * preparations and sieges of 16:45-23:00 (siege_schedule.xml: 17:00-21:00 plus up to 10 minutes of preparation, the agent fight Tuesday
 * 20:50), midnight (the daily jobs) and Wednesday 08:45-09:15 (the weekly dominion calculation). @return the reason it does not fit, if any
 */
std::optional<std::string> soakWindowConflict(std::chrono::sys_seconds from, std::chrono::sys_seconds to) {
	using namespace std::chrono;
	for (sys_seconds t = from; t <= to; t += minutes(1)) {
		const sys_days day = floor<days>(t);
		const hh_mm_ss<seconds> time(t - day);
		const int32_t minute = static_cast<int32_t>(time.hours().count() * 60 + time.minutes().count());
		if (minute >= 16 * 60 + 45 && minute < 23 * 60)
			return "the window touches the evening siege schedule (16:45-23:00 UTC)";
		if (minute == 0 && t != from)
			return "the window crosses midnight";
		if (weekday(day) == Wednesday && minute >= 8 * 60 + 45 && minute <= 9 * 60 + 15)
			return "the window touches Wednesday 09:00";
	}
	if (floor<hours>(from) == floor<hours>(to))
		return "the window does not cross an hh:00";
	return std::nullopt;
}

void runM5iSoak() {
	using namespace std::chrono;
	const std::string testName = "gs.scenario.m5i_soak";
	if (std::getenv("AION_SCENARIO_SOAK") == nullptr) { // D17: an 80-minute run, only in a slot the owner chose
		GTEST_SKIP() << testName << ": skipped (opt-in: set AION_SCENARIO_SOAK=1)";
		return;
	}
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
	constexpr minutes SOAK_LENGTH{65};
	const sys_seconds start = floor<seconds>(system_clock::now());
	// startup (10 min at most in Debug) + the seed + the soak
	if (const std::optional<std::string> conflict = soakWindowConflict(start, start + minutes(15) + SOAK_LENGTH)) {
		GTEST_SKIP() << testName << ": skipped (" << *conflict << "; started " << std::format("{:%F %T}", start) << " UTC)";
		return;
	}
	const std::filesystem::path outputDir = std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "m5i_soak";

	CaseLog cases;
	struct ReportPrinter {
		const CaseLog& cases;
		const std::string& testName;
		~ReportPrinter() { std::cout << cases.report(testName) << std::flush; }
	} printer{cases, testName};

	std::map<std::string, std::string> soakKeys;
	soakKeys["gameserver.geodata.enable"] = "false";
	soakKeys["gameserver.npcshouts.enable"] = "false";
	soakKeys["gameserver.analysis.quest_handlers"] = "false";
	for (const auto& [key, value] : m5iSiegeKeys())
		soakKeys[key] = value;
	soakKeys["gameserver.siege.assault.enable"] = "true"; // the shipped setting (siege.properties:11)

	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.schemaPrefix = "m5is";
	config.gameServerProperties = soakKeys;
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

	runCase("K0", "the servers start with every siege system on, the Balaur assault as shipped, and the 31 bases", [&] {
		servers.createSchemas();
		servers.startLoginServer();
		servers.startGameServer();
	});

	const std::string suffix = servers.gameSchema().substr(servers.gameSchema().size() - 8);
	ScenarioClient r, e, v;
	std::array<ScenarioClient*, 3> all{&r, &e, &v};
	r.label = "R";
	r.account = "m5isr" + suffix;
	e.label = "E";
	e.account = "m5ise" + suffix;
	v.label = "V";
	v.account = "m5isv" + suffix;
	Character cr{"R", "Soakreshanta", NewCharacter::WARRIOR, "WARRIOR", ""}; // Asmodian, Primum Landing
	Character ce{"E", "Soakeltnen", NewCharacter::WARRIOR, "WARRIOR", ""};   // Elyos, Eltnen
	Character cv{"V", "Soakverteron", NewCharacter::WARRIOR, "WARRIOR", ""}; // Elyos, Verteron
	const steady_clock::time_point steadyZero = steady_clock::now();
	const system_clock::time_point systemZero = system_clock::now();
	const auto wallClockOf = [&](steady_clock::time_point t) { return systemZero + duration_cast<system_clock::duration>(t - steadyZero); };

	const auto createCharacter = [&](ScenarioClient& client, Character& character, bool asmodian) {
		NewCharacter newCharacter;
		newCharacter.name = character.name;
		newCharacter.playerClassId = character.classId;
		newCharacter.asmodian = asmodian;
		client.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(client.key.accountId, client.account, newCharacter, 1));
		expectNext(*client.game, "SM_CREATE_CHARACTER", client.async);
		client.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(client.key.accountId, client.account, newCharacter, 0));
		const decoders::CreateCharacter created = decoders::decodeCreateCharacter(expectNext(*client.game, "SM_CREATE_CHARACTER", client.async).data);
		if (created.responseCode != RESPONSE_OK || !created.player)
			throw std::runtime_error("creating " + character.label + " answered response code " + std::to_string(created.responseCode));
		character.playerId = created.player->playerId;
	};
	const auto endFirstPrologue = [&](ScenarioClient& client, const decoders::Prologue& prologue, std::string_view label) {
		AsyncAllowed async = client.async;
		async.temporarySpawnUpdates(client.npcs.predicate());
		endPrologue(*client.game, client.lastLevelReady, prologue, 0, async, [&] { return collectBurst(*client.game, async); }, label);
		client.model.sync();
	};
	const auto seed = [&](const Character& character, int32_t worldId, float x, float y, float z) {
		database.execute(schema, "UPDATE players SET world_id = " + std::to_string(worldId) + ", x = " + std::to_string(x) + ", y = " + std::to_string(y) +
		                           ", z = " + std::to_string(z) + ", heading = 0 WHERE id = " + std::to_string(character.playerId));
	};

	runCase("K1", "seed: three characters through their prologues, then placed in Reshanta, Eltnen and Verteron", [&] {
		const auto create = [&](ScenarioClient& client, Character& character, bool asmodian, const decoders::Prologue& prologue) {
			logIn(servers, client);
			createCharacter(client, character, asmodian);
			client.character = &character;
			enterWorld(client, false);
			levelReady(client);
			endFirstPrologue(client, prologue, client.label + "'s prologue");
			disconnect(client);
		};
		create(r, cr, true, decoders::ASMODIAN_PROLOGUE);
		create(e, ce, false, decoders::ELYOS_PROLOGUE);
		create(v, cv, false, decoders::ELYOS_PROLOGUE);
		seed(cr, RESHANTA, 1078.71f, 2838.54f, 1636.58f);   // Primum Landing (teleport_location.xml 38)
		seed(ce, 210020000, 948.0f, 2216.0f, 253.0f);       // Eltnen (hotspot_template.xml 30)
		seed(cv, 210030000, 1643.0f, 1500.0f, 120.0f);      // Verteron Citadel (hotspot_template.xml 19)
	});

	runCase("K2", "65 idle minutes across an hh:00: every client gets the hourly SM_FORTRESS_STATUS within 2 s after the hour", [&] {
		enterAs(servers, r, cr);
		enterAs(servers, e, ce);
		enterAs(servers, v, cv);
		std::array<size_t, 3> from{r.mark(), e.mark(), v.mark()};
		const steady_clock::time_point end = steady_clock::now() + SOAK_LENGTH;
		steady_clock::time_point nextPing = steady_clock::now() + seconds(180);
		while (steady_clock::now() < end) {
			for (ScenarioClient* client : all)
				if (client->game && !client->game->client.socket.isClosed())
					client->game->next(20ms);
			if (steady_clock::now() >= nextPing) { // the client's 180 s ping (CM_PING.java), so no client looks dead
				for (ScenarioClient* client : all)
					if (client->game && !client->game->client.socket.isClosed())
						client->game->send(GameSession::CM_PING, GameSession::buildCM_PING());
				nextPing += seconds(180);
			}
		}
		for (size_t i = 0; i < all.size(); i++) {
			ScenarioClient& client = *all[i];
			ASSERT_TRUE(client.game && !client.game->client.socket.isClosed()) << client.label << ": disconnected during the soak";
			int32_t onTheHour = 0;
			for (const Packet& packet : ofName(client.since(from[i]), "SM_FORTRESS_STATUS")) {
				const system_clock::time_point at = wallClockOf(packet.receivedAt);
				const auto sinceHour = at - floor<hours>(at);
				std::cout << client.label << ": SM_FORTRESS_STATUS at " << std::format("{:%T}", floor<milliseconds>(at)) << " UTC" << std::endl;
				if (sinceHour <= seconds(2))
					onTheHour++;
			}
			EXPECT_GE(onTheHour, 1) << client.label << ": no SM_FORTRESS_STATUS within 2 s after an hh:00 (SiegeService's hourly broadcast)";
		}
	});

	std::optional<int32_t> gameServerExit;
	for (ScenarioClient* client : all)
		if (client->game)
			try {
				disconnect(*client);
			} catch (const std::exception& ex) {
				std::cout << "disconnect: " << ex.what() << std::endl;
			}
	if (servers.gameServer() != nullptr)
		gameServerExit = servers.stopGameServer();
	const std::optional<int32_t> loginServerExit = servers.loginServer() != nullptr ? servers.stopLoginServer() : std::nullopt;

	cases.run("K3", "reports: exits, unported sites, census (the leak census and the zombie breaker), lock order, watchdog, ERROR lines", [&] {
		ASSERT_TRUE(gameServerExit) << "the game server did not exit after the stop file was written";
		EXPECT_EQ(*gameServerExit, 0);
		ASSERT_TRUE(loginServerExit);
		EXPECT_TRUE(*loginServerExit == 0 || *loginServerExit == 98) << "the login server exited with " << *loginServerExit;
		EXPECT_TRUE(servers.readReportLines("unported_trace.txt").empty()) << join(servers.readReportLines("unported_trace.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("census.txt").empty()) << join(servers.readReportLines("census.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("lockdep.txt").empty()) << join(servers.readReportLines("lockdep.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("watchdog.txt").empty()) << join(servers.readReportLines("watchdog.txt"), "\n");
		for (const PartialHit& hit : readPartialHits(servers))
			std::cout << "AION_PARTIAL " << hit.line << std::endl;
		ASSERT_NE(servers.gameServer(), nullptr);
		EXPECT_TRUE(servers.gameServer()->findLogLines(" ERROR ").empty()) << join(servers.gameServer()->findLogLines(" ERROR "), "\n");
	});

	finishRun(servers, outputDir, testName);
}

} // namespace

/** `gs.scenario.m5i` (m5i-plan.md 10, I-6): the siege half - the login state, captures, a siege start and stop, an artifact, persistence */
TEST(M5iScenario, Run) {
	runM5iGate(false);
}

/**
 * `gs.scenario.m5i_geo` (m5i-plan.md 10.5): the same script with gameserver.geodata.enable=true - the geo meshes, the geo shields of the
 * fortresses (ShieldService attaches them and drops 1131's sphere template) and the SHIELD DespawnableNodes. X7g (the crossing killed by
 * SiegeShield's CollisionDieActor) waits with C5 (P5-12b.md, I-11 part 2).
 */
TEST(M5iScenarioGeo, Run) {
	runM5iGate(true);
}

/** `gs.scenario.m5i_soak` (m5i-plan.md 10.7, D17, G-09): 65 idle minutes of three clients across an hh:00 - opt-in (AION_SCENARIO_SOAK) */
TEST(M5iSoak, Run) {
	runM5iSoak();
}

} // namespace aion::gameserver::scenario
