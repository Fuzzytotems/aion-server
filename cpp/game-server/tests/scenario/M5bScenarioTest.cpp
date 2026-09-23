// The M5b-1 scenario gate (m5b-plan.md G-03 and G-06, §6): one login server and one game server as child processes on their own test
// schemas, a fake client that logs in, creates an Elyos Warrior, enters the world, walks up to a known Poeta monster, shoots once from out of
// range, closes, kills it, watches it respawn, then dies to it and revives at its bind point - and the reports the server writes at shutdown.
//
// Every assertion is independent of the C++ server code, exactly as the M5a gate is: server packets are read with the decoders of
// tests/scenario/decoders (written from the Java writeImpl methods, m5a-plan.md D9), the expected values come from
// `tools/oracle/oracle.py m5b-monster` (G-01) or from direct database queries, and the packet order comes from PacketSequence with the
// async-allowed set of §5.9.
//
// **This file deliberately does not share M5aScenarioTest.cpp's helpers.** Both gates own one pair of server processes and both are chunk
// P5-SC, but they are written and re-measured by different lanes in the same wave (G-03/G-06 here, G-05 there), and the M5a helpers live in
// an anonymous namespace of that translation unit. Lifting them into a shared header is the right end state and is left as a follow-up; doing
// it inside the wave would have put two lanes into the same 2,600-line file. What is duplicated is the scaffolding (the case log, the burst
// collector, the login conversation, the report readers), never an assertion: no expectation of §6 is stated twice.
//
// It holds TWO gates, like M5aScenarioTest.cpp: M5bScenario.Run (gs.scenario.m5b, geo off) and M5bScenarioGeo.Run (gs.scenario.m5b_geo, geo
// on). They run the SAME scripted fight through one shared body, because G-06 is "the same scripted fight with gameserver.geodata.enable=true"
// - what the geo run adds is the world the fight happens in. See the comment above TEST(M5bScenarioGeo, Run) for what that buys and, just as
// importantly, for the three §6.4 assertions it does NOT make and why.

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
#include <vector>

#include "AsyncAllowed.h"
#include "FakeLoginClient.h"
#include "GameSession.h"
#include "Oracle.h"
#include "PacketSequence.h"
#include "ScenarioServers.h"
#include "decoders/CombatDecoders.h"
#include "decoders/PacketDecoders.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {
namespace {

using namespace std::chrono_literals;
using decoders::DecodeError;
using Packet = GameSession::Packet;

/** the quiet period that ends a burst of server packets (m5a-plan.md §5.4) */
constexpr std::chrono::milliseconds QUIET = 1000ms;
constexpr std::chrono::milliseconds BURST_LIMIT = 90s;

/** SM_CREATE_CHARACTER response codes (SM_CREATE_CHARACTER.java) */
constexpr int32_t RESPONSE_OK = 0;
constexpr int32_t RESPONSE_OPEN_CREATION_WINDOW = 22;

/** PlayerClass.WARRIOR */
constexpr int32_t CLASS_WARRIOR = 0;

/** the Elyos start map and the monster of D11 */
constexpr int32_t ELYOS_START_MAP = 210010000;
constexpr int32_t GATE_MONSTER_NPC_ID = 210663;

/**
 * §6.3 A5b: the npc the **positive** half of A5 is measured against, and the reason A5 needs a second npc at all.
 *
 * `ai="aggressive"` only picks the AI class that looks at what it sees (`AggressiveNpcAI` overrides `handleCreatureSee`); it does not decide
 * whether the npc starts a fight. That decision is `TribeRelationService::isAggressive(npc, creature)` (CreatureEventHandler.java:87), which
 * for a character ends in `TRIBE_RELATIONS_DATA.isAggressiveRelation(npcTribe, PC)`. GATE_MONSTER_NPC_ID is `ai="aggressive"` **and** tribe
 * MONSTER, and the real `tribe_relations.xml` MONSTER row (:2170-2173) carries no `<aggro>` element at all - neither does the PC row
 * (:2302-2305) - so a MONSTER-tribe npc never aggroes a character, in Java exactly as in this port. npc 210673 "paruru slowlegs" is
 * `ai="aggressive"` too and its tribe AGGRESSIVESINGLEMONSTER **does** carry `<aggro>PC PC_DARK</aggro>` (:29-31), so it does.
 *
 * **Why this id of the ones that qualify.** It is the nearest aggressive-to-PC spawn to the Elyos spawn point on the whole of Poeta (586.6 m;
 * the next are the KRALL tursins at 923 m, and the TOWERMAN dukaki towers at 1,184 m); it has exactly **one** spawn spot on the map, so the
 * object K8b pins cannot be confused with a sibling and it is the only aggressive-to-PC npc within 60 m of that spot; and it is level 4 rather
 * than the KRALLs' 8-10, which is what lets K8b's freshly revived character walk away from it. `TribeRelationService::isAggressive`'s
 * hard-coded `case AGGRESSIVESINGLEMONSTER` arm is about YUN_GUARD and breaks for a PC, so the decision for this npc falls through to the
 * relation table exactly as KRALL's does. The unit case of the same pair, both sides, is `tests/ai/UnprovokedAggroTest.cpp`.
 */
constexpr int32_t AGGRESSIVE_TRIBE_NPC_ID = 210673;

/** SM_SYSTEM_MESSAGE.STR_GET_EXP (SM_SYSTEM_MESSAGE.gen.h; "You have gained %num1 XP from %0.") - R1 (a) */
constexpr int32_t STR_GET_EXP = 1370000;

/**
 * §6.2 K4: the stand-off distance of the out-of-range shot. It has to be outside the oracle's `toleranceRange` (3.91 m for this pair) so that
 * A1a can demand TARGET_TOO_FAR_AWAY, and outside the monster's aggro range (8 m plus the two bound radii, 8.81 m) so that the monster has
 * not aggroed and is not already walking towards the character while the shot goes out.
 */
constexpr double STAND_OFF_DISTANCE = 12.0;
/** §6.2 K4b: inside `attackRange` (3.31 m) and inside the aggro range, so the first in-range shot lands and the monster starts to aggro */
constexpr double MELEE_DISTANCE = 2.0;
/** A8: how far the character runs from the monster to make it choose between chasing and giving up */
constexpr double CHASE_AWAY_DISTANCE = 40.0;

/** LifeStatsRestoreService: HpMpRestoreTask's initial delay and period (LifeStatsRestoreService.cpp:204, .h:25) - A6 (iii) */
constexpr std::chrono::milliseconds RESTORE_FIRST_TICK = 1700ms;
constexpr std::chrono::milliseconds RESTORE_PERIOD = 6000ms;

/** RespawnService::IMMEDIATE_DECAY (RespawnService.h:49): the corpse of an npc that dropped nothing is deleted 2 s after its death - R2 */
constexpr std::chrono::milliseconds IMMEDIATE_DECAY = 2000ms;

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

std::vector<Packet> ofName(const std::vector<Packet>& packets, std::string_view name) {
	std::vector<Packet> result;
	for (const Packet& packet : packets)
		if (packet.name == name)
			result.push_back(packet);
	return result;
}

const Packet* firstOfName(const std::vector<Packet>& packets, std::string_view name) {
	for (const Packet& packet : packets)
		if (packet.name == name)
			return &packet;
	return nullptr;
}

/** milliseconds between two recorded packets */
int64_t millisBetween(const Packet& earlier, const Packet& later) {
	return std::chrono::duration_cast<std::chrono::milliseconds>(later.receivedAt - earlier.receivedAt).count();
}

// ---- four decoders this gate needs and decoders/ does not have yet ----------------------------------------------------------------------
//
// G-04's brief is the seven combat packets (CombatDecoders.h). §6.3 T1, R1 (a) and R1 (b) read three more - SM_TARGET_SELECTED,
// SM_TARGET_UPDATE, SM_SYSTEM_MESSAGE's parameter list and SM_STATUPDATE_EXP - and the M5a decoders only reach SM_SYSTEM_MESSAGE's message
// id. They are written here to the same rule as everything in decoders/ (m5a-plan.md D9): from the Java writeImpl alone, never from a C++
// serverpackets header, and each consumes the body exactly. They belong in decoders/ and are a request to the G-04 lane, not a new rule.

/** SM_TARGET_SELECTED (SM_TARGET_SELECTED.java:33-40), a 22-byte body */
struct TargetSelected {
	int32_t targetObjectId = 0;
	uint16_t level = 0;
	int32_t maxHp = 0, currentHp = 0, maxMp = 0, currentMp = 0;
};

TargetSelected decodeTargetSelected(std::span<const uint8_t> body) {
	decoders::BodyReader reader(body, "SM_TARGET_SELECTED");
	TargetSelected target;
	target.targetObjectId = reader.D();
	target.level = reader.H();
	target.maxHp = reader.D();
	target.currentHp = reader.D();
	target.maxMp = reader.D();
	target.currentMp = reader.D();
	reader.expectFullyConsumed();
	return target;
}

/** SM_TARGET_UPDATE (SM_TARGET_UPDATE.java:19-21), an 8-byte body: the player and what it now targets (0 when it unselected) */
struct TargetUpdate {
	int32_t playerObjectId = 0;
	int32_t targetObjectId = 0;
};

TargetUpdate decodeTargetUpdate(std::span<const uint8_t> body) {
	decoders::BodyReader reader(body, "SM_TARGET_UPDATE");
	TargetUpdate update;
	update.playerObjectId = reader.D();
	update.targetObjectId = reader.D();
	reader.expectFullyConsumed();
	return update;
}

/** SM_STATUPDATE_EXP (SM_STATUPDATE_EXP.java:33-40), five writeQ */
struct StatUpdateExp {
	int64_t currentExp = 0, recoverableExp = 0, maxExp = 0, currentBoostExp = 0, maxBoostExp = 0;
};

StatUpdateExp decodeStatUpdateExp(std::span<const uint8_t> body) {
	decoders::BodyReader reader(body, "SM_STATUPDATE_EXP");
	StatUpdateExp exp;
	exp.currentExp = reader.Q();
	exp.recoverableExp = reader.Q();
	exp.maxExp = reader.Q();
	exp.currentBoostExp = reader.Q();
	exp.maxBoostExp = reader.Q();
	reader.expectFullyConsumed();
	return exp;
}

/**
 * SM_SYSTEM_MESSAGE (SM_SYSTEM_MESSAGE.java:28940-28953) with its parameter lists. R1 (a) reads `params[1]`, which is the experience reward
 * as a decimal string: `STR_GET_EXP(name, reward)` fills params with the npc's name and the reward, and writeImpl writes every parameter with
 * `writeS(param.toString())` - so the number really is on the wire and does not have to be inferred.
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
int32_t failedAssertions() {
	const ::testing::TestResult* result = ::testing::UnitTest::GetInstance()->current_test_info()->result();
	int32_t failed = 0;
	for (int i = 0; i < result->total_part_count(); i++)
		if (result->GetTestPartResult(i).failed())
			failed++;
	return failed;
}

/** Runs the cases in order, records their result and never lets one case's exception end the run silently */
class CaseLog {
public:
	bool run(std::string_view id, std::string_view title, const std::function<void()>& body) {
		CaseResult result;
		result.id = id;
		result.title = title;
		result.ran = true;
		const int32_t failedBefore = failedAssertions();
		const auto started = std::chrono::steady_clock::now();
		{
			SCOPED_TRACE(std::string(id) + ": " + std::string(title));
			try {
				body();
			} catch (const std::exception& exception) {
				result.error = exception.what();
				ADD_FAILURE() << id << " (" << title << ") ended with an exception: " << exception.what();
			}
		}
		result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
		result.failed = failedAssertions() > failedBefore;
		results.push_back(result);
		return !result.failed;
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

/**
 * The object ids the server announced as npcs, for the npc half of the async-allowed set (§5.9, m5b-plan.md D2). Identical in purpose to the
 * class of the same name in M5aScenarioTest.cpp: "an npc" means "an object this connection was sent an SM_NPC_INFO for".
 */
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

	std::string describe(int32_t objectId) {
		scan();
		const auto found = ids.find(objectId);
		if (found == ids.end())
			return "object " + std::to_string(objectId) + " (NOT an announced npc)";
		return "npc " + std::to_string(found->second) + " (object " + std::to_string(objectId) + ")";
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
				const decoders::NpcInfo npc = decoders::decodeNpcInfo(packets[scanned].data);
				ids.emplace(npc.objectId, npc.templateId);
			} catch (const DecodeError&) {
				try {
					ids.emplace(decoders::decodeNpcInfoObjectId(packets[scanned].data), 0);
				} catch (const DecodeError&) {
					// the packet announced no id at all
				}
			}
		}
	}

	const GameSession* session = nullptr;
	size_t scanned = 0;
	std::map<int32_t, int32_t> ids;
};

/**
 * A burst ends `quiet` after the last packet the §5.8 sequence still has to explain, i.e. after the last packet `async` does NOT allow
 * (m5b-plan.md D2/G-05: with the three root AI handlers registered the neighbourhood never goes quiet for a whole second). Nothing is
 * dropped - every packet inside the window is returned, the async ones included.
 */
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

/**
 * Reads and records everything that arrives within a FIXED window, and does not wait for a quiet period.
 *
 * collectBurst cannot be used inside the fight: its window ends `quiet` after the last packet the async set does not allow, and during a
 * fight every SM_ATTACK and SM_ATTACK_STATUS about the character is exactly such a packet, so the burst would run to its limit. Worse for A2,
 * it would also let the attack interval pass, and the "too early" CM_ATTACK the case exists to send would arrive on time.
 */
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
 * Waits for an `SM_ATTACK` whose attacker is `attackerObjectId`, recording everything on the way, and answers how long it took.
 *
 * This is the reader of the **unprovoked** aggro (§2.1 step 11, §6.3 A5): `NpcController::see` or `MovementNotifyTask` -> checkAggro ->
 * CREATURE_AGGRO -> the 500 ms AggroNotifier -> `addHate(target, 1)` -> the ATTACK event -> AttackEventHandler -> AttackManager. A fight that
 * the character starts with a CM_ATTACK proves none of it: `addDamage` puts the attacker on the hate list directly, so the monster would swing
 * back through a completely different path.
 */
std::optional<std::chrono::milliseconds> waitForAttackBy(GameSession& session, int32_t attackerObjectId, std::chrono::milliseconds timeout) {
	const auto start = std::chrono::steady_clock::now();
	const auto deadline = start + timeout;
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
		if (packet->name != "SM_ATTACK")
			continue;
		try {
			if (decoders::decodeAttack(packet->data).attackerObjectId == attackerObjectId)
				return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
		} catch (const DecodeError&) {
			// the recording keeps the packet; recordFight reports the decode failure where it belongs
		}
	}
}

/** true when `expected` is a subsequence of `names`, i.e. all of it appears in that relative order */
bool containsInOrder(const std::vector<std::string>& names, const std::vector<std::string>& expected) {
	size_t next = 0;
	for (const std::string& name : names) {
		if (next < expected.size() && name == expected[next])
			next++;
	}
	return next == expected.size();
}

/**
 * Reads until a packet with that name arrives and records everything on the way, whatever it is.
 *
 * expectNext below is the right reader for the login conversation, where the answer to each client packet is the next one and anything else is
 * news. It is the wrong one from K4 on: the character is walking through a populated map, so SM_DELETE and SM_NPC_INFO of the npcs it walks
 * past and out of arrive between any two packets the script asked for, none of them is in the async-allowed set (§5.9 allows npc *activity*,
 * not the knownlist updates a moving character causes), and expectNext would throw on the first one. The order those packets are in is §5.6's
 * business, and the M5a gate owns it; what this gate needs is the one packet its case is about.
 *
 * @throws std::runtime_error on timeout or close
 */
Packet waitFor(GameSession& session, std::string_view name, std::chrono::milliseconds timeout = 15s) {
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	for (;;) {
		const auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
			throw std::runtime_error("timeout waiting for " + std::string(name));
		std::optional<Packet> packet = session.next(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
		if (!packet) {
			if (session.client.socket.isClosed())
				throw std::runtime_error("expected " + std::string(name) + ", the connection closed");
			continue;
		}
		if (packet->name == name)
			return *packet;
	}
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
void expectSequence(const std::vector<Packet>& packets, std::string_view pattern, const AsyncAllowed& async) {
	const PacketSequence sequence = PacketSequence::parse(pattern);
	const std::vector<std::string> names = namesOf(packets);
	const PacketSequence::Result result = sequence.match(names, async.predicate(packets));
	EXPECT_TRUE(result.matched) << result.message << "\n  expected: " << sequence.toString() << "\n  got (" << names.size() << "): " << join(names);
}

/** The CM_ENTER_WORLD part of m5a-plan.md §5.8 (#0 to #32); the M5b gate replays it unchanged, as §6.2 K1-K3 asks */
std::string enterWorldPattern(bool firstEnter, int32_t inventoryPackets) {
	std::string pattern;
	if (firstEnter)
		pattern += "SM_STATS_INFO, SM_ACTION_ANIMATION, SM_NEARBY_QUESTS, ";
	pattern += "SM_HOUSE_SCRIPTS, SM_UNK_3_5_1, SM_ENTER_WORLD_CHECK, ";
	pattern += "SM_SKILL_LIST+, [SM_SKILL_COOLDOWN], [SM_ITEM_COOLDOWN], ";
	pattern += "SM_QUEST_COMPLETED_LIST+, SM_QUEST_LIST, SM_TITLE_INFO{2}, SM_MOTION, ";
	pattern += "SM_AFTER_TIME_CHECK_4_7_5, [SM_UI_SETTINGS]{0..3}, ";
	pattern += "SM_INVENTORY_INFO{" + std::to_string(inventoryPackets) + "}, ";
	pattern += "SM_CHANNEL_INFO, SM_BIND_POINT_INFO, SM_PLAYER_SPAWN, SM_GAME_TIME, ";
	pattern += "SM_WAREHOUSE_INFO{43}, ";
	pattern += "SM_TITLE_INFO, SM_EMOTION_LIST, SM_PRICES, SM_FRIEND_LIST, SM_BLOCK_LIST, ";
	pattern += "SM_INSTANCE_INFO, SM_ABYSS_RANK, SM_STATS_INFO, ";
	pattern += "SM_LEGION_DOMINION_LOC_INFO, SM_MAIL_SERVICE, ";
	pattern += "[SM_SYSTEM_MESSAGE]{0..2}, [SM_ATREIAN_PASSPORT], ";
	pattern += "SM_MACRO_LIST+, SM_RECIPE_LIST, SM_HOUSE_OWNER_INFO";
	return pattern;
}

/** The CM_LEVEL_READY part of m5a-plan.md §5.8 (#33 to #44) */
std::string levelReadyPattern() {
	return "SM_PLAYER_INFO, SM_PLAYER_STATE, SM_ACCOUNT_PROPERTIES, SM_MOTION, "
	       "SM_WINDSTREAM_ANNOUNCE*, "
	       "(SM_NPC_INFO | SM_GATHERABLE_INFO)+, "
	       "SM_RIFT_ANNOUNCE, "
	       "SM_NEARBY_QUESTS, [SM_QUEST_REPEAT], [SM_WEATHER], "
	       "SM_ABNORMAL_STATE, SM_CUBE_UPDATE";
}

// ---- the scenario client ---------------------------------------------------------------------------------------------------------------

struct ScenarioClient {
	std::string account;
	std::string password = "m5bPassword1";
	std::unique_ptr<FakeLoginClient> login;
	std::unique_ptr<GameSession> game;
	FakeLoginClient::SessionKey key;
	int32_t playerId = 0;
	std::string characterName;
};

/** m5a-plan.md §5.2: the login server conversation and the game server login up to SM_CHARACTER_LIST */
decoders::CharacterList logIn(ScenarioServers& servers, ScenarioClient& client, const AsyncAllowed& async) {
	client.login = std::make_unique<FakeLoginClient>(servers.loginClientPort());
	client.login->login(client.account, client.password);
	const FakeLoginClient::ServerList list = client.login->requestServerList();
	bool listed = false;
	for (const FakeLoginClient::GameServerEntry& entry : list.servers)
		if (entry.id == 1)
			listed = entry.online;
	EXPECT_TRUE(listed) << "game server 1 is not listed as online (" << list.servers.size() << " servers)";
	client.key = client.login->play(1);

	client.game = std::make_unique<GameSession>(servers.gameClientPort());
	client.game->readKey();
	client.game->send(GameSession::CM_VERSION_CHECK, GameSession::buildCM_VERSION_CHECK());
	expectNext(*client.game, "SM_VERSION_CHECK", async);
	client.game->send(GameSession::CM_L2AUTH_LOGIN_CHECK,
	                  GameSession::buildCM_L2AUTH_LOGIN_CHECK(client.key.playOk2, client.key.playOk1, client.key.accountId, client.key.loginOk));
	client.game->send(GameSession::CM_MAC_ADDRESS, GameSession::buildCM_MAC_ADDRESS());
	expectNext(*client.game, "SM_L2AUTH_LOGIN_CHECK", async);
	client.game->send(GameSession::CM_TIME_CHECK, GameSession::buildCM_TIME_CHECK(1));
	expectNext(*client.game, "SM_AFTER_TIME_CHECK_4_7_5", async);
	expectNext(*client.game, "SM_TIME_CHECK", async);
	client.game->send(GameSession::CM_CHARACTER_LIST, GameSession::buildCM_CHARACTER_LIST(client.key.playOk2));
	expectNext(*client.game, "SM_ACCOUNT_PROPERTIES", async);
	const Packet characters = expectNext(*client.game, "SM_CHARACTER_LIST", async);
	return decoders::decodeCharacterList(characters.data);
}

// ---- the check output reports (§6.3 Q1-Q3) -----------------------------------------------------------------------------------------------

/** one row of live_counts.txt / live_counts_baseline.txt: "<live>\t<created>\t<qualified class name>" */
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
			continue; // not a counter row
		}
		counts.emplace_back(colons == std::string::npos ? qualified : qualified.substr(colons + 2), count);
	}
	return counts;
}

/** One section of m5b_partial_allowlist.txt: §A hit at least once, §B hit exactly zero times, §C counted but not pinned */
enum class AllowlistSection { HitAtLeastOnce, HitNever, NotPinned };

struct AllowlistEntry {
	std::string site;
	AllowlistSection section = AllowlistSection::NotPinned;
};

/**
 * Reads tests/scenario/m5b_partial_allowlist.txt with its three sections. The section markers are the "# §A"/"# §B"/"# §C" comment lines of
 * that file, which is why they are a fixed spelling there: a row that moves between sections must change what the gate asserts about it.
 */
std::vector<AllowlistEntry> readAllowlist() {
	std::vector<AllowlistEntry> entries;
	std::ifstream in(AION_SCENARIO_M5B_PARTIAL_ALLOWLIST, std::ios::binary);
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

/** the name of a section, for the diagnostic table Q1 prints */
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

/**
 * An allow-list entry matches a site of partial_trace.txt. An entry WITH a line number must match the whole site: a prefix match would let
 * "InstanceService.cpp:133" also cover :1330 to :1339. An entry without one is an explicit whole-file wildcard.
 */
bool allowlistEntryMatches(const std::string& entry, const std::string& site) {
	if (entry.find(':') != std::string::npos)
		return entry == site;
	return site.starts_with(entry) && (site.size() == entry.size() || site[entry.size()] == ':');
}

/** "<hits>\t<site>\t<function>\t<reason>" of partial_trace.txt -> site and hit count */
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

/** The end of a run: the two test schemas are dropped, and a failed run says where its evidence is (the M5a finishRun, verbatim in intent) */
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

// ---- the fight recording ----------------------------------------------------------------------------------------------------------------

/**
 * One pass over a slice of the session's recording, decoded with the independent combat decoders. Every §6.3 attack assertion reads this
 * structure instead of the raw packets, so that the decode happens once and a body that does not decode is a failure of the case that
 * collected it rather than of the assertion that happens to look first.
 */
struct FightRecording {
	struct AttackPacket {
		size_t index = 0;
		decoders::Attack attack;
		int32_t totalDamage = 0;
		std::chrono::steady_clock::time_point at;
	};
	struct StatusPacket {
		size_t index = 0;
		decoders::AttackStatusUpdate status;
		std::chrono::steady_clock::time_point at;
	};
	struct HpPacket {
		size_t index = 0;
		decoders::StatUpdateHp hp;
		std::chrono::steady_clock::time_point at;
	};

	std::vector<AttackPacket> attacks;
	std::vector<StatusPacket> statuses;
	std::vector<HpPacket> hpUpdates;
	std::vector<std::pair<size_t, decoders::AttackResponse>> responses;
	std::vector<std::pair<size_t, decoders::Emotion>> emotions;
	/** the decode failures, so a §6.3 assertion never silently sees a shorter stream than the run produced */
	std::vector<std::string> decodeFailures;

	std::vector<AttackPacket> attacksBy(int32_t attackerObjectId) const {
		std::vector<AttackPacket> result;
		for (const AttackPacket& attack : attacks)
			if (attack.attack.attackerObjectId == attackerObjectId)
				result.push_back(attack);
		return result;
	}

	std::vector<AttackPacket> attacksBetween(int32_t attackerObjectId, int32_t targetObjectId) const {
		std::vector<AttackPacket> result;
		for (const AttackPacket& attack : attacks)
			if (attack.attack.attackerObjectId == attackerObjectId && attack.attack.targetObjectId == targetObjectId)
				result.push_back(attack);
		return result;
	}

	std::vector<StatusPacket> statusesOf(int32_t creatureObjectId) const {
		std::vector<StatusPacket> result;
		for (const StatusPacket& status : statuses)
			if (status.status.creatureObjectId == creatureObjectId)
				result.push_back(status);
		return result;
	}
};

/** Decodes the packets [from, end) of the session's recording into a FightRecording */
FightRecording recordFight(const GameSession& session, size_t from) {
	FightRecording recording;
	const std::vector<Packet>& packets = session.recorded();
	for (size_t i = from; i < packets.size(); i++) {
		const Packet& packet = packets[i];
		try {
			if (packet.name == "SM_ATTACK") {
				FightRecording::AttackPacket attack;
				attack.index = i;
				attack.at = packet.receivedAt;
				attack.attack = decoders::decodeAttack(packet.data);
				for (const decoders::AttackResultEntry& entry : attack.attack.results)
					attack.totalDamage += entry.damage;
				recording.attacks.push_back(attack);
			} else if (packet.name == "SM_ATTACK_STATUS") {
				recording.statuses.push_back({i, decoders::decodeAttackStatus(packet.data), packet.receivedAt});
			} else if (packet.name == "SM_STATUPDATE_HP") {
				recording.hpUpdates.push_back({i, decoders::decodeStatUpdateHp(packet.data), packet.receivedAt});
			} else if (packet.name == "SM_ATTACK_RESPONSE") {
				recording.responses.emplace_back(i, decoders::decodeAttackResponse(packet.data));
			} else if (packet.name == "SM_EMOTION") {
				recording.emotions.emplace_back(i, decoders::decodeEmotion(packet.data));
			}
		} catch (const DecodeError& error) {
			recording.decodeFailures.push_back(packet.name + " at " + std::to_string(i) + ": " + error.what());
		}
	}
	return recording;
}

// ---- the gate ---------------------------------------------------------------------------------------------------------------------------

/** What separates gs.scenario.m5b from gs.scenario.m5b_geo */
struct GateVariant {
	bool geodata = false;
	std::string testName;      // gs.scenario.m5b
	std::string outputSubdir;  // m5b
	std::string schemaPrefix;  // m5b
	std::string accountPrefix; // m5bg
};

void runM5bGate(const GateVariant& variant) {
	// A skipped gate is NOT a passed gate (m5a-plan.md §5.10, §6.5): without the database URLs or a Python interpreter the gate skips itself
	// for developer convenience, ScenarioTests.cmake turns that into CTest's "***Skipped" and CTest counts a skip as passed.
	// AION_SCENARIO_REQUIRE=1 - the default of the CTest registration - turns every skip reason into a failure that names the variable.
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
		// data/geo is what separates this run from gs.scenario.m5b; without it every geo statement below would describe the other gate's
		// configuration. A missing prerequisite, exactly as RunStartupSmoke.cmake MODE geo treats it.
		const std::filesystem::path geoDirectory = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data" / "geo";
		size_t geoFiles = 0;
		if (std::filesystem::is_directory(geoDirectory))
			for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(geoDirectory))
				if (entry.path().extension() == ".geo")
					geoFiles++;
		if (geoFiles == 0) {
			unavailable("the Java game server checkout has no data/geo/*.geo files, which this gate exists to run against: check out or unpack "
			            "game-server/data/geo (151 .geo files in the 4.8 tree)");
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

	// ---- §6.1 processes, databases and profile ----
	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.schemaPrefix = variant.schemaPrefix;
	// The D1 profile of M5b-1, on top of the M5a set that ScenarioServers::gameServerArguments already merges:
	//  - geodata: false for gs.scenario.m5b, true for gs.scenario.m5b_geo (the one key that separates the two runs)
	//  - npcshouts: the default made explicit, which keeps NpcShoutsService (6 unported bodies, P5-14) off the fight path
	//  - rates.xp.solo: the default made explicit, because R1 asserts an exact integer that this rate multiplies
	//  - soulsickness.disable=0: REQUIRED. bindRevive revives a character outside EVENT_MODE with setSoulSickness = true, which reaches
	//    SkillEngine::getSkill (AION_UNPORTED until M5b-2) and would throw out of K8's revive. Java's own guard in front of that call is
	//    `!player.hasPermission(MembershipConfig.DISABLE_SOULSICKNESS)` and the @Property default is 10, which exempts nobody; 0 exempts
	//    every account and takes the branch Java itself provides. A configuration answer, not a C++ deviation (D1).
	config.gameServerProperties["gameserver.geodata.enable"] = variant.geodata ? "true" : "false";
	config.gameServerProperties["gameserver.npcshouts.enable"] = "false";
	config.gameServerProperties["gameserver.rates.xp.solo"] = "1.0, 2.0";
	config.gameServerProperties["gameserver.soulsickness.disable"] = "0";
	// the geo startup is seconds in a checked RelWithDebInfo tree and minutes in a Debug one (m5a-client-session.md)
	config.startupTimeout = variant.geodata ? 25min : 10min;
	config.stopTimeout = 3min;
	ScenarioServers servers(config, *environment);
	const std::string schema = servers.gameSchema();
	const ScenarioDatabase& database = servers.gameDatabase();

	bool ok = true;
	const auto runCase = [&](std::string_view id, std::string_view title, const std::function<void()>& body) {
		if (!ok)
			cases.skip(id, title, "an earlier case failed");
		else
			ok = cases.run(id, title, body);
	};

	ok = cases.run("K-0", "the servers start (§6.1)", [&] {
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
				for (const std::string& line : gameServer->findLogLines("is not ported yet", 5))
					diagnosis.push_back("unported: " + line);
				for (const std::string& line : gameServer->findLogLines(" ERROR ", 5))
					diagnosis.push_back(line);
			}
			throw std::runtime_error(join(diagnosis, "\n  "));
		}
	});

	ScenarioClient a;
	AnnouncedNpcs announcedNpcs;
	a.account = variant.accountPrefix + servers.gameSchema().substr(servers.gameSchema().size() - 8);
	a.characterName = "Scenariofighter";

	// ---- K0: the monster oracle answers (G-01) ----
	OracleCreation elyos;
	OracleMonster monster;
	OracleMonster aggressive;
	runCase("K0", "the creation and monster oracles answer (G-01)", [&] {
		elyos = oracle->creation("ELYOS", "WARRIOR");
		ASSERT_FALSE(elyos.items.empty());
		EXPECT_EQ(elyos.mapId, ELYOS_START_MAP);

		monster = oracle->monster(ELYOS_START_MAP, GATE_MONSTER_NPC_ID, 1);
		// the template of D11, re-derived by the oracle rather than quoted from the plan (§8 risk 20)
		EXPECT_EQ(monster.level, 2);
		EXPECT_EQ(monster.maxHp, 199);
		EXPECT_EQ(monster.rating, "NORMAL");
		EXPECT_EQ(monster.rank, "DISCIPLINED");
		EXPECT_EQ(monster.race, "BEAST");
		EXPECT_EQ(monster.tribe, "MONSTER");
		EXPECT_EQ(monster.ai, "aggressive") << "the gate's monster must have a root AI that A-06 registers";
		EXPECT_EQ(monster.aggroRange, 8);
		EXPECT_EQ(monster.aggroAngle, 270);
		EXPECT_EQ(monster.npcAttackSpeed, 2142);
		EXPECT_GT(monster.boundRadius, 0.0f);
		// V2's rule (m5a-plan.md §5.5): only an id with no pool, walker or randomWalk spot may be pinned to an exact position, and R4 pins one
		EXPECT_TRUE(monster.pinned) << "npc " << GATE_MONSTER_NPC_ID << " has a pool, walker or randomWalk spot, so R4 cannot pin its respawn";
		ASSERT_TRUE(monster.nearestPlainSpot) << "no spot of npc " << GATE_MONSTER_NPC_ID << " is fixed, spawned and free of a static id (D11)";
		EXPECT_EQ(monster.nearestPlainSpot->staticId, 0) << "D11: the gate does not pick up the placeable-object couplings of a static id";
		EXPECT_TRUE(monster.nearestPlainSpot->fixed);
		EXPECT_TRUE(monster.nearestPlainSpot->spawned);
		EXPECT_EQ(monster.respawnTime, 20);
		// the two ranges of the first hit (G-01) and the two stand-off distances the script derives from them
		EXPECT_GT(monster.attackRange, 0.0f);
		EXPECT_GT(monster.toleranceRange, monster.attackRange);
		EXPECT_GT(STAND_OFF_DISTANCE, static_cast<double>(monster.toleranceRange))
		  << "K4's stand-off point must be outside the tolerance range, or A1a asserts nothing";
		EXPECT_GT(STAND_OFF_DISTANCE, monster.aggroRange + 2.0 * monster.boundRadius)
		  << "K4's stand-off point must be outside the aggro range, or the monster is already coming when the out-of-range shot goes out";
		EXPECT_LT(MELEE_DISTANCE, static_cast<double>(monster.attackRange)) << "K4b's melee point must be inside the attack range";
		// the experience of one kill (D7), computed by the oracle and never hardcoded here
		EXPECT_GT(monster.awarded, 0);
		EXPECT_GT(monster.expNeed, 0);
		EXPECT_EQ(monster.awarded, std::min<int64_t>(monster.experienceReward, static_cast<int64_t>(monster.expNeed * 0.2f)))
		  << "D7: the reward is capped at expNeed * 0.2f, and the cap is the point";
		EXPECT_GT(monster.playerAttackSpeed, 0);
		std::cout << "K0: npc " << GATE_MONSTER_NPC_ID << " at (" << monster.nearestPlainSpot->x << ", " << monster.nearestPlainSpot->y << ", "
		          << monster.nearestPlainSpot->z << "), " << monster.nearestPlainSpot->distance << " m from the spawn point; attackRange "
		          << monster.attackRange << ", toleranceRange " << monster.toleranceRange << "; one kill awards " << monster.awarded
		          << " exp of the " << monster.experienceReward << " it computes, expNeed " << monster.expNeed << std::endl;

		// A5b's npc, picked by the oracle exactly as the gate's own monster is - the id is a constant, every coordinate and every template
		// value below comes from the static data through `oracle.py m5b-monster`. The two rows of THIS block are the whole of A5's finding:
		// both npcs are `ai="aggressive"`, and only the second one has a tribe the relation table makes aggressive to PC.
		aggressive = oracle->monster(ELYOS_START_MAP, AGGRESSIVE_TRIBE_NPC_ID, 1);
		EXPECT_EQ(aggressive.ai, "aggressive") << "A5b: the two npcs of A5 must differ in their TRIBE and in nothing else that matters";
		EXPECT_EQ(monster.ai, aggressive.ai) << "A5b: the gate's monster and A5b's npc carry the same `ai` attribute";
		EXPECT_EQ(aggressive.tribe, "AGGRESSIVESINGLEMONSTER")
		  << "A5b: npc " << AGGRESSIVE_TRIBE_NPC_ID << " is in tribe " << aggressive.tribe
		  << ", and the case is built on that tribe's `<aggro>PC PC_DARK</aggro>` row (tribe_relations.xml:29-31)";
		EXPECT_NE(aggressive.tribe, monster.tribe) << "A5b: the two npcs of A5 would then prove the same thing twice";
		EXPECT_EQ(aggressive.level, 4) << "A5b: a level 4 npc is what K8b's 25 %-HP character can walk away from";
		EXPECT_EQ(aggressive.aggroRange, 7);
		EXPECT_TRUE(aggressive.pinned) << "A5b: npc " << AGGRESSIVE_TRIBE_NPC_ID << " has a pool, walker or randomWalk spot, so K8b cannot pin it";
		EXPECT_EQ(aggressive.spots.size(), 1u)
		  << "A5b: npc " << AGGRESSIVE_TRIBE_NPC_ID << " has " << aggressive.spots.size()
		  << " spawn spots on the map; K8b reads one object id out of the walk and a second spot would make that ambiguous";
		ASSERT_TRUE(aggressive.nearestPlainSpot) << "A5b: no spot of npc " << AGGRESSIVE_TRIBE_NPC_ID
		                                         << " is fixed, spawned and free of a static id";
		EXPECT_TRUE(aggressive.nearestPlainSpot->fixed);
		EXPECT_TRUE(aggressive.nearestPlainSpot->spawned);
		EXPECT_EQ(aggressive.nearestPlainSpot->staticId, 0);
		// Npc::getShortAggroRange (Npc.cpp:245-248): `aggroRange < 8 ? aggroRange / 2 : 4`, i.e. 3 m for an srange of 7. K8b stops inside it,
		// so `isInSeeRange`'s second term is true whichever way the npc happens to face and the case does not depend on its heading.
		const int32_t shortAggroRange = aggressive.aggroRange < 8 ? aggressive.aggroRange / 2 : 4;
		EXPECT_LT(MELEE_DISTANCE, static_cast<double>(shortAggroRange))
		  << "A5b: K8b's stopping point must be inside the short aggro range (" << shortAggroRange << " m), or the npc's sangle of "
		  << aggressive.aggroAngle << " decides the case";
		EXPECT_GT(STAND_OFF_DISTANCE, aggressive.aggroRange + 2.0 * aggressive.boundRadius)
		  << "A5b: K8b reads the object id from a distance at which the npc cannot have aggroed yet";
		std::cout << "K0: A5b's npc " << AGGRESSIVE_TRIBE_NPC_ID << " (tribe " << aggressive.tribe << ", ai " << aggressive.ai << ", level "
		          << aggressive.level << ", srange " << aggressive.aggroRange << ", short " << shortAggroRange << ") at ("
		          << aggressive.nearestPlainSpot->x << ", " << aggressive.nearestPlainSpot->y << ", " << aggressive.nearestPlainSpot->z << "), "
		          << aggressive.nearestPlainSpot->distance << " m from the spawn point; the gate's own monster is tribe " << monster.tribe
		          << " with the same ai" << std::endl;
	});

	AsyncAllowed async = AsyncAllowed::m5aDefault();

	// ---- K1: login and create an Elyos Warrior (M5a cases 1-2 replayed) ----
	runCase("K1", "login and create an Elyos Warrior", [&] {
		const decoders::CharacterList list = logIn(servers, a, async);
		EXPECT_EQ(list.characterCount, 0) << "a fresh account must have no character";

		NewCharacter warrior;
		warrior.name = a.characterName;
		warrior.asmodian = false;
		warrior.playerClassId = CLASS_WARRIOR;
		a.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(a.key.accountId, a.account, warrior, 1));
		EXPECT_EQ(decoders::decodeCreateCharacter(expectNext(*a.game, "SM_CREATE_CHARACTER", async).data).responseCode, RESPONSE_OPEN_CREATION_WINDOW);
		a.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(a.key.accountId, a.account, warrior, 0));
		const decoders::CreateCharacter created = decoders::decodeCreateCharacter(expectNext(*a.game, "SM_CREATE_CHARACTER", async).data);
		ASSERT_EQ(created.responseCode, RESPONSE_OK);
		ASSERT_TRUE(created.player);
		a.playerId = created.player->playerId;
		EXPECT_EQ(created.player->level, 1) << "D7's experience arithmetic is for a level 1 character against a level 2 monster";
		EXPECT_EQ(created.player->classId, CLASS_WARRIOR) << "S-3: a magical main-hand weapon cannot auto-attack until M5b-2";
		// the exp the fight has to move, read before it moves (R1 (c) compares against this value)
		EXPECT_EQ(database.queryLong(schema, "SELECT exp FROM players WHERE id = " + std::to_string(a.playerId)), 0)
		  << "a level 1 character starts at exp 0";
	});

	// ---- K2: enter world ----
	std::optional<decoders::StatsInfo> enterStats;
	runCase("K2", "enter world", [&] {
		async.selfPlayerState(a.playerId);
		announcedNpcs.follow(a.game.get());
		async.npcActivity(announcedNpcs.predicate());
		a.game->send(GameSession::CM_MAY_LOGIN_INTO_GAME, GameSession::buildCM_MAY_LOGIN_INTO_GAME());
		expectNext(*a.game, "SM_MAY_LOGIN_INTO_GAME", async);
		a.game->send(GameSession::CM_ENTER_WORLD, GameSession::buildCM_ENTER_WORLD(a.playerId));
		const std::vector<Packet> burst = collectBurst(*a.game, async);
		ASSERT_FALSE(burst.empty()) << "no packet after CM_ENTER_WORLD";
		const int32_t inventoryPackets = static_cast<int32_t>((elyos.items.size() + 9) / 10) + 1;
		expectSequence(burst, enterWorldPattern(true, inventoryPackets), async);

		const Packet* spawn = firstOfName(burst, "SM_PLAYER_SPAWN");
		ASSERT_NE(spawn, nullptr);
		const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(spawn->data);
		EXPECT_EQ(spawned.worldId, elyos.mapId);
		EXPECT_NEAR(spawned.x, elyos.x, 0.01);
		EXPECT_NEAR(spawned.y, elyos.y, 0.01);
		EXPECT_NEAR(spawned.z, elyos.z, 0.01);

		const std::vector<Packet> stats = ofName(burst, "SM_STATS_INFO");
		ASSERT_FALSE(stats.empty());
		enterStats = decoders::decodeStatsInfo(stats.back().data);
		EXPECT_EQ(enterStats->currentHp, enterStats->maxHp) << "the character enters the fight at full HP, which is where A6 starts counting";
		EXPECT_EQ(enterStats->expShown, 0);
		// the pace fightUntil needs, from the packet and cross-checked against the oracle's own PlayerGameStats computation
		EXPECT_GT(enterStats->attackSpeed, 0);
		EXPECT_EQ(enterStats->attackSpeed, monster.playerAttackSpeed)
		  << "the attack speed of SM_STATS_INFO and the oracle's PlayerGameStats.getAttackSpeed disagree";
	});

	// ---- K3: level ready, and the monster's object id ----
	int32_t monsterObjectId = 0;
	int32_t monsterMaxHpAnnounced = 0;
	runCase("K3", "level ready and the monster is announced", [&] {
		a.game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
		const std::vector<Packet> burst = collectBurst(*a.game, async);
		ASSERT_FALSE(burst.empty()) << "no packet after CM_LEVEL_READY";
		expectSequence(burst, levelReadyPattern(), async);

		// The object the gate fights is the SM_NPC_INFO of npc 210663 that stands on the oracle's chosen spot. Matching by position and not by
		// template id alone is what keeps the fight on the D11 spot: 210663 has 38 spots on Poeta and more than one can be within 100 m.
		ASSERT_TRUE(monster.nearestPlainSpot);
		const OracleMonsterSpot& spot = *monster.nearestPlainSpot;
		std::vector<std::string> otherSpots;
		for (const Packet& packet : ofName(burst, "SM_NPC_INFO")) {
			const decoders::NpcInfo npc = decoders::decodeNpcInfo(packet.data);
			if (npc.templateId != GATE_MONSTER_NPC_ID)
				continue;
			if (std::abs(npc.x - spot.x) <= 0.01f && std::abs(npc.y - spot.y) <= 0.01f && std::abs(npc.z - spot.z) <= 0.01f) {
				monsterObjectId = npc.objectId;
				monsterMaxHpAnnounced = npc.maxHp;
				EXPECT_EQ(npc.hpPercentage, 100) << "the monster has not been fought yet";
				EXPECT_EQ(static_cast<int32_t>(npc.level), monster.level);
				EXPECT_EQ(npc.staticId, 0) << "D11: the chosen spot carries no static id";
			} else {
				otherSpots.push_back("(" + std::to_string(npc.x) + ", " + std::to_string(npc.y) + ", " + std::to_string(npc.z) + ")");
			}
		}
		ASSERT_NE(monsterObjectId, 0) << "no SM_NPC_INFO for npc " << GATE_MONSTER_NPC_ID << " at the oracle's spot (" << spot.x << ", " << spot.y
		                              << ", " << spot.z << "); the burst announced it at " << (otherSpots.empty() ? "no other spot" : join(otherSpots));
		EXPECT_EQ(monsterMaxHpAnnounced, monster.maxHp) << "SM_NPC_INFO announces a maxHp the npc template does not have";
		std::cout << "K3: the gate's monster is object " << monsterObjectId << " (npc " << GATE_MONSTER_NPC_ID << ", maxHp "
		          << monsterMaxHpAnnounced << ")" << std::endl;
	});

	/** walks the character from `fromX/Y/Z` to `toX/Y/Z` in 5 m steps and stops there (the §5.6 move shape, m5a-plan.md case 5) */
	float atX = elyos.x, atY = elyos.y, atZ = elyos.z;
	const auto walkTo = [&](float toX, float toY, float toZ) {
		const double total = distance2d(atX, atY, toX, toY);
		const int32_t steps = std::max(1, static_cast<int32_t>(total / 5.0));
		const float fromX = atX, fromY = atY, fromZ = atZ;
		for (int32_t step = 1; step <= steps; step++) {
			const float t = static_cast<float>(step) / static_cast<float>(steps);
			const float x = fromX + (toX - fromX) * t;
			const float y = fromY + (toY - fromY) * t;
			const float z = fromZ + (toZ - fromZ) * t;
			a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(x, y, z, 0, static_cast<int8_t>(0xE0), toX, toY, toZ));
			std::this_thread::sleep_for(120ms);
		}
		// the stop move, and then a second so that MoveController::isInMove() is false before the next CM_ATTACK: attackTarget widens nothing
		// for a moving attacker, but PositionUtil reads the target position of a creature that is still in move (§6.2 K4)
		a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(toX, toY, toZ, 0, 0));
		atX = toX;
		atY = toY;
		atZ = toZ;
		std::this_thread::sleep_for(1s);
	};

	/** a point `distance` metres from `spot` on the line towards `fromX/fromY` (so the character walks a straight line to it) */
	const auto pointNear = [&](const OracleMonsterSpot& spot, double distance, float fromX, float fromY) {
		const double dx = fromX - spot.x, dy = fromY - spot.y;
		const double length = std::sqrt(dx * dx + dy * dy);
		const double scale = length <= 0.001 ? 0.0 : distance / length;
		return std::array<float, 3>{static_cast<float>(spot.x + dx * scale), static_cast<float>(spot.y + dy * scale), spot.z};
	};

	/** the same, for the spot the fight of K4-K8 happens at */
	const auto pointNearSpot = [&](double distance, float fromX, float fromY) {
		return pointNear(*monster.nearestPlainSpot, distance, fromX, fromY);
	};

	/**
	 * `walkTo` for a long approach: the same 5 m steps, but each step's pause **reads** the socket instead of sleeping on it, and the walk ends
	 * with the same stop-move and the same second of quiet.
	 *
	 * It is a second lambda and not a flag on `walkTo` on purpose. Draining during a walk moves everything that arrived into `recorded()`, and
	 * K4b's and K8's walks are followed by `waitForAttackBy`, which only sees what arrives *after* it is called: a drain in the last pause of
	 * those walks could swallow the very SM_ATTACK the A5 window is about. Here the opposite is true - the walk is 500 m long, it crosses map
	 * regions and known lists the whole way, and leaving hundreds of SM_NPC_INFO unread in the socket for fifteen seconds is how a scenario
	 * client stalls the server it is measuring. K8b reads the object id it needs out of the recording afterwards, from `from` on.
	 */
	const auto trekTo = [&](float toX, float toY, float toZ) {
		const double total = distance2d(atX, atY, toX, toY);
		const int32_t steps = std::max(1, static_cast<int32_t>(total / 5.0));
		const float fromX = atX, fromY = atY, fromZ = atZ;
		for (int32_t step = 1; step <= steps; step++) {
			const float t = static_cast<float>(step) / static_cast<float>(steps);
			const float x = fromX + (toX - fromX) * t;
			const float y = fromY + (toY - fromY) * t;
			const float z = fromZ + (toZ - fromZ) * t;
			a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(x, y, z, 0, static_cast<int8_t>(0xE0), toX, toY, toZ));
			collectFor(*a.game, 120ms);
		}
		a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(toX, toY, toZ, 0, 0));
		atX = toX;
		atY = toY;
		atZ = toZ;
		collectFor(*a.game, 1s);
	};

	// ---- K4: approach, target, and one shot from out of range (T1, A1a) ----
	size_t fightWindowStart = 0;
	runCase("K4", "approach, target and one shot from 12 m (T1, A1a)", [&] {
		ASSERT_NE(monsterObjectId, 0);
		const std::array<float, 3> standOff = pointNearSpot(STAND_OFF_DISTANCE, elyos.x, elyos.y);
		walkTo(standOff[0], standOff[1], standOff[2]);
		fightWindowStart = a.game->recorded().size();

		// T1: CM_TARGET_SELECT -> SM_TARGET_SELECTED with the monster's object id, its body consumed exactly.
		//
		// **Rev 3 (the gate lane, measured): SM_TARGET_UPDATE is NOT observable here, and the row is inverted rather than dropped.**
		// PlayerController.java:153 sends it with `PacketSendUtility.broadcastToSightedPlayers(getOwner(), ...)`, which walks the owner's known
		// list - and an object is not in its own known list, so the character that changed its target is the one player who never receives the
		// packet. A solo scenario client cannot see it at all, and rev 2's "SM_TARGET_UPDATE is broadcast" was another assertion a fake client
		// cannot make. What IS observable is its absence, and that is a real failure mode: a port that wrote `broadcastPacket(owner, packet,
		// true)` - the overload that does include the owner, and the one most of the neighbouring call sites use - would send it to self and
		// fail here. Seeing the packet arrive somewhere needs a second character in sight range, which is a case this gate does not have.
		a.game->send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(monsterObjectId));
		const Packet selected = waitFor(*a.game, "SM_TARGET_SELECTED", 10s);
		const TargetSelected target = decodeTargetSelected(selected.data);
		EXPECT_EQ(target.targetObjectId, monsterObjectId);
		EXPECT_EQ(target.maxHp, monster.maxHp) << "T1: SM_TARGET_SELECTED announces a maxHp the npc template does not have";
		EXPECT_EQ(target.currentHp, monster.maxHp) << "T1: the monster has not been fought yet";
		EXPECT_EQ(static_cast<int32_t>(target.level), monster.level);
		for (const Packet& packet : collectFor(*a.game, 1200ms)) {
			if (packet.name != "SM_TARGET_UPDATE")
				continue;
			const TargetUpdate targetUpdate = decodeTargetUpdate(packet.data);
			ADD_FAILURE() << "T1: SM_TARGET_UPDATE reached the character that changed its target (player " << targetUpdate.playerObjectId
			              << ", target " << targetUpdate.targetObjectId
			              << "). PlayerController.java:153 broadcasts it to the players who SEE the owner, and the owner is not in its own known "
			                 "list, so this is broadcastPacket(..., toSelf) where Java has broadcastToSightedPlayers";
		}

		// A1a: ONE CM_ATTACK from 12 m. It must be answered by SM_ATTACK_RESPONSE.TARGET_TOO_FAR_AWAY and by no SM_ATTACK at all.
		// What it proves: the range gate of PlayerController.java:406-409 exists and rejects, and the first-hit tolerance of :404-405 is
		// BOUNDED (12 m > attackRange + calculateMaxCoveredDistance(owner, 100)). What it cannot prove: that the tolerance is applied at all -
		// A1b says why the gate gave that up and where it went instead.
		const size_t before = a.game->recorded().size();
		a.game->send(GameSession::CM_ATTACK, GameSession::buildCM_ATTACK(monsterObjectId));
		const std::vector<Packet> answer = collectFor(*a.game, 2500ms);
		const FightRecording recording = recordFight(*a.game, before);
		EXPECT_TRUE(recording.decodeFailures.empty()) << "A1a: " << join(recording.decodeFailures, "\n  ");
		ASSERT_EQ(recording.responses.size(), 1u)
		  << "A1a: the out-of-range CM_ATTACK was answered by " << recording.responses.size() << " SM_ATTACK_RESPONSE, not exactly one; the burst was "
		  << join(namesOf(answer));
		EXPECT_EQ(recording.responses[0].second.message, decoders::ATTACK_RESPONSE_TARGET_TOO_FAR_AWAY)
		  << "A1a: the answer was SM_ATTACK_RESPONSE message " << static_cast<int32_t>(recording.responses[0].second.message)
		  << ", not TARGET_TOO_FAR_AWAY(4)";
		EXPECT_TRUE(recording.attacksBy(a.playerId).empty())
		  << "A1a: the server accepted an attack from " << STAND_OFF_DISTANCE << " m (attackRange " << monster.attackRange << ", toleranceRange "
		  << monster.toleranceRange << ")";
		// and the monster has not noticed the character: 12 m is outside its aggro range
		EXPECT_TRUE(recording.attacksBy(monsterObjectId).empty()) << "A1a: the monster aggroed from outside its srange of " << monster.aggroRange;
	});

	// ---- K4b: close and hit (A5a's measurement, then A1b) ----
	std::optional<std::chrono::milliseconds> unprovokedAggro;
	runCase("K4b", "close to 2 m, watch a MONSTER-tribe npc not start a fight, and land the first hit (A5a, A1b)", [&] {
		const std::array<float, 3> melee = pointNearSpot(MELEE_DISTANCE, atX, atY);
		walkTo(melee[0], melee[1], melee[2]);

		// A5a's measurement, and it has to be taken HERE, before the character attacks: the aggro chain of §2.1 step 11 is the only thing that
		// can make a monster swing at a character that has not touched it. Once a CM_ATTACK has landed, `AggroList::addDamage` puts the
		// attacker on the hate list directly and the monster answers through `onAddHate` instead, so anything measured after the first hit says
		// something about the damage path and nothing about aggro. The window is generous because the chain has a 500 ms AggroNotifier in it
		// and the npc would still have to walk into its own 2 m attack range - i.e. it is long enough that a silence means a decision and not a
		// slow clock. K6 asserts what the silence means; K8b asserts the other side of the same decision against a tribe that does aggro.
		unprovokedAggro = waitForAttackBy(*a.game, monsterObjectId, 20s);
		std::cout << "A5a: the MONSTER-tribe monster "
		          << (unprovokedAggro ? "attacked " + std::to_string(unprovokedAggro->count()) + " ms after the character stopped " +
		                                  std::to_string(MELEE_DISTANCE) + " m away, without being touched"
		                              : "did NOT attack within 20 s of the character stopping next to it, without being touched - which is what "
		                                "a tribe with no <aggro> row does")
		          << std::endl;

		// A1b: the first in-range CM_ATTACK is answered by an SM_ATTACK whose attacker is the player and whose target is the monster, and by no
		// SM_ATTACK_RESPONSE. It proves the whole CM_ATTACK -> PlayerRestrictions.canAttack -> range -> GeoService.canSee ->
		// CreatureController.attackTarget -> SM_ATTACK chain. It deliberately does NOT prove the first-hit tolerance: that branch is guarded by
		// `if (!target.getAggroList().isHating(getOwner()))` and the monster's srange is 8 m, so by the time the character is inside the ~0.6 m
		// tolerance band it is already hating and the branch is dead. The band is a unit case of A-08, not a gate row (§6.3 A1b, §11 item 7).
		const size_t before = a.game->recorded().size();
		a.game->send(GameSession::CM_ATTACK, GameSession::buildCM_ATTACK(monsterObjectId));
		const std::vector<Packet> answer = collectFor(*a.game, 1200ms);
		const FightRecording recording = recordFight(*a.game, before);
		EXPECT_TRUE(recording.decodeFailures.empty()) << "A1b: " << join(recording.decodeFailures, "\n  ");
		const std::vector<FightRecording::AttackPacket> mine = recording.attacksBetween(a.playerId, monsterObjectId);
		EXPECT_FALSE(mine.empty()) << "A1b: the in-range CM_ATTACK produced no SM_ATTACK from the player against the monster; the burst was "
		                           << join(namesOf(answer));
		std::vector<std::string> rejections;
		for (const auto& [index, response] : recording.responses)
			rejections.push_back("message " + std::to_string(static_cast<int32_t>(response.message)));
		EXPECT_TRUE(rejections.empty()) << "A1b: the in-range CM_ATTACK was rejected: " << join(rejections);
		if (!mine.empty()) {
			EXPECT_EQ(mine[0].attack.attackerObjectId, a.playerId);
			EXPECT_EQ(mine[0].attack.targetObjectId, monsterObjectId);
			EXPECT_FALSE(mine[0].attack.results.empty()) << "A1b: SM_ATTACK carried an empty attack result list";
		}
	});

	// ---- K5 and K6: the fight (A2-A7) ----
	// The two cases run over ONE recording, as §6.2 says: K6 "runs concurrently with K5 and is asserted from the same recording".
	size_t killWindowStart = 0;
	bool monsterDied = false;
	std::optional<FightRecording> fight;
	runCase("K5", "the fight until one of them dies (A2)", [&] {
		ASSERT_NE(monsterObjectId, 0);
		ASSERT_TRUE(enterStats);
		killWindowStart = a.game->recorded().size();

		// A2: a CM_ATTACK sent less than `attackSpeed - 300` ms after the previous one is answered by SM_ATTACK_RESPONSE.STOP_WITHOUT_MESSAGE
		// and by no SM_ATTACK (the hack check of PlayerController.java:424-426, `milis - lastAttackMillis + 300 < attackSpeed`).
		//
		// TWO CM_ATTACK go out back to back, because that is what makes the assertion independent of how long K4b took. Whichever way the first
		// one falls, the SECOND is within microseconds of it:
		//   - if the first is accepted it sets lastAttackMillis, so the second is `attackSpeed` ms too early and must be refused;
		//   - if the first is itself refused (K4b's hit was recent enough) lastAttackMillis is unchanged, so the second is refused as well.
		// So a correct server answers with at least one STOP_WITHOUT_MESSAGE and produces at most one SM_ATTACK, and a server with no hack
		// check at all produces two SM_ATTACK and no response - it fails both halves.
		const size_t hackBefore = a.game->recorded().size();
		a.game->send(GameSession::CM_ATTACK, GameSession::buildCM_ATTACK(monsterObjectId, 0));
		a.game->send(GameSession::CM_ATTACK, GameSession::buildCM_ATTACK(monsterObjectId, 1));
		const std::vector<Packet> hackAnswer = collectFor(*a.game, 900ms);
		const FightRecording hackRecording = recordFight(*a.game, hackBefore);
		size_t stopWithoutMessage = 0;
		std::vector<std::string> otherResponses;
		for (const auto& [index, response] : hackRecording.responses) {
			if (response.message == decoders::ATTACK_RESPONSE_STOP_WITHOUT_MESSAGE)
				stopWithoutMessage++;
			else
				otherResponses.push_back("message " + std::to_string(static_cast<int32_t>(response.message)));
		}
		EXPECT_GE(stopWithoutMessage, 1u) << "A2: two CM_ATTACK sent back to back (the interval is " << enterStats->attackSpeed
		                                  << " ms, the margin 300 ms) were not answered by STOP_WITHOUT_MESSAGE(7); the answers were "
		                                  << (otherResponses.empty() ? "none" : join(otherResponses)) << " and the burst was "
		                                  << join(namesOf(hackAnswer));
		EXPECT_LE(hackRecording.attacksBy(a.playerId).size(), 1u)
		  << "A2: both of two CM_ATTACK sent back to back produced an SM_ATTACK, so the attack-speed check did not run";

		// the fight proper: CM_ATTACK paced at the attack speed until the monster's SM_EMOTION(DIE) arrives. The predicate also answers true
		// for the CHARACTER's death, so that a lost fight ends the case with a message instead of running into the 90 s timeout.
		bool playerDied = false;
		const GameSession::FightOutcome outcome = a.game->fightUntil(
		  monsterObjectId, std::chrono::milliseconds(enterStats->attackSpeed),
		  [&](const Packet& packet) {
			  if (packet.name != "SM_EMOTION")
				  return false;
			  try {
				  const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
				  if (emotion.emotionType != decoders::EMOTION_DIE)
					  return false;
				  if (emotion.senderObjectId == monsterObjectId) {
					  monsterDied = true;
					  return true;
				  }
				  if (emotion.senderObjectId == a.playerId) {
					  playerDied = true;
					  return true;
				  }
			  } catch (const DecodeError&) {
				  // a body that does not decode is reported by recordFight below, not swallowed here
			  }
			  return false;
		  },
		  90s, 60);
		EXPECT_FALSE(outcome.closed) << "K5: the connection closed during the fight";
		EXPECT_FALSE(playerDied) << "K5: the monster killed the character; K5 is the case in which the CHARACTER wins (D12 arranges the other "
		                            "way round in K8)";
		ASSERT_TRUE(monsterDied) << "K5: the monster did not die within " << outcome.elapsed.count() << " ms and " << outcome.attacksSent
		                         << " attacks";
		std::cout << "K5: the monster died after " << outcome.attacksSent << " CM_ATTACK in " << outcome.elapsed.count() << " ms" << std::endl;
		fight = recordFight(*a.game, fightWindowStart);
		EXPECT_TRUE(fight->decodeFailures.empty()) << "K5: packets of the fight did not decode:\n  " << join(fight->decodeFailures, "\n  ");
	});

	// K6 is pure analysis over the recording K5 already has: nothing after it depends on anything it does, so it runs even when it fails and its
	// failure does not skip K7-K9. That is not a softening - every EXPECT in it still fails the gate - it is what keeps ONE red assertion from
	// costing the run the kill, the reward, the respawn, the death and the whole report bar.
	if (ok)
		cases.run("K6", "the monster's half of the same recording (A3-A7)", [&] {
		ASSERT_TRUE(fight);
		ASSERT_TRUE(enterStats);
		const FightRecording& recording = *fight;

		// ---- A3: the monster's SM_ATTACK_STATUS stream ----
		const std::vector<FightRecording::StatusPacket> monsterStatuses = recording.statusesOf(monsterObjectId);
		ASSERT_FALSE(monsterStatuses.empty()) << "A3: the monster was never damaged";
		int32_t previousPercentage = 101;
		int64_t appliedSum = 0;
		std::vector<int32_t> applied;
		for (size_t i = 0; i < monsterStatuses.size(); i++) {
			const decoders::AttackStatusUpdate& status = monsterStatuses[i].status;
			const std::string which = "A3: the monster's SM_ATTACK_STATUS " + std::to_string(i + 1) + " of " + std::to_string(monsterStatuses.size());
			// (i) the melee auto-attack path calls onAttack(creature, damage, status, criticalProcEffect) -> the private overload with
			// TYPE.REGULAR and LOG.REGULAR (CreatureController.java:185-187). No type 3 (NATURAL_HP) may appear: nothing starts an npc restore
			// task during a fight, because the only caller of scheduleHpRestoreTask is NpcController.loseAggro(true) and the monster never
			// loses aggro (D18).
			EXPECT_EQ(status.type, decoders::ATTACK_STATUS_TYPE_REGULAR) << which << " carries type " << static_cast<int32_t>(status.type);
			EXPECT_EQ(status.logId, decoders::ATTACK_STATUS_LOG_REGULAR) << which << " carries logId " << static_cast<int32_t>(status.logId);
			EXPECT_EQ(status.skillId, 0) << which << " carries a skill id, and the melee path has none";
			// (ii) the percentage is monotonically non-increasing and the last is 0
			EXPECT_LE(static_cast<int32_t>(status.hpOrMp), previousPercentage) << which << ": the monster's HP percentage went up";
			previousPercentage = status.hpOrMp;
			applied.push_back(status.value);
			appliedSum += status.value;
		}
		EXPECT_EQ(static_cast<int32_t>(monsterStatuses.back().status.hpOrMp), 0) << "A3: the monster's last HP percentage is not 0";

		// (iii) the count equals the number of the player's SM_ATTACK that carried a non-zero total damage: sendAttackStatusPacketUpdate fires
		// only when newHp != previousHp || skillId != 0, and skillId is 0 on this path (CreatureLifeStats.cpp:97-98)
		const std::vector<FightRecording::AttackPacket> myHits = recording.attacksBetween(a.playerId, monsterObjectId);
		std::vector<int32_t> nonZeroRaw;
		int64_t rawSum = 0;
		for (const FightRecording::AttackPacket& hit : myHits) {
			rawSum += hit.totalDamage;
			if (hit.totalDamage > 0)
				nonZeroRaw.push_back(hit.totalDamage);
		}
		std::vector<std::string> attackers;
		for (const FightRecording::AttackPacket& attack : recording.attacks)
			if (attack.attack.targetObjectId == monsterObjectId && attack.attack.attackerObjectId != a.playerId)
				attackers.push_back(announcedNpcs.describe(attack.attack.attackerObjectId));
		EXPECT_EQ(monsterStatuses.size(), nonZeroRaw.size())
		  << "A3 (iii): " << monsterStatuses.size() << " SM_ATTACK_STATUS for the monster against " << nonZeroRaw.size()
		  << " damaging SM_ATTACK from the player (" << myHits.size() << " SM_ATTACK in total)"
		  << (attackers.empty() ? std::string() : "; something else also attacked it: " + join(attackers));

		// ---- A4: the two sums ----
		// (i) the applied damage sums to exactly maxHp. The `value` of SM_ATTACK_STATUS is previousHp - newHp with
		// newHp = clamp(currentHp - value, 0, currentHp), and TYPE.REGULAR takes the default arm of writeImpl, so the wire carries it positive.
		EXPECT_EQ(appliedSum, monster.maxHp) << "A4 (i): the applied damage sums to " << appliedSum << ", not to the monster's maxHp of "
		                                     << monster.maxHp;
		// (ii) the raw damage sums to >= maxHp, and the excess is confined to the last hit: SM_ATTACK carries each AttackResult.getDamage()
		// unclamped, so sum(raw) - sum(applied) must be exactly the overkill of the killing blow.
		EXPECT_GE(rawSum, monster.maxHp) << "A4 (ii): the raw damage of SM_ATTACK sums to less than the monster's HP";
		if (!nonZeroRaw.empty() && applied.size() == nonZeroRaw.size()) {
			int64_t appliedBeforeLast = 0;
			for (size_t i = 0; i + 1 < applied.size(); i++) {
				EXPECT_EQ(applied[i], nonZeroRaw[i]) << "A4 (ii): hit " << (i + 1) << " was applied as " << applied[i] << " and broadcast as "
				                                     << nonZeroRaw[i] << "; only the killing blow may be clamped";
				appliedBeforeLast += applied[i];
			}
			const int64_t remainingBeforeLast = monster.maxHp - appliedBeforeLast;
			const int64_t expectedExcess = std::max<int64_t>(0, nonZeroRaw.back() - remainingBeforeLast);
			EXPECT_EQ(rawSum - appliedSum, expectedExcess)
			  << "A4 (ii): sum(raw) - sum(applied) is " << (rawSum - appliedSum) << ", and the killing blow's overkill is " << expectedExcess
			  << " (raw " << nonZeroRaw.back() << " against " << remainingBeforeLast << " HP left)";
		}
		// every per-hit raw damage lies in [0, maxHp], and a 0 only accompanies a dodge or a resist
		for (const FightRecording::AttackPacket& hit : myHits)
			for (const decoders::AttackResultEntry& entry : hit.attack.results) {
				EXPECT_GE(entry.damage, 0) << "A4: SM_ATTACK carried a negative damage";
				EXPECT_LE(entry.damage, monster.maxHp) << "A4: one hit of a level 1 Warrior carried " << entry.damage
				                                       << " damage against a monster with " << monster.maxHp << " HP";
				if (entry.damage == 0) {
					const int8_t status = entry.attackStatusId;
					const bool dodgeOrResist =
					  status == decoders::ATTACK_STATUS_DODGE || status == decoders::ATTACK_STATUS_OFFHAND_DODGE ||
					  status == decoders::ATTACK_STATUS_RESIST || status == decoders::ATTACK_STATUS_OFFHAND_RESIST ||
					  status == decoders::ATTACK_STATUS_CRITICAL_DODGE || status == decoders::ATTACK_STATUS_OFFHAND_CRITICAL_DODGE ||
					  status == decoders::ATTACK_STATUS_CRITICAL_RESIST || status == decoders::ATTACK_STATUS_OFFHAND_CRITICAL_RESIST;
					EXPECT_TRUE(dodgeOrResist) << "A4: a 0 damage with AttackStatus id " << static_cast<int32_t>(status)
					                           << ", which is neither a dodge nor a resist";
				}
			}
		std::cout << "A3/A4: " << myHits.size() << " SM_ATTACK from the player, " << monsterStatuses.size() << " SM_ATTACK_STATUS for the monster, "
		          << rawSum << " raw damage, " << appliedSum << " applied" << std::endl;

		// ---- A5a: the monster answers when it is hit, and only then ----
		//
		// **Rev 4 (the gate lane) turns rev 3's red row into the statement a faithful server makes, and splits A5 in two.** Rev 3 asserted that
		// npc 210663 attacks a character that stands 2 m from it untouched, and the gate found that it does not. That is not a defect: which
		// npc starts a fight is a TRIBE decision, not an `ai="aggressive"` decision. `ai="aggressive"` picks the AI class that looks at what it
		// sees (AggressiveNpcAI overrides handleCreatureSee); whether it aggroes is `TribeRelationService::isAggressive(npc, creature)`
		// (CreatureEventHandler.java:87), which for a character ends in `TRIBE_RELATIONS_DATA.isAggressiveRelation(npcTribe, PC)`. npc 210663 is
		// tribe MONSTER, whose row in the real tribe_relations.xml (:2170-2173) has NO `<aggro>` element at all, and the PC row (:2302-2305) has
		// none either - so `isAggressiveRelation(MONSTER, PC)` is false in Java exactly as it is here, and rev 3's row asserted a falsehood.
		//
		// What the monster IS is *hostile*: `TribeRelationService::isHostile` has a hard-coded `baseTribe == MONSTER && PC` arm, which is why it
		// fights back the instant it is hit and why every earlier test and the first real-client session saw a monster that "fights back". Both
		// halves are asserted here so that neither can be lost: it answers damage (the rows below), and it does not start the fight (the row
		// after them). The positive side of the same decision - an npc whose tribe IS aggressive to PC, attacking an untouched character - is
		// K8b/A5b, and the unit case of both is tests/ai/UnprovokedAggroTest.cpp.
		const std::vector<FightRecording::AttackPacket> monsterHits = recording.attacksBy(monsterObjectId);
		ASSERT_FALSE(monsterHits.empty()) << "A5a: the monster never attacked at all, although the character was hitting it - the hostile half of "
		                                     "TribeRelationService::isHostile (its hard-coded MONSTER-vs-PC arm) and AggroList::addDamage";
		for (const FightRecording::AttackPacket& hit : monsterHits)
			EXPECT_EQ(hit.attack.targetObjectId, a.playerId) << "A5a: the monster attacked something that is not the character";
		// the measurement K4b took: a MONSTER-tribe npc does NOT swing at a character that stands inside its aggro range and touches nothing.
		// A negative row cannot stand alone - a server that had lost the whole AI would also pass it - which is exactly why K8b exists and why
		// this file asserts BOTH: the same 2 m, the same 20 s, the same chain, one tribe that has an <aggro>PC</aggro> row and one that has not.
		EXPECT_FALSE(unprovokedAggro.has_value())
		  << "A5a: npc " << GATE_MONSTER_NPC_ID << " attacked a character that had not touched it, "
		  << (unprovokedAggro ? unprovokedAggro->count() : 0)
		  << " ms after it stopped. Its tribe is MONSTER, whose tribe_relations.xml row (:2170-2173) has no <aggro> element, so "
		     "isAggressiveRelation(MONSTER, PC) is false and CreatureEventHandler::checkAggro must take its else branch. A port that reads the "
		     "`ai` attribute instead of the tribe, or an isAggressiveRelation that falls back to the base tribe both ways, fails exactly here";

		// ---- A6: the character's HP, reconstructed from absolute values ----
		// SM_STATUPDATE_HP carries absolute currentHp and maxHp as two writeD; SM_ATTACK_STATUS carries a percentage, so the reconstruction is
		// the only way to compare the two. Rev 2 replaced rev 1's "strictly decreasing", which was wrong: a Player's life stats use
		// HpMpRestoreTask, whose run() has no AI-state check at all, so a correct Java server regenerates the character's HP during the fight.
		ASSERT_FALSE(recording.hpUpdates.empty()) << "A6: the character was never sent an SM_STATUPDATE_HP";
		const int32_t playerMaxHp = recording.hpUpdates[0].hp.maxHp;
		EXPECT_EQ(playerMaxHp, enterStats->maxHp) << "A6: SM_STATUPDATE_HP and SM_STATS_INFO disagree about the character's maxHp";
		int32_t reconstructed = enterStats->maxHp;
		size_t damageTicks = 0, regenTicks = 0, compared = 0;
		std::vector<std::chrono::steady_clock::time_point> regenAt;
		std::optional<std::chrono::steady_clock::time_point> firstDamageAt;
		// one ordered pass over the two streams: CreatureLifeStats::reduceHp sends SM_ATTACK_STATUS first and only then runs onHpChanged,
		// which is what sends SM_STATUPDATE_HP (CreatureLifeStats.cpp:97-100, PlayerLifeStats.cpp:46-47), so the status always precedes the
		// absolute value it explains
		{
			auto status = recording.statuses.begin();
			for (const FightRecording::HpPacket& hp : recording.hpUpdates) {
				for (; status != recording.statuses.end() && status->index < hp.index; ++status) {
					if (status->status.creatureObjectId != a.playerId)
						continue;
					if (status->status.type == decoders::ATTACK_STATUS_TYPE_REGULAR) {
						reconstructed -= status->status.value;
						damageTicks++;
						if (!firstDamageAt)
							firstDamageAt = status->at;
					} else if (status->status.type == decoders::ATTACK_STATUS_TYPE_NATURAL_HP) {
						reconstructed += status->status.value;
						regenTicks++;
						regenAt.push_back(status->at);
					}
				}
				EXPECT_EQ(hp.hp.currentHp, reconstructed)
				  << "A6 (i): the SM_STATUPDATE_HP at packet " << hp.index << " says " << hp.hp.currentHp
				  << " HP, and the SM_ATTACK_STATUS stream before it reconstructs " << reconstructed;
				compared++;
			}
		}
		EXPECT_GT(compared, 0u) << "A6 (i): nothing was compared";
		EXPECT_GT(damageTicks, 0u) << "A6 (ii): no type 5 (REGULAR) SM_ATTACK_STATUS for the character - it was never damaged";
		// (iii) at least one type 3 (NATURAL_HP) packet, the first ~1,700 ms after the first damage and the rest ~6,000 ms apart
		EXPECT_GT(regenTicks, 0u)
		  << "A6 (iii): the character never regenerated. HpMpRestoreTask has no AIState.FIGHT check (LifeStatsRestoreService.java:71-77), so a "
		     "faithful port regenerates throughout the fight; the FIGHT check lives in HpRestoreTask, which only npcs and summons schedule (D18)";
		if (!regenAt.empty() && firstDamageAt) {
			const int64_t first = std::chrono::duration_cast<std::chrono::milliseconds>(regenAt[0] - *firstDamageAt).count();
			EXPECT_NEAR(static_cast<double>(first), static_cast<double>(RESTORE_FIRST_TICK.count()), 900.0)
			  << "A6 (iii): the first regeneration tick came " << first << " ms after the first damage, not about " << RESTORE_FIRST_TICK.count();
			for (size_t i = 1; i < regenAt.size(); i++) {
				const int64_t period = std::chrono::duration_cast<std::chrono::milliseconds>(regenAt[i] - regenAt[i - 1]).count();
				EXPECT_NEAR(static_cast<double>(period), static_cast<double>(RESTORE_PERIOD.count()), 1500.0)
				  << "A6 (iii): regeneration tick " << (i + 1) << " came " << period << " ms after the one before it";
			}
		}
		std::cout << "A6: " << damageTicks << " damage ticks and " << regenTicks << " regeneration ticks on the character, " << compared
		          << " SM_STATUPDATE_HP compared" << std::endl;

		// ---- A7: the monster's attack tempo ----
		// the interval between two consecutive monster SM_ATTACK is within +-25 % of the template's attack_speed, after the first
		if (monsterHits.size() >= 3) {
			for (size_t i = 2; i < monsterHits.size(); i++) {
				const int64_t interval = std::chrono::duration_cast<std::chrono::milliseconds>(monsterHits[i].at - monsterHits[i - 1].at).count();
				EXPECT_NEAR(static_cast<double>(interval), monster.npcAttackSpeed, monster.npcAttackSpeed * 0.25)
				  << "A7: the monster's attack " << (i + 1) << " came " << interval << " ms after the one before it, and its attack_speed is "
				  << monster.npcAttackSpeed;
			}
		} else {
			std::cout << "A7: only " << monsterHits.size() << " monster attacks in this fight, so no interval was compared" << std::endl;
		}
	});
	else
		cases.skip("K6", "the monster's half of the same recording (A3-A7)", "an earlier case failed");

	// ---- K7: the monster dies (D1a, D1b, R1 (a) and (b), R2, R4) ----
	runCase("K7", "the kill: death, reward, decay and respawn (D1a, D1b, R1, R2, R4)", [&] {
		ASSERT_TRUE(monsterDied) << "the monster did not die, so nothing below can be asserted";
		ASSERT_TRUE(fight);
		const FightRecording& recording = *fight;

		// ---- D1a: SM_EMOTION(DIE) for the monster, with the last attacker in the fifth field ----
		std::optional<decoders::Emotion> death;
		size_t deathIndex = 0;
		for (const auto& [index, emotion] : recording.emotions)
			if (emotion.emotionType == decoders::EMOTION_DIE && emotion.senderObjectId == monsterObjectId) {
				death = emotion;
				deathIndex = index;
			}
		ASSERT_TRUE(death) << "D1a: no SM_EMOTION(DIE) for the monster";
		EXPECT_EQ(death->targetObjectId, a.playerId)
		  << "D1a: SM_EMOTION's DIE arm writes the last attacker's object id as its fifth field, and it is 0 only when the creature killed "
		     "itself (CreatureController.java:164-165); this one says " << death->targetObjectId;

		// ---- D1b: no further SM_ATTACK from the monster after its death ----
		const std::vector<Packet>& packets = a.game->recorded();
		std::vector<std::string> afterDeath;
		for (const FightRecording::AttackPacket& attack : recording.attacks)
			if (attack.index > deathIndex && attack.attack.attackerObjectId == monsterObjectId)
				afterDeath.push_back("packet " + std::to_string(attack.index));
		EXPECT_TRUE(afterDeath.empty()) << "D1b: the monster attacked after it died: " << join(afterDeath);

		// ---- R1 (a): the experience message, from packets ----
		// exactly one SM_SYSTEM_MESSAGE with msgId 1370000 (STR_GET_EXP), whose SECOND string parameter is the oracle's awarded value.
		// The STR_GET_EXP variant is itself the proof that repose energy was 0 and that the number is the whole reward (§8 risk 17).
		std::vector<SystemMessage> expMessages;
		std::vector<std::string> otherMessages;
		for (size_t i = killWindowStart; i < packets.size(); i++) {
			if (packets[i].name != "SM_SYSTEM_MESSAGE")
				continue;
			const SystemMessage message = decodeSystemMessage(packets[i].data);
			if (message.messageId == STR_GET_EXP)
				expMessages.push_back(message);
			else
				otherMessages.push_back(std::to_string(message.messageId));
		}
		ASSERT_EQ(expMessages.size(), 1u) << "R1 (a): " << expMessages.size() << " SM_SYSTEM_MESSAGE with msgId " << STR_GET_EXP
		                                  << " (STR_GET_EXP), not exactly one; the other system messages of the kill were "
		                                  << (otherMessages.empty() ? "none" : join(otherMessages));
		ASSERT_GE(expMessages[0].params.size(), 2u) << "R1 (a): STR_GET_EXP(name, reward) writes two parameters, this one wrote "
		                                            << expMessages[0].params.size();
		EXPECT_EQ(expMessages[0].params[1], std::to_string(monster.awarded))
		  << "R1 (a): STR_GET_EXP's second parameter is the reward. The oracle computes " << monster.awarded << " ("
		  << monster.experienceReward << " capped at expNeed * 0.2f = " << static_cast<int64_t>(monster.expNeed * 0.2f)
		  << "); a port that drops Math.min awards " << monster.experienceReward << ", and one that drops Math.round truncates";

		// ---- R1 (b): SM_STATUPDATE_EXP ----
		std::vector<StatUpdateExp> expUpdates;
		for (size_t i = killWindowStart; i < packets.size(); i++)
			if (packets[i].name == "SM_STATUPDATE_EXP")
				expUpdates.push_back(decodeStatUpdateExp(packets[i].data));
		ASSERT_FALSE(expUpdates.empty()) << "R1 (b): no SM_STATUPDATE_EXP after the kill (PlayerCommonData::setExp sends one unconditionally)";
		EXPECT_EQ(expUpdates.back().currentExp, monster.awarded) << "R1 (b): getExpShown() after the kill";
		EXPECT_EQ(expUpdates.back().maxExp, monster.expNeed)
		  << "R1 (b): getExpNeed() is getStartExpForLevel(level + 1) - getStartExpForLevel(level), and the table is 1-based (D7)";
		EXPECT_EQ(expUpdates.back().recoverableExp, 0) << "R1 (b): a character that never died has no recoverable exp";

		// ---- R2: what NpcAI::ask answered ----
		// REWARD_AP false -> no SM_ABYSS_RANK. The arm is self-enforcing besides: D16 leaves AbyssPointsService::addAp AION_UNPORTED, so a
		// port that answers REWARD_AP true throws, NpcController::onDie's catch logs an ERROR and Q1's two checks both fail.
		std::vector<std::string> abyssPackets;
		for (size_t i = killWindowStart; i < packets.size(); i++)
			if (packets[i].name == "SM_ABYSS_RANK")
				abyssPackets.push_back("packet " + std::to_string(i));
		EXPECT_TRUE(abyssPackets.empty())
		  << "R2: NpcAI.ask answers REWARD_AP with `wt == ABYSS || wt != ELYSEA && wt != ASMODAE && apRewardingRaces.contains(getRace())` "
		     "(NpcAI.java:153-156), and Poeta is ELYSEA, so the answer is false and no abyss point is awarded: " << join(abyssPackets);
		// ALLOW_DECAY true -> the corpse is NOT deleted immediately. With DummyAI::ask, which answers false to everything, onDie takes the
		// instant delete_() arm and the corpse vanishes at once (D8) - this is the assertion that catches a lane that forgets `ask`.
		const auto deathAt = packets[deathIndex].receivedAt;
		std::vector<std::string> instantDeletes;
		for (size_t i = deathIndex; i < packets.size(); i++) {
			if (packets[i].name != "SM_DELETE")
				continue;
			if (decoders::decodeDeleteObjectId(packets[i].data) != monsterObjectId)
				continue;
			const int64_t after = std::chrono::duration_cast<std::chrono::milliseconds>(packets[i].receivedAt - deathAt).count();
			if (after + 300 < IMMEDIATE_DECAY.count())
				instantDeletes.push_back(std::to_string(after) + " ms after the death");
		}
		EXPECT_TRUE(instantDeletes.empty())
		  << "R2: the corpse was deleted before RespawnService::IMMEDIATE_DECAY (" << IMMEDIATE_DECAY.count()
		  << " ms), i.e. onDie took its instant-despawn arm: " << join(instantDeletes);

		// ---- R4: the respawn ----
		// After the spawn's respawn_time plus slack, a new SM_NPC_INFO for the id at the same position to +-0.01 and with HP 100 %; before it,
		// an SM_DELETE for the old object id. A respawn at a wrong z is exactly the geo class of bug F-1 warned about, which is why
		// gs.scenario.m5b_geo runs the same assertion against a world that has its terrain.
		const OracleMonsterSpot& spot = *monster.nearestPlainSpot;
		const auto respawnDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(monster.respawnTime) + 25s;
		int32_t respawnedObjectId = 0;
		bool oldDeleted = false;
		std::vector<std::string> respawnsElsewhere;
		const size_t respawnFrom = packets.size();
		while (std::chrono::steady_clock::now() < respawnDeadline && respawnedObjectId == 0) {
			std::optional<Packet> packet = a.game->next(2s);
			if (!packet) {
				if (a.game->client.socket.isClosed())
					break;
				continue;
			}
			if (packet->name == "SM_DELETE" && decoders::decodeDeleteObjectId(packet->data) == monsterObjectId)
				oldDeleted = true;
			if (packet->name != "SM_NPC_INFO")
				continue;
			const decoders::NpcInfo npc = decoders::decodeNpcInfo(packet->data);
			if (npc.templateId != GATE_MONSTER_NPC_ID || npc.objectId == monsterObjectId)
				continue;
			if (std::abs(npc.x - spot.x) <= 0.01f && std::abs(npc.y - spot.y) <= 0.01f && std::abs(npc.z - spot.z) <= 0.01f) {
				respawnedObjectId = npc.objectId;
				EXPECT_EQ(npc.hpPercentage, 100) << "R4: the respawned monster is not at full HP";
				EXPECT_EQ(npc.maxHp, monster.maxHp);
			} else {
				respawnsElsewhere.push_back("(" + std::to_string(npc.x) + ", " + std::to_string(npc.y) + ", " + std::to_string(npc.z) + ")");
			}
		}
		// the delete may have arrived before the loop started (2 s after the death, while R1 was being asserted): scan the recording too
		for (size_t i = deathIndex; i < respawnFrom && !oldDeleted; i++)
			if (packets[i].name == "SM_DELETE" && decoders::decodeDeleteObjectId(packets[i].data) == monsterObjectId)
				oldDeleted = true;
		EXPECT_TRUE(oldDeleted) << "R4: no SM_DELETE for the dead monster's object id " << monsterObjectId;
		EXPECT_NE(respawnedObjectId, 0) << "R4: npc " << GATE_MONSTER_NPC_ID << " did not respawn at (" << spot.x << ", " << spot.y << ", "
		                                << spot.z << ") within " << monster.respawnTime << " s + 25 s"
		                                << (respawnsElsewhere.empty() ? std::string() : "; it respawned at " + join(respawnsElsewhere));
		if (respawnedObjectId != 0) {
			EXPECT_NE(respawnedObjectId, monsterObjectId) << "R4: a killed npc respawns as a NEW object (Q3 counts on it)";
			std::cout << "R4: npc " << GATE_MONSTER_NPC_ID << " respawned as object " << respawnedObjectId << " at the same spot" << std::endl;
			monsterObjectId = respawnedObjectId;
		}
	});

	// ---- K7c: A8, the chase ----
	// Not a gating case: its assertion is deliberately a disjunction (§6.3 A8 "the monster either follows or the fight ends"), and what the
	// monster chooses depends on the map's aiInfo, so a failure here must not cost the run its K7b, K8 and K9 evidence.
	if (ok)
		cases.run("K7c", "the character runs 40 m and the monster decides (A8)", [&] {
			ASSERT_NE(monsterObjectId, 0);
			// Pull the respawned monster the same way K4/K4b pulled the first one: OUT of aggro range first and then in. Walking in is what
			// the pull needs - the character stands 2 m from the spot when the respawn happens, and a character that does not move gives
			// CreatureEventHandler::checkAggro no CREATURE_MOVED to run on. Measured: with only a stop-move at the same position the respawned
			// monster did not attack within 24 s; with the approach below it behaves exactly as it does in K4b.
			const std::array<float, 3> back = pointNearSpot(STAND_OFF_DISTANCE, elyos.x, elyos.y);
			walkTo(back[0], back[1], back[2]);
			const std::array<float, 3> melee = pointNearSpot(MELEE_DISTANCE, atX, atY);
			walkTo(melee[0], melee[1], melee[2]);
			const size_t pullFrom = a.game->recorded().size();
			// A8 is about what the monster does when its target runs away, not about how the fight started, so the pull falls back to one
			// CM_ATTACK when the aggro does not come - which, since rev 4, is what a MONSTER-tribe npc always does (A5a). A5a and A5b are the
			// rows that own the aggro decision, in K4b and K8b; repeating either here would report the same finding from a non-gating case.
			std::optional<std::chrono::milliseconds> aggroed = waitForAttackBy(*a.game, monsterObjectId, 15s);
			if (!aggroed) {
				a.game->send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(monsterObjectId));
				a.game->send(GameSession::CM_ATTACK, GameSession::buildCM_ATTACK(monsterObjectId));
				aggroed = waitForAttackBy(*a.game, monsterObjectId, 15s);
				std::cout << "A8: the respawned monster did not aggro on its own; the pull used one CM_ATTACK" << std::endl;
			}
			const FightRecording pull = recordFight(*a.game, pullFrom);
			EXPECT_TRUE(pull.decodeFailures.empty()) << "A8: packets of the pull did not decode:\n  " << join(pull.decodeFailures, "\n  ");
			ASSERT_TRUE(aggroed) << "A8: the respawned monster neither aggroed nor answered a CM_ATTACK, so there is no chase to observe";

			const std::array<float, 3> away = pointNearSpot(CHASE_AWAY_DISTANCE, elyos.x, elyos.y);
			const size_t chaseFrom = a.game->recorded().size();
			walkTo(away[0], away[1], away[2]);
			const std::vector<Packet> chase = collectFor(*a.game, 20s);

			// the disjunction: either the monster's SM_MOVE positions approach the character, or the fight ends (no further monster SM_ATTACK)
			const OracleMonsterSpot& spot = *monster.nearestPlainSpot;
			bool moved = false;
			for (const Packet& packet : chase)
				if (packet.name == "SM_MOVE" && decoders::decodeMoveObjectId(packet.data) == monsterObjectId)
					moved = true;
			const FightRecording chaseRecording = recordFight(*a.game, chaseFrom);
			size_t attacksWhileAway = 0;
			for (const FightRecording::AttackPacket& attack : chaseRecording.attacks)
				if (attack.attack.attackerObjectId == monsterObjectId)
					attacksWhileAway++;
			EXPECT_TRUE(moved || attacksWhileAway == 0)
			  << "A8: the monster neither moved (AttackManager.targetTooFar -> moveToTargetObject) nor gave up: it kept attacking from "
			  << CHASE_AWAY_DISTANCE << " m away, " << attacksWhileAway << " times";
			// no forced move: the server must not correct the character's position while it runs (m5a-plan.md §5.6 M3)
			EXPECT_TRUE(ofName(chase, "SM_FORCED_MOVE").empty()) << "A8: the server corrected the character's position during the chase";
			std::cout << "A8: the monster " << (moved ? "moved" : "did not move") << " while the character was " << CHASE_AWAY_DISTANCE
			          << " m away, and attacked " << attacksWhileAway << " times; its spot is (" << spot.x << ", " << spot.y << ")" << std::endl;

			// walk back out of aggro range and give the monster time to go home, so K8 can pull it from its spot again
			const std::array<float, 3> standOff = pointNearSpot(STAND_OFF_DISTANCE, elyos.x, elyos.y);
			walkTo(standOff[0], standOff[1], standOff[2]);
			collectFor(*a.game, 20s);
		});

	// ---- K7b: quit, read the row, re-enter (R1 (c)) ----
	int64_t apBeforeQuit = 0;
	int64_t expAfterQuit = 0;
	runCase("K7b", "quit, the experience reaches the database, re-enter with 1 HP (R1 (c), D12)", [&] {
		const std::string player = std::to_string(a.playerId);
		// R2's abyss half is a POST-QUIT read and cannot be a before/after pair: there is no `abyss_rank` row at all while the character has
		// never left the world (measured: the SELECT answers nothing here, and 0 after the quit). AbyssRankDAO writes the row from
		// PlayerService::storePlayer, the same store D17 is about, so the only honest form is "after the quit it is 0" - which is what a
		// REWARD_AP that answered true would break, on top of throwing in the AION_UNPORTED AbyssPointsService::addAp that D16 leaves in place.

		// This is the ONLY point at which the kill's experience reaches the database (D17): PeriodicSaveService schedules no character save in
		// 4.8, so PlayerService::storePlayer runs from the enter-world and leave-world paths alone. A mid-run read returns the login value
		// whatever the server did, which is why rev 1's R1 could not work.
		a.game->send(GameSession::CM_QUIT, GameSession::buildCM_QUIT(true));
		waitFor(*a.game, "SM_QUIT_RESPONSE", 30s);
		expAfterQuit = database.queryLong(schema, "SELECT exp FROM players WHERE id = " + player).value_or(-1);
		EXPECT_EQ(expAfterQuit, monster.awarded)
		  << "R1 (c): players.exp after the quit. A port that updates the packet but not PlayerCommonData.exp fails exactly here and nowhere "
		     "else, because nothing writes the row while the character is online";
		EXPECT_EQ(database.queryLong(schema, "SELECT COUNT(*) FROM abyss_rank WHERE player_id = " + player).value_or(0), 1)
		  << "R2: the quit wrote no abyss_rank row, so the read below would pass for want of a row";
		apBeforeQuit = database.queryLong(schema, "SELECT ap FROM abyss_rank WHERE player_id = " + player).value_or(-1);
		EXPECT_EQ(apBeforeQuit, 0)
		  << "R2: abyss_rank.ap is " << apBeforeQuit << " after a PvE kill on an ELYSEA map. NpcAI.ask answers REWARD_AP with "
		     "`wt == ABYSS || wt != ELYSEA && wt != ASMODAE && apRewardingRaces.contains(getRace())` (NpcAI.java:153-156), Poeta is ELYSEA, so "
		     "the answer is false and the race term is never reached";

		// D12: the character's death is arranged, not hoped for. A level 1 Warrior in starter gear beats a level 2 sparkie most of the time.
		const std::optional<int64_t> maxHp = database.queryLong(schema, "SELECT hp FROM player_life_stats WHERE player_id = " + player);
		ASSERT_TRUE(maxHp) << "no player_life_stats row";
		database.execute(schema, "UPDATE player_life_stats SET hp = 1 WHERE player_id = " + player);
		std::this_thread::sleep_for(1500ms); // gameserver.character.reentry.time is 1 second in the scenario profile

		a.game->send(GameSession::CM_ENTER_WORLD, GameSession::buildCM_ENTER_WORLD(a.playerId));
		const std::vector<Packet> reenter = collectBurst(*a.game, async);
		const int32_t inventoryPackets = static_cast<int32_t>((elyos.items.size() + 9) / 10) + 1;
		expectSequence(reenter, enterWorldPattern(false, inventoryPackets), async);
		const std::vector<Packet> stats = ofName(reenter, "SM_STATS_INFO");
		ASSERT_FALSE(stats.empty());
		const decoders::StatsInfo statsInfo = decoders::decodeStatsInfo(stats.back().data);
		EXPECT_EQ(statsInfo.currentHp, 1) << "D12: the seeded 1 HP was not restored from the database";
		EXPECT_EQ(statsInfo.expShown, monster.awarded) << "R1 (c): the stored experience came back with the character";
		a.game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
		collectBurst(*a.game, async);
	});

	// ---- K8: the player dies and revives (P1, P2, P3 (a)) ----
	size_t deathWindowStart = 0;
	runCase("K8", "the character dies and revives at its bind point (P1, P2, P3 (a))", [&] {
		ASSERT_NE(monsterObjectId, 0);
		deathWindowStart = a.game->recorded().size();
		// walk into aggro range and do NOT attack. The walk is also what ends the spawn protection the re-entry started
		// (PlayerController.java:626): with it active PlayerController::onAttack returns before any damage and the character cannot die.
		const std::array<float, 3> melee = pointNearSpot(MELEE_DISTANCE, atX, atY);
		walkTo(melee[0], melee[1], melee[2]);
		// The case is "walk into aggro range and do NOT attack", and that is what it does first. It falls back to a single CM_ATTACK when the
		// monster has not swung within 20 s, because what P1, P2 and P3 are about is the character's death and its revive, not the aggro path -
		// A5a owns that, in K4b, against a monster the character had not touched. The fallback prints, so a run never hides which path it took.
		// Since rev 4 the fallback is the EXPECTED path and not a safety net: npc 210663 is tribe MONSTER, which has no `<aggro>` row, so it
		// never starts a fight (A5a). The wait is kept at its full length so that this case observes the same silence K4b measures, one case
		// after the character came back into the world; K8b is where a character is attacked without touching anything.
		if (!waitForAttackBy(*a.game, monsterObjectId, 20s)) {
			std::cout << "P1: the monster did not aggro the standing character within 20 s; the pull used one CM_ATTACK" << std::endl;
			a.game->send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(monsterObjectId));
			a.game->send(GameSession::CM_ATTACK, GameSession::buildCM_ATTACK(monsterObjectId));
		}

		std::optional<decoders::Emotion> death;
		const auto deadline = std::chrono::steady_clock::now() + 90s;
		while (std::chrono::steady_clock::now() < deadline && !death) {
			std::optional<Packet> packet = a.game->next(2s);
			if (!packet) {
				if (a.game->client.socket.isClosed())
					break;
				continue;
			}
			if (packet->name != "SM_EMOTION")
				continue;
			try {
				const decoders::Emotion emotion = decoders::decodeEmotion(packet->data);
				if (emotion.emotionType == decoders::EMOTION_DIE && emotion.senderObjectId == a.playerId)
					death = emotion;
			} catch (const DecodeError&) {
				// reported by the recording below
			}
		}
		ASSERT_TRUE(death) << "P1: the monster did not kill the character within 90 s, although its HP was seeded to 1 (D12)";
		EXPECT_EQ(death->targetObjectId, monsterObjectId) << "P1: SM_EMOTION(DIE) names the last attacker; this one says " << death->targetObjectId;

		// P1: SM_DIE about 500 ms later (PlayerController::scheduleShowResurrectionOptions -> showResurrectionOptions)
		const auto diedAt = std::chrono::steady_clock::now();
		const Packet die = waitFor(*a.game, "SM_DIE", 20s);
		const decoders::Die dieBody = decoders::decodeDie(die.data);
		EXPECT_FALSE(dieBody.allowReviveByItem) << "P1: a level 1 Warrior has no self-resurrection item";
		EXPECT_EQ(dieBody.remainingKiskTimeSeconds, 0) << "P1: there is no kisk on Poeta";
		EXPECT_FALSE(dieBody.allowInstanceRevive) << "P1: Poeta is not an instance, so SM_DIE selects ReviveType.BIND_REVIVE";
		std::cout << "P1: SM_DIE arrived " << std::chrono::duration_cast<std::chrono::milliseconds>(die.receivedAt - diedAt).count()
		          << " ms after the death emotion was read" << std::endl;
		EXPECT_EQ(database.queryLong(schema, "SELECT online FROM players WHERE id = " + std::to_string(a.playerId)), 1)
		  << "P1: a dead character stays online";

		// P2: CM_REVIVE(BIND_REVIVE). The observable burst is SM_CHANNEL_INFO, SM_PLAYER_INFO, SM_STATS_INFO, SM_MOTION and NOT
		// SM_PLAYER_SPAWN: a character that dies on Poeta and binds to the Elyos spawn point is on the same map and the same instance, so
		// SpawnTask::run takes the spawnOnSameMap arm (TeleportService.java:524-526); SM_CHANNEL_INFO + SM_PLAYER_SPAWN is the OTHER arm, the
		// one that reloads the map and waits for CM_LEVEL_READY.
		const size_t reviveFrom = a.game->recorded().size();
		a.game->send(GameSession::CM_REVIVE, GameSession::buildCM_REVIVE(GameSession::BIND_REVIVE));
		const std::vector<Packet> revive = collectBurst(*a.game, async, 1500ms, 30s);
		// The four packets are asserted as a SUBSEQUENCE and not as the whole burst: `revive(...)` broadcasts SM_EMOTION(RESURRECT) and
		// `updateStatsAndSpeedVisually()` runs before the teleport, so the burst legitimately begins before SpawnTask::run's own first packet.
		// What P2 owns is the ORDER of the spawnOnSameMap arm and the absence of the other arm's SM_PLAYER_SPAWN.
		EXPECT_TRUE(containsInOrder(namesOf(revive), {"SM_CHANNEL_INFO", "SM_PLAYER_INFO", "SM_STATS_INFO", "SM_MOTION"}))
		  << "P2: the first four packets of TeleportService's spawnOnSameMap arm (TeleportService.java:209-212) are not in the revive burst in "
		     "that order; got "
		  << join(namesOf(revive));
		EXPECT_TRUE(ofName(revive, "SM_PLAYER_SPAWN").empty())
		  << "P2: SM_PLAYER_SPAWN belongs to the other arm of SpawnTask::run, the one that reloads the map and waits for CM_LEVEL_READY; a "
		     "character that dies on Poeta and binds to the Elyos spawn point is on the same map and the same instance. Got "
		  << join(namesOf(revive));
		const std::vector<Packet> reviveStats = ofName(revive, "SM_STATS_INFO");
		ASSERT_FALSE(reviveStats.empty()) << "P2: no SM_STATS_INFO after the revive";
		// the FIRST one: PlayerLifeStats::onHpChanged triggers the restore task when previousHp was 0, so a regeneration tick 1,700 ms later
		// would move the percentage this row is about
		const decoders::StatsInfo revived = decoders::decodeStatsInfo(reviveStats.front().data);
		// PlayerReviveService.bindRevive outside EVENT_MODE is revive(player, 25, 25, true, skillId) (PlayerReviveService.java:104-108)
		EXPECT_GT(revived.currentHp, 0) << "P2: the character is still dead after CM_REVIVE(BIND_REVIVE)";
		EXPECT_EQ(revived.currentHp, static_cast<int32_t>(revived.maxHp * 25 / 100))
		  << "P2: bindRevive revives with 25 % HP (" << revived.currentHp << " of " << revived.maxHp << ")";
		EXPECT_EQ(revived.currentMp, static_cast<int32_t>(revived.maxMp * 25 / 100))
		  << "P2: bindRevive revives with 25 % MP (" << revived.currentMp << " of " << revived.maxMp << ")";

		// P3 (a): NO SM_STATUPDATE_EXP between the death and the revive. PlayerController::onDie guards calculateExpLoss with getLevel() > 4,
		// and calculateExpLoss ends by sending one unconditionally (PlayerCommonData.java:144-146), so its absence is the proof the guard held.
		// The same row catches a second bug: the full Java guard is `getLevel() > 4 && !hasAbnormalEffect(Effect::isNoDeathPenalty)`, and
		// EffectController::hasAbnormalEffect is AION_UNPORTED - at level 1 Java's && short-circuits before it, and a port that swaps the
		// operands throws. Q1's empty unported_trace.txt is what sees that.
		const std::vector<Packet>& packets = a.game->recorded();
		std::vector<std::string> expPackets;
		for (size_t i = deathWindowStart; i < reviveFrom; i++)
			if (packets[i].name == "SM_STATUPDATE_EXP")
				expPackets.push_back("packet " + std::to_string(i));
		EXPECT_TRUE(expPackets.empty()) << "P3 (a): the death sent " << expPackets.size()
		                                << " SM_STATUPDATE_EXP, so calculateExpLoss ran for a level 1 character: " << join(expPackets);
	});

	// ---- K8b: A5b, the other side of the aggro decision ----
	//
	// **Rev 4 (the gate lane).** A5a states that a MONSTER-tribe npc never starts a fight. On its own that is a negative row, and a negative row
	// passes for a server whose whole aggro chain is dead - which is exactly the failure mode A5 exists to catch. This case is the positive half:
	// the same 2 m, the same silence-free window, the same chain, against an npc whose TRIBE the relation table makes aggressive to PC. Between
	// them the two rows pin the decision itself: `ai="aggressive"` picks the AI class, `isAggressiveRelation(tribe, PC)` decides the fight.
	//
	//   the character stops moving -> CreatureController::onStopMove -> MovementNotifyTask (a 500 ms FIFO periodic task manager)
	//   -> CREATURE_MOVED on every npc of the character's known list -> NpcAI::handleCreatureMoved -> CreatureEventHandler::checkAggro
	//   -> isInSeeRange && isAggressive && !isFriend && isEnemyFrom && validateAggro && GeoService::canSee -> CREATURE_AGGRO
	//   -> AggroEventHandler::onAggro -> a 500 ms AggroNotifier -> AggroList::addHate(target, 1) -> NpcController::onAddHate -> the ATTACK
	//   event -> AttackEventHandler::onAttack -> AIState::FIGHT + AttackManager::startAttacking -> the npc walks into range and swings
	//
	// **Where it is in the run, and why here.** Last, after K8, because A5b's npc is a level 4 that hits a level 1 character hard: anywhere
	// earlier it would take HP into K5's fight (which the character must win), into K7b's quit or into K8's arranged death, and a gate whose
	// assertions depend on how one uncontrolled npc rolled is not a gate. After K8 the only thing left is K9's report bar, which does not care
	// where the character stands - and K9 runs whatever this case does, because it is not one of the `runCase` chain's dependants.
	//
	// **And the character is walked out of it rather than left to die.** It revives at 25 % HP in K8, walks 586 m, and is attacked by something
	// that out-levels it three to one; the case therefore reads the ONE packet A5b is about and then runs, and if the npc was faster than the
	// retreat it revives the character again, so that Q1's census and Q2's `Player` row are not asked to explain a body this case left behind.
	runCase("K8b", "an aggressive-TRIBE npc attacks the untouched character (A5b)", [&] {
		// §6.6, the mutation this row is proved with: point the SAME case at the gate's MONSTER-tribe monster by changing these two lines to
		// `monster` and `GATE_MONSTER_NPC_ID`. Everything below follows the two of them - the spot, the template values, the messages - so the
		// mutated run is this exact script against a tribe with no `<aggro>` row, and A5b has to go red while K0 and every other row stays
		// green. That is how it was measured before this file was written (2026-09-23), and it is two lines away from being measured again.
		const OracleMonster& target = aggressive;
		const int32_t targetNpcId = AGGRESSIVE_TRIBE_NPC_ID;

		ASSERT_TRUE(target.nearestPlainSpot) << "A5b: K0 found no plain spot for npc " << targetNpcId;
		const OracleMonsterSpot& spot = *target.nearestPlainSpot;

		// K8's CM_REVIVE(BIND_REVIVE) teleported the character to its bind point, and for a character that has never used a bind stone that is
		// the race's spawn point - the position K2 asserted SM_PLAYER_SPAWN carries. The walk cursor has to be moved with it, or the first
		// steps of the trek would interpolate from a position the character left two cases ago.
		atX = elyos.x;
		atY = elyos.y;
		atZ = elyos.z;

		// 12 m first: outside the npc's 7 m aggro range, so the object id is read from a distance at which nothing can have aggroed yet. The
		// trek drains as it walks (see trekTo), and the SM_NPC_INFO that announces the npc arrives somewhere in the middle of it.
		const size_t trekFrom = a.game->recorded().size();
		const std::array<float, 3> standOff = pointNear(spot, STAND_OFF_DISTANCE, elyos.x, elyos.y);
		trekTo(standOff[0], standOff[1], standOff[2]);

		// The object id is whatever the server announced for that spot. The scan covers the WHOLE recording and not only the walk: an npc enters
		// a character's known list once, so whether its SM_NPC_INFO arrived during this walk or earlier depends on how far its spot is from
		// wherever the character last stood - a property of the map, not of the aggro decision this case is about. It matters for the §6.6
		// mutation above: aimed at the gate's own monster, 53 m from the bind point, the announcement is part of K8's revive burst, and a
		// walk-only scan would fail on a missing packet instead of on A5b's own row. `announcedDuringWalk` keeps that diagnostic.
		int32_t aggressiveObjectId = 0;
		bool announcedDuringWalk = false;
		std::vector<std::string> elsewhere;
		const std::vector<Packet>& walked = a.game->recorded();
		for (size_t i = 0; i < walked.size(); i++) {
			if (walked[i].name != "SM_NPC_INFO")
				continue;
			decoders::NpcInfo npc;
			try {
				npc = decoders::decodeNpcInfo(walked[i].data);
			} catch (const DecodeError&) {
				continue; // a packet that does not decode is not this npc; the announcement below fails and says so
			}
			if (npc.templateId != targetNpcId)
				continue;
			if (std::abs(npc.x - spot.x) <= 0.01f && std::abs(npc.y - spot.y) <= 0.01f && std::abs(npc.z - spot.z) <= 0.01f) {
				aggressiveObjectId = npc.objectId;
				announcedDuringWalk = i >= trekFrom;
				EXPECT_EQ(static_cast<int32_t>(npc.level), target.level) << "A5b: SM_NPC_INFO announces a level the template does not have";
				EXPECT_EQ(npc.maxHp, target.maxHp);
			} else {
				elsewhere.push_back("(" + std::to_string(npc.x) + ", " + std::to_string(npc.y) + ", " + std::to_string(npc.z) + ")");
			}
		}
		ASSERT_NE(aggressiveObjectId, 0) << "A5b: the server never sent an SM_NPC_INFO for npc " << targetNpcId << " at its spot (" << spot.x
		                                 << ", " << spot.y << ", " << spot.z << "), although the character walked to "
		                                 << static_cast<int32_t>(STAND_OFF_DISTANCE) << " m of it; it was announced at "
		                                 << (elsewhere.empty() ? "no other position" : join(elsewhere));
		std::cout << "A5b: npc " << targetNpcId << " is object " << aggressiveObjectId << ", announced "
		          << (announcedDuringWalk ? "while the character walked up to it" : "before this case began") << std::endl;

		// and from 12 m it has not attacked: the stand-off is outside its srange, exactly as K4's is outside the gate monster's
		const FightRecording approach = recordFight(*a.game, trekFrom);
		EXPECT_TRUE(approach.attacksBy(aggressiveObjectId).empty())
		  << "A5b: npc " << targetNpcId << " attacked from beyond its aggro range of " << target.aggroRange << " m";

		// the whole case: walk to 2 m, touch nothing, and wait. 2 m is inside the npc's short aggro range (3 m for an srange of 7,
		// Npc.cpp:245-248), so `isInSeeRange` is true whichever way it faces, and K0 asserted that. The window matches A5a's: the chain has a
		// 500 ms AggroNotifier in it and the npc still has to walk into its own attack range.
		const size_t aggroFrom = a.game->recorded().size();
		const std::array<float, 3> melee = pointNear(spot, MELEE_DISTANCE, atX, atY);
		walkTo(melee[0], melee[1], melee[2]);
		const std::optional<std::chrono::milliseconds> attacked = waitForAttackBy(*a.game, aggressiveObjectId, 20s);
		std::cout << "A5b: npc " << targetNpcId << " (tribe " << target.tribe << ") "
		          << (attacked ? "attacked " + std::to_string(attacked->count()) + " ms after the character stopped " +
		                           std::to_string(MELEE_DISTANCE) + " m away, without being touched"
		                       : "did NOT attack within 20 s")
		          << std::endl;
		EXPECT_TRUE(attacked.has_value())
		  << "A5b: npc " << targetNpcId << " (tribe " << target.tribe << ", ai " << target.ai << ", srange " << target.aggroRange
		  << ") did not attack a character standing " << MELEE_DISTANCE
		  << " m away within 20 s, and the character never touched it. Its tribe row carries <aggro>PC PC_DARK</aggro> "
		     "(tribe_relations.xml:29-31), so isAggressiveRelation(AGGRESSIVESINGLEMONSTER, PC) is true and the whole chain of §2.1 steps 8, 9 "
		     "and 11 - CREATURE_MOVED -> checkAggro -> CREATURE_AGGRO -> the 500 ms AggroNotifier -> addHate -> ATTACK -> AttackEventHandler -> "
		     "AttackManager - has to run. This is the row that says the server can START a fight, and it is the half A5a cannot prove";
		if (attacked)
			EXPECT_LE(attacked->count(), 10000) << "A5b: the aggro took " << attacked->count()
			                                    << " ms, which is more than the 500 ms AggroNotifier and one walk into attack range";

		// what it attacked is the character, and the character attacked nothing: without that second half the row would prove the damage path
		const FightRecording pull = recordFight(*a.game, aggroFrom);
		EXPECT_TRUE(pull.decodeFailures.empty()) << "A5b: packets of the aggro did not decode:\n  " << join(pull.decodeFailures, "\n  ");
		for (const FightRecording::AttackPacket& hit : pull.attacksBy(aggressiveObjectId))
			EXPECT_EQ(hit.attack.targetObjectId, a.playerId) << "A5b: the npc attacked something that is not the character";
		EXPECT_TRUE(pull.attacksBy(a.playerId).empty())
		  << "A5b: the character attacked in this window, so what came back is AggroList::addDamage and onAddHate - the damage path A3 and A4 "
		     "own - and not the aggro path";

		// walk out of it: 60 m is outside both the npc's aggro range and the distance it chases from, and the retreat also drains the socket
		const std::array<float, 3> away = pointNear(spot, 60.0, elyos.x, elyos.y);
		walkTo(away[0], away[1], away[2]);
		const std::vector<Packet> retreat = collectFor(*a.game, 5s);
		std::vector<std::string> stillSwinging;
		for (const Packet& packet : retreat) {
			if (packet.name != "SM_ATTACK")
				continue;
			try {
				if (decoders::decodeAttack(packet.data).attackerObjectId == aggressiveObjectId)
					stillSwinging.push_back("one more swing");
			} catch (const DecodeError&) {
				// the retreat is not an assertion about SM_ATTACK's body; A5b's own window above reported any decode failure
			}
		}

		// and if it was faster than the retreat, the character is revived rather than left dead for Q1's census to find. This is not an
		// assertion about the revive - P2 owns that - it is the case cleaning up after itself.
		bool died = false;
		const std::vector<Packet>& afterAggro = a.game->recorded();
		for (size_t i = aggroFrom; i < afterAggro.size(); i++) {
			if (afterAggro[i].name != "SM_EMOTION")
				continue;
			try {
				const decoders::Emotion emotion = decoders::decodeEmotion(afterAggro[i].data);
				if (emotion.emotionType == decoders::EMOTION_DIE && emotion.senderObjectId == a.playerId)
					died = true;
			} catch (const DecodeError&) {
				// same as above
			}
		}
		if (died) {
			a.game->send(GameSession::CM_REVIVE, GameSession::buildCM_REVIVE(GameSession::BIND_REVIVE));
			const std::vector<Packet> revived = collectBurst(*a.game, async, 1500ms, 30s);
			const std::vector<Packet> stats = ofName(revived, "SM_STATS_INFO");
			ASSERT_FALSE(stats.empty()) << "A5b: the character died to npc " << targetNpcId
			                            << " and the bind revive that has to undo it sent no SM_STATS_INFO";
			EXPECT_GT(decoders::decodeStatsInfo(stats.front().data).currentHp, 0)
			  << "A5b: the character is still dead after the clean-up revive, and K9's census would have to explain it";
		}
		std::cout << "A5b: the character " << (died ? "died to the npc and was revived at its bind point" : "walked away alive")
		          << "; the npc swung " << (attacked ? stillSwinging.size() + 1 : 0) << " time(s) in all" << std::endl;
	});

	// ---- K9: the reports and the shutdown ----
	int64_t connectionsAtShutdown = 0;
	std::optional<int32_t> gameServerExit;
	if (a.game && !a.game->client.socket.isClosed())
		connectionsAtShutdown = 1;
	// the stop file is written WITH the character online, as the M5a gate's case 7 does
	gameServerExit = servers.stopGameServer();
	if (a.game)
		a.game->waitClosed(60s);
	const std::optional<int32_t> loginServerExit = servers.stopLoginServer();

	cases.run("K9", "reports (Q1, Q2, Q3), P3 (b) and R3", [&] {
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
		  << "Q1: the game server wrote no check output in " << servers.checkOutputDir()
		  << " (the summary file keeps the literal name m5a_summary.txt for every scenario run, CheckOutput.cpp:241)";

		// ---- P3 (b): the experience after the shutdown store ----
		EXPECT_EQ(database.queryLong(schema, "SELECT exp FROM players WHERE id = " + std::to_string(a.playerId)).value_or(-1), expAfterQuit)
		  << "P3 (b): players.exp changed over the death; at level 1 the getLevel() > 4 guard of calculateExpLoss must hold";
		EXPECT_EQ(database.queryLong(schema, "SELECT ap FROM abyss_rank WHERE player_id = " + std::to_string(a.playerId)).value_or(-1), 0)
		  << "R2: abyss_rank.ap is not 0 after the shutdown store; it was " << apBeforeQuit << " at the K7b post-quit read";

		// ---- Q1: the M5a Q8 bar ----
		EXPECT_TRUE(servers.readReportLines("unported_trace.txt").empty()) << "Q1: AION_UNPORTED sites were reached:\n"
		                                                                   << join(servers.readReportLines("unported_trace.txt"), "\n");

		// the two-sided allow-list of §6.1: every site is on the list, every §A row was hit at least once, every §B row exactly zero times
		const std::vector<AllowlistEntry> allowlist = readAllowlist();
		ASSERT_FALSE(allowlist.empty()) << "Q1: tests/scenario/m5b_partial_allowlist.txt is empty or missing";
		const std::vector<PartialHit> partials = readPartialHits(servers);
		std::map<std::string, int64_t> hitsByEntry;
		for (const AllowlistEntry& entry : allowlist)
			hitsByEntry[entry.site] = 0;
		for (const PartialHit& hit : partials) {
			bool allowed = false;
			for (const AllowlistEntry& entry : allowlist)
				if (allowlistEntryMatches(entry.site, hit.site)) {
					allowed = true;
					hitsByEntry[entry.site] += hit.hits;
				}
			EXPECT_TRUE(allowed) << "Q1: the AION_PARTIAL site " << hit.site << " is not in tests/scenario/m5b_partial_allowlist.txt ("
			                     << hit.line << ")";
		}
		for (const AllowlistEntry& entry : allowlist) {
			if (entry.section == AllowlistSection::HitAtLeastOnce)
				EXPECT_GT(hitsByEntry[entry.site], 0)
				  << "Q1: the section A row " << entry.site
				  << " was never hit. Such a row stops being reached either because a port closed the partial (move it off the list) or because "
				     "the scripted path stopped covering it, which is the more interesting case";
			else if (entry.section == AllowlistSection::HitNever)
				EXPECT_EQ(hitsByEntry[entry.site], 0)
				  << "Q1: the section B row " << entry.site << " was hit " << hitsByEntry[entry.site]
				  << " times, and it exists so that the world does not throw, not so that the scripted path reaches it";
		}
		// R3: registerDrop exactly once per kill, and the gate's scripted path makes exactly one kill. NpcController::doReward reaches it only
		// inside `if (attacker.equals(winner) && ask(REWARD_LOOT))` and only for an `attacker instanceof Player`, so the npcs that fight each
		// other in the neighbourhood cannot add to this count (§6.3 R3 and the rev-2 note after it).
		const std::string registerDropSite = "aion/gameserver/services/drop/DropRegistrationService.cpp:43";
		ASSERT_TRUE(hitsByEntry.contains(registerDropSite))
		  << "R3: tests/scenario/m5b_partial_allowlist.txt has no row for " << registerDropSite
		  << "; the AION_PARTIAL of D5 moved, and both the list and this assertion have to move with it";
		EXPECT_EQ(hitsByEntry[registerDropSite], monsterDied ? 1 : 0)
		  << "R3: DropRegistrationService::registerDrop was hit " << hitsByEntry[registerDropSite] << " times for "
		  << (monsterDied ? 1 : 0) << " kill(s)";
		std::cout << "Q1: AION_PARTIAL hits by allow-list row (hits, section, site):\n";
		for (const AllowlistEntry& entry : allowlist)
			std::cout << "  " << hitsByEntry[entry.site] << "\t" << sectionName(entry.section) << "\t" << entry.site << "\n";
		std::cout << std::flush;

		// the census, the lock order validator and the watchdog
		const std::vector<std::string> census = servers.readReportLines("census.txt");
		EXPECT_TRUE(census.empty()) << "Q1: the final census reports leaks:\n" << join(census, "\n");
		EXPECT_TRUE(servers.readReportLines("lockdep.txt").empty()) << "Q1: the lock order validator reported:\n"
		                                                            << join(servers.readReportLines("lockdep.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("watchdog.txt").empty()) << "Q1: the watchdog dumped:\n"
		                                                             << join(servers.readReportLines("watchdog.txt"), "\n");

		const std::map<std::string, std::vector<std::string>> summary = servers.readSummary();
		const auto value = [&](std::string_view key) -> std::string {
			const auto found = summary.find(std::string(key));
			return found == summary.end() || found->second.empty() ? std::string() : found->second[0];
		};
		EXPECT_EQ(value("started"), "true") << "Q1: the game server never started";
		EXPECT_EQ(value("exitCode"), "0") << "Q1: the game server reported exit code " << value("exitCode");
		EXPECT_EQ(value("knownListNotifyFailures"), "0") << "Q1: KnownList swallowed notification exceptions: " << value("knownListNotifyFailures");
		// §10.2: the live-instance counters are compiled out of a release build, where "liveLeaks 0" means "not measured"
		EXPECT_EQ(value("liveCountsEnabled"), "true")
		  << "Q1: the game server reports liveCountsEnabled " << value("liveCountsEnabled")
		  << ", so its live-count rows measured nothing: build it checked";
		const auto notPorted = summary.find("notPortedClientPacket");
		if (notPorted != summary.end())
			ADD_FAILURE() << "Q1: the scripted path sent client packets that are not ported: " << join(notPorted->second);

		// m5b-client-session.md S-2: "1 objects removed from the world are still alive" and "1 objects still tracked" at the end of a plain
		// five-kill session, with no census to name it. THIS IS THE ROW THAT MUST CATCH IT. census.txt above names the class of anything that
		// left the world before runFinalCensus ran; censusTracked is the count RuntimeLifecycle::shutdown reports afterwards, which covers the
		// objects that leave the world during the shutdown itself and therefore appear in NO report file (CheckOutput.h). Both have to be 0:
		// a non-zero census.txt says which class leaked, a non-zero censusTracked says only how many, and the gate must fail on either.
		//
		// Two sources, because neither is sufficient alone and the row must not be able to pass vacuously:
		//   (1) m5a_summary.txt's `censusTracked`, which E-03 added for exactly this - but it is an optional and the shutdown path does not
		//       supply it yet, so today it reads "unknown". A gate that only checked this row would pass on the very session it was written
		//       for. It is asserted when it carries a number and reported as a wiring gap when it does not.
		//   (2) the two log lines the user actually saw. RuntimeLifecycle.cpp:119 logs the WARN "Runtime shutdown: N objects removed from the
		//       world are still alive" when the count is non-zero, and ShutdownHook.cpp:303 logs "Runtime shut down: ... N objects still
		//       tracked" on every run. The second is always there, so it is the source that cannot go missing.
		// The gate fails when either source says non-zero, and it fails when NEITHER source is available.
		const std::string censusTracked = value("censusTracked");
		std::optional<int64_t> trackedFromLog;
		ASSERT_NE(servers.gameServer(), nullptr);
		for (const std::string& line : servers.gameServer()->findLogLines("objects still tracked", 5)) {
			const size_t tracked = line.rfind(", ");
			if (tracked == std::string::npos)
				continue;
			try {
				trackedFromLog = std::stoll(line.substr(tracked + 2));
			} catch (const std::exception&) {
				// the line's shape changed; the assertion below then fails for want of a source, which is the right answer
			}
		}
		const std::vector<std::string> stillAlive = servers.gameServer()->findLogLines("objects removed from the world are still alive", 5);
		EXPECT_TRUE(stillAlive.empty()) << "Q1: m5b-client-session.md S-2, reproduced: " << join(stillAlive, "\n")
		                                << "\n  census.txt has " << census.size() << " row(s)"
		                                << (census.empty() ? " - so the object left the world during the shutdown itself, after runFinalCensus "
		                                                     "ran, and no report file names it"
		                                                   : ":\n  " + join(census, "\n  "));
		if (censusTracked != "unknown") {
			EXPECT_EQ(censusTracked, "0") << "Q1: m5a_summary.txt reports censusTracked " << censusTracked
			                              << " (m5b-client-session.md S-2); census.txt has " << census.size() << " row(s)";
		} else {
			ASSERT_TRUE(trackedFromLog)
			  << "Q1: m5a_summary.txt says `censusTracked unknown` AND the game server log has no \"objects still tracked\" line, so nothing "
			     "measured the S-2 number this run. One of the two has to work: CheckOutput::Summary::censusTracked is filled by the caller that "
			     "performs the shutdown (CheckOutput.h), and no caller fills it yet - a request to the E-03 lane";
			std::cout << "Q1: m5a_summary.txt says `censusTracked unknown` - nobody fills CheckOutput::Summary::censusTracked yet (E-03 added "
			             "the field and the writer, not the wiring). The ShutdownHook log line is the source this run used instead."
			          << std::endl;
		}
		if (trackedFromLog)
			EXPECT_EQ(*trackedFromLog, 0)
			  << "Q1: the ShutdownHook reported " << *trackedFromLog
			  << " object(s) still tracked after the runtime shut down - this is m5b-client-session.md S-2, \"1 objects still tracked\", and the "
			     "number is what the user saw. census.txt has "
			  << census.size() << " row(s)"
			  << (census.empty() ? "; an empty census with a non-zero count means the object left the world during the shutdown itself, after "
			                       "runFinalCensus ran, and only a census with a later trigger point can name its class"
			                     : ":\n  " + join(census, "\n  "));

		// no ERROR line in either log (read line by line: a failing run's log can be hundreds of megabytes). This is what turns
		// NpcController::onDie's `catch (const std::exception&)` into a failure - every one of the four calls it wraps is new M5b code, and
		// m5b-client-session.md S-1 was exactly such a swallowed exception, one per kill.
		std::vector<std::string> errors;
		ASSERT_NE(servers.gameServer(), nullptr);
		for (const std::string& line : servers.gameServer()->findLogLines(" ERROR "))
			errors.push_back("game server: " + line);
		if (servers.loginServer() != nullptr)
			for (const std::string& line : servers.loginServer()->findLogLines(" ERROR "))
				errors.push_back("login server: " + line);
		EXPECT_TRUE(errors.empty()) << "Q1: ERROR lines in the server logs:\n" << join(errors, "\n");
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
			EXPECT_TRUE(fileErrors.empty()) << "Q1: " << errorLog << " is not empty:\n" << join(fileErrors, "\n");
		} else {
			ADD_FAILURE() << "Q1: the game server wrote no " << errorLog << " (is --log-folder still passed?)";
		}
		EXPECT_TRUE(servers.gameServer()->findLogLines("did not leave world cleanly", 5).empty())
		  << "Q1: objects did not leave the world cleanly:\n" << join(servers.gameServer()->findLogLines("did not leave world cleanly", 5), "\n");
		EXPECT_TRUE(servers.gameServer()->findLogLines("stale pin", 5).empty()) << "Q1: stale pins:\n"
		                                                                        << join(servers.gameServer()->findLogLines("stale pin", 5), "\n");
		EXPECT_EQ(value("zombieCuts"), "0") << "Q1: the zombie breaker cut references";

		// ---- Q2: the combat classes ----
		// The plan's rev-2 Q2 asked for 0 live AggroInfo, AttackResult, DamageList, DropNpc, KnownObject, Player and AbyssRank with created > 0
		// for the first four. E-03 measured all of them while porting CheckOutput and three of the seven do not have that shape; §6 is
		// corrected with this code and the reasons are in CheckOutput.h:
		//  - AggroInfo is BOUNDED, not zero: an AggroList is an OwnedPart of a Creature, so an AggroInfo lives exactly as long as the creature
		//    that hates, the shutdown does not despawn the world, and the npcs of the world fight each other since A-06.
		//  - KnownObject is bounded for the same reason (a walking npc builds a known list of the npcs around it).
		//  - DamageList is not RefCounted at all - AggroList::getFinalDamageList returns it by value - so no counter exists and a row for it
		//    would be silently dead in every build forever. Its arithmetic is B-08's unit assertion.
		//  - DropNpc's only constructor call site is behind registerDrop's AION_PARTIAL, so created stays 0 until M5b-3; the row is asserted as
		//    0/0 rather than dropped, because that is what makes it fail the day the partial goes away without this gate being updated.
		// The numbers come from m5a_summary.txt's `liveCount <class> <live> <created>` rows and not from live_counts.txt, which is the file the
		// M5a gate reads. That is E-03's contract and the difference matters: live_counts.txt has a row only for a class that was ever created,
		// so a DropNpc that is never constructed has NO row there and a gate reading it would silently assert nothing about the one class whose
		// whole point is to stay at zero. summaryLiveClasses() reports every requested name, and "a name no counter matches reports 0 0".
		const auto summaryLiveCount = [&](std::string_view unqualified) -> std::optional<LiveCount> {
			const auto rows = summary.find("liveCount");
			if (rows == summary.end())
				return std::nullopt;
			for (const std::string& row : rows->second) {
				std::istringstream in(row);
				std::string qualified;
				LiveCount count;
				if (!(in >> qualified >> count.live >> count.created))
					continue;
				const size_t colons = qualified.rfind("::");
				if ((colons == std::string::npos ? qualified : qualified.substr(colons + 2)) != unqualified)
					continue;
				count.line = row;
				return count;
			}
			return std::nullopt;
		};
		const std::vector<std::pair<std::string, LiveCount>> finalCounts = readLiveCounts(servers, "live_counts.txt");
		const auto liveCountOf = [&](std::string_view name) { return summaryLiveCount(name); };
		const std::optional<LiveCount> attackResult = liveCountOf("AttackResult");
		ASSERT_TRUE(attackResult) << "Q2: m5a_summary.txt has no `liveCount ...AttackResult` row (CheckOutput::summaryLiveClasses, E-03)";
		EXPECT_EQ(attackResult->live, 0)
		  << "Q2: " << attackResult->line
		  << " - AttackUtil makes one AttackResult per hit and nothing keeps one once the hit was applied; this is the row that catches §8 risk "
		     "2, the DelayedOnAttack that pins both creatures";
		EXPECT_GT(attackResult->created, 0)
		  << "Q2: " << attackResult->line << " - the created half is what makes this an assertion instead of a guard: a fight was fought";
		const std::optional<LiveCount> aggroInfo = liveCountOf("AggroInfo");
		ASSERT_TRUE(aggroInfo) << "Q2: m5a_summary.txt has no `liveCount ...AggroInfo` row";
		EXPECT_GT(aggroInfo->created, 0) << "Q2: " << aggroInfo->line << " - no AggroInfo was ever created, so nothing ever hated anything";
		const std::optional<LiveCount> dropNpc = liveCountOf("DropNpc");
		ASSERT_TRUE(dropNpc) << "Q2: m5a_summary.txt has no `liveCount ...DropNpc` row";
		EXPECT_EQ(dropNpc->live, 0) << "Q2: " << dropNpc->line;
		EXPECT_EQ(dropNpc->created, 0)
		  << "Q2: " << dropNpc->line
		  << " - DropNpc's only constructor call site is DropRegistrationService::initDropNpc, which registerDrop's whole-body AION_PARTIAL "
		     "(D5) never reaches. A non-zero created here means the drop path came back and this gate has to grow with it";
		// Player and AbyssRank are zeroLiveClasses() rows, so a leftover is already an ERROR line and a `liveLeak` row; live_counts.txt is where
		// their numbers are, and the gate reads them there because summaryLiveClasses() does not carry them.
		std::map<std::string, LiveCount> byName;
		for (const auto& [name, count] : finalCounts)
			byName[name] = count;
		for (const std::string& name : {"Player", "AbyssRank"}) {
			const auto found = byName.find(name);
			if (found != byName.end()) {
				EXPECT_EQ(found->second.live, 0) << "Q2: live instances left: " << found->second.line;
				EXPECT_GT(found->second.created, 0) << "Q2: " << found->second.line << " - no " << name << " was ever created";
			}
		}
		std::cout << "Q2: AggroInfo " << (aggroInfo ? aggroInfo->line : "(no row)") << "; AttackResult "
		          << (attackResult ? attackResult->line : "(no row)") << "; DropNpc " << (dropNpc ? dropNpc->line : "(no row)") << std::endl;

		// ---- Q3: no npc corpse is retained ----
		//
		// **Rev 3 (the gate lane, measured): rev 2's equality `live == baseline` is not an invariant of this server and had to go.** It assumed
		// the gate's own kill is the only npc death of the run. It is not: since A-06 the npcs of the character's active map region fight and
		// kill each other (AsyncAllowed.h documents the same finding for the M5a gate), and a killed npc is removed from the world at decay and
		// comes back only after its spawn's respawn time. So at the arbitrary instant the stop file is written, `live` is
		// `baseline - (npcs that have died and not yet respawned)`, which is BELOW the baseline, not equal to it. This run measured
		// `82127 -> 82125` with 18 respawns, and rev 2's row would have failed a correct server.
		//
		// What survives, and what it still catches: a corpse that is never reclaimed is an Npc that is alive and NOT in the world while its
		// replacement is, so it pushes `live` ABOVE the baseline once the respawns have caught up. `live <= baseline` is therefore the
		// directional half that a retained corpse breaks, and `created > baseline` is what proves RespawnService ran at all. The sharp,
		// class-naming leak check is Q1's census, and R4 is what proves the gate's OWN monster came back as a new object at the same spot.
		try {
			std::optional<LiveCount> baselineNpc;
			for (const auto& [name, count] : readLiveCounts(servers, "live_counts_baseline.txt"))
				if (name == "Npc")
					baselineNpc = count;
			const std::optional<LiveCount> finalNpc = liveCountOf("Npc");
			ASSERT_TRUE(baselineNpc) << "Q3: live_counts_baseline.txt has no Npc row";
			ASSERT_TRUE(finalNpc) << "Q3: m5a_summary.txt has no `liveCount ...Npc` row";
			EXPECT_LE(finalNpc->live, baselineNpc->live)
			  << "Q3: the world held " << baselineNpc->live << " live Npc after the spawns and " << finalNpc->live
			  << " at the end. More Npc alive than the world was spawned with means a killed npc was replaced by its respawn and never "
			     "reclaimed - the one leak a fight can introduce that M5a could not";
			EXPECT_GT(finalNpc->created, baselineNpc->created)
			  << "Q3: no Npc was created after the baseline, so nothing respawned in the whole run - including the monster R4 watched come back";
			std::cout << "Q3: Npc live " << baselineNpc->live << " -> " << finalNpc->live << " (" << (baselineNpc->live - finalNpc->live)
			          << " died and had not respawned when the stop file was written), created " << baselineNpc->created << " -> "
			          << finalNpc->created << " (+" << (finalNpc->created - baselineNpc->created) << " respawns, of which 1 is the gate's kill)"
			          << std::endl;
		} catch (const std::exception& exception) {
			ADD_FAILURE() << "Q3: no live_counts_baseline.txt to compare against (" << exception.what() << ")";
		}

		// The account-level classes Java's own shutdown keeps alive once per client that is still connected when the stop file is written
		// (AionConnection.java:239-243 returns from onDisconnect before LoginServer.onDisconnect, and LoginServer.java:119 is the only place
		// that unregisters the connection). The gate leaves exactly `connectionsAtShutdown` open and knows it, so the bound is checkable.
		const std::set<std::string> perConnection = {"Account",          "AccountTime",      "PlayerAccountData",
		                                            "PlayerCommonData", "PlayerAppearance", "ConnectionAliveChecker"};
		for (const auto& [name, count] : finalCounts) {
			if (perConnection.contains(name) || name.ends_with("Storage"))
				EXPECT_LE(count.live, connectionsAtShutdown)
				  << "Q2: live instances left: " << count.line << "\n  Java keeps at most one of these per client that is still connected when "
				  << "the stop file is written, and " << connectionsAtShutdown << " connection(s) were open; anything above that is a leak";
		}
	});

	finishRun(servers, outputDir, variant.testName);
}

} // namespace

// ---- the gates ---------------------------------------------------------------------------------------------------------------------------

/** `gs.scenario.m5b` (G-03): the scripted fight of §6.2 with `gameserver.geodata.enable=false` */
TEST(M5bScenario, Run) {
	runM5bGate({false, "gs.scenario.m5b", "m5b", "m5b", "m5ba"});
}

/**
 * `gs.scenario.m5b_geo` (G-06): the same scripted fight with `gameserver.geodata.enable=true`, its own output directory, its own schema pair
 * and its own CTest entry, sharing the RESOURCE_LOCK so that the two never run at the same time.
 *
 * **What it buys, and it is one thing, measured rather than hoped for:** R4's respawn position is asserted to +-0.01 m *in a world that has
 * its terrain*, which is what a geo z-snap added to the respawn path breaks - the same argument wave B made for `gs.scenario.m5a_geo`, and the
 * one row of the §6.6 mutation table that says "gs.scenario.m5b_geo only". Everything else the fight asserts runs again against a server that
 * loaded 151 .geo files, which is the F-1 class of bug (m5a-client-session.md): an exception, an ERROR, an AION_UNPORTED site or a hang that
 * only a geo-built world reaches, on the path a player actually walks.
 *
 * **What it does NOT do, and the plan should not be read as if it did.** §6.4 asks for three assertions this file cannot make today:
 *   - GEO-A1, an attack from behind a rock answered by `SM_ATTACK_RESPONSE.STOP_OBSTACLE_IN_THE_WAY`;
 *   - GEO-A2, `SimpleAttackManager`'s 15-second giveup arm, reachable only while `canSee` is false and the target is in attack range;
 *   - GEO-A3, a character within 8 m behind geometry that is not aggroed.
 * All three need one thing that does not exist: a *position the oracle picks from the geo mesh*. `tools/oracle` has a `geo` package, but it
 * serves the M4 static-data check and has no line-of-sight probe, and G-01's brief (the `m5b-monster` command) does not include one. Guessing a
 * position and asserting the obstacle would be a gate that passes when the geometry happens to be in the way and fails when the mesh changes;
 * probing a list of candidates and reporting whichever answered would be a row that cannot fail, which is the shape §6.6 exists to reject.
 * The honest answer is to record the gap: closing it is one oracle command - "give me a point at attack range from (x, y, z) whose segment to
 * it crosses the mesh" - written against the same `.geo` reader `tools/oracle/geo` already has, and then three cases here.
 */
TEST(M5bScenarioGeo, Run) {
	runM5bGate({true, "gs.scenario.m5b_geo", "m5b_geo", "m5bgeo", "m5bg"});
}

} // namespace aion::gameserver::scenario
