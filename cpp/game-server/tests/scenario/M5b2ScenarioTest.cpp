// The M5b-2 scenario gate (m5b2-plan.md G-03 and G-04, §10): one login server and one game server as child processes on their own test
// schemas, one account with an Elyos Warrior and an Elyos Mage (D4), and the level-1 abilities of §2.4 cast against two Poeta monsters: the
// Warrior's instant chain skill with its cooldown, an npc's skill cast at the Warrior (X9), the Mage's cast bar with its MP cost and its
// interruption, a 20-second debuff on a monster,
// a two-template self-buff and a heal (both seeded into player_skills, D3), the enter-world passives, the revive debuff Soul Sickness (D5) and
// the saved-effect and saved-cooldown round trips of a quit - then the reports the server writes at shutdown, with the G-07 relations of the
// skill classes.
//
// Every expectation is independent of the C++ server code, exactly as the M5a and M5b gates are: server packets are read with the decoders of
// tests/scenario/decoders (SkillDecoders.h, CombatDecoders.h and PacketDecoders.h, written from the Java writeImpl methods, m5a-plan.md D9),
// and every number comes from `tools/oracle/oracle.py` - m5b2-skills (the template constants of D8), m5a-creation (the base stats and the
// passive stat model of X1) and m5b-monster (the monsters, their spots, and npc 210133 - kept away from the melee cases, fought in S5b) - or
// from the Java arithmetic of
// the method the assertion is about, restated here in float exactly as Java evaluates it and cited at the line.
//
// **This file deliberately does not share M5bScenarioTest.cpp's helpers**, for the reason that file gives about M5aScenarioTest.cpp: both gates
// own one pair of server processes, their helpers live in anonymous namespaces, and lifting them into a shared header is a follow-up. What is
// duplicated is scaffolding (the case log, the burst collector, the login conversation, the report readers), never an assertion.
//
// It holds TWO gates: M5b2Scenario.Run (gs.scenario.m5b2, geo off) and M5b2ScenarioGeo.Run (gs.scenario.m5b2_geo, geo on), the same script
// through one shared body. The comment above TEST(M5b2ScenarioGeo, Run) says what the geo run does and does not add (G-04).
//
// **Npc casting (m5b2-plan.md N-01/N-02).** Npcs choose skill attacks since stage 2 (SkillAttackManager, GeneralNpcAI::chooseSkillAttack),
// and the gate keeps the two kinds of fight apart:
//  - the melee and player-skill cases fight npc 210663, whose npc_skills list is EMPTY (asserted in S0 from the oracle), so those fights are
//    melee whatever the AI can do, at two spots that are at least NPC_SKILL_KEEP_AWAY from every spot of npc 210133 (the level-1 striped kerub
//    with 16419 Brandish, 76.8 m from the Elyos spawn) and of npc 210134, also asserted in S0;
//  - S5b (X9, §10.2 C11) fights npc 210133 on purpose, with the Warrior, between its own cases and the Mage's: the kerub is pulled with one
//    swing and then left to fight until it has cast 16419 at the Warrior and the cast has landed;
//  - every skill packet the gate reads is matched on the CASTER's object id (castAndWait and the recordings below), so an npc casting at another
//    npc in sight changes nothing it asserts.
// The allow-list rows of the two npc partials (GeneralNpcAI.cpp:122 and the four SkillAttackManager.cpp rows) were deleted by the join: the
// sites no longer exist.

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
#include "decoders/SkillDecoders.h"

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

/** PlayerClass ids */
constexpr int32_t CLASS_WARRIOR = 0;
constexpr int32_t CLASS_MAGE = 6;

/** the Elyos start map and the monster of m5b-plan.md D11, which m5b2-plan.md §10.1 keeps for the melee cases */
constexpr int32_t ELYOS_START_MAP = 210010000;
constexpr int32_t GATE_MONSTER_NPC_ID = 210663;
/** §10.1's npc-skill target and its level-2 sibling: the two Poeta npcs with an npc_skills list nearest the Elyos spawn (§2.4 (b)) */
constexpr int32_t NPC_SKILL_NPC_IDS[] = {210133, 210134};
/** X9's npc skill: npc 210133's one npc_skills entry (npc_skills.xml:2816-2818, prob 25, target MOST_HATED by NpcSkillTemplate's default) */
constexpr uint16_t BRANDISH = 16419;
/**
 * X9's budget, in the kerub's attack decisions that roll the entry's chance: each such decision asks NpcSkillTemplateEntry.isReady, which
 * passes on `Rnd.chance() < prob` (NpcSkillTemplateEntry.java:41-55), so 33 of them without a cast happen with probability 0.75^33 = 7.5e-5
 * (§10.3 X9). A decision rolls only once the initial skill delay has passed - `Rnd.get(attackSpeed, 3 * attackSpeed)` after the fight started,
 * drawn again at every call (NpcGameStats.java:226-228, SkillAttackManager.java:129) - and, after a cast, once the next skill delay has passed
 * (`next_skill_time` -1: `Rnd.get(3000, 9000)`, NpcGameStats.java:186-191, Skill.java:302-308).
 */
constexpr int32_t X9_DECISIONS = 33;
constexpr std::chrono::milliseconds X9_NEXT_SKILL_DELAY_MAX = 9000ms;
/**
 * The distance every spot the characters stand at keeps from every spot of NPC_SKILL_NPC_IDS. It is not a range the npc can cast from - 16419
 * has first_target_range 2 - but the distance past which a fight of this gate cannot pull one of them into it: the kerubs' srange is 7 m, a
 * monster that chases the Mage runs at most a few metres, and 30 m is outside both plus the two bound radii.
 */
constexpr double NPC_SKILL_KEEP_AWAY = 30.0;

/** the level-1 abilities of §2.4, cast by the gate; every constant of them comes from the oracle, only the ids are named here */
constexpr uint16_t FEROCIOUS_STRIKE = 2864; // Warrior, instant, chain
constexpr uint16_t FLAME_BOLT = 1282;       // Mage, a cast bar and an MP cost
constexpr uint16_t ROOT = 1328;             // Mage, a DEBUFF with a lifetime and a cooldown
constexpr uint16_t HEALING_LIGHT = 1838;    // Priest's heal, seeded into the Mage's player_skills (D3)
constexpr uint16_t FOCUSED_EVASION = 3195;  // Scout's two-template self-buff, seeded likewise
constexpr int32_t SOUL_SICKNESS = 8291;     // PlayerController.updateSoulSickness's default skill (PlayerController.java:730-731)

/** the melee distance of m5b-plan.md K4b: inside the Warrior's attack range and inside 2864's first target range (1 m + weapon range) */
constexpr double MELEE_DISTANCE = 2.0;
/**
 * The Mage's casting distance: inside 1282's and 1328's first_target_range (25 m, asserted in S0), outside the monster's aggro range plus the
 * two bound radii (asserted too) and far enough that a monster which starts to chase needs a noticeable walk - which is what makes X6's
 * "no SM_MOVE while rooted" a statement about the root and not about a monster that was already in melee range.
 */
constexpr double CAST_DISTANCE = 10.0;
/** the minimum distance between the two monster spots the gate uses, so the Root silence of S12 is not disturbed by S8-S11's fight */
constexpr double SECOND_SPOT_MIN_DISTANCE = 20.0;

/** SM_SYSTEM_MESSAGE ids (SM_SYSTEM_MESSAGE.java) */
constexpr int32_t STR_SKILL_NOT_READY = 1300021;
constexpr int32_t STR_SKILL_CANCELED = 1300023;
constexpr int32_t STR_SKILL_OBSTACLE = 1300030;

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

int64_t millisBetween(std::chrono::steady_clock::time_point earlier, std::chrono::steady_clock::time_point later) {
	return std::chrono::duration_cast<std::chrono::milliseconds>(later - earlier).count();
}

/**
 * Java Math.round(float): half up on the exact float value (the JDK 7+ implementation, not floor(f + 0.5f)). A float plus 0.5 is exact in
 * double, so floor of that double is the same rounding - which is what CreatureGameStats.checkMaxHPChanged applies to `currentHp * percent`.
 */
int32_t javaRound(float value) {
	return static_cast<int32_t>(std::floor(static_cast<double>(value) + 0.5));
}

/**
 * Stat2.getCurrent() of a MAXHP or MAXMP whose base is `base` (baseRate 1) and whose bonus already holds `itemBonus` (the oracle's bonus
 * modifiers of the equipment) after one more bonus StatRateFunction of `percent`: StatRateFunction.apply adds `baseValue * getValue() / 100f`
 * to the bonus (int * int, then a float division, StatRateFunction.java:22-29), and getCurrent is `(int) ((base * baseRate + bonus * bonusRate
 * + base * fixedBonusRate) * finalRate)` (Stat2.java:64-69), all in float. The rate bonus is applied first: CreatureGameStats sorts the
 * functions by priority, RATE bonus 50 before ADD bonus 60 (StatFunction.java:54). A <change func="PERCENT"> of a BufEffect is exactly such a
 * function (BufEffect.java:81-83), which is what the soul sickness registers.
 */
int32_t withPercentBonus(int32_t base, float itemBonus, int32_t percent, int32_t lowerCap) {
	const float bonus = static_cast<float>(base * percent) / 100.0f + itemBonus;
	const float current = (static_cast<float>(base) * 1.0f + bonus * 1.0f + static_cast<float>(base) * 0.0f) * 1.0f;
	// applyStatFunctions ends with StatCapUtil.calculateBaseValue (CreatureGameStats.java:142): below the lower cap the bonus is set to
	// `lowerCap - getExactCurrentWithoutBonus()`, which makes the current exactly the cap (StatCapUtil.java:104-115)
	return current < static_cast<float>(lowerCap) ? lowerCap : static_cast<int32_t>(current);
}

/** StatCapUtil.registerDefaults: a player's MAXHP is capped below at 100 and its MAXMP at 1, neither above (StatCapUtil.java:18-19) */
constexpr int32_t PLAYER_MAXHP_LOWER_CAP = 100;
constexpr int32_t PLAYER_MAXMP_LOWER_CAP = 1;

/**
 * CreatureGameStats.checkMaxHPChanged / checkMaxMPChanged (CreatureGameStats.java:370-390): when the current maximum moves from `oldMax` to
 * `newMax`, the current value becomes `Math.min(Math.round(current * (1f * newMax / oldMax)), newMax)` - int times float, in float.
 */
int32_t rescaledToNewMax(int32_t current, int32_t oldMax, int32_t newMax) {
	const float percent = 1.0f * static_cast<float>(newMax) / static_cast<float>(oldMax);
	return std::min(javaRound(static_cast<float>(current) * percent), newMax);
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

/** The object ids the server announced as npcs, for the npc half of the async-allowed set (m5b-plan.md D2), as in M5bScenarioTest.cpp */
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
 * A burst ends `quiet` after the last packet the §5.8 sequence still has to explain, i.e. after the last packet `async` does NOT allow
 * (M5bScenarioTest.cpp's collectBurst). Nothing is dropped - every packet inside the window is returned, the async ones included.
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
 * Reads and records everything that arrives within a FIXED window (M5bScenarioTest.cpp's collectFor). It is also how this gate waits: a
 * cooldown or an effect lifetime is spent reading the socket, never sleeping on it, so a monster fighting the character cannot fill a socket
 * nobody reads while the server is being measured.
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

/** collectFor until a point in time (nothing when it has passed) */
void drainUntil(GameSession& session, std::chrono::steady_clock::time_point until) {
	const auto now = std::chrono::steady_clock::now();
	if (until > now)
		collectFor(session, std::chrono::ceil<std::chrono::milliseconds>(until - now));
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
void expectSequence(const std::vector<Packet>& packets, std::string_view pattern, const AsyncAllowed& async) {
	const PacketSequence sequence = PacketSequence::parse(pattern);
	const std::vector<std::string> names = namesOf(packets);
	const PacketSequence::Result result = sequence.match(names, async.predicate(packets));
	EXPECT_TRUE(result.matched) << result.message << "\n  expected: " << sequence.toString() << "\n  got (" << names.size() << "): " << join(names);
}

/** The CM_ENTER_WORLD part of m5a-plan.md §5.8, as M5bScenarioTest.cpp replays it */
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

// ---- the skill recording ----------------------------------------------------------------------------------------------------------------

/**
 * One pass over a slice of the session's recording, decoded with the independent decoders. Every §10.3 assertion reads this structure instead
 * of the raw packets, so the decode happens once and a body that does not decode is a failure of the case that collected it.
 */
struct SkillRecording {
	template <typename T>
	struct At {
		size_t index = 0;
		T value;
		std::chrono::steady_clock::time_point at;
	};

	std::vector<At<decoders::CastSpell>> casts;
	std::vector<At<decoders::CastSpellResult>> results;
	std::vector<At<decoders::SkillCancel>> cancels;
	std::vector<At<decoders::AbnormalState>> abnormalStates;
	std::vector<At<decoders::AbnormalEffect>> abnormalEffects;
	std::vector<At<decoders::AttackStatusUpdate>> statuses;
	std::vector<At<decoders::StatUpdateMp>> mpUpdates;
	std::vector<At<decoders::StatUpdateHp>> hpUpdates;
	std::vector<At<decoders::StatsInfo>> statsInfos;
	std::vector<At<decoders::Emotion>> emotions;
	std::vector<At<int32_t>> moves;          // SM_MOVE: the mover's object id
	std::vector<At<int32_t>> systemMessages; // SM_SYSTEM_MESSAGE: the message id
	std::vector<At<decoders::SkillCooldown>> cooldowns;
	/** the decode failures, so an assertion never silently sees a shorter stream than the run produced */
	std::vector<std::string> decodeFailures;

	template <typename T, typename Predicate>
	static std::vector<At<T>> where(const std::vector<At<T>>& values, Predicate predicate) {
		std::vector<At<T>> result;
		for (const At<T>& value : values)
			if (predicate(value.value))
				result.push_back(value);
		return result;
	}

	std::vector<At<decoders::CastSpell>> castsBy(int32_t caster, uint16_t skillId) const {
		return where(casts, [&](const decoders::CastSpell& c) { return c.effectorObjectId == caster && c.spellId == skillId; });
	}
	std::vector<At<decoders::CastSpellResult>> resultsBy(int32_t caster, uint16_t skillId) const {
		return where(results, [&](const decoders::CastSpellResult& r) { return r.effectorObjectId == caster && r.skillId == skillId; });
	}
	std::vector<At<decoders::SkillCancel>> cancelsBy(int32_t caster, uint16_t skillId) const {
		return where(cancels, [&](const decoders::SkillCancel& c) { return c.creatureObjectId == caster && c.skillId == skillId; });
	}
	std::vector<At<decoders::AttackStatusUpdate>> statusesOf(int32_t creature) const {
		return where(statuses, [&](const decoders::AttackStatusUpdate& s) { return s.creatureObjectId == creature; });
	}
	std::vector<At<decoders::AbnormalEffect>> abnormalEffectsOf(int32_t creature) const {
		return where(abnormalEffects, [&](const decoders::AbnormalEffect& e) { return e.effectedObjectId == creature; });
	}
	std::vector<At<int32_t>> movesOf(int32_t creature) const {
		return where(moves, [&](int32_t mover) { return mover == creature; });
	}
	bool hasMessage(int32_t messageId) const {
		return !where(systemMessages, [&](int32_t id) { return id == messageId; }).empty();
	}
	bool diedAt(int32_t creature) const {
		return !where(emotions, [&](const decoders::Emotion& e) { return e.emotionType == decoders::EMOTION_DIE && e.senderObjectId == creature; })
		          .empty();
	}
};

/** Decodes the packets [from, to) of the session's recording into a SkillRecording */
SkillRecording recordSkills(const GameSession& session, size_t from, std::optional<size_t> to = std::nullopt) {
	SkillRecording recording;
	const std::vector<Packet>& packets = session.recorded();
	const size_t end = std::min(packets.size(), to.value_or(packets.size()));
	for (size_t i = from; i < end; i++) {
		const Packet& packet = packets[i];
		try {
			if (packet.name == "SM_CASTSPELL")
				recording.casts.push_back({i, decoders::decodeCastSpell(packet.data), packet.receivedAt});
			else if (packet.name == "SM_CASTSPELL_RESULT")
				recording.results.push_back({i, decoders::decodeCastSpellResult(packet.data), packet.receivedAt});
			else if (packet.name == "SM_SKILL_CANCEL")
				recording.cancels.push_back({i, decoders::decodeSkillCancel(packet.data), packet.receivedAt});
			else if (packet.name == "SM_ABNORMAL_STATE")
				recording.abnormalStates.push_back({i, decoders::decodeAbnormalState(packet.data), packet.receivedAt});
			else if (packet.name == "SM_ABNORMAL_EFFECT")
				recording.abnormalEffects.push_back({i, decoders::decodeAbnormalEffect(packet.data), packet.receivedAt});
			else if (packet.name == "SM_ATTACK_STATUS")
				recording.statuses.push_back({i, decoders::decodeAttackStatus(packet.data), packet.receivedAt});
			else if (packet.name == "SM_STATUPDATE_MP")
				recording.mpUpdates.push_back({i, decoders::decodeStatUpdateMp(packet.data), packet.receivedAt});
			else if (packet.name == "SM_STATUPDATE_HP")
				recording.hpUpdates.push_back({i, decoders::decodeStatUpdateHp(packet.data), packet.receivedAt});
			else if (packet.name == "SM_STATS_INFO")
				recording.statsInfos.push_back({i, decoders::decodeStatsInfo(packet.data), packet.receivedAt});
			else if (packet.name == "SM_EMOTION")
				recording.emotions.push_back({i, decoders::decodeEmotion(packet.data), packet.receivedAt});
			else if (packet.name == "SM_MOVE")
				recording.moves.push_back({i, decoders::decodeMoveObjectId(packet.data), packet.receivedAt});
			else if (packet.name == "SM_SYSTEM_MESSAGE")
				recording.systemMessages.push_back({i, decoders::decodeSystemMessageId(packet.data), packet.receivedAt});
			else if (packet.name == "SM_SKILL_COOLDOWN")
				recording.cooldowns.push_back({i, decoders::decodeSkillCooldown(packet.data), packet.receivedAt});
		} catch (const DecodeError& error) {
			recording.decodeFailures.push_back(packet.name + " at " + std::to_string(i) + ": " + error.what());
		}
	}
	return recording;
}

/** the entry of `skillId` in an abnormal packet's list, if any */
std::optional<decoders::AbnormalEntry> entryOf(const std::vector<decoders::AbnormalEntry>& entries, int32_t skillId) {
	for (const decoders::AbnormalEntry& entry : entries)
		if (entry.skillId == skillId)
			return entry;
	return std::nullopt;
}

/** Effect.getSuccessfulEffectsAsByte for an effect whose every template succeeded and that has no sub effect: sum of 1 << (position + 3) */
uint8_t successfulEffectsByte(const OracleSkillTemplate& skill) {
	int32_t value = 0;
	for (const OracleSkillEffect& effect : skill.effects)
		value += 1 << (effect.position + 3);
	return static_cast<uint8_t>(value);
}

/** the EffectResult ids that make Effect.getSuccessfulEffectsAsByte answer 0 or 1 and Skill.endCast count the target as resisted */
bool dodgedOrResisted(const decoders::CastResultEffect& effect) {
	return effect.effectResult == decoders::EFFECT_RESULT_DODGE || effect.effectResult == decoders::EFFECT_RESULT_RESIST;
}

// ---- the scenario client ---------------------------------------------------------------------------------------------------------------

struct ScenarioClient {
	std::string account;
	std::string password = "m5b2Password1";
	std::unique_ptr<FakeLoginClient> login;
	std::unique_ptr<GameSession> game;
	FakeLoginClient::SessionKey key;
	int32_t warriorId = 0;
	int32_t mageId = 0;
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

// ---- the check output reports (X12, X13) --------------------------------------------------------------------------------------------------

/** One section of m5b2_partial_allowlist.txt: §A hit at least once, §B hit exactly zero times, §C counted but not pinned */
enum class AllowlistSection { HitAtLeastOnce, HitNever, NotPinned };

struct AllowlistEntry {
	std::string site;
	AllowlistSection section = AllowlistSection::NotPinned;
};

/** Reads tests/scenario/m5b2_partial_allowlist.txt with its three sections ("# --- SECTION A/B/C" marker lines, as M5b's list) */
std::vector<AllowlistEntry> readAllowlist() {
	std::vector<AllowlistEntry> entries;
	std::ifstream in(AION_SCENARIO_M5B2_PARTIAL_ALLOWLIST, std::ios::binary);
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

/** "<live>\t<created>\t<qualified class name>" rows of live_counts.txt / live_counts_baseline.txt */
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

// ---- the gate ---------------------------------------------------------------------------------------------------------------------------

/** What separates gs.scenario.m5b2 from gs.scenario.m5b2_geo */
struct GateVariant {
	bool geodata = false;
	std::string testName;      // gs.scenario.m5b2
	std::string outputSubdir;  // m5b2
	std::string schemaPrefix;  // m5b2
	std::string accountPrefix; // m5b2a
};

void runM5b2Gate(const GateVariant& variant) {
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
	// The M5b-1 profile minus gameserver.soulsickness.disable (D5): the revive of S15 takes Java's real path and its debuff is X10. That path
	// includes revive's isNoResurrectPenalty guard (PlayerReviveService.java:194) since the gate lane's fix pass; no effect of this script is a
	// NoResurrectPenaltyEffect, so the gate sees it answer false, and effects_mz OtherEffectsTest covers the true side. Geodata is the one key that
	// separates the two variants; npcshouts off keeps NpcShoutsService off the fight path, as in the M5b gate.
	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.schemaPrefix = variant.schemaPrefix;
	config.gameServerProperties["gameserver.geodata.enable"] = variant.geodata ? "true" : "false";
	config.gameServerProperties["gameserver.npcshouts.enable"] = "false";
	// D5 is "the key leaves the profile", but leaving it out is not enough: the game server also reads the Java tree's config/mygs.properties,
	// the user's local play profile (untracked), and the M5b-1 one of 2026-09-22 carries `gameserver.soulsickness.disable = 0`. The first run of
	// this gate measured exactly that - no soul sickness after the revive. The gate therefore states MembershipConfig's own @Property default,
	// 10 (MembershipConfig.java:37-38), under which Player.hasPermission exempts no scenario account and updateSoulSickness casts 8291.
	config.gameServerProperties["gameserver.soulsickness.disable"] = "10";
	// m5b3-plan.md D4 (G-05), m5b2.properties.example's M5b-3 block: no drop rule fires at a drop rate of 0 (Rates.get(killer, DROP_RATES)
	// multiplies every rule's chance: DropRegistrationService.java:218, DropModifiers.java:53-57, DropGroup.java:64), while registerDrop still
	// runs to its end for every kill of the characters. What this gate's kills would do with drops (§2.4 of that plan): monster A (210663)
	// drops on 58.2 % of kills and the kerub (210133) on 78.5 %, and a corpse with loot stands 300 s (RespawnService.WITH_DROP_DECAY) instead of
	// 2 s - A's corpse beside its respawn in S7 and S10, a live DropNpc and its items at the stop. At 0 every corpse decays after 2 s, and
	// X12's drop rows assert that registerDrop ran once per kill and made no drop item.
	config.gameServerProperties["gameserver.rates.drop"] = "0";
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

	ScenarioClient a;
	AnnouncedNpcs announcedNpcs;
	a.account = variant.accountPrefix + servers.gameSchema().substr(servers.gameSchema().size() - 8);
	const std::string warriorName = "Scenariowarrior";
	const std::string mageName = "Scenariomage";

	// ---- S0: the oracles answer, and the plan's premises are re-derived from them (m5b2-plan.md G-01, §12) ----
	OracleCreation warriorCreation;
	OracleCreation mageCreation;
	OracleSkills warriorSkills;
	OracleSkills mageSkills;
	OracleMonster monster;
	OracleMonster kerub;                      // npc 210133, X9's caster
	std::optional<OracleNpcSkill> kerubSkill; // its npc_skills entry of 16419
	std::optional<OracleMonsterSpot> spotA; // the Warrior's fight, then the Mage's cast bar, interruption and heal fight
	std::optional<OracleMonsterSpot> spotB; // the Root and the Mage's death
	runCase("S0", "the oracles answer and the plan's premises hold (G-01)", [&] {
		warriorCreation = oracle->creation("ELYOS", "WARRIOR");
		mageCreation = oracle->creation("ELYOS", "MAGE");
		ASSERT_EQ(warriorCreation.mapId, ELYOS_START_MAP);
		ASSERT_EQ(mageCreation.mapId, ELYOS_START_MAP);
		warriorSkills = oracle->skills("ELYOS", "WARRIOR");
		// the Mage's autolearn set, the two skills S1 seeds into player_skills (D3), the soul sickness at the first death, X9's npc skill, and
		// the npc_skills lists of the gate's own monster and of the npc-skill npcs (kept away from the melee cases, one of them fought in S5b)
		std::vector<int32_t> npcs{GATE_MONSTER_NPC_ID};
		npcs.insert(npcs.end(), std::begin(NPC_SKILL_NPC_IDS), std::end(NPC_SKILL_NPC_IDS));
		mageSkills = oracle->skills("ELYOS", "MAGE", 1,
		                            {std::to_string(HEALING_LIGHT), std::to_string(FOCUSED_EVASION), std::to_string(SOUL_SICKNESS) + ":1",
		                             std::to_string(BRANDISH) + ":1"},
		                            npcs, 1);

		// §2.4's table, re-derived: what each skill must be for its case to prove what §10.3 says it proves
		const OracleSkillTemplate& strike = warriorSkills.skill(FEROCIOUS_STRIKE);
		EXPECT_TRUE(std::ranges::count(strike.sources, "autolearn") == 1) << "2864 must be on a fresh Warrior's bar (no learn path, §2.4)";
		EXPECT_EQ(strike.castDuration, 0) << "X2 is the instant skill";
		EXPECT_GT(strike.cooldownMillis, 3000) << "S4's cooldown row needs a cooldown longer than the skill-use interval";
		EXPECT_TRUE(strike.chainCategory.has_value()) << "X3 is the chain byte";
		const OracleSkillTemplate& bolt = mageSkills.skill(FLAME_BOLT);
		EXPECT_TRUE(bolt.castDuration && *bolt.castDuration >= 1000) << "X4 and X5 need a cast bar to measure and to interrupt";
		EXPECT_TRUE(bolt.mpCost && *bolt.mpCost > 0) << "X4 is the MP cost";
		EXPECT_EQ(bolt.cooldownMillis, 0) << "S8-S10 cast 1282 several times in a row";
		const OracleSkillTemplate& root = mageSkills.skill(ROOT);
		EXPECT_EQ(root.castDuration, 0);
		ASSERT_TRUE(root.effectDuration && *root.effectDuration > 5000) << "X6 is a debuff with a lifetime";
		EXPECT_EQ(root.effectDurationRandomTime, 0) << "X6 asserts the lifetime exactly (D8), which a randomtime would make a range";
		ASSERT_TRUE(root.targetSlot);
		EXPECT_EQ(root.targetSlot->name, "DEBUFF");
		EXPECT_GT(root.cooldownMillis, 28000) << "S13 needs a cooldown that is still more than 28 s away when the Mage quits (PlayerCooldownsDAO)";
		ASSERT_EQ(root.effects.size(), 1u);
		EXPECT_EQ(root.effects[0].effectClass, "RootEffect");
		const OracleSkillTemplate& evasion = mageSkills.skill(FOCUSED_EVASION);
		ASSERT_EQ(evasion.effects.size(), 2u) << "X7 is a skill whose two templates become one Effect";
		ASSERT_TRUE(evasion.targetSlot);
		EXPECT_EQ(evasion.targetSlot->name, "BUFF");
		EXPECT_TRUE(evasion.effectDuration && *evasion.effectDuration > 0);
		const OracleSkillTemplate& heal = mageSkills.skill(HEALING_LIGHT);
		ASSERT_EQ(heal.effects.size(), 1u);
		EXPECT_EQ(heal.effects[0].effectClass, "HealInstantEffect");
		const OracleSkillTemplate& sickness = mageSkills.skill(SOUL_SICKNESS);
		// PROVOKED is SkillMethod.PROVOKED (Skill.java:127-135): no cast bar and no SM_CASTSPELL/SM_CASTSPELL_RESULT (Skill.java:665). Not the
		// isSkillPresent gate §10.3 named: updateSoulSickness calls SkillEngine.getSkill, which has none (SkillEngine.java:86-100, PlayerController
		// .java:732) - only getSkillFor does
		EXPECT_EQ(sickness.activation, "PROVOKED") << "X10: the soul sickness is a PROVOKED skill";
		ASSERT_TRUE(sickness.targetSlot);
		ASSERT_TRUE(sickness.effectDuration && *sickness.effectDuration > 28000)
		  << "S16 needs the soul sickness to outlast PlayerEffectsDAO's 28 s threshold";
		EXPECT_EQ(mageSkills.deathCount, 1);

		// the gate's monster owns no npc skill, so npc casting (N-01/N-02) cannot change its fights
		bool listed = false;
		for (const OracleNpcSkills& npc : mageSkills.npcs) {
			if (npc.npcId == GATE_MONSTER_NPC_ID) {
				listed = true;
				EXPECT_TRUE(npc.skills.empty()) << "npc " << GATE_MONSTER_NPC_ID << " has an npc_skills list; its fights would change with N-02";
			}
		}
		EXPECT_TRUE(listed) << "the oracle reported no npc_skills answer for npc " << GATE_MONSTER_NPC_ID;

		monster = oracle->monster(ELYOS_START_MAP, GATE_MONSTER_NPC_ID, 1);
		EXPECT_EQ(monster.tribe, "MONSTER") << "a MONSTER-tribe npc never starts a fight (m5b-plan.md A5a), so an untouched one stays idle";
		ASSERT_TRUE(monster.nearestPlainSpot) << "no spot of npc " << GATE_MONSTER_NPC_ID << " is fixed, spawned and free of a static id";

		// the npc-skill npcs are kept out of the melee and player-skill fights (see the file comment; S5b fights 210133 on purpose, 76.8 m from the
		// spawn and far from both spots): the two spots are the nearest plain spots of the
		// monster that are NPC_SKILL_KEEP_AWAY from every spot of npcs 210133 and 210134 - picked by that rule from the oracle's answers, so the
		// rule holds by construction and the assertion below is its record
		std::vector<std::pair<int32_t, OracleMonsterSpot>> kept;
		for (const int32_t npcId : NPC_SKILL_NPC_IDS) {
			const OracleMonster answer = oracle->monster(ELYOS_START_MAP, npcId, 1);
			if (npcId == NPC_SKILL_NPC_IDS[0])
				kerub = answer;
			for (const OracleMonsterSpot& spot : answer.spots)
				kept.emplace_back(npcId, spot);
		}
		ASSERT_FALSE(kept.empty()) << "the oracle knows no spot of the npc-skill npcs; §10.1's premise changed";
		const auto clear = [&](const OracleMonsterSpot& fight) {
			return std::ranges::all_of(kept, [&](const auto& npc) { return distance2d(npc.second.x, npc.second.y, fight.x, fight.y) >= NPC_SKILL_KEEP_AWAY; });
		};
		for (const OracleMonsterSpot& spot : monster.spots) { // nearest first
			if (!spot.fixed || !spot.spawned || spot.staticId != 0 || !clear(spot))
				continue;
			if (!spotA)
				spotA = spot;
			else if (distance2d(spot.x, spot.y, spotA->x, spotA->y) >= SECOND_SPOT_MIN_DISTANCE) {
				spotB = spot;
				break;
			}
		}
		ASSERT_TRUE(spotA && spotB) << "fewer than two plain spots of npc " << GATE_MONSTER_NPC_ID << " are " << NPC_SKILL_KEEP_AWAY
		                            << " m from the npc-skill npcs and " << SECOND_SPOT_MIN_DISTANCE << " m apart";
		for (const auto& [npcId, spot] : kept)
			for (const OracleMonsterSpot* fight : {&*spotA, &*spotB})
				EXPECT_GE(distance2d(spot.x, spot.y, fight->x, fight->y), NPC_SKILL_KEEP_AWAY)
				  << "npc " << npcId << " (an npc_skills npc) has a spot at (" << spot.x << ", " << spot.y << "), within " << NPC_SKILL_KEEP_AWAY
				  << " m of the gate's fight at (" << fight->x << ", " << fight->y << ")";
		EXPECT_LT(CAST_DISTANCE, 25.0);
		const float aggroReach = static_cast<float>(monster.aggroRange) + 2.0f * monster.boundRadius;
		EXPECT_GT(CAST_DISTANCE, aggroReach) << "the Mage casts from outside the monster's aggro range";
		for (const OracleNpcSkills& npc : mageSkills.npcs)
			if (npc.npcId == NPC_SKILL_NPC_IDS[0])
				for (const OracleNpcSkill& skill : npc.skills)
					if (skill.skillId == BRANDISH)
						kerubSkill = skill;
		EXPECT_TRUE(kerubSkill) << "§10.1's premise: npc 210133 owns 16419 Brandish";
		// X9's premises (S5b): the kerub has a plain spot to be pulled at, and - a MONSTER-tribe npc starting no fight (m5b-plan.md A5a) - it
		// must be pulled; its entry rolls a chance on every decision and casts with a cast bar, so the result follows the cast
		EXPECT_TRUE(kerub.nearestPlainSpot) << "X9: no spot of npc 210133 is fixed, spawned and free of a static id";
		EXPECT_EQ(kerub.tribe, "MONSTER") << "X9: S5b pulls the kerub with one swing, which assumes it does not start the fight itself";
		EXPECT_TRUE(kerubSkill && kerubSkill->prob > 0 && kerubSkill->prob < 100 && kerubSkill->castDuration > 0)
		  << "X9: 16419's entry is expected to roll a chance and to have a cast bar";

		// X1's oracle (m5a-creation statsInfo): the Warrior's model must answer; the Mage's spellbook is a magical main hand (0 and 0)
		EXPECT_TRUE(warriorCreation.mainHandPAttackBase && warriorCreation.mainHandPAttackCurrent)
		  << "the oracle does not model the Warrior's main hand attack: " << join(warriorCreation.statsInfoNotModelled);
		EXPECT_TRUE(mageCreation.mainHandPAttackBase && mageCreation.mainHandPAttackCurrent)
		  << "the oracle does not model the Mage's main hand attack: " << join(mageCreation.statsInfoNotModelled);
		std::cout << "S0: spot A (" << spotA->x << ", " << spotA->y << ", " << spotA->z << ") " << spotA->distance << " m, spot B (" << spotB->x << ", "
		          << spotB->y << ", " << spotB->z << ") " << spotB->distance << " m from the Elyos spawn; 2864 cooldown " << strike.cooldownMillis
		          << " ms, 1282 cast " << bolt.castDuration.value_or(-1) << " ms for " << bolt.mpCost.value_or(-1) << " MP, 1328 "
		          << root.effectDuration.value_or(-1) << " ms (cooldown " << root.cooldownMillis << " ms), 3195 "
		          << evasion.effectDuration.value_or(-1) << " ms, 8291 " << sickness.effectDuration.value_or(-1) << " ms; the Warrior's main hand "
		          << warriorCreation.mainHandPAttackBase.value_or(-1) << "/" << warriorCreation.mainHandPAttackCurrent.value_or(-1) << std::endl;
	});

	AsyncAllowed async = AsyncAllowed::m5aDefault();

	// ---- S1: one account, two characters (D4), and the seeded skills of D3 ----
	runCase("S1", "login, create an Elyos Warrior and an Elyos Mage, seed the Mage's skills (D3, D4)", [&] {
		const decoders::CharacterList list = logIn(servers, a, async);
		EXPECT_EQ(list.characterCount, 0) << "a fresh account must have no character";
		const auto create = [&](const std::string& name, int32_t classId) {
			NewCharacter character;
			character.name = name;
			character.asmodian = false;
			character.playerClassId = classId;
			a.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(a.key.accountId, a.account, character, 1));
			EXPECT_EQ(decoders::decodeCreateCharacter(expectNext(*a.game, "SM_CREATE_CHARACTER", async).data).responseCode, RESPONSE_OPEN_CREATION_WINDOW);
			a.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(a.key.accountId, a.account, character, 0));
			const decoders::CreateCharacter created = decoders::decodeCreateCharacter(expectNext(*a.game, "SM_CREATE_CHARACTER", async).data);
			if (created.responseCode != RESPONSE_OK || !created.player)
				throw std::runtime_error("creating " + name + " answered response code " + std::to_string(created.responseCode));
			EXPECT_EQ(created.player->classId, classId);
			EXPECT_EQ(created.player->level, 1);
			return created.player->playerId;
		};
		a.warriorId = create(warriorName, CLASS_WARRIOR);
		a.mageId = create(mageName, CLASS_MAGE);

		// D3: a player_skills row is what SkillEngine.getSkillFor's isSkillPresent gate reads (SkillEngine.java:58-60); the level is the one the
		// oracle reports for the extra skill (the template's lvl)
		for (const uint16_t seeded : {HEALING_LIGHT, FOCUSED_EVASION}) {
			const OracleSkillTemplate& skill = mageSkills.skill(seeded);
			database.execute(schema, "INSERT INTO player_skills (player_id, skill_id, skill_level) VALUES (" + std::to_string(a.mageId) + ", " +
			                           std::to_string(seeded) + ", " + std::to_string(skill.level) + ")");
		}
		EXPECT_EQ(database.queryLong(schema, "SELECT COUNT(*) FROM player_skills WHERE player_id = " + std::to_string(a.mageId) + " AND skill_id IN (" +
		                                       std::to_string(HEALING_LIGHT) + ", " + std::to_string(FOCUSED_EVASION) + ")"),
		          2);
	});

	/** CM_ENTER_WORLD for `playerId`, the §5.8 sequence, the SM_SKILL_LIST check against `expectedSkills` and the last SM_STATS_INFO */
	const auto enterWorld = [&](int32_t playerId, bool firstEnter, const OracleCreation& creation, const std::vector<OracleSkill>& expectedSkills,
	                            std::vector<Packet>* burstOut) {
		async = AsyncAllowed::m5aDefault();
		async.selfPlayerState(playerId);
		announcedNpcs.follow(a.game.get());
		async.npcActivity(announcedNpcs.predicate());
		a.game->send(GameSession::CM_ENTER_WORLD, GameSession::buildCM_ENTER_WORLD(playerId));
		const std::vector<Packet> burst = collectBurst(*a.game, async);
		if (burst.empty())
			throw std::runtime_error("no packet after CM_ENTER_WORLD");
		const int32_t inventoryPackets = static_cast<int32_t>((creation.items.size() + 9) / 10) + 1;
		expectSequence(burst, enterWorldPattern(firstEnter, inventoryPackets), async);
		std::vector<std::string> sentSkills;
		for (const Packet& packet : ofName(burst, "SM_SKILL_LIST"))
			for (const decoders::SkillEntry& entry : decoders::decodeSkillList(packet.data).skills)
				sentSkills.push_back(std::to_string(entry.skillId) + "/" + std::to_string(entry.skillLevel));
		std::vector<std::string> wanted;
		for (const OracleSkill& skill : expectedSkills)
			wanted.push_back(std::to_string(skill.skillId) + "/" + std::to_string(skill.level));
		std::ranges::sort(sentSkills);
		std::ranges::sort(wanted);
		EXPECT_EQ(sentSkills, wanted) << "SM_SKILL_LIST against the oracle's autolearn set (and the seeded rows of D3)";
		const std::vector<Packet> stats = ofName(burst, "SM_STATS_INFO");
		if (stats.empty())
			throw std::runtime_error("no SM_STATS_INFO in the enter-world burst");
		if (burstOut != nullptr)
			*burstOut = burst;
		return decoders::decodeStatsInfo(stats.back().data);
	};

	/**
	 * CM_LEVEL_READY and - for a character that enters where nothing is fighting it - its §5.8 sequence. A re-entry next to a monster that still
	 * hates the character is not such a place: the npc's AggroList keeps the player's object id over the logout, so the fight resumes inside
	 * the burst, and the order of that burst is §5.8's business, which the M5a gate owns (the M5b gate's K7b reads its re-entry the same way).
	 */
	const auto levelReady = [&](std::vector<Packet>* burstOut, bool assertSequence = true) {
		a.game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
		const std::vector<Packet> burst = collectBurst(*a.game, async);
		if (burst.empty())
			throw std::runtime_error("no packet after CM_LEVEL_READY");
		if (assertSequence)
			expectSequence(burst, levelReadyPattern(), async);
		if (burstOut != nullptr)
			*burstOut = burst;
	};

	/** the object id of the npc of `templateId` standing on `spot`, from every SM_NPC_INFO recorded so far (the latest wins) */
	const auto objectAt = [&](const OracleMonsterSpot& spot, std::optional<int32_t> excluding = std::nullopt,
	                          int32_t templateId = GATE_MONSTER_NPC_ID) -> std::optional<int32_t> {
		std::optional<int32_t> found;
		for (const Packet& packet : a.game->recorded()) {
			if (packet.name != "SM_NPC_INFO")
				continue;
			try {
				const decoders::NpcInfo npc = decoders::decodeNpcInfo(packet.data);
				if (npc.templateId != templateId || (excluding && npc.objectId == *excluding))
					continue;
				if (std::abs(npc.x - spot.x) <= 0.01f && std::abs(npc.y - spot.y) <= 0.01f && std::abs(npc.z - spot.z) <= 0.01f)
					found = npc.objectId;
			} catch (const DecodeError&) {
				// a packet that does not decode is not this npc
			}
		}
		return found;
	};

	/** waits for an SM_NPC_INFO of `templateId` at `spot` whose object is not `excluding` (a respawn), recording everything */
	const auto waitForNpcAt = [&](const OracleMonsterSpot& spot, std::optional<int32_t> excluding, std::chrono::milliseconds timeout,
	                              int32_t templateId = GATE_MONSTER_NPC_ID) -> std::optional<int32_t> {
		if (std::optional<int32_t> known = objectAt(spot, excluding, templateId))
			return known;
		const std::optional<size_t> index = readUntil(
		  *a.game,
		  [&](const Packet& packet) {
			  if (packet.name != "SM_NPC_INFO")
				  return false;
			  try {
				  const decoders::NpcInfo npc = decoders::decodeNpcInfo(packet.data);
				  return npc.templateId == templateId && (!excluding || npc.objectId != *excluding) && std::abs(npc.x - spot.x) <= 0.01f &&
				         std::abs(npc.y - spot.y) <= 0.01f && std::abs(npc.z - spot.z) <= 0.01f;
			  } catch (const DecodeError&) {
				  return false;
			  }
		  },
		  timeout);
		if (!index)
			return std::nullopt;
		return decoders::decodeNpcInfo(a.game->recorded()[*index].data).objectId;
	};

	// the walk cursor and the two walks of the M5b gate: `walkTo` sleeps between its 5 m steps, `trekTo` drains the socket while it walks
	float atX = warriorCreation.x, atY = warriorCreation.y, atZ = warriorCreation.z;
	const auto walkTo = [&](float toX, float toY, float toZ) {
		const double total = distance2d(atX, atY, toX, toY);
		const int32_t steps = std::max(1, static_cast<int32_t>(total / 5.0));
		const float fromX = atX, fromY = atY, fromZ = atZ;
		for (int32_t step = 1; step <= steps; step++) {
			const float t = static_cast<float>(step) / static_cast<float>(steps);
			a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(fromX + (toX - fromX) * t, fromY + (toY - fromY) * t, fromZ + (toZ - fromZ) * t,
			                                                             0, static_cast<int8_t>(0xE0), toX, toY, toZ));
			collectFor(*a.game, 120ms);
		}
		a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(toX, toY, toZ, 0, 0));
		atX = toX;
		atY = toY;
		atZ = toZ;
		collectFor(*a.game, 1s);
	};
	/** a point `distance` metres from `spot` on the line towards (fromX, fromY) */
	const auto pointNear = [](const OracleMonsterSpot& spot, double distance, float fromX, float fromY) {
		const double dx = fromX - spot.x, dy = fromY - spot.y;
		const double length = std::sqrt(dx * dx + dy * dy);
		const double scale = length <= 0.001 ? 0.0 : distance / length;
		return std::array<float, 3>{static_cast<float>(spot.x + dx * scale), static_cast<float>(spot.y + dy * scale), spot.z};
	};

	/**
	 * castAndWait with the one refusal the gate paces around rather than asserts: CM_CASTSPELL refuses a cast that comes before the previous
	 * skill's animation has finished with STR_SKILL_NOT_READY (CM_CASTSPELL.java:96-102, Player.nextSkillUse). Such a refusal sends neither
	 * SM_CASTSPELL nor SM_CASTSPELL_RESULT, so the cast is simply sent again a moment later, at most `attempts` times.
	 */
	const auto cast = [&](int32_t casterId, uint16_t skillId, int32_t targetId, std::chrono::milliseconds timeout,
	                      const std::optional<GameSession::CastInterruption>& interruption = std::nullopt, int32_t attempts = 4) {
		GameSession::CastOutcome outcome;
		for (int32_t attempt = 0; attempt < attempts; attempt++) {
			GameSession::CastRequest request;
			request.spellId = skillId;
			request.level = 1;
			request.targetType = decoders::CAST_TARGET_OBJECT;
			request.targetObjectId = targetId;
			outcome = a.game->castAndWait(casterId, request, timeout, interruption);
			if (outcome.castSpell || outcome.ended() || outcome.closed)
				return outcome;
			const SkillRecording refusal = recordSkills(*a.game, outcome.firstPacket);
			if (!refusal.hasMessage(STR_SKILL_NOT_READY))
				return outcome;
			collectFor(*a.game, 600ms);
		}
		return outcome;
	};

	// ---- S2: enter world as the Warrior, and X1 ----
	std::optional<decoders::StatsInfo> warriorStats;
	runCase("S2", "the Warrior enters the world with its passives applied (X1)", [&] {
		std::vector<Packet> burst;
		warriorStats = enterWorld(a.warriorId, true, warriorCreation, warriorSkills.characterSkills, &burst);
		const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(firstOfName(burst, "SM_PLAYER_SPAWN")->data);
		EXPECT_EQ(spawned.worldId, warriorCreation.mapId);

		// X1: EVERY SM_STATS_INFO of the burst carries the oracle's main hand attack - the first one (§5.8 #0) as well as #26. The passives are
		// applied in PlayerEnterWorldService.activatePassiveSkillEffects BEFORE setClientConnection (PlayerEnterWorldService.java:186-187), so
		// no packet of the burst may show the attack without them.
		ASSERT_TRUE(warriorCreation.mainHandPAttackBase && warriorCreation.mainHandPAttackCurrent);
		for (const Packet& packet : ofName(burst, "SM_STATS_INFO")) {
			const decoders::StatsInfo info = decoders::decodeStatsInfo(packet.data);
			EXPECT_EQ(info.baseMainHandPAttack, *warriorCreation.mainHandPAttackBase)
			  << "X1: the base main hand attack is the weapon's mean damage at the class power (19 for the Training Sword at power 110)";
			EXPECT_EQ(info.mainHandPAttack, *warriorCreation.mainHandPAttackCurrent)
			  << "X1: the current main hand attack with the passives applied - 37 Basic Sword Training's 16 % fixed bonus and 140's +7 bonus "
			     "(tools/oracle m5a-creation statsInfo). Equal to the base: SkillEngine::applyEffectDirectly left partial (O-09); higher: a "
			     "passive applied twice, or a weapon mastery that skips its weapon-group check";
			EXPECT_EQ(info.baseMaxHp, warriorCreation.baseMaxHp) << "X1: the passives never touch the base max HP (D2)";
			EXPECT_EQ(info.maxHp, warriorCreation.maxHpCurrent) << "X1: the current max HP - the base plus the equipment's bonus modifiers";
			EXPECT_EQ(info.baseMaxMp, warriorCreation.baseMaxMp);
			EXPECT_EQ(info.maxMp, warriorCreation.maxMpCurrent);
		}
		std::cout << "X1: the Warrior's main hand attack is " << warriorStats->baseMainHandPAttack << "/" << warriorStats->mainHandPAttack
		          << " (base/current), the oracle's " << *warriorCreation.mainHandPAttackBase << "/" << *warriorCreation.mainHandPAttackCurrent
		          << std::endl;
	});

	// ---- S3: level ready, and monster A ----
	int32_t monsterA = 0;
	runCase("S3", "level ready and monster A is announced", [&] {
		levelReady(nullptr);
		monsterA = objectAt(*spotA).value_or(0);
		ASSERT_NE(monsterA, 0) << "no SM_NPC_INFO for npc " << GATE_MONSTER_NPC_ID << " at spot A (" << spotA->x << ", " << spotA->y << ", " << spotA->z << ")";
	});

	// ---- S4: the instant skill, its chain byte and its cooldown (X2, X3, the cooldown row) ----
	runCase("S4", "the Warrior's instant chain skill and its cooldown (X2, X3, cooldown)", [&] {
		const OracleSkillTemplate& strike = warriorSkills.skill(FEROCIOUS_STRIKE);
		const std::array<float, 3> melee = pointNear(*spotA, MELEE_DISTANCE, atX, atY);
		walkTo(melee[0], melee[1], melee[2]);
		a.game->send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(monsterA));
		waitFor(*a.game, "SM_TARGET_SELECTED", 10s);

		// Accepted casts: the first one, and - only when every accepted cast so far was dodged or resisted - one more after each cooldown, at most
		// three in all. Each accepted cast is asserted in full; X2's damage half needs one that landed.
		int32_t monsterHp = monster.maxHp; // npcs do not regenerate in a fight (m5b-plan.md A3), so the HP the gate knows is the HP it took away
		bool landed = false;
		std::optional<std::chrono::steady_clock::time_point> lastResultAt;
		for (int32_t accepted = 0; accepted < 3 && !landed; accepted++) {
			if (lastResultAt)
				drainUntil(*a.game, *lastResultAt + std::chrono::milliseconds(strike.cooldownMillis + 1000));
			const GameSession::CastOutcome outcome = cast(a.warriorId, FEROCIOUS_STRIKE, monsterA, 5s);
			collectFor(*a.game, 1500ms); // the damage of an instant skill is applied before the result is sent; anything after it is late
			const SkillRecording recording = recordSkills(*a.game, outcome.firstPacket);
			EXPECT_TRUE(recording.decodeFailures.empty()) << join(recording.decodeFailures, "\n  ");

			// X2: SM_CASTSPELL with the oracle's castDuration (0), then SM_CASTSPELL_RESULT for the skill, and no SM_SKILL_CANCEL
			const auto casts = recording.castsBy(a.warriorId, FEROCIOUS_STRIKE);
			const auto results = recording.resultsBy(a.warriorId, FEROCIOUS_STRIKE);
			ASSERT_EQ(casts.size(), 1u) << "X2: CM_CASTSPELL(2864) was answered by " << casts.size() << " SM_CASTSPELL of the Warrior (canUseSkill, "
			                            << "Properties.validate or CM_CASTSPELL refused it); the window was "
			                            << join(namesOf(std::vector<Packet>(a.game->recorded().begin() + static_cast<std::ptrdiff_t>(outcome.firstPacket),
			                                                                a.game->recorded().end())));
			EXPECT_EQ(casts[0].value.castDuration, strike.castDuration.value_or(-1)) << "X2: an instant skill has no cast bar";
			EXPECT_EQ(casts[0].value.targetType, decoders::CAST_TARGET_OBJECT);
			EXPECT_EQ(casts[0].value.targetObjectId, monsterA);
			EXPECT_EQ(casts[0].value.level, 1);
			ASSERT_EQ(results.size(), 1u) << "X2: no SM_CASTSPELL_RESULT for the instant skill";
			EXPECT_TRUE(recording.cancelsBy(a.warriorId, FEROCIOUS_STRIKE).empty()) << "X2: the instant skill was cancelled";
			const decoders::CastSpellResult& result = results[0].value;
			EXPECT_EQ(result.targetObjectId, monsterA);
			EXPECT_EQ(result.skillTemplateLevel, strike.lvl);
			EXPECT_EQ(result.cooldown, strike.cooldown) << "the cooldown row: SM_CASTSPELL_RESULT carries Skill.getCooldown(), the template's value in "
			                                               "units of 100 ms (SM_CASTSPELL_RESULT.java:81)";
			EXPECT_NE(result.chainStatus, decoders::CAST_RESULT_NO_EFFECT) << "X2: status 16 is an empty effect list - Properties picked no target";
			ASSERT_EQ(result.effects.size(), 1u) << "X2: one effected creature, the monster";
			const decoders::CastResultEffect& effect = result.effects[0];
			EXPECT_EQ(effect.effectedObjectId, monsterA);
			const bool missed = dodgedOrResisted(effect);
			// X3: the chain byte. Skill.endCast sets chainSuccess = Rnd.chance() < chain_skill_prob (100) unless every target dodged or resisted
			// (blockedChain), and SM_CASTSPELL_RESULT writes 32 for a success and 0 otherwise (Skill.java:626-637, SM_CASTSPELL_RESULT.java:89-94)
			EXPECT_EQ(result.chainStatus, missed ? decoders::CAST_RESULT_NO_CHAIN : decoders::CAST_RESULT_CHAIN_SUCCESS)
			  << "X3: effect result " << static_cast<int32_t>(effect.effectResult) << " must give the chain byte "
			  << (missed ? "0 (a blocked chain)" : "32 (chain_skill_prob 100)");
			if (missed) {
				EXPECT_EQ(effect.successfulEffects, effect.effectResult == decoders::EFFECT_RESULT_RESIST ? 1 : 0);
				EXPECT_TRUE(recording.statusesOf(monsterA).empty()) << "X2: a dodged skill damaged the monster";
				std::cout << "X2: 2864 was " << (effect.effectResult == decoders::EFFECT_RESULT_DODGE ? "dodged" : "resisted")
				          << "; the next one goes out after the cooldown" << std::endl;
			} else {
				landed = true;
				EXPECT_EQ(effect.successfulEffects, successfulEffectsByte(strike)) << "X2: the skill's one template succeeded (Effect.getSuccessfulEffectsAsByte)";
				// X2's damage half: DamageEffect.applyEffect -> CreatureController.onAttack(effect, TYPE.REGULAR, reserved value) -> reduceHp ->
				// SM_ATTACK_STATUS(type REGULAR, the skill id, previousHp - newHp) - and the damage the result announced is the damage applied
				ASSERT_EQ(effect.reserved.size(), 1u);
				EXPECT_EQ(effect.reserved[0].resourceType, decoders::RESERVED_RESOURCE_HP);
				EXPECT_GT(effect.reserved[0].value, 0) << "X2: the reserved HP of a damage effect is the damage (EffectReserved.getValueToSend)";
				const auto statuses = recording.statusesOf(monsterA);
				ASSERT_EQ(statuses.size(), 1u) << "X2: the skill's damage reached the monster " << statuses.size() << " times";
				EXPECT_EQ(statuses[0].value.skillId, FEROCIOUS_STRIKE);
				EXPECT_EQ(statuses[0].value.type, decoders::ATTACK_STATUS_TYPE_REGULAR);
				EXPECT_EQ(statuses[0].value.value, std::min(effect.reserved[0].value, monsterHp))
				  << "X2: SM_ATTACK_STATUS applied " << statuses[0].value.value << " of the " << effect.reserved[0].value << " the result announced, with "
				  << monsterHp << " HP left";
				// no order is asserted between the damage and the result: Skill.endCast applies the effects at once only for an isInstantSkill()
				// - hitTime 0 or an instant motion (Skill.java:660-663, 1084-1086) - and a skill without a cast bar still has the hit time of its
				// animation (Skill.updateHitTime), so 2864's damage follows its result by that hit time
				monsterHp -= statuses[0].value.value;
			}
			lastResultAt = results[0].at;

			// the cooldown row, on the first accepted cast only: a CM_CASTSPELL one second before the cooldown ends is refused by
			// PlayerRestrictions.canUseSkill -> Player.isSkillDisabled (PlayerRestrictions.java:87-88, Player.java:1524-1536), well after the
			// skill-use interval that could also refuse it
			if (accepted == 0) {
				drainUntil(*a.game, results[0].at + std::chrono::milliseconds(strike.cooldownMillis - 1000));
				const size_t early = a.game->recorded().size();
				GameSession::CastRequest request;
				request.spellId = FEROCIOUS_STRIKE;
				request.targetObjectId = monsterA;
				a.game->send(GameSession::CM_CASTSPELL, GameSession::buildCM_CASTSPELL(request));
				collectFor(*a.game, 800ms);
				const SkillRecording refused = recordSkills(*a.game, early);
				EXPECT_TRUE(refused.castsBy(a.warriorId, FEROCIOUS_STRIKE).empty() && refused.resultsBy(a.warriorId, FEROCIOUS_STRIKE).empty())
				  << "cooldown: 2864 was cast again " << (strike.cooldownMillis - 1000) << " ms after the first, inside its " << strike.cooldownMillis
				  << " ms cooldown (Skill.setCooldowns / Creature.isSkillDisabled)";
				// Player.isSkillDisabled tells the player (Player.java:1532-1534). CM_CASTSPELL's skill-use audit sends the same message, but
				// nine seconds after an animation it cannot be what refused this cast
				EXPECT_TRUE(refused.hasMessage(STR_SKILL_NOT_READY)) << "cooldown: Player.isSkillDisabled refuses with STR_SKILL_NOT_READY";
			}
		}
		EXPECT_TRUE(landed) << "X2: none of three casts of 2864 landed; the damage half is unproven";

		// ... and the cooldown ends: one more cast after it is accepted (the cooldown is not ten times too long, and it is not permanent)
		drainUntil(*a.game, *lastResultAt + std::chrono::milliseconds(strike.cooldownMillis + 1000));
		const GameSession::CastOutcome after = cast(a.warriorId, FEROCIOUS_STRIKE, monsterA, 5s);
		EXPECT_TRUE(after.castSpellResult.has_value()) << "cooldown: 2864 was still refused " << (strike.cooldownMillis + 1000)
		                                               << " ms after the previous cast, and its cooldown is " << strike.cooldownMillis << " ms";
	});

	// ---- S5: finish monster A (not an assertion of this gate: m5b-plan.md K5 owns the melee fight) ----
	runCase("S5", "the Warrior kills monster A", [&] {
		bool monsterDied = false, warriorDied = false;
		const GameSession::FightOutcome outcome = a.game->fightUntil(
		  monsterA, std::chrono::milliseconds(warriorStats->attackSpeed),
		  [&](const Packet& packet) {
			  if (packet.name != "SM_EMOTION")
				  return false;
			  try {
				  const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
				  if (emotion.emotionType != decoders::EMOTION_DIE)
					  return false;
				  monsterDied = monsterDied || emotion.senderObjectId == monsterA;
				  warriorDied = warriorDied || emotion.senderObjectId == a.warriorId;
				  return monsterDied || warriorDied;
			  } catch (const DecodeError&) {
				  return false;
			  }
		  },
		  90s, 60);
		ASSERT_FALSE(warriorDied) << "monster A killed the Warrior";
		ASSERT_TRUE(monsterDied) << "monster A did not die within " << outcome.elapsed.count() << " ms";
	});

	// ---- S5b: the npc casts (X9, §10.2 C11) ----
	// The Warrior walks from monster A's corpse to the oracle's nearest plain spot of npc 210133 and pulls the kerub with ONE swing - a MONSTER-
	// tribe npc starts no fight (m5b-plan.md A5a) - and from then on sends nothing at it. The kerub fights back, and every attack decision it
	// makes asks its skill list first (GeneralNpcAI.chooseAttackIntention -> chooseSkillAttack -> SkillAttackManager.chooseNextSkill,
	// GeneralNpcAI.java:115-138). The recording ends at the first cast that landed and did its damage, or once X9_DECISIONS decisions rolled
	// the entry's chance without a cast; then the Warrior finishes the kerub, so no npc still hates it when it quits in S6.
	// S5b does not block the cases after it (cases.run, not runCase): nothing of the Mage's reads its outcome, so a failure here leaves S6-S17 to
	// run - which is also what lets X9's mutants show every other row green.
	if (ok)
		cases.run("S5b", "npc 210133 casts Brandish 16419 at the Warrior (X9)", [&] {
			ASSERT_TRUE(kerub.nearestPlainSpot && kerubSkill && warriorStats);
			const OracleMonsterSpot& spot = *kerub.nearestPlainSpot;
			const std::array<float, 3> melee = pointNear(spot, MELEE_DISTANCE, atX, atY);
			walkTo(melee[0], melee[1], melee[2]);
			const std::optional<int32_t> found = waitForNpcAt(spot, std::nullopt, 15s, NPC_SKILL_NPC_IDS[0]);
			ASSERT_TRUE(found) << "no SM_NPC_INFO for npc " << NPC_SKILL_NPC_IDS[0] << " at its spot (" << spot.x << ", " << spot.y << ", " << spot.z
			                   << ")";
			const int32_t kerubId = *found;
			a.game->send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(kerubId));
			waitFor(*a.game, "SM_TARGET_SELECTED", 10s);
			const size_t from = a.game->recorded().size();

			/** the arrival times of the SM_ATTACK of `attacker` at `target` recorded since `from` */
			const auto swings = [&](int32_t attacker, int32_t target) {
				std::vector<std::chrono::steady_clock::time_point> at;
				const std::vector<Packet>& packets = a.game->recorded();
				for (size_t i = from; i < packets.size(); i++) {
					if (packets[i].name != "SM_ATTACK")
						continue;
					try {
						const decoders::Attack attack = decoders::decodeAttack(packets[i].data);
						if (attack.attackerObjectId == attacker && attack.targetObjectId == target)
							at.push_back(packets[i].receivedAt);
					} catch (const DecodeError&) {
						// not a swing this case can count; recordSkills' decode failures do not cover SM_ATTACK, the fight cases do
					}
				}
				return at;
			};

			// the pull: one swing, sent again only while it has not gone out (a refused CM_ATTACK sends no SM_ATTACK), at most three times
			for (int32_t attempt = 0; attempt < 3 && swings(a.warriorId, kerubId).empty(); attempt++) {
				a.game->send(GameSession::CM_ATTACK, GameSession::buildCM_ATTACK(kerubId));
				collectFor(*a.game, std::chrono::milliseconds(warriorStats->attackSpeed));
			}
			const std::vector<std::chrono::steady_clock::time_point> pulls = swings(a.warriorId, kerubId);
			ASSERT_FALSE(pulls.empty()) << "the Warrior's swing at the kerub never went out";
			// a decision after this point has passed the initial skill delay whatever its draw: at most 3 x the attack speed after the fight
			// started (AttackManager.startAttacking -> setFightStartingTime, AttackManager.java:24), which follows the Warrior's swing within
			// milliseconds - the 500 ms margin covers that
			const auto skillDelayOver = pulls.front() + std::chrono::milliseconds(3 * kerub.npcAttackSpeed) + 500ms;
			const auto castWindow = std::chrono::milliseconds(kerubSkill->castDuration) + 3s;

			// the kerub's decisions: its melee swings at the Warrior and the casts it started. One ROLLS the entry's chance when it comes after
			// the initial skill delay and more than X9_NEXT_SKILL_DELAY_MAX after the kerub's last cast
			int32_t decisions = 0, rolled = 0;
			std::string stoppedBecause;
			const auto deadline = std::chrono::steady_clock::now() + 180s;
			for (;;) {
				collectFor(*a.game, 500ms);
				const auto now = std::chrono::steady_clock::now();
				const SkillRecording seen = recordSkills(*a.game, from);
				const auto casts = seen.castsBy(kerubId, BRANDISH);
				const auto results = seen.resultsBy(kerubId, BRANDISH);
				const size_t cancels = seen.cancelsBy(kerubId, BRANDISH).size();
				std::vector<std::chrono::steady_clock::time_point> decidedAt = swings(kerubId, a.warriorId);
				for (const auto& cast : casts)
					decidedAt.push_back(cast.at);
				decisions = static_cast<int32_t>(decidedAt.size());
				rolled = 0;
				for (const auto& at : decidedAt) {
					const bool nextSkillDelay =
					  std::ranges::any_of(casts, [&](const auto& cast) { return cast.at < at && at - cast.at < X9_NEXT_SKILL_DELAY_MAX; });
					if (at > skillDelayOver && !nextSkillDelay)
						rolled++;
				}
				// the first result whose effect landed, and whether its damage has arrived. The damage is looked for from the cast's start: an npc
				// sends no client hit time, so its hitTime is 0 (Skill.updateHitTime, Skill.java:416-418), isInstantSkill() holds and endCast applies
				// the effects BEFORE it sends the result (Skill.java:660-667) - the opposite order of the player's 2864 in S4
				std::optional<SkillRecording::At<decoders::CastSpellResult>> landed;
				for (const auto& result : results)
					if (!landed && result.value.effects.size() == 1 && !dodgedOrResisted(result.value.effects[0]))
						landed = result;
				bool damaged = false;
				if (landed) {
					size_t castIndex = 0;
					for (const auto& cast : casts)
						if (cast.index < landed->index)
							castIndex = cast.index;
					for (const auto& status : seen.statusesOf(a.warriorId))
						damaged = damaged || (status.index > castIndex && status.value.skillId == BRANDISH);
				}
				if (seen.diedAt(a.warriorId)) {
					stoppedBecause = "the Warrior died";
					break;
				}
				if (a.game->client.socket.isClosed()) {
					stoppedBecause = "the connection closed";
					break;
				}
				if (landed && (damaged || now - landed->at > 3s)) {
					stoppedBecause = damaged ? "a cast landed and did its damage" : "a cast landed and no damage followed within 3 s";
					break;
				}
				if (casts.size() > results.size() + cancels) {
					if (now - casts.back().at < castWindow)
						continue; // a cast is running
					stoppedBecause = "a cast was still without a result " + std::to_string(castWindow.count()) + " ms after it started";
					break;
				}
				if (rolled >= X9_DECISIONS) {
					stoppedBecause = std::to_string(rolled) + " decisions rolled the chance";
					break;
				}
				if (now >= deadline) {
					stoppedBecause = "the 180 s deadline";
					break;
				}
			}
			const size_t until = a.game->recorded().size();

			// not an assertion: the Warrior finishes the kerub, so its fight is over before S6's quit (and before any row below can end the case)
			const SkillRecording fight = recordSkills(*a.game, from, until);
			bool kerubDied = false, warriorDied = fight.diedAt(a.warriorId);
			if (!warriorDied)
				a.game->fightUntil(
				  kerubId, std::chrono::milliseconds(warriorStats->attackSpeed),
				  [&](const Packet& packet) {
					  if (packet.name != "SM_EMOTION")
						  return false;
					  try {
						  const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
						  if (emotion.emotionType != decoders::EMOTION_DIE)
							  return false;
						  kerubDied = kerubDied || emotion.senderObjectId == kerubId;
						  warriorDied = warriorDied || emotion.senderObjectId == a.warriorId;
						  return kerubDied || warriorDied;
					  } catch (const DecodeError&) {
						  return false;
					  }
				  },
				  60s, 40);
			std::cout << "X9: the kerub (object " << kerubId << ") made " << decisions << " attack decisions, " << rolled
			          << " of them rolling 16419's chance; the recording stopped because " << stoppedBecause << "; the kerub "
			          << (kerubDied ? "then died to the Warrior" : "was not finished") << std::endl;

			// X9 has five rows, each killed by its own mutant (m5b2-plan.md §10.8): (1) the cast, (2) its cast bar, (3) its result, (4) its damage and
			// (5) the damage's value. "At the Warrior" is part of the matching, not a row of its own: a cast or a result aimed elsewhere is not one.
			const SkillRecording& recording = fight;
			EXPECT_TRUE(recording.decodeFailures.empty()) << join(recording.decodeFailures, "\n  ");
			ASSERT_FALSE(recording.diedAt(a.warriorId)) << "the kerub killed the Warrior before X9 was decided (" << decisions << " decisions)";
			const auto casts = SkillRecording::where(recording.castsBy(kerubId, BRANDISH), [&](const decoders::CastSpell& cast) {
				return cast.targetType == decoders::CAST_TARGET_OBJECT && cast.targetObjectId == a.warriorId;
			});
			const auto results = SkillRecording::where(recording.resultsBy(kerubId, BRANDISH), [&](const decoders::CastSpellResult& result) {
				return result.targetObjectId == a.warriorId && result.effects.size() == 1 && result.effects[0].effectedObjectId == a.warriorId;
			});

			// (1) the cast: at least one SM_CASTSPELL of the kerub for 16419 at the Warrior - its MOST_HATED creature, the npc_skills row's target
			// by NpcSkillTemplate's default (SkillAttackManager.skillAction)
			const double noCast = std::pow(1.0 - kerubSkill->prob / 100.0, rolled);
			ASSERT_FALSE(casts.empty()) << "X9: npc 210133 made " << decisions << " attack decisions, " << rolled << " of them rolling 16419's chance "
			                            << "(prob " << kerubSkill->prob << ", NpcSkillTemplateEntry.chanceReady), and cast it at the Warrior at none; the "
			                            << "probability of that is " << noCast << ". GeneralNpcAI.chooseSkillAttack answers false, NpcSkillEntry.isReady "
			                            << "or conditionReady refuses everything, or SkillAttackManager.skillAction never reaches useSkill";
			// (2) the npc's cast bar
			for (const auto& cast : casts)
				EXPECT_EQ(cast.value.castDuration, kerubSkill->castDuration)
				  << "X9: an npc's cast bar is Math.round(duration * castSpeed / 1000f) at the npc's own cast speed (Skill.java:338-340), the "
				     "oracle's "
				  << kerubSkill->castDuration << " ms";

			// (3) the result: every cast ended in the kerub's own SM_CASTSPELL_RESULT naming the Warrior as target and effected creature - the npc's
			// scheduled endCast ran (a cancelled cast has no result either)
			ASSERT_EQ(results.size(), casts.size())
			  << "X9: " << casts.size() << " casts of the kerub at the Warrior and " << results.size() << " SM_CASTSPELL_RESULT of them - Skill.useSkill "
			  << "schedules endCast at castDuration for an npc as for a player (Skill.java:309-314), and endCast sends the result";

			// (4) and (5) the damage: the first cast that landed reduced the Warrior's HP, by what its result announced (SkillAttackInstantEffect is a
			// DamageEffect: applyEffect -> onAttack(REGULAR, the reserved value) -> SM_ATTACK_STATUS with the skill id, DamageEffect.java:28-40).
			// No order is asserted between the damage and the result; the damage is looked for from the cast's start (see the loop above: an npc's
			// hit time is 0, so Java applies the effects before it sends the result)
			const auto landed = std::ranges::find_if(results, [](const auto& result) { return !dodgedOrResisted(result.value.effects[0]); });
			ASSERT_NE(landed, results.end()) << "X9: all " << results.size() << " Brandish casts were dodged or resisted, so the damage half is unproven";
			ASSERT_EQ(landed->value.effects[0].reserved.size(), 1u) << "X9: a DamageEffect reserves one value, the HP it takes";
			const size_t landedCast = casts[static_cast<size_t>(landed - results.begin())].index;
			std::optional<SkillRecording::At<decoders::AttackStatusUpdate>> damage;
			for (const auto& status : recording.statusesOf(a.warriorId))
				if (!damage && status.index > landedCast && status.value.skillId == BRANDISH)
					damage = status;
			ASSERT_TRUE(damage) << "X9: the kerub's Brandish landed (effect result " << static_cast<int32_t>(landed->value.effects[0].effectResult)
			                    << ") and no SM_ATTACK_STATUS of the Warrior carries 16419 - its SkillAttackInstantEffect did no damage";
			EXPECT_EQ(damage->value.value, landed->value.effects[0].reserved[0].value)
			  << "X9: SM_ATTACK_STATUS applied " << damage->value.value << " of the " << landed->value.effects[0].reserved[0].value
			  << " the result announced";
			std::cout << "X9: Brandish cast " << millisBetween(pulls.front(), casts.front().at) << " ms into the fight, result "
			          << millisBetween(casts.front().at, results.front().at) << " ms after its SM_CASTSPELL, " << damage->value.value << " damage"
			          << std::endl;
		});

	// ---- S6: the Mage enters the world (X1's other class, D3's seeded skills) ----
	std::optional<decoders::StatsInfo> mageStats;
	runCase("S6", "the Warrior quits and the Mage enters the world with its seeded skills (X1, D3)", [&] {
		a.game->send(GameSession::CM_QUIT, GameSession::buildCM_QUIT(true));
		waitFor(*a.game, "SM_QUIT_RESPONSE", 30s);
		std::this_thread::sleep_for(1500ms); // gameserver.character.reentry.time is 1 second in the scenario profile
		std::vector<OracleSkill> expected = mageSkills.characterSkills;
		for (const uint16_t seeded : {HEALING_LIGHT, FOCUSED_EVASION})
			expected.push_back({seeded, mageSkills.skill(seeded).level});
		std::vector<Packet> burst;
		mageStats = enterWorld(a.mageId, true, mageCreation, expected, &burst);
		atX = mageCreation.x;
		atY = mageCreation.y;
		atZ = mageCreation.z;
		EXPECT_EQ(mageStats->baseMaxHp, mageCreation.baseMaxHp);
		EXPECT_EQ(mageStats->maxHp, mageCreation.maxHpCurrent) << "X1: no passive of a fresh Mage is on MAXHP";
		EXPECT_EQ(mageStats->baseMaxMp, mageCreation.baseMaxMp);
		EXPECT_EQ(mageStats->maxMp, mageCreation.maxMpCurrent) << "X1: the robe's two MAXMP bonus modifiers (the oracle's statsInfo.maxMp)";
		EXPECT_EQ(mageStats->currentMp, mageStats->maxMp) << "X4 counts the MP down from a full bar";
		ASSERT_TRUE(mageCreation.mainHandPAttackBase && mageCreation.mainHandPAttackCurrent);
		EXPECT_EQ(mageStats->baseMainHandPAttack, *mageCreation.mainHandPAttackBase) << "X1: a magical main hand has no physical attack";
		EXPECT_EQ(mageStats->mainHandPAttack, *mageCreation.mainHandPAttackCurrent);
		levelReady(nullptr);
	});

	// ---- S7: the two monsters of the Mage ----
	int32_t monsterB = 0;
	runCase("S7", "monster A has respawned and monster B is announced", [&] {
		// A died in S5; it comes back as a new object after its respawn time (m5b-plan.md R4), and the Mage walks towards it meanwhile
		const std::array<float, 3> standOff = pointNear(*spotA, CAST_DISTANCE, atX, atY);
		walkTo(standOff[0], standOff[1], standOff[2]);
		const std::optional<int32_t> respawned = waitForNpcAt(*spotA, monsterA, std::chrono::seconds(monster.respawnTime) + 30s);
		ASSERT_TRUE(respawned) << "monster A did not respawn at spot A";
		monsterA = *respawned;
		monsterB = objectAt(*spotB).value_or(0);
		std::cout << "S7: monster A is object " << monsterA << ", monster B " << (monsterB == 0 ? "not announced yet" : "object " + std::to_string(monsterB))
		          << std::endl;
		a.game->send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(monsterA));
		waitFor(*a.game, "SM_TARGET_SELECTED", 10s);
		collectFor(*a.game, 1s);
	});

	const OracleSkillTemplate* boltPointer = nullptr;
	int32_t mageMp = 0;
	// ---- S8: the interruption (X5) ----
	runCase("S8", "a CM_MOVE 300 ms into Flame Bolt cancels it and costs no MP (X5)", [&] {
		const OracleSkillTemplate& bolt = mageSkills.skill(FLAME_BOLT);
		boltPointer = &bolt;
		mageMp = mageStats->currentMp;
		// one step of 1 m away from the monster, 300 ms into the cast: PlayerController.onStartMove -> cancelCurrentSkill(null)
		// (PlayerController.java:484-489, 514-546)
		const std::array<float, 3> step = pointNear(*spotA, CAST_DISTANCE + 1.0, atX, atY);
		GameSession::CastInterruption move;
		move.after = 300ms;
		move.opcode = GameSession::CM_MOVE;
		// from where the Mage stands towards the step: POSITION | MANUAL on a character that is not in move is CM_MOVE's start-move arm
		// (CM_MOVE.java:195-197)
		move.body = GameSession::buildCM_MOVE(atX, atY, atZ, 0, static_cast<int8_t>(0xE0), step[0], step[1], step[2]);
		const GameSession::CastOutcome outcome = cast(a.mageId, FLAME_BOLT, monsterA, std::chrono::milliseconds(*bolt.castDuration + 3000), move);
		// the rest of the cast's time, and more: an endCast that still ran would send its result here
		collectFor(*a.game, std::chrono::milliseconds(*bolt.castDuration + 1500));
		a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(step[0], step[1], step[2], 0, 0));
		atX = step[0];
		atY = step[1];
		atZ = step[2];
		const SkillRecording recording = recordSkills(*a.game, outcome.firstPacket);
		EXPECT_TRUE(recording.decodeFailures.empty()) << join(recording.decodeFailures, "\n  ");

		const auto casts = recording.castsBy(a.mageId, FLAME_BOLT);
		ASSERT_EQ(casts.size(), 1u) << "X5: the cast never started, so there was nothing to interrupt";
		ASSERT_TRUE(outcome.interruptionSentAt) << "X5: the cast ended before the CM_MOVE could be sent";
		EXPECT_EQ(casts[0].value.castDuration, *bolt.castDuration);
		const auto cancels = recording.cancelsBy(a.mageId, FLAME_BOLT);
		ASSERT_EQ(cancels.size(), 1u) << "X5: the CM_MOVE was not answered by SM_SKILL_CANCEL (PlayerController.onStartMove -> cancelCurrentSkill)";
		EXPECT_TRUE(cancels[0].at >= *outcome.interruptionSentAt) << "X5: the cast was cancelled before the move was sent";
		EXPECT_TRUE(recording.hasMessage(STR_SKILL_CANCELED)) << "X5: cancelCurrentSkill(null) tells the caster STR_SKILL_CANCELED";
		EXPECT_TRUE(recording.resultsBy(a.mageId, FLAME_BOLT).empty())
		  << "X5: SM_CASTSPELL_RESULT arrived after the cancel - the scheduled endCast still ran. It returns when the effector is no longer casting "
		     "OR the skill is cancelled (Skill.java:558), and CreatureController.abortCast does both (CreatureController.java:495-501), so a "
		     "cancelCast that forgets isCancelled alone is not enough to reach this row";
		// the MP: payCastCosts runs in endCast (Skill.java:569-572), which a cancelled cast never reaches
		for (const auto& status : recording.statusesOf(a.mageId))
			EXPECT_NE(status.value.type, decoders::ATTACK_STATUS_TYPE_USED_MP)
			  << "X5: the cancelled cast charged " << -status.value.value << " MP - the cost was paid at the start of the cast, not in endCast";
		for (const auto& mp : recording.mpUpdates)
			EXPECT_EQ(mp.value.currentMp, mageMp) << "X5: the Mage's MP moved during a cancelled cast";
		EXPECT_TRUE(recording.statusesOf(monsterA).empty()) << "X5: the cancelled Flame Bolt damaged the monster";
		std::cout << "X5: SM_SKILL_CANCEL " << millisBetween(*outcome.interruptionSentAt, cancels[0].at) << " ms after the CM_MOVE, "
		          << millisBetween(casts[0].at, cancels[0].at) << " ms into the " << *bolt.castDuration << " ms cast" << std::endl;
	});

	// ---- S9: the cast bar and the MP (X4) ----
	runCase("S9", "Flame Bolt: the cast bar, the two-phase cast and the MP it costs (X4)", [&] {
		const OracleSkillTemplate& bolt = *boltPointer;
		collectFor(*a.game, 1500ms);
		const GameSession::CastOutcome outcome = cast(a.mageId, FLAME_BOLT, monsterA, std::chrono::milliseconds(*bolt.castDuration + 4000));
		collectFor(*a.game, 1500ms); // a cast skill applies its effects hitTime after endCast (Skill.java:664); the damage arrives after the result
		const SkillRecording recording = recordSkills(*a.game, outcome.firstPacket);
		EXPECT_TRUE(recording.decodeFailures.empty()) << join(recording.decodeFailures, "\n  ");
		const auto casts = recording.castsBy(a.mageId, FLAME_BOLT);
		const auto results = recording.resultsBy(a.mageId, FLAME_BOLT);
		ASSERT_EQ(casts.size(), 1u) << "X4: CM_CASTSPELL(1282) started no cast";
		ASSERT_EQ(results.size(), 1u) << "X4: the cast never ended";
		EXPECT_TRUE(recording.cancelsBy(a.mageId, FLAME_BOLT).empty()) << "X4: nothing touched the Mage, yet the cast was cancelled";

		// the cast bar: SM_CASTSPELL's castDuration is the template's duration for a level-1 character with no cast speed (D8, exact), and the
		// result comes no earlier than that: Skill.useSkill schedules endCast at castDuration (Skill.java:309-314)
		EXPECT_EQ(casts[0].value.castDuration, *bolt.castDuration) << "X4: the cast bar";
		ASSERT_TRUE(bolt.castSpeed);
		EXPECT_FLOAT_EQ(casts[0].value.castSpeed, *bolt.castSpeed);
		EXPECT_EQ(casts[0].value.allowAnimationBoostByCastSpeed, bolt.allowAnimationBoost);
		const int64_t castTook = millisBetween(casts[0].at, results[0].at);
		EXPECT_GE(castTook, *bolt.castDuration - 200) << "X4: SM_CASTSPELL_RESULT came " << castTook << " ms after SM_CASTSPELL, and the cast bar is "
		                                              << *bolt.castDuration << " ms - endCast ran at the start of the cast";
		EXPECT_LE(castTook, *bolt.castDuration + 1500) << "X4: the cast took far longer than its bar";

		// the MP: MpCondition.validate reduces it itself with TYPE.USED_MP (MpCondition.java:31-35), from payCastCosts in endCast; the wire
		// carries the value negated (SM_ATTACK_STATUS.java:137-140) and SM_STATUPDATE_MP the absolute value
		std::vector<SkillRecording::At<decoders::AttackStatusUpdate>> costs;
		for (const auto& status : recording.statusesOf(a.mageId))
			if (status.value.type == decoders::ATTACK_STATUS_TYPE_USED_MP)
				costs.push_back(status);
		ASSERT_EQ(costs.size(), 1u) << "X4: the cast charged its MP " << costs.size() << " times - MpCondition only checked the MP";
		EXPECT_EQ(costs[0].value.value, -*bolt.mpCost) << "X4: the MP cost is the template's <mp> end condition (D8, exact)";
		EXPECT_GE(millisBetween(casts[0].at, costs[0].at), *bolt.castDuration - 200)
		  << "X4: the MP was charged at the start of the cast; payCastCosts belongs to endCast";
		const auto mpAfterCost = std::ranges::find_if(recording.mpUpdates, [&](const auto& mp) { return mp.index > costs[0].index; });
		ASSERT_NE(mpAfterCost, recording.mpUpdates.end()) << "X4: no SM_STATUPDATE_MP after the MP cost";
		EXPECT_EQ(mpAfterCost->value.currentMp, mageMp - *bolt.mpCost) << "X4: the absolute MP did not fall by the cost";
		mageMp = mpAfterCost->value.currentMp;

		const decoders::CastSpellResult& result = results[0].value;
		EXPECT_EQ(result.cooldown, bolt.cooldown);
		ASSERT_EQ(result.effects.size(), 1u);
		EXPECT_EQ(result.effects[0].effectedObjectId, monsterA);
		const bool missed = dodgedOrResisted(result.effects[0]);
		EXPECT_EQ(result.chainStatus, missed ? decoders::CAST_RESULT_NO_CHAIN : decoders::CAST_RESULT_CHAIN_SUCCESS) << "X3 for the Mage's chain skill";
		if (!missed) {
			const auto damage = recording.statusesOf(monsterA);
			ASSERT_FALSE(damage.empty()) << "X4: the Flame Bolt that landed did not damage the monster";
			EXPECT_EQ(damage[0].value.skillId, FLAME_BOLT);
			ASSERT_FALSE(result.effects[0].reserved.empty());
			EXPECT_EQ(damage[0].value.value, std::min(result.effects[0].reserved[0].value, monster.maxHp))
			  << "X4: the damage applied is the damage the result announced";
		}
		std::cout << "X4: SM_CASTSPELL_RESULT " << castTook << " ms after SM_CASTSPELL, " << *bolt.mpCost << " MP charged "
		          << millisBetween(casts[0].at, costs[0].at) << " ms into the cast; " << (missed ? "resisted" : "landed") << std::endl;
	});

	// ---- S10: monster A fights back until it has hit the Mage, then dies to more Flame Bolts (the setup of X8, not an assertion) ----
	runCase("S10", "monster A hits the Mage and dies to Flame Bolts", [&] {
		const size_t from = a.game->recorded().size();
		// monster A was hit in S9 (or resisted it) and hates the Mage; wait until it has damaged the Mage once, so X8 has HP to heal
		const std::optional<size_t> hit = readUntil(
		  *a.game,
		  [&](const Packet& packet) {
			  if (packet.name != "SM_ATTACK_STATUS")
				  return false;
			  try {
				  const decoders::AttackStatusUpdate status = decoders::decodeAttackStatus(packet.data);
				  return status.creatureObjectId == a.mageId && status.type == decoders::ATTACK_STATUS_TYPE_REGULAR && status.value > 0;
			  } catch (const DecodeError&) {
				  return false;
			  }
		  },
		  30s);
		ASSERT_TRUE(hit) << "monster A never damaged the Mage; X8 has nothing to heal";
		for (int32_t casts = 0; casts < 15 && !recordSkills(*a.game, from).diedAt(monsterA); casts++) {
			cast(a.mageId, FLAME_BOLT, monsterA, std::chrono::milliseconds(*boltPointer->castDuration + 4000));
			collectFor(*a.game, 1500ms);
			ASSERT_FALSE(recordSkills(*a.game, from).diedAt(a.mageId)) << "monster A killed the Mage";
		}
		ASSERT_TRUE(recordSkills(*a.game, from).diedAt(monsterA)) << "fifteen Flame Bolts did not kill monster A";
		collectFor(*a.game, 1s);
	});

	// ---- S11: the heal (X8) ----
	runCase("S11", "Healing Light raises the Mage's HP, never past its maximum (X8)", [&] {
		const OracleSkillTemplate& heal = mageSkills.skill(HEALING_LIGHT);
		// the HP before the heal, from the last absolute value the Mage was sent (SM_STATUPDATE_HP carries it, SM_ATTACK_STATUS a percentage)
		const SkillRecording before = recordSkills(*a.game, 0);
		ASSERT_FALSE(before.hpUpdates.empty());
		const int32_t hpBefore = before.hpUpdates.back().value.currentHp;
		const int32_t maxHp = before.hpUpdates.back().value.maxHp;
		ASSERT_LT(hpBefore, maxHp) << "X8: the Mage is at full HP, so a heal has nothing to show";
		// TARGETORME with no target heals the caster (FirstTargetProperty.java:26-31)
		a.game->send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(0));
		collectFor(*a.game, 500ms);
		const GameSession::CastOutcome outcome = cast(a.mageId, HEALING_LIGHT, 0, std::chrono::milliseconds(heal.castDuration.value_or(0) + 4000));
		collectFor(*a.game, 1500ms);
		const SkillRecording recording = recordSkills(*a.game, outcome.firstPacket);
		EXPECT_TRUE(recording.decodeFailures.empty()) << join(recording.decodeFailures, "\n  ");
		const auto results = recording.resultsBy(a.mageId, HEALING_LIGHT);
		ASSERT_EQ(results.size(), 1u) << "X8: Healing Light did not complete";
		ASSERT_EQ(results[0].value.effects.size(), 1u);
		EXPECT_EQ(results[0].value.effects[0].effectedObjectId, a.mageId) << "X8: TARGETORME without a target heals the caster";
		// AbstractHealEffect.applyEffect -> increaseHp(TYPE.REGULAR, heal) -> SM_ATTACK_STATUS(REGULAR, newHp - previousHp) with skill id 0
		// (CreatureLifeStats.java:164-193), then SM_STATUPDATE_HP with the new absolute value
		std::optional<SkillRecording::At<decoders::AttackStatusUpdate>> healed;
		for (const auto& status : recording.statusesOf(a.mageId))
			if (status.value.type == decoders::ATTACK_STATUS_TYPE_REGULAR && status.index > results[0].index && !healed)
				healed = status;
		ASSERT_TRUE(healed) << "X8: no SM_ATTACK_STATUS for the heal";
		// not the sign: a REGULAR SM_ATTACK_STATUS carries `newHp - previousHp` for a heal and `previousHp - newHp` for damage, both written as they
		// are (CreatureLifeStats.java:110/191, SM_ATTACK_STATUS.java default arm), so a heal negated into reduceHp looks the same here. The
		// SM_STATUPDATE_HP relation below is what catches that (the X8 mutant of 2026-09-24: 127 found, 5 "healed", 122 after)
		EXPECT_GT(healed->value.value, 0) << "X8: the heal changed nothing";
		const auto hpAfter = std::ranges::find_if(recording.hpUpdates, [&](const auto& hp) { return hp.index > healed->index; });
		ASSERT_NE(hpAfter, recording.hpUpdates.end()) << "X8: no SM_STATUPDATE_HP after the heal";
		// HpMpRestoreTask regenerates on its own thread (a NATURAL_HP SM_ATTACK_STATUS, then SM_STATUPDATE_HP), so a tick can land anywhere around
		// the heal - and an SM_STATUPDATE_HP carries the HP at the moment it is written. The relation is therefore the A6 reconstruction of the
		// M5b gate: the last absolute value before the heal, plus every HP-raising status up to the heal (the HP the heal found), plus the heal and
		// the ticks after it, must be the next absolute value. It reads the WHOLE recording, not the cast's window: a tick between `before` and the
		// cast's first packet (the 500 ms after CM_TARGET_SELECT, a refused attempt) is in neither, and a window that starts at the cast then
		// misses it. The clean run of 2026-09-24 measured exactly that - 123 found, 6 healed, 132 after, where the 6 was already Java's cap
		// `maxHp - currentHp` of a Mage at 126 (AbstractHealEffect.calculateHealValue).
		const SkillRecording whole = recordSkills(*a.game, 0);
		int32_t base = hpBefore;
		size_t baseIndex = 0;
		for (const auto& hp : whole.hpUpdates)
			if (hp.index < healed->index) {
				base = hp.value.currentHp;
				baseIndex = hp.index;
			}
		int32_t hpFound = base, ticksAfter = 0;
		for (const auto& status : whole.statusesOf(a.mageId)) {
			if (status.value.type != decoders::ATTACK_STATUS_TYPE_NATURAL_HP || status.index <= baseIndex)
				continue;
			if (status.index < healed->index)
				hpFound += status.value.value;
			else if (status.index < hpAfter->index)
				ticksAfter += status.value.value;
		}
		EXPECT_LE(healed->value.value, maxHp - hpFound) << "X8: the heal of " << healed->value.value << " found " << hpFound << " of " << maxHp
		                                                << " HP: an overheal past the maximum";
		EXPECT_EQ(hpAfter->value.currentHp, hpFound + healed->value.value + ticksAfter)
		  << "X8: SM_STATUPDATE_HP after the heal (" << hpFound << " HP found, " << healed->value.value << " healed, " << ticksAfter << " regenerated)";
		EXPECT_LE(hpAfter->value.currentHp, hpAfter->value.maxHp);
		std::cout << "X8: Healing Light raised the Mage from " << hpFound << " by " << healed->value.value << " to " << hpAfter->value.currentHp << " of "
		          << hpAfter->value.maxHp << std::endl;
	});

	// ---- S12: the debuff with a lifetime (X6), the two-template self-buff (X7) and the cooldown refusal on the Mage ----
	std::optional<std::chrono::steady_clock::time_point> rootCastAt;
	runCase("S12", "Root on an untouched monster for its whole lifetime, Focused Evasion on the Mage (X6, X7)", [&] {
		const OracleSkillTemplate& root = mageSkills.skill(ROOT);
		const OracleSkillTemplate& evasion = mageSkills.skill(FOCUSED_EVASION);
		const std::array<float, 3> standOff = pointNear(*spotB, CAST_DISTANCE, atX, atY);
		walkTo(standOff[0], standOff[1], standOff[2]);
		if (monsterB == 0)
			monsterB = waitForNpcAt(*spotB, std::nullopt, 15s).value_or(0);
		ASSERT_NE(monsterB, 0) << "no SM_NPC_INFO for npc " << GATE_MONSTER_NPC_ID << " at spot B";
		a.game->send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(monsterB));
		waitFor(*a.game, "SM_TARGET_SELECTED", 10s);
		collectFor(*a.game, 1s);

		const GameSession::CastOutcome outcome = cast(a.mageId, ROOT, monsterB, 5s);
		ASSERT_TRUE(outcome.castSpellResult) << "X6: Root did not complete";
		rootCastAt = a.game->recorded()[*outcome.castSpellResult].receivedAt;
		const size_t windowFrom = outcome.firstPacket;

		// X7 inside the root's lifetime: a self-buff touches nobody but the Mage, so the silence towards the monster holds
		collectFor(*a.game, 1500ms);
		const GameSession::CastOutcome buff = cast(a.mageId, FOCUSED_EVASION, a.mageId, 5s);
		ASSERT_TRUE(buff.castSpellResult) << "X7: Focused Evasion did not complete";
		// the Mage's own cooldown row: Root again, well inside its cooldown, is refused by Player.isSkillDisabled with STR_SKILL_NOT_READY
		collectFor(*a.game, 1500ms);
		const size_t early = a.game->recorded().size();
		GameSession::CastRequest again;
		again.spellId = ROOT;
		again.targetObjectId = monsterB;
		a.game->send(GameSession::CM_CASTSPELL, GameSession::buildCM_CASTSPELL(again));
		collectFor(*a.game, 800ms);
		const SkillRecording refused = recordSkills(*a.game, early);
		EXPECT_TRUE(refused.castsBy(a.mageId, ROOT).empty()) << "cooldown: Root was cast again inside its " << root.cooldownMillis << " ms cooldown";
		EXPECT_TRUE(refused.hasMessage(STR_SKILL_NOT_READY)) << "cooldown: Player.isSkillDisabled refuses with STR_SKILL_NOT_READY";

		// the silence: nothing more is sent at the monster until the root has ended, plus a margin for the monster to start its chase
		const auto end = *rootCastAt + std::chrono::milliseconds(*root.effectDuration) + 6s;
		while (std::chrono::steady_clock::now() < end)
			collectFor(*a.game, 1s);
		const SkillRecording recording = recordSkills(*a.game, windowFrom);
		EXPECT_TRUE(recording.decodeFailures.empty()) << join(recording.decodeFailures, "\n  ");

		// ---- X6 ----
		const auto results = recording.resultsBy(a.mageId, ROOT);
		ASSERT_EQ(results.size(), 1u);
		EXPECT_EQ(results[0].value.cooldown, root.cooldown) << "the cooldown row: the result carries the template's cooldown";
		ASSERT_EQ(results[0].value.effects.size(), 1u);
		ASSERT_FALSE(dodgedOrResisted(results[0].value.effects[0]))
		  << "X6: Root was resisted (accmod2 500 makes that all but impossible for a level-2 monster); nothing below can be asserted";
		EXPECT_EQ(results[0].value.chainStatus, decoders::CAST_RESULT_CHAIN_SUCCESS)
		  << "a skill without a chain category keeps chainSuccess's initial true (Skill.java:73), so a non-empty result is 32";
		EXPECT_EQ(results[0].value.effects[0].successfulEffects, successfulEffectsByte(root));
		// the monster's effects reach the Mage as SM_ABNORMAL_EFFECT (EffectController.broadCastEffects, EffectController.java:304-308), with
		// effectType 1 for a non-player; the first one after the result carries the root, written after Effect.startEffect set the duration
		const auto effects = recording.abnormalEffectsOf(monsterB);
		std::optional<SkillRecording::At<decoders::AbnormalEffect>> started, ended;
		for (const auto& packet : effects) {
			const bool carries = entryOf(packet.value.effects, ROOT).has_value();
			if (carries && !started)
				started = packet;
			else if (!carries && started && !ended)
				ended = packet;
		}
		ASSERT_TRUE(started) << "X6: no SM_ABNORMAL_EFFECT for monster B carries skill " << ROOT;
		const decoders::AbnormalEntry entry = *entryOf(started->value.effects, ROOT);
		EXPECT_EQ(started->value.effectType, decoders::ABNORMAL_EFFECT_TYPE_CREATURE);
		EXPECT_EQ(entry.targetSlotOrdinal, root.targetSlot->ordinal) << "X6: the DEBUFF slot";
		EXPECT_EQ(entry.skillLevel, 1);
		EXPECT_LE(entry.remainingTimeToDisplay, *root.effectDuration) << "X6: remaining time above the template's duration2 + duration1 x level";
		EXPECT_GE(entry.remainingTimeToDisplay, *root.effectDuration - 1000)
		  << "X6: remaining time " << entry.remainingTimeToDisplay << " right after the start, and the duration is " << *root.effectDuration
		  << " (duration2 + duration1 x skillLevel, Effect.calculateTemplateDuration)";
		ASSERT_TRUE(ended) << "X6: no SM_ABNORMAL_EFFECT without the root arrived - the effect never ended (no end task)";
		const int64_t lifetime = millisBetween(started->at, ended->at);
		EXPECT_NEAR(static_cast<double>(lifetime), static_cast<double>(*root.effectDuration), 1000.0)
		  << "X6: the root lasted " << lifetime << " ms on the wire, and its duration is " << *root.effectDuration << " ms";
		// the root roots: the monster hates the Mage from the moment the effect started (Effect.startEffect broadcasts the hate AFTER the
		// templates started, Effect.java:661-667) and cannot walk to it. NpcMoveController.moveToTargetObject only marks the move as started, and
		// the move task's moveToDestination then finds canPerformMove refusing and answers with a STOP move - movement mask IMMEDIATE, the npc
		// where it stands (NpcMoveController.java:71-80, 134-141). §10.3's "no SM_MOVE" is therefore not Java; what Java promises is that every
		// SM_MOVE of the rooted monster is such a stop at its spot, and that the first one after the root is a START move (POSITION | MANUAL,
		// NPC_STARTMOVE) - the half that proves the monster WANTED to move, so the rooted half is not vacuous.
		std::vector<std::string> movesWhileRooted;
		size_t stopsWhileRooted = 0;
		std::optional<std::pair<int64_t, decoders::NpcMove>> firstStartAfter;
		const std::vector<Packet>& all = a.game->recorded();
		for (size_t i = started->index; i < all.size(); i++) {
			const Packet& packet = all[i];
			if (packet.name != "SM_MOVE" || decoders::decodeMoveObjectId(packet.data) != monsterB)
				continue;
			const decoders::NpcMove move = decoders::decodeNpcMove(packet.data);
			const int64_t into = millisBetween(started->at, packet.receivedAt);
			const bool rooted = i < ended->index;
			const bool starts = move.target.has_value();
			if (rooted) {
				if (starts || move.movementMask != decoders::MOVEMENT_MASK_IMMEDIATE || distance2d(move.x, move.y, spotB->x, spotB->y) > 0.5)
					movesWhileRooted.push_back("mask " + std::to_string(move.movementMask) + " at (" + std::to_string(move.x) + ", " + std::to_string(move.y) +
					                           ") " + std::to_string(into) + " ms into the root");
				else
					stopsWhileRooted++;
			} else if (starts && !firstStartAfter) {
				firstStartAfter = std::pair{millisBetween(ended->at, packet.receivedAt), move};
			}
		}
		EXPECT_TRUE(movesWhileRooted.empty()) << "X6: the rooted monster moved: " << join(movesWhileRooted)
		                                      << " (RootEffect.startEffect's setAbnormal(ROOT) did not stop it)";
		EXPECT_TRUE(firstStartAfter) << "X6: the monster never started a move after the root ended, so the rooted rows prove nothing about the root";
		std::cout << "X6: the root lasted " << lifetime << " ms; the monster sent " << stopsWhileRooted << " stop move(s) at its spot while rooted and "
		          << (firstStartAfter ? "started its chase " + std::to_string(firstStartAfter->first) + " ms after the root ended" : std::string("never moved after"))
		          << std::endl;

		// ---- X7 ----
		const auto buffResults = recording.resultsBy(a.mageId, FOCUSED_EVASION);
		ASSERT_EQ(buffResults.size(), 1u);
		ASSERT_EQ(buffResults[0].value.effects.size(), 1u);
		EXPECT_EQ(buffResults[0].value.effects[0].effectedObjectId, a.mageId);
		// the two templates of the skill are ONE Effect (Skill.endCast makes one Effect per effected creature, Skill.java:584-585) whose success
		// byte has a bit per successful position (Effect.getSuccessfulEffectsAsByte: 1 << (position + 3)); the second template's preeffect="1"
		// is what a broken preeffect chain drops
		EXPECT_EQ(buffResults[0].value.effects[0].successfulEffects, successfulEffectsByte(evasion))
		  << "X7: not every template of the self-buff succeeded (the preeffect chain)";
		std::optional<SkillRecording::At<decoders::AbnormalState>> buffOn, buffOff;
		for (const auto& state : recording.abnormalStates) {
			const bool carries = entryOf(state.value.effects, FOCUSED_EVASION).has_value();
			if (carries && !buffOn)
				buffOn = state;
			else if (!carries && buffOn && !buffOff)
				buffOff = state;
		}
		ASSERT_TRUE(buffOn) << "X7: no SM_ABNORMAL_STATE carries the self-buff (PlayerEffectController.updatePlayerEffectIcons)";
		const decoders::AbnormalEntry buffEntry = *entryOf(buffOn->value.effects, FOCUSED_EVASION);
		// SM_ABNORMAL_STATE writes one entry per Effect (SM_ABNORMAL_STATE.java:32-38), so the two templates are one icon
		EXPECT_EQ(std::ranges::count_if(buffOn->value.effects, [](const decoders::AbnormalEntry& e) { return e.skillId == FOCUSED_EVASION; }), 1)
		  << "X7: one Effect, one entry";
		EXPECT_EQ(buffEntry.effectorObjectId, a.mageId);
		EXPECT_EQ(buffEntry.targetSlotOrdinal, evasion.targetSlot->ordinal) << "X7: the BUFF slot";
		EXPECT_LE(buffEntry.remainingTimeToDisplay, *evasion.effectDuration);
		EXPECT_GE(buffEntry.remainingTimeToDisplay, *evasion.effectDuration - 1000);
		ASSERT_TRUE(buffOff) << "X7: the self-buff never ended";
		EXPECT_NEAR(static_cast<double>(millisBetween(buffOn->at, buffOff->at)), static_cast<double>(*evasion.effectDuration), 1000.0)
		  << "X7: the self-buff's lifetime";
	});

	// ---- S13: quit inside Root's cooldown, the cooldown row in the database, re-enter with 1 HP and the cooldown back (the cooldown rows) ----
	runCase("S13", "Root's cooldown survives a quit (player_cooldowns, SM_SKILL_COOLDOWN)", [&] {
		const OracleSkillTemplate& root = mageSkills.skill(ROOT);
		ASSERT_TRUE(rootCastAt);
		a.game->send(GameSession::CM_QUIT, GameSession::buildCM_QUIT(true));
		waitFor(*a.game, "SM_QUIT_RESPONSE", 30s);
		const int64_t leftAtQuit = root.cooldownMillis - millisBetween(*rootCastAt, std::chrono::steady_clock::now());
		ASSERT_GT(leftAtQuit, 28000) << "S12 took too long: PlayerCooldownsDAO only stores a cooldown with more than 28 s left";
		// PlayerLeaveWorldService -> PlayerCooldownsDAO.storePlayerCooldowns: one row per cooldown id with more than 28 s left
		const std::string mage = std::to_string(a.mageId);
		EXPECT_EQ(database.queryLong(schema, "SELECT COUNT(*) FROM player_cooldowns WHERE player_id = " + mage + " AND cooldown_id = " +
		                                       std::to_string(root.cooldownId)),
		          1)
		  << "the cooldown row: Root's cooldown id " << root.cooldownId << " was not stored at the quit";

		// D12's seed, for S14's death: 1 HP
		database.execute(schema, "UPDATE player_life_stats SET hp = 1 WHERE player_id = " + mage);
		std::this_thread::sleep_for(1500ms);
		std::vector<OracleSkill> expected = mageSkills.characterSkills;
		for (const uint16_t seeded : {HEALING_LIGHT, FOCUSED_EVASION})
			expected.push_back({seeded, mageSkills.skill(seeded).level});
		std::vector<Packet> burst;
		const decoders::StatsInfo stats = enterWorld(a.mageId, false, mageCreation, expected, &burst);
		EXPECT_EQ(stats.currentHp, 1) << "D12: the seeded 1 HP was not restored";
		// PlayerEnterWorldService sends SM_SKILL_COOLDOWN with every stored cooldown of a skill in the list (SM_SKILL_COOLDOWN.java:25-35)
		const std::vector<Packet> cooldownPackets = ofName(burst, "SM_SKILL_COOLDOWN");
		ASSERT_EQ(cooldownPackets.size(), 1u) << "the cooldown row: no SM_SKILL_COOLDOWN in the enter-world burst";
		const decoders::SkillCooldown cooldown = decoders::decodeSkillCooldown(cooldownPackets[0].data);
		const auto rootCooldown = std::ranges::find(cooldown.cooldowns, ROOT, &decoders::SkillCooldownEntry::skillId);
		ASSERT_NE(rootCooldown, cooldown.cooldowns.end()) << "the cooldown row: Root is not in SM_SKILL_COOLDOWN";
		EXPECT_EQ(rootCooldown->durationMillis, root.cooldownMillis) << "the cooldown row: the template's cooldown x 100";
		const int64_t leftNow = (root.cooldownMillis - millisBetween(*rootCastAt, std::chrono::steady_clock::now())) / 1000;
		EXPECT_GT(rootCooldown->remainingSeconds, 0);
		EXPECT_NEAR(static_cast<double>(rootCooldown->remainingSeconds), static_cast<double>(leftNow), 2.0)
		  << "the cooldown row: the stored expiration is absolute (reuse_delay), not re-based on the load";
		levelReady(nullptr, false); // the Mage re-enters where it quit, next to monster B, which still hates it
	});

	// ---- S14: the Mage dies to monster B (the setup of X10, not an assertion) ----
	runCase("S14", "the Mage (1 HP) dies to monster B", [&] {
		// the Mage is where it quit, CAST_DISTANCE from spot B. When B still hates it, its chase ends the 1 HP inside S13's burst; when the logout
		// made B give up, nothing touches the Mage, and a pull is needed: a Flame Bolt, or - if that cast does not go out - an auto-attack, which a
		// spellbook reaches from its 15 m (stage 0). The pull is repeated while B has not swung at the Mage, and every step is kept for the message.
		const size_t from = a.game->recorded().size();
		const auto died = [&] { return recordSkills(*a.game, 0).diedAt(a.mageId); };
		const auto swungSince = [&](size_t since) {
			for (const auto& status : recordSkills(*a.game, since).statusesOf(a.mageId))
				if (status.value.type == decoders::ATTACK_STATUS_TYPE_REGULAR)
					return true;
			return false;
		};
		std::vector<std::string> steps;
		collectFor(*a.game, 3s);
		const auto deadline = std::chrono::steady_clock::now() + 75s;
		while (!died() && std::chrono::steady_clock::now() < deadline) {
			const size_t pullFrom = a.game->recorded().size();
			if (!swungSince(from)) {
				a.game->send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(monsterB));
				collectFor(*a.game, 500ms);
				const GameSession::CastOutcome pull = cast(a.mageId, FLAME_BOLT, monsterB, std::chrono::milliseconds(*boltPointer->castDuration + 4000));
				std::string refusal;
				if (!pull.castSpell)
					for (const auto& message : recordSkills(*a.game, pullFrom).systemMessages)
						refusal += (refusal.empty() ? " (system messages " : ", ") + std::to_string(message.value);
				steps.push_back(std::string("Flame Bolt ") + (pull.castSpellResult ? "completed" : pull.skillCancel ? "cancelled" : "not cast") +
				                (refusal.empty() ? "" : refusal + ")"));
				if (!pull.castSpell) {
					a.game->send(GameSession::CM_ATTACK, GameSession::buildCM_ATTACK(monsterB));
					steps.push_back("CM_ATTACK");
				}
			}
			collectFor(*a.game, 8s);
			if (!died())
				steps.push_back(swungSince(pullFrom) ? "B swung, the Mage lives" : "B did not swing in 8 s");
		}
		// what monster B did meanwhile, for the message only (a diagnosis of the one S14 failure of 2026-09-24, m5b2-plan.md §10.8)
		const auto monsterBSummary = [&] {
			const SkillRecording seen = recordSkills(*a.game, from);
			int32_t attacksAtMage = 0, attacksElsewhere = 0, deletes = 0, infos = 0;
			std::optional<decoders::NpcMove> lastMove;
			for (size_t i = from; i < a.game->recorded().size(); i++) {
				const Packet& packet = a.game->recorded()[i];
				try {
					if (packet.name == "SM_ATTACK") {
						const decoders::Attack attack = decoders::decodeAttack(packet.data);
						if (attack.attackerObjectId == monsterB)
							(attack.targetObjectId == a.mageId ? attacksAtMage : attacksElsewhere)++;
					} else if (packet.name == "SM_MOVE" && decoders::decodeMoveObjectId(packet.data) == monsterB) {
						lastMove = decoders::decodeNpcMove(packet.data);
					} else if (packet.name == "SM_DELETE" && decoders::decodeDeleteObjectId(packet.data) == monsterB) {
						deletes++;
					} else if (packet.name == "SM_NPC_INFO" && decoders::decodeNpcInfoObjectId(packet.data) == monsterB) {
						infos++;
					}
				} catch (const DecodeError&) {
					// a body that does not decode says nothing about B
				}
			}
			return "monster B (object " + std::to_string(monsterB) + "): " + std::to_string(attacksAtMage) + " SM_ATTACK at the Mage, " +
			       std::to_string(attacksElsewhere) + " elsewhere, " + std::to_string(seen.movesOf(monsterB).size()) + " SM_MOVE" +
			       (lastMove ? " (the last mask " + std::to_string(lastMove->movementMask) + " at " + std::to_string(lastMove->x) + ", " +
			                     std::to_string(lastMove->y) + ", " + std::to_string(distance2d(lastMove->x, lastMove->y, spotB->x, spotB->y)) +
			                     " m from spot B)"
			                 : std::string()) +
			       ", " + std::to_string(seen.statusesOf(monsterB).size()) + " SM_ATTACK_STATUS, " + (seen.diedAt(monsterB) ? "died" : "did not die") +
			       ", " + std::to_string(deletes) + " SM_DELETE, " + std::to_string(infos) + " SM_NPC_INFO";
		};
		ASSERT_TRUE(died()) << "monster B did not kill the 1-HP Mage within 75 s: " << join(steps, "; ") << "; " << monsterBSummary()
		                    << "; the last packets were "
		                    << join(namesOf(std::vector<Packet>(a.game->recorded().end() -
		                                                           std::min<std::ptrdiff_t>(40, static_cast<std::ptrdiff_t>(a.game->recorded().size() - from)),
		                                                         a.game->recorded().end())));
		// SM_DIE follows the death emotion about 500 ms later (PlayerController.scheduleShowResurrectionOptions), unless it already arrived
		const SkillRecording all = recordSkills(*a.game, 0);
		size_t deathIndex = 0;
		for (const auto& emotion : all.emotions)
			if (emotion.value.emotionType == decoders::EMOTION_DIE && emotion.value.senderObjectId == a.mageId)
				deathIndex = emotion.index;
		bool dieSent = false;
		for (size_t i = deathIndex; i < a.game->recorded().size(); i++)
			dieSent = dieSent || a.game->recorded()[i].name == "SM_DIE";
		if (!dieSent)
			waitFor(*a.game, "SM_DIE", 20s);
		collectFor(*a.game, 500ms);
	});

	// ---- S15: the revive debuff (X10) ----
	std::optional<int32_t> sicknessLeftAtRevive;
	std::optional<std::chrono::steady_clock::time_point> revivedAt;
	int32_t sickMaxHp = 0, sickMaxMp = 0;
	runCase("S15", "CM_REVIVE(BIND_REVIVE) brings Soul Sickness 8291 and its stat penalty (X10)", [&] {
		const OracleSkillTemplate& sickness = mageSkills.skill(SOUL_SICKNESS);
		// the -30 % of MAXHP and MAXMP, read from the template's <change> elements
		std::optional<int32_t> hpPercent, mpPercent;
		for (const OracleSkillEffect& effect : sickness.effects)
			for (const OracleStatChange& change : effect.changes) {
				if (change.stat == "MAXHP" && change.func == "PERCENT")
					hpPercent = change.value;
				else if (change.stat == "MAXMP" && change.func == "PERCENT")
					mpPercent = change.value;
			}
		ASSERT_TRUE(hpPercent && mpPercent) << "the oracle's 8291 carries no MAXHP/MAXMP PERCENT change";

		const size_t reviveFrom = a.game->recorded().size();
		a.game->send(GameSession::CM_REVIVE, GameSession::buildCM_REVIVE(GameSession::BIND_REVIVE));
		const std::vector<Packet> revive = collectBurst(*a.game, async, 1500ms, 30s);
		revivedAt = std::chrono::steady_clock::now();
		const SkillRecording recording = recordSkills(*a.game, reviveFrom);
		EXPECT_TRUE(recording.decodeFailures.empty()) << join(recording.decodeFailures, "\n  ");

		// the debuff: PlayerReviveService.bindRevive -> revive(player, 25, 25, true, 0) -> updateSoulSickness -> SkillEngine.getSkill(player,
		// 8291, deathCount, player).useSkill() (PlayerReviveService.java:189-213, PlayerController.java:712-733). A PROVOKED skill sends no
		// SM_CASTSPELL and no result; the effect reaches the Mage as its own SM_ABNORMAL_STATE
		std::optional<decoders::AbnormalEntry> entry;
		for (const auto& state : recording.abnormalStates)
			if (!entry)
				entry = entryOf(state.value.effects, SOUL_SICKNESS);
		ASSERT_TRUE(entry) << "X10: no SM_ABNORMAL_STATE after the revive carries skill " << SOUL_SICKNESS
		                   << " - PlayerController.updateSoulSickness did not cast it (its hasPermission(DISABLE_SOULSICKNESS) guard, SkillEngine.getSkill, "
		                      "the PROVOKED useSkill), or the profile disables the soul sickness; the burst was "
		                   << join(namesOf(revive));
		EXPECT_EQ(entry->skillLevel, mageSkills.deathCount) << "X10: the soul sickness is cast at the death count, the first death being 1";
		EXPECT_EQ(entry->targetSlotOrdinal, sickness.targetSlot->ordinal) << "X10: the SPEC2 slot";
		EXPECT_EQ(entry->effectorObjectId, a.mageId) << "X10: the Mage casts it on itself";
		EXPECT_LE(entry->remainingTimeToDisplay, *sickness.effectDuration);
		EXPECT_GE(entry->remainingTimeToDisplay, *sickness.effectDuration - 1500)
		  << "X10: duration2 + duration1 x deathCount = " << *sickness.effectDuration << " ms";
		sicknessLeftAtRevive = entry->remainingTimeToDisplay;

		// the stat penalty: every BufEffect <change func="PERCENT"> is a bonus StatRateFunction, so the CURRENT maxima fall and the bases stay
		// (withPercentBonus above); then checkMaxHPChanged / checkMaxMPChanged rescale the current values the revive had just set to 25 % of the
		// old maxima (setCurrentHpPercent runs before updateSoulSickness, PlayerReviveService.java:196-202): `(int) ((long) getMaxHp() * 25 / 100)`
		// of the CURRENT maximum (CreatureLifeStats.java:339-341), which for the Mage includes the robe's 47 bonus MP
		ASSERT_TRUE(mageCreation.maxHpCurrent && mageCreation.maxMpCurrent && mageCreation.maxHpBonus && mageCreation.maxMpBonus);
		const int32_t maxHpBefore = *mageCreation.maxHpCurrent, maxMpBefore = *mageCreation.maxMpCurrent;
		// the Mage's 132 HP fall to 92.4, below a player's MAXHP floor of 100, so StatCapUtil makes the current max exactly 100 (withPercentBonus)
		sickMaxHp = withPercentBonus(mageCreation.baseMaxHp, *mageCreation.maxHpBonus, *hpPercent, PLAYER_MAXHP_LOWER_CAP);
		sickMaxMp = withPercentBonus(mageCreation.baseMaxMp, *mageCreation.maxMpBonus, *mpPercent, PLAYER_MAXMP_LOWER_CAP);
		const int32_t revivedHp = static_cast<int32_t>(static_cast<int64_t>(maxHpBefore) * 25 / 100);
		const int32_t revivedMp = static_cast<int32_t>(static_cast<int64_t>(maxMpBefore) * 25 / 100);
		const int32_t expectedHp = rescaledToNewMax(revivedHp, maxHpBefore, sickMaxHp);
		const int32_t expectedMp = rescaledToNewMax(revivedMp, maxMpBefore, sickMaxMp);
		ASSERT_FALSE(recording.statsInfos.empty()) << "X10: no SM_STATS_INFO after the revive";
		std::vector<std::string> sequence;
		for (const auto& stats : recording.statsInfos)
			sequence.push_back(std::to_string(stats.value.currentHp) + "/" + std::to_string(stats.value.maxHp) + " HP " +
			                   std::to_string(stats.value.currentMp) + "/" + std::to_string(stats.value.maxMp) + " MP");
		const decoders::StatsInfo& last = recording.statsInfos.back().value;
		EXPECT_EQ(last.maxHp, sickMaxHp) << "X10: the current max HP under the soul sickness (" << *hpPercent << " % of " << mageCreation.baseMaxHp
		                                 << " as a bonus); the SM_STATS_INFO of the burst were " << join(sequence, "; ");
		EXPECT_EQ(last.baseMaxHp, mageCreation.baseMaxHp) << "X10: a StatdownEffect that writes the base instead of the bonus";
		EXPECT_EQ(last.maxMp, sickMaxMp) << "X10: the current max MP under the soul sickness";
		EXPECT_EQ(last.baseMaxMp, mageCreation.baseMaxMp);
		const auto both = std::ranges::find_if(recording.statsInfos, [&](const auto& s) { return s.value.maxHp == sickMaxHp && s.value.maxMp == sickMaxMp; });
		ASSERT_NE(both, recording.statsInfos.end()) << "X10: no SM_STATS_INFO shows both penalties; they were " << join(sequence, "; ");
		EXPECT_EQ(both->value.currentHp, expectedHp) << "X10: 25 % of " << maxHpBefore << " (" << revivedHp << ") rescaled to the new maximum "
		                                             << sickMaxHp << " by checkMaxHPChanged; the SM_STATS_INFO were " << join(sequence, "; ");
		EXPECT_EQ(both->value.currentMp, expectedMp) << "X10: 25 % of " << maxMpBefore << " (" << revivedMp << ") rescaled to " << sickMaxMp
		                                             << " by checkMaxMPChanged (Math.round of the exact float product, half up)";
		std::cout << "X10: soul sickness " << entry->remainingTimeToDisplay << " ms left; SM_STATS_INFO of the revive: " << join(sequence, "; ")
		          << "; expected " << expectedHp << "/" << sickMaxHp << " HP, " << expectedMp << "/" << sickMaxMp << " MP" << std::endl;
	});

	// ---- S16: the saved effect round trip (X11) ----
	runCase("S16", "the soul sickness survives a quit (player_effects) and comes back with less time (X11)", [&] {
		ASSERT_TRUE(sicknessLeftAtRevive && revivedAt);
		a.game->send(GameSession::CM_QUIT, GameSession::buildCM_QUIT(true));
		waitFor(*a.game, "SM_QUIT_RESPONSE", 30s);
		// PlayerEffectsDAO.storePlayerEffects: every effect with canSaveOnLogout() and more than 28 s left (PlayerEffectsDAO.java:60-80)
		const std::vector<std::vector<std::optional<std::string>>> rows = database.queryRows(
		  schema, "SELECT skill_id, skill_lvl, remaining_time FROM player_effects WHERE player_id = " + std::to_string(a.mageId), 3);
		ASSERT_EQ(rows.size(), 1u) << "X11: player_effects holds " << rows.size() << " rows for the Mage, and exactly the soul sickness is saved";
		EXPECT_EQ(rows[0][0].value_or(""), std::to_string(SOUL_SICKNESS));
		EXPECT_EQ(rows[0][1].value_or(""), std::to_string(mageSkills.deathCount));
		const int32_t stored = std::stoi(rows[0][2].value_or("0"));
		EXPECT_GT(stored, 28000);
		EXPECT_LT(stored, *sicknessLeftAtRevive) << "X11: the stored remaining time is not less than at the revive";

		std::this_thread::sleep_for(1500ms);
		std::vector<OracleSkill> expected = mageSkills.characterSkills;
		for (const uint16_t seeded : {HEALING_LIGHT, FOCUSED_EVASION})
			expected.push_back({seeded, mageSkills.skill(seeded).level});
		std::vector<Packet> enterBurst;
		const decoders::StatsInfo stats = enterWorld(a.mageId, false, mageCreation, expected, &enterBurst);
		// PlayerEffectController.addSavedEffect restarts the effect with its stat functions, so the maxima come back penalised
		EXPECT_EQ(stats.maxHp, sickMaxHp) << "X11: the restored soul sickness did not bring its MAXHP penalty back";
		EXPECT_EQ(stats.baseMaxHp, mageCreation.baseMaxHp);
		std::vector<Packet> readyBurst;
		levelReady(&readyBurst, false); // the revived Mage is below its maxima, so HpMpRestoreTask ticks inside the burst
		std::optional<decoders::AbnormalEntry> entry;
		for (const Packet& packet : ofName(readyBurst, "SM_ABNORMAL_STATE"))
			if (!entry)
				entry = entryOf(decoders::decodeAbnormalState(packet.data).effects, SOUL_SICKNESS);
		ASSERT_TRUE(entry) << "X11: the level-ready SM_ABNORMAL_STATE does not carry the restored soul sickness (addSavedEffect)";
		EXPECT_GT(entry->remainingTimeToDisplay, 0);
		EXPECT_LE(entry->remainingTimeToDisplay, stored) << "X11: the remaining time grew over the quit - an endTime re-based on the load";
		std::cout << "X11: " << *sicknessLeftAtRevive << " ms left at the revive, " << stored << " ms stored, " << entry->remainingTimeToDisplay
		          << " ms after the re-entry" << std::endl;
	});

	// ---- S17: the reports and the shutdown (X12, X13) ----
	std::optional<int32_t> gameServerExit;
	int64_t connectionsAtShutdown = 0;
	if (a.game && !a.game->client.socket.isClosed())
		connectionsAtShutdown = 1;
	gameServerExit = servers.stopGameServer(); // with the Mage online, as the M5a gate's case 7 does
	if (a.game)
		a.game->waitClosed(60s);
	const std::optional<int32_t> loginServerExit = servers.stopLoginServer();

	cases.run("S17", "reports: the Q8 bar, the allow-list and the G-07 relations (X12, X13)", [&] {
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
		  << "X12: the game server wrote no check output in " << servers.checkOutputDir();

		// ---- X12: the M5a Q8 bar ----
		EXPECT_TRUE(servers.readReportLines("unported_trace.txt").empty())
		  << "X12: AION_UNPORTED sites were reached - a skill outside §2.4's subset, or an engine body the scripted path needs:\n"
		  << join(servers.readReportLines("unported_trace.txt"), "\n");
		const std::vector<AllowlistEntry> allowlist = readAllowlist();
		ASSERT_FALSE(allowlist.empty()) << "X12: tests/scenario/m5b2_partial_allowlist.txt is empty or missing";
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
			EXPECT_TRUE(allowed) << "X12: the AION_PARTIAL site " << hit.site << " is not in tests/scenario/m5b2_partial_allowlist.txt (" << hit.line << ")";
		}
		for (const AllowlistEntry& entry : allowlist) {
			if (entry.section == AllowlistSection::HitAtLeastOnce)
				EXPECT_GT(hitsByEntry[entry.site], 0) << "X12: the section A row " << entry.site << " was never hit";
			else if (entry.section == AllowlistSection::HitNever)
				EXPECT_EQ(hitsByEntry[entry.site], 0) << "X12: the section B row " << entry.site << " was hit " << hitsByEntry[entry.site] << " times";
		}
		std::cout << "X12: AION_PARTIAL hits by allow-list row (hits, section, site):\n";
		for (const AllowlistEntry& entry : allowlist)
			std::cout << "  " << hitsByEntry[entry.site] << "\t" << sectionName(entry.section) << "\t" << entry.site << "\n";
		std::cout << std::flush;

		const std::vector<std::string> census = servers.readReportLines("census.txt");
		EXPECT_TRUE(census.empty()) << "X12: the final census reports leaks:\n" << join(census, "\n");
		EXPECT_TRUE(servers.readReportLines("lockdep.txt").empty()) << "X12: the lock order validator reported:\n"
		                                                            << join(servers.readReportLines("lockdep.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("watchdog.txt").empty()) << "X12: the watchdog dumped:\n"
		                                                             << join(servers.readReportLines("watchdog.txt"), "\n");
		const std::map<std::string, std::vector<std::string>> summary = servers.readSummary();
		const auto value = [&](std::string_view key) -> std::string {
			const auto found = summary.find(std::string(key));
			return found == summary.end() || found->second.empty() ? std::string() : found->second[0];
		};
		EXPECT_EQ(value("started"), "true");
		EXPECT_EQ(value("exitCode"), "0");
		EXPECT_EQ(value("knownListNotifyFailures"), "0");
		EXPECT_EQ(value("liveCountsEnabled"), "true") << "X12/X13: a release build counts nothing: build it checked";
		EXPECT_EQ(value("zombieCuts"), "0");
		const auto notPorted = summary.find("notPortedClientPacket");
		if (notPorted != summary.end())
			ADD_FAILURE() << "X12: the scripted path sent client packets that are not ported: " << join(notPorted->second);
		std::vector<std::string> errors;
		ASSERT_NE(servers.gameServer(), nullptr);
		for (const std::string& line : servers.gameServer()->findLogLines(" ERROR "))
			errors.push_back("game server: " + line);
		if (servers.loginServer() != nullptr)
			for (const std::string& line : servers.loginServer()->findLogLines(" ERROR "))
				errors.push_back("login server: " + line);
		EXPECT_TRUE(errors.empty()) << "X12: ERROR lines in the server logs (CreatureController::useSkill and NpcController::onDie swallow into "
		                               "ERROR lines):\n"
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
			EXPECT_TRUE(fileErrors.empty()) << "X12: " << errorLog << " is not empty:\n" << join(fileErrors, "\n");
		} else {
			ADD_FAILURE() << "X12: the game server wrote no " << errorLog;
		}
		// G-04: FirstTargetRangeProperty refuses a first target it cannot see with STR_SKILL_OBSTACLE (FirstTargetRangeProperty.java:67-72).
		// Every target of this script stands in the open, so the message must never come - in the geo run the only one where canSee can
		// answer false. The cast rows above fail on such a refusal too; this row names the cause.
		size_t obstacles = 0;
		if (a.game)
			for (const Packet& packet : a.game->recorded())
				if (packet.name == "SM_SYSTEM_MESSAGE" && decoders::decodeSystemMessageId(packet.data) == STR_SKILL_OBSTACLE)
					obstacles++;
		EXPECT_EQ(obstacles, 0u) << "G-04: " << obstacles << " cast(s) at a target in the open were refused with STR_SKILL_OBSTACLE"
		                         << (variant.geodata ? " - GeoService::canSee answers false for an unobstructed line of sight" : "");
		EXPECT_TRUE(servers.gameServer()->findLogLines("did not leave world cleanly", 5).empty());
		EXPECT_TRUE(servers.gameServer()->findLogLines("stale pin", 5).empty());
		EXPECT_TRUE(servers.gameServer()->findLogLines("objects removed from the world are still alive", 5).empty())
		  << "X12: m5b-client-session.md S-2: " << join(servers.gameServer()->findLogLines("objects removed from the world are still alive", 5), "\n");

		// ---- X13: the skill classes against their G-07 relations (CheckOutput.cpp heldEffects) ----
		// m5b2-plan.md §10.3 X13 asked for 0 live Effect, EffectReserved and Skill. That premise is wrong since part 3: the post-spawn statup
		// buffs keep their Effects and Skills alive with their npcs at every shutdown (309 of each in the gate runs of 2026-09-24, the
		// StatFunctionProxy precedent of docs/deviations/P5-14.md). What a leak of THIS gate's casts breaks is the equality between what is
		// alive and what the creatures still in the world hold; the summary writes both sides.
		const auto liveCount = [&](std::string_view unqualified) -> std::optional<LiveCount> {
			const auto rows = summary.find("liveCount");
			if (rows == summary.end())
				return std::nullopt;
			for (const std::string& row : rows->second) {
				std::istringstream in(row);
				std::string qualified;
				LiveCount count;
				if (!(in >> qualified >> count.live >> count.created))
					continue;
				if (qualified != unqualified)
					continue;
				count.line = row;
				return count;
			}
			return std::nullopt;
		};
		const auto held = [&](std::string_view key) -> std::optional<int64_t> {
			try {
				return std::stoll(value(key));
			} catch (const std::exception&) {
				return std::nullopt;
			}
		};
		const std::optional<LiveCount> effect = liveCount("skillengine::model::Effect");
		const std::optional<LiveCount> reserved = liveCount("skillengine::model::EffectReserved");
		const std::optional<LiveCount> skill = liveCount("skillengine::model::Skill");
		const std::optional<LiveCount> listener = liveCount("controllers::observer::StartMovingListener");
		const std::optional<int64_t> effectsHeld = held("effectsHeld");
		const std::optional<int64_t> skillsHeld = held("skillsHeld");
		const std::optional<int64_t> reservedCapacity = held("effectReservedCapacity");
		ASSERT_TRUE(effect && reserved && skill && listener) << "X13: m5a_summary.txt lacks a liveCount row of the skill classes (CheckOutput G-07)";
		ASSERT_TRUE(effectsHeld && skillsHeld && reservedCapacity)
		  << "X13: m5a_summary.txt has no effectsHeld/skillsHeld/effectReservedCapacity number: '" << value("effectsHeld") << "', '"
		  << value("skillsHeld") << "', '" << value("effectReservedCapacity") << "'";
		EXPECT_EQ(effect->live, *effectsHeld)
		  << "X13: " << effect->line << " against " << *effectsHeld << " Effects held by the creatures of the world: an Effect that ended and is "
		  << "still retained - an observer its endEffect did not remove, a stat function it did not take back, a task it did not cancel";
		EXPECT_GT(effect->created, *effectsHeld) << "X13: the gate's own effects ended; created must exceed what is still held";
		EXPECT_EQ(skill->live, *skillsHeld) << "X13: " << skill->line << " against " << *skillsHeld << " Skills the held Effects and the casting "
		                                    << "creatures reference: a Skill kept past its cast";
		EXPECT_GT(skill->created, *skillsHeld);
		// Every Skill owns one StartMovingListener, so the two counts move together. The row is NOT a check of Skill.removeObservers: useSkill
		// attaches the listener for one notification (ObserveController.java:31-34), so one that removeObservers forgot is dropped at the caster's
		// next move, and a Player's ObserveController goes with the Player at logout. The review's mutant R3 (removeObservers without the
		// listener) passed this gate; SkillCastPhasesTest checks removeObservers after endCast and cancelCast.
		EXPECT_EQ(listener->live, skill->live) << "X13: " << listener->line << " against " << skill->line
		                                       << ": a StartMovingListener alive without its Skill, or a Skill without one";
		// The capacity counts the held Effects' templates of the nine classes that store an EffectReserved (CheckOutput.cpp storesReserved), so
		// it is 0 while the held Effects are the post-spawn statup buffs; a bound, because Effect.h has no accessor for what an Effect stores
		EXPECT_LE(reserved->live, *reservedCapacity) << "X13: " << reserved->line << " against a capacity of " << *reservedCapacity
		                                             << " (one per template of the held Effects whose class stores an EffectReserved): an "
		                                                "EffectReserved kept past its Effect";
		EXPECT_GT(reserved->created, 0) << "X13: the gate's damage and heal casts create EffectReserveds; none were created";
		for (const std::string_view observer : {"skillengine::effect::RootEffect_ActionObserver", "skillengine::effect::AlwaysDodgeEffect_AttackStatusObserver",
		                                        "skillengine::effect::AlwaysResistEffect_AttackStatusObserver"}) {
			const std::optional<LiveCount> count = liveCount(observer);
			ASSERT_TRUE(count) << "X13: no liveCount row for " << observer;
			EXPECT_GT(count->created, 0) << "X13: " << count->line << " - the gate's Root and Focused Evasion create one each";
			EXPECT_LE(count->live, *effectsHeld) << "X13: " << count->line << " - an effect observer outlives the effects that could hold it";
		}
		std::cout << "X13: " << effect->line << " / effectsHeld " << *effectsHeld << "; " << skill->line << " / skillsHeld " << *skillsHeld << "; "
		          << listener->line << "; " << reserved->line << " / capacity " << *reservedCapacity << std::endl;

		// ---- X12, the drop rows (m5b3-plan.md D4, G-05): what the allow-list's DropRegistrationService.cpp:43 row stood for until M5b-3 ----
		// The characters' kills are read from the recording: an npc's SM_EMOTION(DIE) names its last attacker (CreatureController.java:164-165),
		// and every kill of this script is made by one character alone (monster A in S5 and its respawn in S10, the kerub in S5b when the
		// Warrior finishes it), so that attacker is also doReward's most-damage winner, for whom registerDrop runs (NpcController.java:244-245).
		// registerDrop's last statement but one sends the winner SM_LOOT_STATUS(corpse, LOOT_ENABLE) (DropRegistrationService.java:104-106),
		// for an empty drop set too, with getLootEffect over that set: 0 at gameserver.rates.drop = 0. The free-for-all task finds no DropNpc
		// once the 2 s corpse despawned (DropService.java:54-70, 78-82), so each corpse gets exactly one.
		std::set<int32_t> kills;
		std::map<int32_t, int32_t> lootEnables;
		std::vector<std::string> otherLootStatuses;
		std::vector<std::string> dropDecodeFailures;
		if (a.game)
			for (const Packet& packet : a.game->recorded()) {
				try {
					if (packet.name == "SM_EMOTION") {
						const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
						if (emotion.emotionType == decoders::EMOTION_DIE && emotion.senderObjectId != a.warriorId &&
						    emotion.senderObjectId != a.mageId && (emotion.targetObjectId == a.warriorId || emotion.targetObjectId == a.mageId))
							kills.insert(emotion.senderObjectId);
					} else if (packet.name == "SM_LOOT_STATUS") {
						const decoders::LootStatus loot = decoders::decodeLootStatus(packet.data);
						if (loot.status == decoders::LOOT_STATUS_LOOT_ENABLE && loot.lootEffectId == 0)
							lootEnables[loot.targetObjectId]++;
						else
							otherLootStatuses.push_back("(target " + std::to_string(loot.targetObjectId) + ", status " +
							                            std::to_string(static_cast<int32_t>(loot.status)) + ", lootEffectId " +
							                            std::to_string(loot.lootEffectId) + ")");
					}
				} catch (const DecodeError& error) {
					dropDecodeFailures.push_back(packet.name + ": " + error.what());
				}
			}
		EXPECT_TRUE(dropDecodeFailures.empty()) << "X12: " << join(dropDecodeFailures, "\n  ");
		EXPECT_TRUE(otherLootStatuses.empty()) << "X12: SM_LOOT_STATUS other than LOOT_ENABLE with lootEffectId 0 (an empty drop set at rates.drop 0): "
		                                       << join(otherLootStatuses);
		std::vector<std::string> killLines;
		for (const int32_t npc : kills) {
			killLines.push_back(std::to_string(npc));
			EXPECT_EQ(lootEnables[npc], 1) << "X12: the characters killed npc object " << npc << " and got " << lootEnables[npc]
			                               << " SM_LOOT_STATUS(LOOT_ENABLE) for its corpse (registerDrop runs once per kill, to its end)";
		}
		for (const auto& [target, count] : lootEnables)
			EXPECT_TRUE(kills.contains(target)) << "X12: " << count << " SM_LOOT_STATUS(LOOT_ENABLE) for object " << target
			                                    << ", which no character killed";
		EXPECT_GE(kills.size(), 2u) << "X12: the recording holds " << kills.size() << " kills of the characters; S5 and S10 kill monster A twice";
		const std::optional<LiveCount> dropNpc = liveCount("model::gameobjects::DropNpc");
		if (!dropNpc) {
			ADD_FAILURE() << "X12: m5a_summary.txt has no liveCount row for DropNpc (CheckOutput G-06)";
		} else {
			EXPECT_EQ(dropNpc->created, static_cast<int64_t>(kills.size()))
			  << "X12: " << dropNpc->line << " against " << kills.size() << " kills (initDropNpc makes one per registerDrop)";
			EXPECT_EQ(dropNpc->live, 0) << "X12: " << dropNpc->line << " - every corpse despawned 2 s after its kill and unregisterDrop released it";
		}
		EXPECT_EQ(value("dropNpcsHeld"), "0") << "X12: DropRegistrationService still held a DropNpc at the stop";
		EXPECT_EQ(value("dropItemsHeld"), "0") << "X12: DropRegistrationService still held drop items at the stop";
		for (const std::string_view name : {"model::drop::DropItem", "RuntimeDropItem"}) {
			const std::optional<LiveCount> dropItem = liveCount(name);
			if (!dropItem) {
				ADD_FAILURE() << "X12: m5a_summary.txt has no liveCount row for " << name << " (CheckOutput G-06)";
				continue;
			}
			// a check of the profile key, not a proof: at the default rate this script's kills (monster A twice, the kerub once) would make no drop
			// item in 0.418^2 x 0.215 = 3.8 % of runs (m5b3-plan.md §2.4), so a lost key goes unseen in those
			EXPECT_EQ(dropItem->created, 0) << "X12: " << dropItem->line << " - at gameserver.rates.drop = 0 no drop rule fires, so the profile key "
			                                                                  "did not reach the server or a drop path ignores the rate";
			EXPECT_EQ(dropItem->live, 0) << "X12: " << dropItem->line;
		}
		std::cout << "X12: the characters killed " << (killLines.empty() ? std::string("nothing") : join(killLines)) << "; DropNpc "
		          << (dropNpc ? dropNpc->line : std::string("(no row)")) << std::endl;

		// the account-level classes Java keeps for a connection still open at the stop (M5bScenarioTest.cpp's Q2 tail): AionConnection.onDisconnect
		// returns before LoginServer.onDisconnect (AionConnection.java:239-243), so the Account survives with the PlayerAccountData of EVERY
		// character on it - its common data and appearance included - which is two here (D4), where the M5b gate had one
		const int64_t charactersOnAccount = 2;
		const std::set<std::string> perConnection = {"Account", "AccountTime", "ConnectionAliveChecker"};
		const std::set<std::string> perCharacter = {"PlayerAccountData", "PlayerCommonData", "PlayerAppearance"};
		for (const auto& [name, count] : readLiveCounts(servers, "live_counts.txt")) {
			if (perConnection.contains(name) || name.ends_with("Storage"))
				EXPECT_LE(count.live, connectionsAtShutdown) << "X12: live instances left: " << count.line;
			if (perCharacter.contains(name))
				EXPECT_LE(count.live, connectionsAtShutdown * charactersOnAccount) << "X12: live instances left: " << count.line;
			if (name == "Player")
				EXPECT_EQ(count.live, 0) << "X12: live instances left: " << count.line;
		}
	});

	finishRun(servers, outputDir, variant.testName);
}

} // namespace

// ---- the gates ---------------------------------------------------------------------------------------------------------------------------

/** `gs.scenario.m5b2` (G-03): the abilities of §10.2 with `gameserver.geodata.enable=false` */
TEST(M5b2Scenario, Run) {
	runM5b2Gate({false, "gs.scenario.m5b2", "m5b2", "m5b2", "m5b2a"});
}

/**
 * `gs.scenario.m5b2_geo` (G-04): the same script with `gameserver.geodata.enable=true`, its own output directory, schema pair and CTest entry,
 * under the same RESOURCE_LOCK.
 *
 * **What geo changes on the cast path, verified against the Java before writing this (§10.5):** exactly two property checks call the geo
 * engine. FirstTargetRangeProperty.set asks `GeoService.canSee(effector, firstTarget)` at the cast start and again at the cast end for every
 * first target that is not the caster (FirstTargetRangeProperty.java:67-72), and refuses with STR_SKILL_OBSTACLE when it answers false;
 * TargetRangeProperty asks it for the members of an AREA skill (TargetRangeProperty.java:159-163), which no skill of this gate is. The effect
 * classes that consult geo (StaggerEffect, StumbleEffect, the dashes, the pulls) are not on the scripted path: a Warrior's sword and a Mage's
 * spellbook proc neither (D13).
 *
 * So what this run adds is the positive half of that check against a world that has its terrain: every cast of S4, S8-S12 and S14 targets a
 * monster standing in the open at 2 or 10 m, and each of them must pass canSee with the geo mesh loaded - a GeoService.canSee that puts the
 * ray under the ground or through the caster's own collision would refuse them here and nowhere else (X4, X6 and S4 fail with
 * STR_SKILL_OBSTACLE, which the cast windows record). The NEGATIVE half - a target behind a rock refused with STR_SKILL_OBSTACLE - is the same
 * gap gs.scenario.m5b_geo records for GEO-A1: it needs a position the oracle picks from the mesh (a line-of-sight probe `tools/oracle/geo`
 * does not have), and a guessed position would be a row that passes or fails with the geometry rather than with the port.
 */
TEST(M5b2ScenarioGeo, Run) {
	runM5b2Gate({true, "gs.scenario.m5b2_geo", "m5b2_geo", "m5b2geo", "m5b2g"});
}

} // namespace aion::gameserver::scenario
