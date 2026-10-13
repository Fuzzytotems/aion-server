// The M5h-2 land-house gate gs.scenario.m5h2 (m5h-plan.md 14.3, H2-4): one login server and one game server as child processes on their own
// test schemas, the housing keys of M5h2GateProfile.h (the auto-fill at second 10, the auction end at second 20 and the maintenance at second 40
// of every minute), and TWO Elyos accounts online at once - A, who is outbid, and B, who wins:
//   L1 the characters (quest 18802 seeded complete: HousingService.canOwnHouse); L2 the bids list (the one auto-filled HOUSE); L3 the bids, the
//   refusals (the highest bidder again, twice the bid), A outbid and refunded by mail; L4 the prolonged auction ends, B wins (the house, the
//   mail); L5 the rent; L6 the door (closed by B, A refused, B enters); L7 the relationship crystal (FriendPortalAI's page, the teleport home,
//   the leave by the door); G0-G3 the admin commands of H2-5 by a third account G at access level 9 (//auction, //house, //legion on an Elyos
//   mansion and a seeded legion); L8 the maintenance cron's overdue notices, then across a restart with B's rent 15 days overdue the impound;
//   L9 the reports.
// Not gated here, to gate once #223 (lane A's SummonGroupGateEffect) is merged: the house gate's group-member arm and its no-return zone
// (HouseGateAI is unit-tested in tests/handlers_ai_core/HouseAiHandlersTest.cpp).
//
// Timing: with the auction end firing every minute, every bid lies within 5 minutes of it, so AuctionEndTask.tryProlongAuction prolongs the
// auction to 5 minutes after the last bid and the prolongation task ends it (L4 waits up to 8 minutes). Bidding is closed on Sunday from 12:00
// (HousingBidService.isBiddingTime): a run then skips L2-L8 with that reason.
//
// Every expectation is independent of the C++ server code: server packets are read by name with the decoders of tests/scenario/decoders, and
// every id comes from `tools/oracle/oracle.py m5h-housing` (the message ids, HouseDoorState, the auctioned land house and its npcs) or from the
// Java method an assertion is about. This file deliberately does not share the other gates' helpers (M5b3ScenarioTest.cpp gives the reason);
// the scaffolding below is M5hScenarioTest.cpp's, never an assertion.

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
#include "M5h2GateProfile.h"
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

/** the gate's oracle answer of m5h-legion */

// ---- the land-house gate (m5h-plan.md 14.3, H2-4) ---------------------------------------------------------------------------------------

/** the client packet ids of ClientPacketInfo.gen.inc (the internal opcodes GameSession sends) */
constexpr int32_t CM_HOUSE_SETTINGS = 73;
constexpr int32_t CM_HOUSE_TELEPORT_BACK = 95;
constexpr int32_t CM_GET_HOUSE_BIDS = 218;
constexpr int32_t CM_REGISTER_HOUSE = 219;
constexpr int32_t CM_PLACE_BID = 221;
constexpr int32_t CM_HOUSE_TELEPORT = 222;
constexpr int32_t CM_HOUSE_PAY_RENT = 223;
constexpr int32_t CM_HOUSE_OPEN_DOOR = 226;
constexpr int32_t KINAH_ITEM = 182400001;
/** [Housing] And A Home for Every Daeva (HousingService.canOwnHouse: an Elyos must have completed 18802 to bid) */
constexpr int32_t STUDIO_QUEST = 18802;
/** M5h2GateProfile.h: gameserver.housing.auction.default_bid.house, the initial offer of every auto-filled HOUSE */
constexpr int64_t DEFAULT_BID = 100000;
constexpr int64_t A_BID = 150000;
constexpr int64_t B_BID = 200000;
constexpr int64_t A_KINAH = 1000000;
constexpr int64_t B_KINAH = 100000000;
/** DialogPage.HOUSING_FRIENDLIST.id() (DialogPage.java): FriendPortalAI.handleDialogStart's page */
constexpr uint16_t HOUSING_FRIENDLIST_PAGE = 36;
constexpr int32_t CM_CHAT_MESSAGE_PUBLIC = 27;
/** the GM's seeded legion (G0): an id at the top of the seeded object id range, which IDFactory does not reach in a run */
constexpr int32_t GM_LEGION_ID = ScenarioDatabase::SEEDED_OBJECT_ID_END - 0x100;

/** the gate's oracle answers of m5h-housing: the message ids, the door states and (after L2) the auctioned land house */
struct LandHouseAnswer {
	std::map<std::string, int32_t> messages;
	std::map<std::string, int32_t> doorStates;
	int32_t address = 0, land = 0, map = 0, building = 0, teleportNpc = 0, managerNpc = 0;
	int64_t fee = 0, goldPrice = 0;
	float x = 0, y = 0, z = 0;
	std::map<std::string, std::array<float, 3>> houseNpcs;

	int32_t message(const std::string& name) const {
		const auto found = messages.find(name);
		if (found == messages.end())
			throw std::runtime_error("m5h-housing: no message " + name);
		return found->second;
	}
};

void parseLandHouse(const std::string& text, LandHouseAnswer& answer) {
	const json parsed = json::parse(text);
	for (const auto& [name, id] : parsed.at("messages").items())
		answer.messages[name] = id.get<int32_t>();
	for (const auto& [name, id] : parsed.at("doorStates").items())
		answer.doorStates[name] = id.get<int32_t>();
	for (const auto& [address, house] : parsed.at("landHouses").items()) {
		answer.address = std::stoi(address);
		answer.land = house.at("land").get<int32_t>();
		answer.map = house.at("map").get<int32_t>();
		answer.building = house.at("building").get<int32_t>();
		answer.teleportNpc = house.at("teleportNpc").get<int32_t>();
		answer.managerNpc = house.at("managerNpc").get<int32_t>();
		answer.fee = house.at("fee").get<int64_t>();
		answer.goldPrice = house.at("goldPrice").get<int64_t>();
		answer.x = house.at("x").get<float>();
		answer.y = house.at("y").get<float>();
		answer.z = house.at("z").get<float>();
		for (const auto& [type, spot] : house.at("houseNpcs").items())
			answer.houseNpcs[type] = {spot.at("x").get<float>(), spot.at("y").get<float>(), spot.at("z").get<float>()};
	}
}

/** the wall clock's second of the minute and whether bidding is closed (HousingBidService.isBiddingTime: Sunday from 12:00, server time) */
struct WallClock {
	int32_t second = 0;
	bool sundayAfternoon = false;
};

WallClock wallClock() {
	const std::chrono::zoned_time local{std::chrono::current_zone(), std::chrono::system_clock::now()};
	const std::chrono::local_seconds seconds = std::chrono::floor<std::chrono::seconds>(local.get_local_time());
	const std::chrono::local_days day = std::chrono::floor<std::chrono::days>(seconds);
	const std::chrono::hh_mm_ss<std::chrono::seconds> time{seconds - day};
	WallClock clock;
	clock.second = static_cast<int32_t>(time.seconds().count());
	clock.sundayAfternoon = std::chrono::weekday{day} == std::chrono::Sunday && time.hours().count() >= 12;
	return clock;
}

void runM5h2Gate() {
	const std::string testName = "gs.scenario.m5h2";
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
	const std::filesystem::path outputDir = std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "m5h2";
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

	// the M5h gate's base keys, then the housing keys of M5h2GateProfile.h
	std::map<std::string, std::string> gateKeys;
	gateKeys["gameserver.geodata.enable"] = "false";
	gateKeys["gameserver.npcshouts.enable"] = "false";
	gateKeys["gameserver.analysis.quest_handlers"] = "false";
	for (const auto& [key, value] : m5h2HousingKeys())
		gateKeys[key] = value;

	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.schemaPrefix = "m5h2";
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

	ok = cases.run("S-0", "the servers start with the housing keys (M5h2GateProfile.h)", [&] {
		servers.createSchemas();
		servers.startLoginServer();
		servers.startGameServer();
	});

	const std::string suffix = servers.gameSchema().substr(servers.gameSchema().size() - 8);
	ScenarioClient a, b, g;
	std::array<ScenarioClient*, 3> all{&a, &b, &g};
	const std::array<std::string, 3> labels{"A", "B", "G"};
	for (size_t i = 0; i < all.size(); i++) {
		all[i]->label = labels[i];
		all[i]->account = "m5hh" + std::string(1, static_cast<char>('a' + i)) + suffix;
		all[i]->password = "m5hPassword1";
	}
	// two Elyos bidders: A is outbid, B wins; names are letters only (NameRestrictionService)
	Character ca{"A", "Houseloser", NewCharacter::WARRIOR, "WARRIOR", ""};
	Character cb{"B", "Housewinner", NewCharacter::MAGE, "MAGE", ""};
	Character cg{"G", "Housegm", NewCharacter::WARRIOR, "WARRIOR", ""}; // the GM of G0-G3, created in G0
	std::array<Character*, 2> characters{&ca, &cb};

	// ---- the two-client helpers (M5hScenarioTest.cpp's) ----
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
	/**
	 * The keep-alive of the long waits (L4, L8): AionConnection's ConnectionAliveChecker closes a connection without a client packet for
	 * CM_PING.CLIENT_PING_INTERVAL (180 s) + 5 s, and CM_PING counts a ping sooner than 178 s after the previous one as a timer cheat. So each
	 * client pings 179 s after its last ping (the first one, which CM_PING does not check, 179 s after its enter world).
	 */
	std::map<const ScenarioClient*, std::chrono::steady_clock::time_point> lastPing;
	const auto keepAlive = [&] {
		const auto now = std::chrono::steady_clock::now();
		for (ScenarioClient* client : all) {
			if (!client->game || client->game->client.socket.isClosed() || !lastPing.contains(client))
				continue;
			if (now - lastPing[client] >= 179s) {
				client->game->send(GameSession::CM_PING, GameSession::buildCM_PING());
				lastPing[client] = now;
			}
		}
	};
	const auto until = [&](std::chrono::milliseconds timeout, const std::function<bool()>& done) {
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		while (!done()) {
			if (std::chrono::steady_clock::now() >= deadline)
				return false;
			keepAlive();
			drainAll(100ms);
		}
		return true;
	};
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
	const auto settle = [&] {
		drainAll(1500ms);
		catchUp();
	};
	const auto count = [&](std::string_view sql) { return database.queryLong(schema, sql).value_or(-1); };

	// ---- L0: the oracle ----
	LandHouseAnswer housing;
	const std::vector<std::string> messageNames{"STR_MSG_HOUSING_BID_SUCCESS", "STR_MSG_HOUSING_PRICE_CHANGE", "STR_MSG_HOUSING_BID_CANCEL",
		"STR_MSG_HOUSING_BID_WIN", "STR_MSG_HOUSING_CANT_BID_EXCESS_ACCOUNT", "STR_MSG_HOUSING_CANT_BID_SUCC_BID_HOUSE",
		"STR_MSG_HOUSING_CANT_ENTER_NO_RIGHT2", "STR_MSG_HOUSING_ORDER_CLOSE_DOOR_ALL", "STR_MSG_HOUSING_CANT_AUCTION_TIMEOUT"};
	const auto oracleArguments = [&](std::optional<int32_t> address) {
		std::vector<std::string> arguments{"m5h-housing", "--message"};
		for (const std::string& name : messageNames)
			arguments.push_back(name);
		if (address) {
			arguments.push_back("--land-house");
			arguments.push_back(std::to_string(*address));
		}
		return arguments;
	};
	runCase("L0", "the oracle answers: m5h-housing (the message ids, HouseDoorState)", [&] {
		parseLandHouse(oracle->run(oracleArguments(std::nullopt)), housing);
		EXPECT_EQ(housing.doorStates.at("CLOSED"), 3);
		EXPECT_EQ(housing.messages.size(), messageNames.size());
	});
	const auto hmsg = [&](const std::string& name) { return housing.message(name); };

	// ---- L1: the two bidders ----
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
	const auto endFirstPrologue = [&](ScenarioClient& client, std::string_view label) {
		AsyncAllowed async = client.async;
		async.temporarySpawnUpdates(client.npcs.predicate());
		endPrologue(*client.game, client.lastLevelReady, decoders::ELYOS_PROLOGUE, 0, async, [&] { return collectBurst(*client.game, async); }, label);
		client.model.sync();
	};
	const auto seedKinah = [&](int32_t playerId, int64_t kinah) {
		database.execute(schema, "DELETE FROM inventory WHERE item_owner = " + std::to_string(playerId) + " AND item_id = " + std::to_string(KINAH_ITEM));
		database.seedInventoryItem(schema, {playerId, KINAH_ITEM, kinah, 0, 65535});
	};
	runCase("L1", "two Elyos accounts: create A and B, the prologue, the seeds (kinah, quest 18802 complete), then both enter", [&] {
		for (size_t i = 0; i < characters.size(); i++) {
			ScenarioClient& client = *all[i];
			logIn(servers, client);
			createCharacter(client, *characters[i]);
			client.character = characters[i];
			enterWorld(client, true);
			levelReady(client);
			endFirstPrologue(client, client.label + "'s prologue (1000)");
			disconnect(client);
		}
		seedKinah(ca.playerId, A_KINAH);
		seedKinah(cb.playerId, B_KINAH);
		for (const Character* character : characters)
			database.execute(schema, "INSERT INTO player_quests (player_id, quest_id, status, complete_count) VALUES (" +
			                           std::to_string(character->playerId) + ", " + std::to_string(STUDIO_QUEST) + ", 'COMPLETE', 1)");
		for (size_t i = 0; i < characters.size(); i++) {
			enterAs(servers, *all[i], *characters[i]);
			lastPing[all[i]] = std::chrono::steady_clock::now();
			settle();
		}
		EXPECT_EQ(a.model.kinah(), A_KINAH);
		EXPECT_EQ(b.model.kinah(), B_KINAH);
	});

	// ---- L2: the bids list ----
	const auto bidsOf = [&](const std::vector<Packet>& packets) {
		std::vector<decoders::HouseBids> found;
		for (const Packet& packet : ofName(packets, "SM_HOUSE_BIDS"))
			found.push_back(decoders::decodeHouseBids(packet.data));
		return found;
	};
	const auto listBids = [&](ScenarioClient& client) {
		const size_t from = client.mark();
		client.game->send(CM_GET_HOUSE_BIDS, {});
		if (!until(10s, [&] {
			    const std::vector<decoders::HouseBids> lists = bidsOf(client.since(from));
			    return !lists.empty() && lists.back().isLast;
		    }))
			throw std::runtime_error(client.label + ": no last SM_HOUSE_BIDS after CM_GET_HOUSE_BIDS");
		std::vector<decoders::HouseBidRow> rows;
		for (const decoders::HouseBids& list : bidsOf(client.since(from)))
			rows.insert(rows.end(), list.rows.begin(), list.rows.end());
		return std::make_pair(bidsOf(client.since(from)), rows);
	};
	const bool biddingClosed = wallClock().sundayAfternoon;
	decoders::HouseBidRow auctioned;
	int32_t houseObjectId = 0;
	if (biddingClosed && ok) {
		ok = false;
		skipReason = "Sunday from 12:00 server time: HousingBidService.isBiddingTime refuses every bid";
	}
	runCase("L2", "the bids list: A CM_GET_HOUSE_BIDS between seconds 11 and 17 (after the auto-fill at :10, before the auction end at :20)", [&] {
		// AuctionAutoFillTask fills one Elyos HOUSE at second 10 of every minute and AuctionEndTask ends it unsold at second 20
		const auto deadline = std::chrono::steady_clock::now() + 75s;
		while (std::chrono::steady_clock::now() < deadline) {
			const int32_t second = wallClock().second;
			if (second >= 11 && second <= 17)
				break;
			drainAll(250ms);
		}
		const auto [lists, rows] = listBids(a);
		ASSERT_EQ(rows.size(), 1u) << "gameserver.housing.auction.auto_fill.limit.HOUSE = 1, the other types 0";
		EXPECT_TRUE(lists.front().isFirst);
		EXPECT_EQ(lists.back().lastBidListIndex, 0) << "A has no bid";
		EXPECT_EQ(lists.back().registeredListIndex, 0) << "A registered no house";
		auctioned = rows[0];
		EXPECT_EQ(auctioned.highestBid, DEFAULT_BID) << "House.getDefaultAuctionPrice: HOUSE_MIN_BID";
		EXPECT_EQ(auctioned.fixed100000, 100000);
		EXPECT_EQ(auctioned.bidCount, 0);
		EXPECT_GT(auctioned.remainingSeconds, 0);
		EXPECT_LE(auctioned.remainingSeconds, 10) << "AuctionEndTask's next run at second 20";
		parseLandHouse(oracle->run(oracleArguments(auctioned.address)), housing);
		EXPECT_EQ(auctioned.landId, housing.land) << "houses.xml: the address's land";
		EXPECT_EQ(auctioned.buildingId, housing.building) << "the land's default building";
		houseObjectId = static_cast<int32_t>(count("SELECT id FROM houses WHERE address = " + std::to_string(auctioned.address)));
		EXPECT_GT(houseObjectId, 0) << "HousingBidService.auction saved the NEW house before its bid";
		EXPECT_EQ(count("SELECT COUNT(*) FROM house_bids WHERE house_id = " + std::to_string(houseObjectId)), 1) << "the initial offer";
	});

	// ---- L3: the bids ----
	const auto bid = [&](ScenarioClient& client, int64_t kinah) {
		const size_t from = client.mark();
		client.game->send(CM_PLACE_BID, Body().D(auctioned.listIndex).Q(kinah).data);
		until(10s, [&] { return !ofName(client.since(from), "SM_RECEIVE_BIDS").empty(); });
		settle();
		return client.since(from);
	};
	const std::string address = std::to_string(auctioned.address);
	runCase("L3", "bids: A 150,000, A again (refused), B 200,000 - A outbid and refunded by mail; A twice the bid (refused)", [&] {
		std::vector<Packet> wa = bid(a, A_BID);
		EXPECT_EQ(countMessage(wa, hmsg("STR_MSG_HOUSING_BID_SUCCESS"), {address}), 1u);
		EXPECT_EQ(countMessage(wa, hmsg("STR_MSG_HOUSING_PRICE_CHANGE"), {std::to_string(A_BID)}), 1u);
		EXPECT_EQ(ofName(wa, "SM_RECEIVE_BIDS").size(), 1u);
		EXPECT_EQ(a.model.kinah(), A_KINAH - A_BID) << "player.getInventory().decreaseKinah(bid.getKinah())";
		// A again: already the highest bidder
		wa = bid(a, A_BID + 10000);
		EXPECT_EQ(countMessage(wa, hmsg("STR_MSG_HOUSING_CANT_BID_SUCC_BID_HOUSE")), 1u);
		EXPECT_EQ(a.model.kinah(), A_KINAH - A_BID) << "nothing taken";

		const size_t aFrom = a.mark();
		const std::vector<Packet> wb = bid(b, B_BID);
		EXPECT_EQ(countMessage(wb, hmsg("STR_MSG_HOUSING_BID_SUCCESS"), {address}), 1u);
		EXPECT_EQ(b.model.kinah(), B_KINAH - B_BID);
		settle();
		const std::vector<Packet> outbid = a.since(aFrom);
		EXPECT_EQ(countMessage(outbid, hmsg("STR_MSG_HOUSING_BID_CANCEL")), 1u) << "the previous bidder online: STR_MSG_HOUSING_BID_CANCEL";
		EXPECT_EQ(ofName(outbid, "SM_RECEIVE_BIDS").size(), 1u);
		EXPECT_EQ(a.model.kinah(), A_KINAH - A_BID) << "the refund goes by mail";
		EXPECT_TRUE(until(10s, [&] {
			return count("SELECT COUNT(*) FROM mail WHERE mail_recipient_id = " + std::to_string(ca.playerId) +
			             " AND sender_name = '$$HS_AUCTION_MAIL' AND attached_kinah_count = " + std::to_string(A_BID)) == 1;
		})) << "MailFormatter.sendHouseAuctionMail(FAILED_BID, previousBid.getKinah())";
		// A twice the current bid: the step limit (AUCTION_BID_STEP_LIMIT 100: the offer must stay below twice the highest bid)
		const std::vector<Packet> tooMuch = bid(a, 2 * B_BID);
		EXPECT_EQ(countMessage(tooMuch, hmsg("STR_MSG_HOUSING_CANT_BID_EXCESS_ACCOUNT")), 1u);
		EXPECT_EQ(count("SELECT COUNT(*) FROM house_bids WHERE house_id = " + std::to_string(houseObjectId)), 3) << "the offer and two bids";

		const auto [lists, rows] = listBids(b);
		ASSERT_EQ(rows.size(), 1u);
		EXPECT_EQ(rows[0].highestBid, B_BID);
		EXPECT_EQ(rows[0].bidCount, 2);
		EXPECT_GT(rows[0].remainingSeconds, 60) << "AuctionEndTask.tryProlongAuction: a bid within 5 minutes of the end prolongs it by 5 minutes";
		EXPECT_EQ(lists.back().lastBidListIndex, auctioned.listIndex) << "findLastBid(B)";
		EXPECT_EQ(lists.back().lastBidKinah, B_BID);
	});

	// ---- L4: the end of the auction ----
	runCase("L4", "the prolonged auction ends: B wins the house (STR_MSG_HOUSING_BID_WIN), the house is B's", [&] {
		const size_t from = b.mark();
		ASSERT_TRUE(until(8min, [&] { return countMessage(b.since(from), hmsg("STR_MSG_HOUSING_BID_WIN"), {address}) == 1; }))
		  << "no STR_MSG_HOUSING_BID_WIN within 8 minutes (the prolongation is 5 minutes after the last bid)";
		settle();
		EXPECT_TRUE(until(10s, [&] {
			return count("SELECT player_id FROM houses WHERE id = " + std::to_string(houseObjectId)) == cb.playerId;
		})) << "HousingService.changeOwner(house, buyer)";
		EXPECT_EQ(count("SELECT COUNT(*) FROM house_bids WHERE house_id = " + std::to_string(houseObjectId)), 0) << "HouseBidsDAO.deleteHouseBids";
		EXPECT_EQ(count("SELECT COUNT(*) FROM mail WHERE mail_recipient_id = " + std::to_string(cb.playerId) + " AND sender_name = '$$HS_AUCTION_MAIL'"), 1)
		  << "the WIN_BID mail";
		const auto [lists, rows] = listBids(a);
		for (const decoders::HouseBidRow& row : rows)
			EXPECT_NE(row.address, auctioned.address) << "the sold house is no longer listed";
	});

	// ---- L5: the rent ----
	runCase("L5", "the rent: B CM_HOUSE_PAY_RENT(1) pays one period of the land's fee", [&] {
		const int64_t before = b.model.kinah();
		const size_t from = b.mark();
		b.game->send(CM_HOUSE_PAY_RENT, Body().C(1).data);
		ASSERT_TRUE(until(10s, [&] { return !ofName(b.since(from), "SM_HOUSE_PAY_RENT").empty(); }));
		settle();
		EXPECT_EQ(decoders::decodeHousePayRent(ofName(b.since(from), "SM_HOUSE_PAY_RENT")[0].data), 1);
		EXPECT_EQ(before - b.model.kinah(), housing.fee) << "house.getLand().getMaintenanceFee() * weekCount";
		EXPECT_TRUE(until(10s, [&] {
			return count("SELECT COUNT(*) FROM houses WHERE id = " + std::to_string(houseObjectId) + " AND next_pay IS NOT NULL") == 1;
		})) << "house.setNextPay, house.save";
	});

	// ---- L6: the door ----
	/** a FADE_OUT_BEAM teleport: SM_TELEPORT_LOC, then CM_TELEPORT_ANIMATION_DONE; another map answers SM_PLAYER_SPAWN (and CM_LEVEL_READY), the same
	 * map SM_PLAYER_INFO (TeleportService.SpawnTask.run) */
	const auto teleportArrive = [&](ScenarioClient& client, size_t from, std::string_view label) {
		if (!until(15s, [&] { return !ofName(client.since(from), "SM_TELEPORT_LOC").empty(); }))
			throw std::runtime_error(std::string(label) + ": no SM_TELEPORT_LOC");
		const decoders::TeleportLoc loc = decoders::decodeTeleportLoc(ofName(client.since(from), "SM_TELEPORT_LOC")[0].data);
		EXPECT_EQ(loc.animation, decoders::TELEPORT_ANIMATION_FADE_OUT_BEAM) << label;
		const size_t done = client.mark();
		client.game->send(GameSession::CM_TELEPORT_ANIMATION_DONE, GameSession::buildCM_TELEPORT_ANIMATION_DONE());
		if (!until(15s, [&] {
			    const std::vector<Packet> arrival = client.since(done);
			    return !ofName(arrival, "SM_PLAYER_SPAWN").empty() || !ofName(arrival, "SM_PLAYER_INFO").empty();
		    }))
			throw std::runtime_error(std::string(label) + ": no arrival after CM_TELEPORT_ANIMATION_DONE");
		if (!ofName(client.since(done), "SM_PLAYER_SPAWN").empty())
			levelReady(client);
		client.x = loc.x;
		client.y = loc.y;
		client.z = loc.z;
		settle();
		return loc;
	};
	runCase("L6", "the door: B closes it; A is refused (STR_MSG_HOUSING_CANT_ENTER_NO_RIGHT2); B enters through it", [&] {
		size_t from = b.mark();
		b.game->send(CM_HOUSE_SETTINGS, Body().C(housing.doorStates.at("CLOSED")).C(0).S("").data);
		ASSERT_TRUE(until(10s, [&] { return countMessage(b.since(from), hmsg("STR_MSG_HOUSING_ORDER_CLOSE_DOOR_ALL")) == 1; }));
		from = a.mark();
		a.game->send(CM_HOUSE_OPEN_DOOR, Body().D(auctioned.address).C(0).data);
		ASSERT_TRUE(until(10s, [&] { return countMessage(a.since(from), hmsg("STR_MSG_HOUSING_CANT_ENTER_NO_RIGHT2")) == 1; }));
		settle();
		EXPECT_TRUE(ofName(a.since(from), "SM_TELEPORT_LOC").empty()) << "house.canEnter(A) is false: no teleport";
		from = b.mark();
		b.game->send(CM_HOUSE_OPEN_DOOR, Body().D(auctioned.address).C(0).data);
		const decoders::TeleportLoc loc = teleportArrive(b, from, "B enters");
		EXPECT_EQ(loc.mapId, housing.map) << "HouseController.teleportNearHouseDoor(player, false): the house's map";
		const auto& crystal = housing.houseNpcs.at("TELEPORT");
		const auto& butler = housing.houseNpcs.at("MANAGER");
		EXPECT_NEAR(loc.x, (crystal[0] + butler[0]) / 2, 0.01) << "the midpoint between the butler and the relationship crystal";
		EXPECT_NEAR(loc.y, (crystal[1] + butler[1]) / 2, 0.01);
	});

	// ---- L7: the relationship crystal ----
	runCase("L7", "the relationship crystal: B's dialog (friend list page), B teleports home (CM_HOUSE_TELEPORT 1) and leaves by the door; A refused", [&] {
		const auto& spot = housing.houseNpcs.at("TELEPORT");
		std::optional<int32_t> crystal;
		ASSERT_TRUE(until(10s, [&] { return (crystal = npcObject(b, housing.teleportNpc, spot[0], spot[1])).has_value(); }))
		  << "B was shown no relationship crystal (" << housing.teleportNpc << ") at its spot";
		size_t from = b.mark();
		b.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(*crystal));
		ASSERT_TRUE(until(10s, [&] { return !ofName(b.since(from), "SM_DIALOG_WINDOW").empty(); })) << "FriendPortalAI.handleDialogStart";
		settle();
		EXPECT_EQ(ofName(b.since(from), "SM_FRIEND_LIST").size(), 1u) << "the owner: SM_FRIEND_LIST before the page";
		EXPECT_EQ(decoders::decodeDialogWindow(ofName(b.since(from), "SM_DIALOG_WINDOW")[0].data).dialogPageId, HOUSING_FRIENDLIST_PAGE);
		// A's teleport without a crystal target: CM_HOUSE_TELEPORT returns (an audit line for a foreign id)
		from = a.mark();
		a.game->send(CM_HOUSE_TELEPORT, Body().C(1).D(ca.playerId).D(0).data);
		settle();
		EXPECT_TRUE(ofName(a.since(from), "SM_TELEPORT_LOC").empty()) << "A targets no relationship crystal";
		// B: target the crystal, then teleport to the own house
		b.game->send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(*crystal));
		settle();
		from = b.mark();
		b.game->send(CM_HOUSE_TELEPORT, Body().C(1).D(cb.playerId).D(0).data);
		const decoders::TeleportLoc home = teleportArrive(b, from, "B teleports home");
		EXPECT_EQ(home.mapId, housing.map);
		EXPECT_NEAR(home.x, housing.x, 0.01) << "TeleportService.teleportTo(the house's instance, house.getX(), ...)";
		EXPECT_NEAR(home.y, housing.y, 0.01);
		// no way back by CM_HOUSE_TELEPORT_BACK (no house gate set a return point), the door's leave instead
		from = b.mark();
		b.game->send(CM_HOUSE_TELEPORT_BACK, {});
		settle();
		EXPECT_TRUE(ofName(b.since(from), "SM_TELEPORT_LOC").empty()) << "getBattleReturnCoords() is null";
		from = b.mark();
		b.game->send(CM_HOUSE_OPEN_DOOR, Body().D(auctioned.address).C(1).data);
		const decoders::TeleportLoc out = teleportArrive(b, from, "B leaves");
		EXPECT_EQ(out.mapId, housing.map) << "a land house has no exit map: teleportNearHouseDoor(player, true)";
		EXPECT_GT(distance2d(out.x, out.y, home.x, home.y), 0.5) << "outside, 2.5 m behind the door's midpoint";
	});

	// ---- G0-G3: the admin commands //auction, //house and //legion (m5h-plan.md 14.3, H2-5) ----
	/** the GM's mansion: an Elyos MANSION in Oriel (houses.xml land 327001), which the auto-fill (limit 0 for MANSION) never puts on sale */
	const int32_t mansion = 10034;
	const std::string mansionText = std::to_string(mansion);
	const std::string gmLegion = "Gmbanner";
	LandHouseAnswer mansionHouse;
	/** a GM line in the normal chat (ChatType NORMAL 0): ChatProcessor takes "//" lines as admin commands; @return the SM_MESSAGE texts after it */
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
	const auto said = [](const std::vector<std::string>& texts, std::string_view text) {
		return std::count_if(texts.begin(), texts.end(), [&](const std::string& t) { return t.find(text) != std::string::npos; });
	};
	runCase("G0", "the GM: create G, the prologue, access level 9 (commands.properties: auction, house and legion 9), a seeded legion; G enters", [&] {
		logIn(servers, g);
		createCharacter(g, cg);
		g.character = &cg;
		enterWorld(g, true);
		levelReady(g);
		endFirstPrologue(g, "G's prologue (1000)");
		disconnect(g);
		servers.loginDatabase().execute(servers.loginSchema(), "UPDATE account_data SET access_level = 9 WHERE name = '" + g.account + "'");
		ASSERT_FALSE(ScenarioDatabase::isInvalidObjectId(GM_LEGION_ID));
		database.execute(schema, "INSERT INTO legions (id, name, level, contribution_points) VALUES (" + std::to_string(GM_LEGION_ID) + ", '" + gmLegion +
		                           "', 1, 0)");
		database.execute(schema, "INSERT INTO legion_members (legion_id, player_id, `rank`) VALUES (" + std::to_string(GM_LEGION_ID) + ", " +
		                           std::to_string(cg.playerId) + ", 'BRIGADE_GENERAL')");
		enterAs(servers, g, cg);
		lastPing[&g] = std::chrono::steady_clock::now();
		settle();
		parseLandHouse(oracle->run(oracleArguments(mansion)), mansionHouse);
		EXPECT_EQ(mansionHouse.land, 327001);
	});

	const auto houseRow = [&](std::string_view column) {
		return count("SELECT " + std::string(column) + " FROM houses WHERE address = " + mansionText);
	};
	runCase("G1", "//auction: the mansion auctioned, again (refused), cancelled, ended (not for sale); an unknown address", [&] {
		std::vector<std::string> texts = gm("//auction " + mansionText + " 120000");
		EXPECT_EQ(said(texts, "Address " + mansionText + " was auctioned successfully."), 1);
		const int64_t mansionId = houseRow("id");
		EXPECT_GT(mansionId, 0) << "HousingBidService.auction saved the NEW house";
		EXPECT_EQ(count("SELECT COUNT(*) FROM house_bids WHERE house_id = " + std::to_string(mansionId) + " AND bid = 120000"), 1);
		texts = gm("//auction " + mansionText);
		EXPECT_EQ(said(texts, "Address " + mansionText + " is already in auction."), 1);
		texts = gm("//auction cancel " + mansionText);
		EXPECT_EQ(said(texts, "Canceled auction for mansion " + mansionText), 1);
		EXPECT_EQ(count("SELECT COUNT(*) FROM house_bids WHERE house_id = " + std::to_string(mansionId)), 0);
		texts = gm("//auction end " + mansionText);
		EXPECT_EQ(said(texts, "The house is not for sale."), 1);
		texts = gm("//auction 99999");
		EXPECT_EQ(said(texts, "Invalid address."), 1);
	});

	runCase("G2", "//house: list; own (G targets himself), own again (refused), tp, reloadscripts, revoke, revoke again (refused)", [&] {
		std::vector<std::string> texts = gm("//house list");
		EXPECT_GE(said(texts, "House addresses in "), 6) << "one header per map with houses (Oriel, Pernon and the four Elyos and Asmodian fields)";
		g.game->send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(cg.playerId));
		settle();
		texts = gm("//house own " + mansionText);
		EXPECT_EQ(said(texts, "House HOUSE_" + mansionText + " is now owned by "), 1) << "House.getName(): HOUSE_<address>";
		EXPECT_EQ(houseRow("player_id"), cg.playerId);
		texts = gm("//house own " + mansionText);
		EXPECT_EQ(said(texts, " already owns that house."), 1);
		// tp: TeleportService.teleportTo(admin, the house's instance, its point, getTeleportHeading()) - no animation: the spawn at once
		size_t from = g.mark();
		g.game->send(CM_CHAT_MESSAGE_PUBLIC, Body().C(0).S("//house tp " + mansionText).data);
		ASSERT_TRUE(until(15s, [&] { return !ofName(g.since(from), "SM_PLAYER_SPAWN").empty(); })) << "G was not spawned at the mansion";
		const decoders::PlayerSpawn spawn = decoders::decodePlayerSpawn(ofName(g.since(from), "SM_PLAYER_SPAWN")[0].data);
		levelReady(g);
		settle();
		EXPECT_EQ(spawn.worldId, mansionHouse.map);
		EXPECT_NEAR(spawn.x, mansionHouse.x, 0.01);
		EXPECT_NEAR(spawn.y, mansionHouse.y, 0.01);
		texts = gm("//house reloadscripts " + mansionText);
		EXPECT_EQ(said(texts, "Script reload successful"), 1) << "the mansion's butler (HousingService.spawnHouses)";
		texts = gm("//house revoke " + mansionText);
		EXPECT_EQ(said(texts, "Ownership of house " + mansionText + " was revoked from " + cg.name), 1);
		EXPECT_EQ(houseRow("player_id"), 0);
		texts = gm("//house revoke " + mansionText);
		EXPECT_EQ(said(texts, "House has no owner."), 1);
	});

	runCase("G3", "//legion: info, setlevel (and its range), setpoints, add A, kick A, rename, an unknown legion, disband", [&] {
		const std::string legionId = std::to_string(GM_LEGION_ID);
		std::vector<std::string> texts = gm("//legion info " + gmLegion);
		EXPECT_EQ(said(texts, "Legion name: " + gmLegion), 1);
		EXPECT_EQ(said(texts, "Level: 1"), 1);
		EXPECT_EQ(said(texts, "Members (1):"), 1);
		EXPECT_EQ(said(texts, cg.name + " (lv "), 1);
		EXPECT_EQ(said(texts, ", brigade general)"), 1);
		texts = gm("//legion setlevel " + gmLegion + " 9");
		EXPECT_EQ(said(texts, "Legion level must be between 1 and 8."), 1);
		texts = gm("//legion setlevel " + gmLegion + " 2");
		EXPECT_EQ(said(texts, "Legion " + gmLegion + " level was changed from 1 to 2"), 1);
		EXPECT_TRUE(until(10s, [&] { return count("SELECT level FROM legions WHERE id = " + legionId) == 2; }));
		texts = gm("//legion setpoints " + gmLegion + " 500");
		EXPECT_EQ(said(texts, "Legion " + gmLegion + " contribution points were changed from 0 to 500"), 1);
		EXPECT_TRUE(until(10s, [&] { return count("SELECT contribution_points FROM legions WHERE id = " + legionId) == 500; }));
		texts = gm("//legion add " + gmLegion + " " + ca.name);
		EXPECT_EQ(said(texts, " was added to " + gmLegion), 1) << "ChatCommand.name(target): the chat link of the name";
		EXPECT_TRUE(until(10s, [&] {
			return count("SELECT COUNT(*) FROM legion_members WHERE legion_id = " + legionId + " AND player_id = " + std::to_string(ca.playerId)) == 1;
		}));
		texts = gm("//legion kick " + ca.name);
		EXPECT_EQ(said(texts, " was kicked from the legion."), 1);
		EXPECT_TRUE(until(10s, [&] {
			return count("SELECT COUNT(*) FROM legion_members WHERE player_id = " + std::to_string(ca.playerId)) == 0;
		}));
		const std::string renamed = "Gmbannertwo";
		texts = gm("//legion rename " + gmLegion + " " + renamed);
		EXPECT_EQ(said(texts, "Legion " + gmLegion + " was renamed to " + renamed + "."), 1);
		EXPECT_TRUE(until(10s, [&] { return count("SELECT COUNT(*) FROM legions WHERE name = '" + renamed + "'") == 1; }));
		texts = gm("//legion info Nosuchlegion");
		EXPECT_EQ(said(texts, "Legion Nosuchlegion does not exist."), 1) << "getLegion's IllegalArgumentException, shown by ChatCommand.run";
		texts = gm("//legion disband " + renamed);
		EXPECT_EQ(said(texts, "Legion " + renamed + " was disbanded."), 1);
		EXPECT_TRUE(until(10s, [&] { return count("SELECT COUNT(*) FROM legions WHERE id = " + legionId) == 0; }));
	});

	// ---- L8: the maintenance cron ----
	std::optional<int32_t> gameServerExit;
	bool gameServerRestarted = false;
	const auto checkRunReports = [&](std::string_view label) {
		EXPECT_TRUE(servers.readReportLines("unported_trace.txt").empty()) << label << ": AION_UNPORTED sites were reached:\n"
		                                                                   << join(servers.readReportLines("unported_trace.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("census.txt").empty()) << label << ": " << join(servers.readReportLines("census.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("lockdep.txt").empty()) << label << ": " << join(servers.readReportLines("lockdep.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("watchdog.txt").empty()) << label << ": " << join(servers.readReportLines("watchdog.txt"), "\n");
		for (const PartialHit& hit : readPartialHits(servers))
			std::cout << label << ": AION_PARTIAL " << hit.line << std::endl;
		ASSERT_NE(servers.gameServer(), nullptr);
		EXPECT_TRUE(servers.gameServer()->findLogLines(" ERROR ").empty()) << label << ": " << join(servers.gameServer()->findLogLines(" ERROR "), "\n");
	};
	runCase("L8", "the maintenance cron: the overdue notices by mail; then, B's rent 15 days overdue across a restart, the impound", [&] {
		// every maintenance run after the paid period sends B an overdue notice (MaintenanceTask: impound in 14 days, $$HS_OVERDUE_1ST)
		EXPECT_TRUE(until(4min, [&] {
			return count("SELECT COUNT(*) FROM mail WHERE mail_recipient_id = " + std::to_string(cb.playerId) + " AND sender_name = '$$HS_OVERDUE_1ST'") >= 1;
		})) << "no $$HS_OVERDUE_1ST mail within 4 minutes of the paid minute";
		for (ScenarioClient* client : all)
			disconnect(*client);
		gameServerExit = servers.stopGameServer();
		ASSERT_TRUE(gameServerExit);
		EXPECT_EQ(*gameServerExit, 0);
		checkRunReports("the first run");
		// the house's next payment 15 days ago: the next maintenance run puts it to auction (calculateImpoundDate <= now)
		database.execute(schema, "UPDATE houses SET next_pay = NOW() - INTERVAL 15 DAY WHERE id = " + std::to_string(houseObjectId));
		gameServerExit.reset();
		servers.startGameServer();
		gameServerRestarted = true;
		const auto deadline = std::chrono::steady_clock::now() + 3min;
		while (std::chrono::steady_clock::now() < deadline &&
		       count("SELECT player_id FROM houses WHERE id = " + std::to_string(houseObjectId)) != 0)
			std::this_thread::sleep_for(1s);
		EXPECT_EQ(count("SELECT player_id FROM houses WHERE id = " + std::to_string(houseObjectId)), 0)
		  << "MaintenanceTask.putHouseToAuction: changeOwner(house, 0)";
		EXPECT_TRUE(until(30s, [&] {
			return count("SELECT COUNT(*) FROM mail WHERE mail_recipient_id = " + std::to_string(cb.playerId) +
			             " AND sender_name = '$$HS_OVERDUE_3RD' AND attached_kinah_count = " + std::to_string(static_cast<int64_t>(DEFAULT_BID * 0.9f))) == 1;
		})) << "MailFormatter.sendHouseMaintenanceMail($$HS_OVERDUE_3RD, 90% of the default auction price)";
		EXPECT_FALSE(servers.gameServer()->findLogLines("because maintenance fee was overdue", 5).empty()) << "putHouseToAuction's log line";
	});

	if (servers.gameServer() != nullptr && !gameServerExit)
		gameServerExit = servers.stopGameServer();
	for (ScenarioClient* client : all)
		if (client->game)
			client->game->waitClosed(60s);
	const std::optional<int32_t> loginServerExit = servers.loginServer() != nullptr ? servers.stopLoginServer() : std::nullopt;

	cases.run("L9", "reports: exits, unported sites, census, lock order, watchdog, ERROR lines", [&] {
		ASSERT_TRUE(gameServerExit) << "the game server did not exit after the stop file was written";
		EXPECT_EQ(*gameServerExit, 0);
		ASSERT_TRUE(loginServerExit);
		EXPECT_TRUE(*loginServerExit == 0 || *loginServerExit == 98) << "the login server exited with " << *loginServerExit;
		checkRunReports(gameServerRestarted ? "the restarted run" : "the run");
	});

	finishRun(servers, outputDir, testName);
}

} // namespace

/** `gs.scenario.m5h2` (m5h-plan.md 14.3, H2-4): a land house through the auction, its rent, door and relationship crystal, and the impound */
TEST(M5h2Scenario, Run) {
	runM5h2Gate();
}

} // namespace aion::gameserver::scenario
