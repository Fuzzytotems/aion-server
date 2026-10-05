// The M5c scenario gate (m5c-plan.md G-03, §10): C0-C20. One login server and one game server as child processes on their own test schemas and
// TWO accounts online at once - A, an Elyos Warrior, and B, an Elyos Mage - at Akarios village on Poeta: talking to the merchant 798007, the
// postbox and the function npcs, buying, selling and buying back, an exchange, a cancelled one and one whose partner quits, a private store, mail
// online and offline, soul healing, the database after both quit, identification, a manastone socketed and removed at Seril, the cube
// expansion, an extraction and an enchantment (part 1, stage 2); then A, seeded as a level-10 Gladiator in Sanctum, learns Cooking from
// Hestia, buys Salt from Luelas, crafts Roast Inina at an oven and deletes the recipe (C19, part 2, stage 3: it stands on M5d's QuestState
// restore and C-01, both merged, §20.5-§20.6, §21.7) - then the reports the server writes at shutdown.
//
// Every expectation is independent of the C++ server code, as in the earlier gates: server packets are read with the decoders of
// tests/scenario/decoders (EconomyDecoders.h for the economy packets, ItemDecoders.h, CombatDecoders.h and PacketDecoders.h, all written from
// the Java writeImpl methods, m5a-plan.md D9), and every number comes from `tools/oracle/oracle.py` - m5c-economy (the talk spots and windows,
// soul healing, the cube, Seril's price, the mail commission, identification, extraction, socketing and the equip facts, through
// EconomyOracle.h), m5c-trade (SM_PRICES, the merchant's windows, buy prices, sell rewards and refusals), m5a-creation (the starter
// inventories) and m5b3-item (the item masks) - or from the Java arithmetic of the method an assertion is about, cited at the line.
//
// **This file does not share M5b3ScenarioTest.cpp's helpers**, for the reason that file gives: each gate owns one pair of server processes and
// its helpers live in an anonymous namespace. What is duplicated is scaffolding (the case log, the burst collector, the login conversation,
// the report readers), never an assertion. The inventory model is the shared one (InventoryModel.h), one per client.
//
// **The ledger (X16).** The gate keeps a per-character, per-item-id ledger that starts from the oracle's starter inventories and applies every
// buy, sale, buy-back, exchange, store sale, mail and commission of C5-C13 with the oracles' prices. Three things are compared with it: the
// clients' inventory models (every item packet each client received), the database after both quit (C14), and the re-entry's
// SM_INVENTORY_INFO. And from C8 to C13 no object id may ever be in both clients' models.
//
// **Two clients, one reader at a time.** GameSession reads on demand, so a client that is not being read keeps its packets in the socket
// buffer. Every step reads the acting client first and then drains the other one; a window "of B" is B's packets recorded between two marks.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
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

#include <nlohmann/json.hpp>

#include "AsyncAllowed.h"
#include "EconomyOracle.h"
#include "FakeLoginClient.h"
#include "GameSession.h"
#include "InventoryModel.h"
#include "Oracle.h"
#include "PrologueSupport.h"
#include "ScenarioDatabase.h"
#include "ScenarioServers.h"
#include "../support/NetworkTestSupport.h" // the little endian PacketWriter (C2b)
#include "decoders/CombatDecoders.h"
#include "decoders/EconomyDecoders.h"
#include "decoders/ItemDecoders.h"
#include "decoders/PacketDecoders.h"
#include "decoders/ProgressionDecoders.h" // SM_MESSAGE (C2b)
#include "decoders/QuestDecoders.h" // SM_STATUPDATE_EXP (m5d-plan.md G-02 moved it there)

#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {
namespace {

using namespace std::chrono_literals;
using decoders::DecodeError;
using nlohmann::json;
using Packet = GameSession::Packet;

/** the quiet period that ends a burst of server packets (m5a-plan.md §5.4) */
constexpr std::chrono::milliseconds QUIET = 1000ms;
/** How long collectBurst waits for the FIRST packet of an answer. The quiet period alone (QUIET) ended a burst before the server had
 * answered at all when a loaded machine delayed a re-entry by a little over a second (gs.scenario.m5b K7b and m5c C9, 2026-09-29: "no
 * packet after CM_ENTER_WORLD"). Nothing expects an empty burst, so the longer first wait changes no result, only the time an answer
 * that never comes costs; after the first packet the quiet rule is unchanged. */
constexpr std::chrono::milliseconds FIRST_REPLY_WAIT = 5000ms;
constexpr std::chrono::milliseconds BURST_LIMIT = 90s;
/** how long a step reads for the answer of one client packet when no single packet ends it, and how long a refusal is watched for silence */
constexpr std::chrono::milliseconds STEP = 1500ms;
constexpr std::chrono::milliseconds SILENCE = 1500ms;

/** SM_CREATE_CHARACTER response codes (SM_CREATE_CHARACTER.java) */
constexpr int32_t RESPONSE_OK = 0;
constexpr int32_t RESPONSE_OPEN_CREATION_WINDOW = 22;

/** the Elyos start map and the npcs of §10.2 (the oracle's --npc list, in this order: the first is the reference of the spots) */
constexpr int32_t ELYOS_START_MAP = 210010000;
constexpr int32_t MERCHANT = 798007;  // minalinerk: BUY, SELL
constexpr int32_t POSTBOX = 700000;   // the mailbox (PostboxAI)
constexpr int32_t SERIL = 203336;     // REMOVE_ITEM_OPTION
constexpr int32_t FULLA = 203064;     // RECOVERY
constexpr int32_t CUBE_NPC = 798008;  // baevrunerk: EXTEND_INVENTORY
/** the oracle's --direction for the talk spots: C3's stage-0 finding, along +x the band and far spots also lie in Seril's range (§18) */
constexpr double SPOT_DIRECTION = 270.0;

/** the items of §10.2 */
constexpr int32_t KINAH_ITEM = 182400001;
constexpr int32_t LIFE_POTION = 162000002;
constexpr int32_t LIFE_ELIXIR = 162000052;
constexpr int32_t JUICE = 160000001;
constexpr int32_t BANDAGE = 169300002;
constexpr int32_t TUNIC = 110100355;           // Plainsman's Tunic: C15-C18's armour (a robe piece a level-4 Mage can wear, §18)
constexpr int32_t PLAINSMAN_SWORD = 100000133; // C18's weapon to break
constexpr int32_t MANASTONE = 167000226;       // Manastone: HP +20
constexpr int32_t EXTRACTION_TOOLS = 165000001;

/** C5-C11's quantities */
constexpr int64_t ELIXIRS_BOUGHT = 2;
constexpr int64_t POTIONS_SOLD = 10;
constexpr int64_t POTIONS_EXCHANGED = 10;
constexpr int64_t KINAH_EXCHANGED = 100;
constexpr int64_t BANDAGES_EXCHANGED = 5;
constexpr uint16_t STORE_COUNT = 5;
constexpr int64_t STORE_PRICE = 100;
constexpr int64_t STORE_FIRST_BUY = 3;
constexpr int64_t STORE_SECOND_BUY = 2;
constexpr int64_t MAILED_POTIONS = 5;
constexpr int64_t MAILED_KINAH = 200;
constexpr int64_t KINAH_LETTER = 10;
constexpr int64_t RECOVERABLE_EXP = 1000;
constexpr std::string_view FIRST_MAIL = "162000002:5:200";
constexpr std::string_view KINAH_MAIL = "0:0:10";

/** DialogAction ids (DialogAction.java:17-18, 50, 57, 62, 85) */
constexpr uint16_t DIALOG_BUY = 2;
constexpr uint16_t DIALOG_SELL = 3;
constexpr uint16_t DIALOG_RECOVERY = 35;
constexpr uint16_t DIALOG_REMOVE_ITEM_OPTION = 42;
constexpr uint16_t DIALOG_EXTEND_INVENTORY = 47;
constexpr uint16_t DIALOG_BUY_AGAIN = 70;
/** SM_QUESTION_WINDOW.STR_EXCHANGE_DO_YOU_ACCEPT_EXCHANGE (SM_QUESTION_WINDOW.java:107) */
constexpr int32_t QUESTION_EXCHANGE = 90001;
/** EmotionType.OPEN_PRIVATESHOP / CLOSE_PRIVATESHOP (EmotionType.java:41-42) */
constexpr uint8_t EMOTION_OPEN_PRIVATESHOP = 33;
constexpr uint8_t EMOTION_CLOSE_PRIVATESHOP = 34;
/** ItemSlot.TORSO.getSlotIdMask() (ItemSlot.java:15), where CM_EQUIP_ITEM puts the tunic */
constexpr int64_t TORSO = 1LL << 3;
/** ItemStone.ItemStoneType.MANASTONE.ordinal(): the `category` ItemStoneListDAO stores for a manastone (ItemStone.java:22-27) */
constexpr int32_t ITEM_STONE_CATEGORY_MANASTONE = 0;
/** CM_USE_ITEM's type 2: a target item follows (CM_USE_ITEM.java:38-52) */
constexpr int8_t USE_ITEM_ON_ITEM = 2;
/** CM_MANASTONE's actionType for an enchantment stone and for a manastone (CM_MANASTONE.java runImpl: "case 1: // enchant stone", "case 2: // add
 * manastone"), both handed to EnchantItemAction; GameSession names only the godstone (4) and removal (3) arms */
constexpr uint8_t MANASTONE_ENCHANT = 1;
constexpr uint8_t MANASTONE_ADD = 2;

/** SM_SYSTEM_MESSAGE ids the oracles do not carry (SM_SYSTEM_MESSAGE.java) */
constexpr int32_t STR_BUY_SELL_USER_BUY_FAILED = 1300335;
constexpr int32_t STR_MSG_NOT_ENOUGH_MONEY = 1300759;
constexpr int32_t STR_BUY_SELL_ITEM_CAN_NOT_BE_SELLED_TO_NPC = 1300344;
constexpr int32_t STR_EXCHANGE_ASKED_EXCHANGE_TO_HIM = 1300353;
constexpr int32_t STR_MSG_PERSONAL_SHOP_SELL_ITEM_MULTI = 1400135;
constexpr int32_t STR_MSG_PERSONAL_SHOP_SELL_ITEM = 1400134;
constexpr int32_t STR_MSG_ENCHANT_ITEM_SUCCEED_NEW = 1401681;
constexpr int32_t STR_ENCHANT_ITEM_FAILED = 1300456;

/** the ten client-visible statuses of SM_ENTER_WORLD_CHECK: 0 is the only one that lets the character in */
constexpr uint8_t ENTER_WORLD_OK = 0;

/** C19 (§10.2): the Daeva A becomes, the recipe, the oven and the three distances CM_CRAFT is sent from (X18, X19) */
constexpr std::string_view DAEVA_CLASS = "GLADIATOR";
constexpr int32_t CRAFT_RECIPE = 155001381; // Roast Inina: 1 Inina + 2 Salt -> 2 Roast Inina (recipe_templates.xml:12124-12130)
constexpr int32_t OVEN = 150000009;         // the Ovens of Sanctum (spawns/Statics/110010000_Sanctum.xml:48-53)
constexpr double CRAFT_NEAR = 3, CRAFT_TOO_FAR = 7, CRAFT_OUT_OF_PACKET_RANGE = 12;
/**
 * the oracle's --direction for C19's spots: along 45 degrees the 7 m and 12 m spots lie in no other oven's checkCraft range (the default 0
 * leaves oven 104 in range of the 7 m spot); the 3 m spot has oven 104 in range along every direction tried (0-315 by 45), and CM_CRAFT names
 * its target anyway
 */
constexpr double CRAFT_DIRECTION = 45.0;
/** SM_SYSTEM_MESSAGE.STR_COMBINE_TOO_FAR_FROM_TOOL (SM_SYSTEM_MESSAGE.java:15971-15972) */
constexpr int32_t STR_COMBINE_TOO_FAR_FROM_TOOL = 1330040;
/** the one-skill SM_SKILL_LIST of a crafting skill: 1330061 when it is new, 1330064 on a level-up (SkillLearnService.java:48-49) */
constexpr int32_t SKILL_LIST_CRAFT_LEARNED = 1330061;
constexpr int32_t SKILL_LIST_CRAFT_LEVEL_UP = 1330064;
/** SM_CRAFT_ANIMATION's actions: 0 start, 1 in progress, 2 end or cancel (CraftingTask.java onInteractionStart / onSuccessFinish) */
constexpr uint8_t CRAFT_ANIMATION_START = 0;
constexpr uint8_t CRAFT_ANIMATION_PROGRESS = 1;
constexpr uint8_t CRAFT_ANIMATION_END = 2;
/**
 * the Inina C19 seeds beyond the recipe's (the review of 2026-09-28): with exactly the recipe's one Inina a second decreaseByItemId finds
 * nothing to take, so X19 could not see the materials consumed twice; with one more the Inina stack must keep exactly this much
 */
constexpr int64_t SURPLUS_ININA = 1;
/**
 * X20's tolerance for each gap between two SM_CRAFT_UPDATEs and for the whole craft: the runs of stage 3 were off by 0-2 ms, and a first tick
 * delayed by 600 ms passed the earlier +-750 ms on the total (the review of 2026-09-28)
 */
constexpr double TICK_TOLERANCE_MS = 250.0;
/** SM_SKILL_LIST writes 1 as the level of a normal skill - no stigma, an id below 30000 (SkillEntryWriter.java:27, PlayerSkillEntry.isNormalSkill) */
constexpr uint16_t NORMAL_SKILL_ID_LIMIT = 30000;
/** SM_SKILL_REMOVE writes a profession skill's getProfessionFlag(), 1 for a tapping skill (SM_SKILL_REMOVE.java:18, PlayerSkillEntry.java:84-86) */
constexpr uint8_t SKILL_REMOVE_TAPPING_FLAG = 1;

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

template <typename T>
std::string joinNumbers(const std::vector<T>& values) {
	std::vector<std::string> texts;
	for (const T& value : values)
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

/** the packets [from, to) of a session's recording */
std::vector<Packet> slice(const GameSession& session, size_t from, std::optional<size_t> to = std::nullopt) {
	const std::vector<Packet>& packets = session.recorded();
	const size_t end = std::min(packets.size(), to.value_or(packets.size()));
	if (from >= end)
		return {};
	return std::vector<Packet>(packets.begin() + static_cast<std::ptrdiff_t>(from), packets.begin() + static_cast<std::ptrdiff_t>(end));
}

int64_t millisBetween(std::chrono::steady_clock::time_point earlier, std::chrono::steady_clock::time_point later) {
	return std::chrono::duration_cast<std::chrono::milliseconds>(later - earlier).count();
}

/** the SM_SYSTEM_MESSAGE ids of a window, in order */
std::vector<int32_t> messageIds(const std::vector<Packet>& packets) {
	std::vector<int32_t> ids;
	for (const Packet& packet : packets)
		if (packet.name == "SM_SYSTEM_MESSAGE") {
			try {
				ids.push_back(decoders::decodeSystemMessageId(packet.data));
			} catch (const DecodeError&) {
				ids.push_back(-1);
			}
		}
	return ids;
}

bool contains(const std::vector<int32_t>& values, int32_t value) {
	return std::ranges::find(values, value) != values.end();
}

/** every packet of `name` in a window, decoded; a body that does not decode fails the row that reads it and is skipped */
template <typename Decoded, typename Decode>
std::vector<Decoded> decodeAll(const std::vector<Packet>& packets, std::string_view name, Decode decode, std::string_view row) {
	std::vector<Decoded> decoded;
	for (const Packet& packet : packets) {
		if (packet.name != name)
			continue;
		try {
			decoded.push_back(decode(std::span<const uint8_t>(packet.data)));
		} catch (const DecodeError& error) {
			ADD_FAILURE() << row << ": " << name << " does not decode: " << error.what();
		}
	}
	return decoded;
}

/** SM_SKILL_REMOVE (SM_SKILL_REMOVE.java:24-26): writeH(skillId), writeC(the level, a profession skill's getProfessionFlag()), writeC(skillType) */
struct SkillRemove {
	uint16_t skillId = 0;
	uint8_t level = 0;
	uint8_t skillType = 0;

	bool operator==(const SkillRemove&) const = default;
};

std::ostream& operator<<(std::ostream& out, const SkillRemove& remove) {
	return out << "SM_SKILL_REMOVE(" << remove.skillId << ", " << static_cast<int32_t>(remove.level) << ", " << static_cast<int32_t>(remove.skillType)
	           << ")";
}

SkillRemove decodeSkillRemove(std::span<const uint8_t> body) {
	decoders::BodyReader reader(body, "SM_SKILL_REMOVE");
	SkillRemove remove;
	remove.skillId = reader.H();
	remove.level = reader.C();
	remove.skillType = reader.C();
	reader.expectFullyConsumed();
	return remove;
}

/** SM_ACTION_ANIMATION (SM_ACTION_ANIMATION.java:28-30): writeD(targetObjectId), writeH(the ActionAnimation's id), writeD(levelOrObjectId) */
struct ActionAnimationPacket {
	int32_t objectId = 0;
	uint16_t animation = 0;
	int32_t levelOrObjectId = 0;

	bool operator==(const ActionAnimationPacket&) const = default;
};

std::ostream& operator<<(std::ostream& out, const ActionAnimationPacket& packet) {
	return out << "SM_ACTION_ANIMATION(" << packet.objectId << ", " << packet.animation << ", " << packet.levelOrObjectId << ")";
}

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
 * Runs the cases in order, records their result and never lets one case's exception end the run silently.
 * @return whether the NEXT case can run: false after an exception or a fatal (ASSERT_*) failure, which leave the characters' state unknown;
 * true after non-fatal (EXPECT_*) failures only, which leave it as the script intended - so a mutant that breaks one row still shows every
 * later row of the gate (the "others pass" half of a mutation run)
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
		// until the first packet arrives the window is FIRST_REPLY_WAIT at least: a loaded server can answer later than `quiet` (see the constant)
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

// ---- the oracle answers this gate reads besides EconomyOracle.h (tools/oracle/m5c/trade.py, m5b3/item.py) -----------------------------

/** oracle.py m5c-trade --npc 798007 --item ID --count N --race ELYOS: the merchant's windows and one item's buy and sell (m5c/trade.py) */
struct TradeAnswer {
	/** prices.ELYOS.smPrices: SM_PRICES' three bytes */
	std::array<int32_t, 3> smPrices{};
	/** buy.smTradeList */
	int32_t tradeNpcTypeIndex = 0, buyPriceModifier = 0;
	bool tradeShowBuyTab = false, tradeShowSellTab = false;
	std::vector<int32_t> tradeTabs;
	size_t limitedItems = 0;
	/** sell.smSellItem */
	int32_t sellNpcTypeIndex = 0, sellBuyPriceRate = 0;
	bool sellShowBuyTab = false, sellShowSellTab = false;
	std::vector<int32_t> sellTabs;
	/** item.buy / item.sell for the --count */
	int32_t itemId = 0;
	bool buyable = false;
	std::optional<std::string> buyFailure;
	int64_t buyUnitPrice = 0, buyKinah = 0;
	bool sellAccepted = false;
	std::optional<std::string> sellFailure;
	int64_t sellKinah = 0, soldCount = 0;
	std::optional<int64_t> repurchasePrice;
};

std::optional<std::string> failureMessage(const json& node) {
	if (!node.contains("failure") || node.at("failure").is_null())
		return std::nullopt;
	return node.at("failure").at("message").get<std::string>();
}

TradeAnswer parseTrade(const std::string& text) {
	const json root = json::parse(text);
	if (root.at("format").get<std::string>() != "aion-m5c-trade")
		throw std::runtime_error("not an m5c-trade answer");
	TradeAnswer answer;
	const json& prices = root.at("prices").at("ELYOS").at("smPrices");
	for (size_t i = 0; i < 3; i++)
		answer.smPrices[i] = prices.at(i).get<int32_t>();
	const json& tradeList = root.at("buy").at("smTradeList");
	answer.tradeNpcTypeIndex = tradeList.at("npcTypeIndex").get<int32_t>();
	answer.buyPriceModifier = tradeList.at("buyPriceModifier").get<int32_t>();
	answer.tradeShowBuyTab = tradeList.at("showBuyTab").get<bool>();
	answer.tradeShowSellTab = tradeList.at("showSellTab").get<bool>();
	answer.tradeTabs = tradeList.at("tabs").get<std::vector<int32_t>>();
	answer.limitedItems = tradeList.at("limitedItems").size();
	const json& sellItem = root.at("sell").at("smSellItem");
	answer.sellNpcTypeIndex = sellItem.at("npcTypeIndex").get<int32_t>();
	answer.sellBuyPriceRate = sellItem.at("buyPriceRate").get<int32_t>();
	answer.sellShowBuyTab = sellItem.at("showBuyTab").get<bool>();
	answer.sellShowSellTab = sellItem.at("showSellTab").get<bool>();
	answer.sellTabs = sellItem.at("tabs").get<std::vector<int32_t>>();
	const json& item = root.at("item");
	answer.itemId = item.at("itemId").get<int32_t>();
	const json& buy = item.at("buy");
	answer.buyable = buy.at("buyable").get<bool>();
	answer.buyFailure = failureMessage(buy);
	answer.buyUnitPrice = buy.at("unitPrice").at("ELYOS").get<int64_t>();
	answer.buyKinah = buy.at("kinah").at("ELYOS").get<int64_t>();
	const json& sell = item.at("sell");
	answer.sellAccepted = sell.at("accepted").get<bool>();
	answer.sellFailure = failureMessage(sell);
	answer.sellKinah = sell.at("kinah").get<int64_t>();
	answer.soldCount = sell.at("soldCount").get<int64_t>();
	if (!sell.at("repurchasePrice").is_null())
		answer.repurchasePrice = sell.at("repurchasePrice").get<int64_t>();
	return answer;
}

/** oracle.py m5b3-item: the mask flags of each item (m5b3/item.py) */
std::map<int32_t, std::set<std::string>> parseMaskFlags(const std::string& text) {
	const json root = json::parse(text);
	std::map<int32_t, std::set<std::string>> flags;
	for (const json& item : root.at("items")) {
		std::set<std::string>& set = flags[item.at("itemId").get<int32_t>()];
		for (const json& flag : item.at("maskFlags"))
			set.insert(flag.get<std::string>());
	}
	return flags;
}

/** m5c-economy's craft.learn.yes (C19, X17), which EconomyOracle.h does not carry: what the yes to Hestia's question takes and teaches */
struct LearnYes {
	int64_t kinahDelta = 0;
	int32_t skillId = 0;
	int32_t skillLevel = 0;
};

LearnYes parseLearnYes(const std::string& economyJson) {
	const json yes = json::parse(economyJson).at("craft").at("learn").at("yes");
	return {yes.at("kinahDelta").get<int64_t>(), yes.at("skill").at("skillId").get<int32_t>(), yes.at("skill").at("level").get<int32_t>()};
}

/** one of the craft's SM_CRAFT_UPDATEs as m5c-economy's craft.recipe.updates states it (m5c-craft's packets); a bar it leaves out is not asserted */
struct CraftUpdateWant {
	int32_t action = 0;
	std::optional<int32_t> success, failure;
	int32_t executionSpeed = 0, delay = 0;
};

/** the rest of m5c-economy's C19 blocks that EconomyOracle.h does not carry (the review of 2026-09-28), read like craft.learn.yes above */
struct C19Extras {
	/** the SM_ACTION_ANIMATIONs (ActionAnimation id, levelOrObjectId) of the learn (craft.learn.yes) and of the craft's level-up (craft.recipe) */
	std::vector<std::pair<uint16_t, int32_t>> learnAnimations, skillUpAnimations;
	/** every analyze tick's SM_CRAFT_UPDATE speed and delay (craft.recipe) */
	int32_t executionSpeed = 0, showBarDelay = 0;
	/** onInteractionStart's pair, onSuccessFinish's update and sendCancelCraft's (craft.recipe.updates) */
	CraftUpdateWant init, start, success, cancel;
	/** skill id -> level after the Daeva's enter world (daeva.enterWorld.skillLevels) */
	std::map<int32_t, int32_t> skillLevels;
	/** the skill the Daeva swap's SM_SKILL_REMOVE names (daeva.enterWorld.daevaSwap), null without a swap */
	std::optional<int32_t> smSkillRemove;
};

C19Extras parseC19Extras(const std::string& economyJson) {
	const json root = json::parse(economyJson);
	const json& recipe = root.at("craft").at("recipe");
	const auto animations = [](const json& list) {
		std::vector<std::pair<uint16_t, int32_t>> result;
		for (const json& animation : list)
			result.emplace_back(animation.at("id").get<uint16_t>(), animation.at("levelOrObjectId").get<int32_t>());
		return result;
	};
	const auto update = [](const json& row) {
		CraftUpdateWant want;
		want.action = row.at("action").get<int32_t>();
		if (row.contains("success"))
			want.success = row.at("success").get<int32_t>();
		if (row.contains("failure"))
			want.failure = row.at("failure").get<int32_t>();
		want.executionSpeed = row.at("executionSpeed").get<int32_t>();
		want.delay = row.at("delay").get<int32_t>();
		return want;
	};
	C19Extras extras;
	extras.learnAnimations = animations(root.at("craft").at("learn").at("yes").at("animations"));
	extras.skillUpAnimations = animations(recipe.at("skillUpAnimations"));
	extras.executionSpeed = recipe.at("executionSpeed").get<int32_t>();
	extras.showBarDelay = recipe.at("showBarDelay").get<int32_t>();
	const json& updates = recipe.at("updates");
	extras.init = update(updates.at("init"));
	extras.start = update(updates.at("start"));
	extras.success = update(updates.at("success"));
	extras.cancel = update(updates.at("cancel"));
	const json& world = root.at("daeva").at("enterWorld");
	for (const auto& [skillId, level] : world.at("skillLevels").items())
		extras.skillLevels[std::stoi(skillId)] = level.get<int32_t>();
	if (const json& swap = world.at("daevaSwap"); !swap.is_null())
		extras.smSkillRemove = swap.at("smSkillRemove").get<int32_t>();
	return extras;
}

/** a decoded SM_CRAFT_UPDATE against the oracle's row: the action, the bars it states, the speed and the delay */
void expectCraftUpdate(const decoders::CraftUpdate& update, const CraftUpdateWant& want, std::string_view what) {
	EXPECT_EQ(update.action, want.action) << what << ": the action";
	if (want.success)
		EXPECT_EQ(update.success, *want.success) << what << ": the success bar";
	if (want.failure)
		EXPECT_EQ(update.failure, *want.failure) << what << ": the failure bar";
	EXPECT_EQ(update.executionSpeed, want.executionSpeed) << what << ": executionSpeed";
	EXPECT_EQ(update.delay, want.delay) << what << ": the bar delay";
}

/** a ChatUtil.l10n parameter as the question window's decoder returns it: the oracle's three UTF-16 code units as UTF-8 */
std::string l10nText(const std::array<uint16_t, 3>& units) {
	std::u16string text;
	for (const uint16_t unit : units)
		text += static_cast<char16_t>(unit);
	return commons::utils::StringUtils::toUtf8(text);
}

// ---- the scenario clients ---------------------------------------------------------------------------------------------------------------

struct ScenarioClient {
	std::string label; // "A" or "B"
	std::string account;
	std::string password = "m5cPassword1";
	std::string name;
	int32_t classId = 0;
	std::unique_ptr<FakeLoginClient> login;
	std::unique_ptr<GameSession> game;
	FakeLoginClient::SessionKey key;
	int32_t playerId = 0;
	InventoryModel model;
	AnnouncedNpcs npcs;
	AsyncAllowed async = AsyncAllowed::m5aDefault();
	/** the walk cursor: where the server has the character after its last CM_MOVE (or its spawn) */
	float x = 0, y = 0, z = 0;
	/** the last enter-world burst */
	std::vector<Packet> lastEnterWorld;
	/** the last level-ready burst (P6-Q prologue: the first one starts quest 1000 and plays its movie) */
	std::vector<Packet> lastLevelReady;

	size_t mark() const { return game ? game->recorded().size() : 0; }
	std::vector<Packet> since(size_t from) const { return game ? slice(*game, from) : std::vector<Packet>{}; }
	/** reads whatever is waiting (and what arrives within `window`) */
	void drain(std::chrono::milliseconds window = 300ms) {
		if (game && !game->client.socket.isClosed())
			collectFor(*game, window);
		model.sync();
	}
	int64_t kinah() {
		model.sync();
		return model.kinah();
	}
	std::optional<int32_t> kinahObject() {
		model.sync();
		for (const auto& [id, item] : model.items)
			if (item.itemId == KINAH_ITEM && item.location == ModelItem::CUBE)
				return id;
		return std::nullopt;
	}
	/** the count of an item id over every stack of the model, equipped ones included */
	int64_t countOf(int32_t itemId) {
		model.sync();
		int64_t count = 0;
		for (const auto& [id, item] : model.items)
			if (item.itemId == itemId)
				count += item.count;
		return count;
	}
	/** the largest unequipped cube stack of an item id (the starter stack), 0 without one */
	int32_t stackOf(int32_t itemId) {
		model.sync();
		int32_t best = 0;
		int64_t bestCount = -1;
		for (const ModelItem& item : model.byItemId(itemId))
			if (item.count > bestCount) {
				best = item.objectId;
				bestCount = item.count;
			}
		return best;
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
std::vector<Packet> enterWorld(ScenarioClient& client) {
	client.async = AsyncAllowed::m5aDefault();
	client.async.selfPlayerState(client.playerId);
	client.async.npcActivity(client.npcs.predicate());
	client.game->send(GameSession::CM_ENTER_WORLD, GameSession::buildCM_ENTER_WORLD(client.playerId));
	std::vector<Packet> burst = collectBurst(*client.game, client.async);
	if (burst.empty())
		throw std::runtime_error(client.label + ": no packet after CM_ENTER_WORLD");
	const std::optional<uint8_t> check = enterWorldCheck(burst);
	if (!check || *check != ENTER_WORLD_OK)
		throw std::runtime_error(client.label + ": the enter world was refused (SM_ENTER_WORLD_CHECK " +
		                         (check ? std::to_string(*check) : std::string("missing")) + "; 3 is REENTRY_TIME, 1 CONNECTION_ERROR): " +
		                         join(namesOf(burst)));
	const Packet* spawn = firstOfName(burst, "SM_PLAYER_SPAWN");
	if (spawn == nullptr)
		throw std::runtime_error(client.label + ": no SM_PLAYER_SPAWN after CM_ENTER_WORLD: " + join(namesOf(burst)));
	const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(spawn->data);
	client.x = spawned.x;
	client.y = spawned.y;
	client.z = spawned.z;
	client.model.sync();
	client.lastEnterWorld = burst;
	return burst;
}

std::vector<Packet> levelReady(ScenarioClient& client) {
	client.game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
	std::vector<Packet> burst = collectBurst(*client.game, client.async);
	if (burst.empty())
		throw std::runtime_error(client.label + ": no packet after CM_LEVEL_READY");
	client.model.sync();
	client.lastLevelReady = burst;
	return burst;
}

/** CM_QUIT(1): back to the character list, the connection kept; then CM_CHARACTER_LIST as the client does (M5a's Q1). @return the list */
decoders::CharacterList quitToCharacterList(ScenarioClient& client) {
	client.game->send(GameSession::CM_QUIT, GameSession::buildCM_QUIT(true));
	waitFor(*client.game, "SM_QUIT_RESPONSE", 30s);
	client.game->send(GameSession::CM_CHARACTER_LIST, GameSession::buildCM_CHARACTER_LIST(client.key.playOk2));
	const Packet list = waitFor(*client.game, "SM_CHARACTER_LIST", 15s);
	client.model.sync();
	return decoders::decodeCharacterList(list.data);
}

/** from the character list back into the world (gameserver.character.reentry.time is 1 s in the scenario profile) */
std::vector<Packet> reenter(ScenarioClient& client) {
	std::this_thread::sleep_for(1500ms);
	client.model.follow(client.game.get());
	std::vector<Packet> burst = enterWorld(client);
	levelReady(client);
	return burst;
}

/** CM_QUIT(0): the character leaves the world (if it is in one) and the connection ends - the only state in which a `players` seed survives (F-3) */
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

/** a new login and the stored character into the world (M5a's Q5). @return the character list and the enter-world burst */
std::pair<decoders::CharacterList, std::vector<Packet>> relogIn(ScenarioServers& servers, ScenarioClient& client) {
	decoders::CharacterList list = logIn(servers, client);
	client.game->send(GameSession::CM_MAY_LOGIN_INTO_GAME, GameSession::buildCM_MAY_LOGIN_INTO_GAME());
	expectNext(*client.game, "SM_MAY_LOGIN_INTO_GAME", client.async);
	std::this_thread::sleep_for(1500ms);
	std::vector<Packet> burst = enterWorld(client);
	levelReady(client);
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

/** the object id of the npc of `templateId` nearest to (x, y), from every SM_NPC_INFO this session recorded (the npcs of §10.2 are fixed) */
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
 * The object id of the static object of `templateId` on the spawn spot `spot` (C19's oven), from every SM_GATHERABLE_INFO this session
 * recorded: the spot's static id and its x, y and z exactly (Java writes the spawn's position, StaticObjectSpawnManager.bringIntoWorld; the
 * oracle's spot is the same float of the same XML attribute). The decoder has checked the state (1: a static object is no door). A packet with
 * the template and the static id at another position is reported and does not match.
 */
std::optional<int32_t> staticObject(const ScenarioClient& client, int32_t templateId, const EconomyTool& spot) {
	if (!client.game)
		return std::nullopt;
	for (const Packet& packet : client.game->recorded()) {
		if (packet.name != "SM_GATHERABLE_INFO")
			continue;
		try {
			const decoders::GatherableInfo object = decoders::decodeGatherableInfo(packet.data);
			if (object.templateId != templateId || object.staticId != spot.staticId)
				continue;
			if (object.x == spot.x && object.y == spot.y && object.z == spot.z)
				return object.objectId;
			ADD_FAILURE() << "the SM_GATHERABLE_INFO of template " << templateId << " and static id " << spot.staticId << " is at (" << object.x << ", "
			              << object.y << ", " << object.z << "), not at its spawn spot (" << spot.x << ", " << spot.y << ", " << spot.z << ")";
		} catch (const DecodeError&) {
			// a packet that does not decode is not this object
		}
	}
	return std::nullopt;
}

/**
 * The packets of a window a row about `client` must not see besides its own answer: the async set, an announced npc turning to or from
 * anyone (NpcController.onTargetChanged broadcasts SM_LOOKATOBJECT with the talker as the target, which the async set only allows for an npc
 * target), and the other character's own movement and emotes.
 */
bool isNoise(ScenarioClient& client, int32_t otherPlayerId, const Packet& packet) {
	if (client.async.allows(packet.name, std::span<const uint8_t>(packet.data)))
		return true;
	try {
		if (packet.name == "SM_LOOKATOBJECT")
			return client.npcs.contains(decoders::decodeLookAtObject(packet.data).objectId) ||
			       decoders::decodeLookAtObject(packet.data).objectId == otherPlayerId;
		if (packet.name == "SM_MOVE")
			return decoders::decodeMoveObjectId(packet.data) == otherPlayerId;
		if (packet.name == "SM_EMOTION")
			return decoders::decodeEmotionHeader(packet.data).objectId == otherPlayerId;
		if (packet.name == "SM_PLAYER_STATE")
			return decoders::decodePlayerStateObjectId(packet.data) == otherPlayerId;
	} catch (const DecodeError&) {
		return false;
	}
	return false;
}

std::vector<Packet> news(ScenarioClient& client, int32_t otherPlayerId, const std::vector<Packet>& packets) {
	std::vector<Packet> result;
	for (const Packet& packet : packets)
		if (!isNoise(client, otherPlayerId, packet))
			result.push_back(packet);
	return result;
}

/** the counts of the kinah item's SM_INVENTORY_UPDATE_ITEMs in a window (Storage.increaseKinah / decreaseKinah, Storage.java) */
std::vector<int64_t> kinahUpdates(const std::vector<Packet>& packets, int32_t kinahObject, std::string_view row) {
	std::vector<int64_t> counts;
	for (const decoders::InventoryUpdateItem& update :
	     decodeAll<decoders::InventoryUpdateItem>(packets, "SM_INVENTORY_UPDATE_ITEM", decoders::decodeInventoryUpdateItem, row))
		if (update.item.objectId == kinahObject && update.item.general)
			counts.push_back(update.item.general->count);
	return counts;
}

/** the SM_INVENTORY_UPDATE_ITEMs of one object in a window */
std::vector<decoders::InventoryUpdateItem> updatesOf(const std::vector<Packet>& packets, int32_t objectId, std::string_view row) {
	std::vector<decoders::InventoryUpdateItem> updates;
	for (const decoders::InventoryUpdateItem& update :
	     decodeAll<decoders::InventoryUpdateItem>(packets, "SM_INVENTORY_UPDATE_ITEM", decoders::decodeInventoryUpdateItem, row))
		if (update.item.objectId == objectId)
			updates.push_back(update);
	return updates;
}

/** the items SM_INVENTORY_ADD_ITEM added in a window */
std::vector<decoders::InventoryItem> addedItems(const std::vector<Packet>& packets, std::string_view row) {
	std::vector<decoders::InventoryItem> items;
	for (const decoders::InventoryAddItem& add :
	     decodeAll<decoders::InventoryAddItem>(packets, "SM_INVENTORY_ADD_ITEM", decoders::decodeInventoryAddItem, row))
		for (const decoders::InventoryItem& item : add.items)
			items.push_back(item);
	return items;
}

/** the object ids SM_DELETE_ITEM removed in a window */
std::vector<int32_t> deletedObjects(const std::vector<Packet>& packets, std::string_view row) {
	std::vector<int32_t> ids;
	for (const decoders::DeleteItem& deleted : decodeAll<decoders::DeleteItem>(packets, "SM_DELETE_ITEM", decoders::decodeDeleteItem, row))
		ids.push_back(deleted.objectId);
	return ids;
}

/** the item packets of a window (a refusal's "no change") */
std::vector<std::string> itemPackets(const std::vector<Packet>& packets) {
	std::vector<std::string> names;
	for (const Packet& packet : packets)
		if (packet.name == "SM_INVENTORY_UPDATE_ITEM" || packet.name == "SM_INVENTORY_ADD_ITEM" || packet.name == "SM_DELETE_ITEM")
			names.push_back(packet.name);
	return names;
}

std::vector<uint8_t> exchangeConfirmations(const std::vector<Packet>& packets, std::string_view row) {
	return decodeAll<uint8_t>(packets, "SM_EXCHANGE_CONFIRMATION", decoders::decodeExchangeConfirmation, row);
}

std::vector<decoders::MailService> mailServices(const std::vector<Packet>& packets, std::string_view row) {
	return decodeAll<decoders::MailService>(packets, "SM_MAIL_SERVICE", decoders::decodeMailService, row);
}

std::vector<decoders::DialogWindow> dialogWindows(const std::vector<Packet>& packets, std::string_view row) {
	return decodeAll<decoders::DialogWindow>(packets, "SM_DIALOG_WINDOW", decoders::decodeDialogWindow, row);
}

std::vector<decoders::QuestionWindow> questionWindows(const std::vector<Packet>& packets, std::string_view row) {
	return decodeAll<decoders::QuestionWindow>(packets, "SM_QUESTION_WINDOW", decoders::decodeQuestionWindow, row);
}

std::vector<decoders::ItemUsageAnimation> usageAnimations(const std::vector<Packet>& packets, std::string_view row) {
	return decodeAll<decoders::ItemUsageAnimation>(packets, "SM_ITEM_USAGE_ANIMATION", decoders::decodeItemUsageAnimation, row);
}

std::vector<decoders::Emotion> emotions(const std::vector<Packet>& packets, std::string_view row) {
	return decodeAll<decoders::Emotion>(packets, "SM_EMOTION", decoders::decodeEmotion, row);
}

// ---- the check output reports (X22) ------------------------------------------------------------------------------------------------------

/** One section of m5c_partial_allowlist.txt: §A hit at least once, §B hit exactly zero times, §C counted but not pinned */
enum class AllowlistSection { HitAtLeastOnce, HitNever, NotPinned };

struct AllowlistEntry {
	std::string site;
	AllowlistSection section = AllowlistSection::NotPinned;
};

/** Reads tests/scenario/m5c_partial_allowlist.txt with its three sections ("# --- SECTION A/B/C" marker lines, as the M5b lists) */
std::vector<AllowlistEntry> readAllowlist() {
	std::vector<AllowlistEntry> entries;
	std::ifstream in(AION_SCENARIO_M5C_PARTIAL_ALLOWLIST, std::ios::binary);
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

/**
 * m5c-plan.md G-07 (2), §20.7 item 2: LegionDominionService::startWeeklyCalculation is scheduled with a hard-coded "0 0 9 ? * WED *"
 * (CronJobService.cpp:185-187, CronJobService.java:70) that no key moves, and it is AION_UNPORTED. A production seam would be a deviation the
 * owner has not decided, so the gate only NAMES a hit of that site as the cron's when its server was up at a Wednesday 09:00 local time - the
 * failure stays (P5-SC.md's rerun rule: rerun it before reading it as a regression).
 */
bool crossesWednesdayNine(std::chrono::system_clock::time_point from, std::chrono::system_clock::time_point to) {
	for (auto at = std::chrono::floor<std::chrono::minutes>(from); at <= to; at += std::chrono::minutes(1)) {
		const std::time_t seconds = std::chrono::system_clock::to_time_t(at);
		std::tm local{};
		localtime_s(&local, &seconds);
		if (local.tm_wday == 3 && local.tm_hour == 9 && local.tm_min == 0)
			return true;
	}
	return false;
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

// ---- the ledger (X16) --------------------------------------------------------------------------------------------------------------------

/** item id -> count of one character, starting from the oracle's starter inventory (the kinah item carries the kinah) */
using Ledger = std::map<int32_t, int64_t>;

Ledger starterLedger(const OracleCreation& creation) {
	Ledger ledger;
	for (const OracleItem& item : creation.items)
		ledger[item.itemId] += item.count;
	return ledger;
}

/** the per-item-id differences "id: have/want" between two ledgers (0-count entries count as absent) */
std::vector<std::string> ledgerDifferences(const Ledger& have, const Ledger& want) {
	std::set<int32_t> ids;
	for (const auto& [id, count] : have)
		ids.insert(id);
	for (const auto& [id, count] : want)
		ids.insert(id);
	std::vector<std::string> differences;
	for (int32_t id : ids) {
		const int64_t a = have.contains(id) ? have.at(id) : 0;
		const int64_t b = want.contains(id) ? want.at(id) : 0;
		if (a != b)
			differences.push_back(std::to_string(id) + ": " + std::to_string(a) + "/" + std::to_string(b));
	}
	return differences;
}

Ledger ledgerOf(const InventoryModel& model) {
	Ledger ledger;
	for (const auto& [id, item] : model.items)
		ledger[item.itemId] += item.count;
	return ledger;
}

// ---- the gate ---------------------------------------------------------------------------------------------------------------------------

void runM5cGate() {
	const std::string testName = "gs.scenario.m5c";
	// A skipped gate is NOT a passed gate (m5a-plan.md §5.10): AION_SCENARIO_REQUIRE=1 - the default of the CTest registration - turns every
	// skip reason into a failure that names the variable.
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
	const std::filesystem::path outputDir = std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "m5c";
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

	// ---- §10.1 processes, databases and profile: game-server/config/m5c.properties.example, key by key ----
	// The M5a set comes from ScenarioServers::m5aProfile (with G-07's far-future wall-clock schedules), then the M5b-1, M5b-2 and M5b-3
	// keys at the values the M5c profile gives them (the drop rate back at 0, m5b3-plan.md D4), then M5c's three (D6): crafting never fails
	// and never crits, and a manastone always sockets.
	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.schemaPrefix = "m5c";
	config.gameServerProperties["gameserver.npcshouts.enable"] = "false";
	config.gameServerProperties["gameserver.rates.xp.solo"] = "1.0, 2.0";
	config.gameServerProperties["gameserver.soulsickness.disable"] = "10";
	config.gameServerProperties["gameserver.rates.drop"] = "0";
	config.gameServerProperties["gameserver.items.ignore_potions_at_full_health"] = "false";
	config.gameServerProperties["gameserver.rates.godstone.activation.rate"] = "1.0";
	config.gameServerProperties["gameserver.rates.godstone.evaluation.cooldown_millis"] = "750";
	config.gameServerProperties["gameserver.drop.announce_quality"] = "MYTHIC";
	config.gameServerProperties["gameserver.craft.fail.chance"] = "0";
	config.gameServerProperties["gameserver.rates.crafting.crit_chances"] = "0, 0";
	config.gameServerProperties["gameserver.rates.manastone_chances"] = "200, 200";
	config.startupTimeout = 10min;
	config.stopTimeout = 3min;
	// the oracles read exactly the properties the server is given: the M5a profile under the gate's own keys, nothing from mygs.properties
	std::vector<std::string> oracleSettings;
	{
		std::map<std::string, std::string> properties = ScenarioServers::m5aProfile();
		for (const auto& [key, value] : config.gameServerProperties)
			properties[key] = value;
		for (const auto& [key, value] : properties)
			oracleSettings.push_back(key + "=" + value);
	}
	ScenarioServers servers(config, *environment);
	const std::string schema = servers.gameSchema();
	const ScenarioDatabase& database = servers.gameDatabase();

	bool ok = true;
	const auto runCase = [&](std::string_view id, std::string_view title, const std::function<void()>& body) {
		if (!ok)
			cases.skip(id, title, "an earlier case ended with an exception or a fatal failure");
		else
			ok = cases.run(id, title, body);
	};

	std::chrono::system_clock::time_point serverUpFrom = std::chrono::system_clock::now();
	ok = cases.run("S-0", "the servers start (§10.1)", [&] {
		servers.createSchemas();
		servers.startLoginServer();
		serverUpFrom = std::chrono::system_clock::now();
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

	ScenarioClient a, b;
	a.label = "A";
	b.label = "B";
	const std::string suffix = servers.gameSchema().substr(servers.gameSchema().size() - 8);
	a.account = "m5ca" + suffix;
	b.account = "m5cb" + suffix;
	a.name = "Economya";
	b.name = "Economyb";
	a.classId = NewCharacter::WARRIOR;
	b.classId = NewCharacter::MAGE;

	// ---- C0: the oracles answer, and the plan's premises are re-derived from them (§10.2 C0, G-01) ----
	EconomyAnswer economy;
	TradeAnswer elixirs, potions, juice, tools;
	OracleCreation warrior, mage;
	std::map<int32_t, std::set<std::string>> masks;
	Ledger ledgerA, ledgerB;
	int64_t seedKinahB = 0;
	runCase("C0", "the oracles answer and the plan's premises hold (m5c-economy, m5c-trade, m5a-creation, m5b3-item)", [&] {
		EconomyRequest request;
		request.noProfile = true;
		request.settings = oracleSettings;
		request.mapId = ELYOS_START_MAP;
		request.npcIds = {MERCHANT, POSTBOX, SERIL, FULLA, CUBE_NPC};
		request.direction = SPOT_DIRECTION;
		request.recoverExp = RECOVERABLE_EXP;
		request.mails = {std::string(FIRST_MAIL), std::string(KINAH_MAIL)};
		request.itemIds = {TUNIC, PLAINSMAN_SWORD};
		request.manastones = {MANASTONE};
		request.playerClass = "MAGE";
		request.race = "ELYOS";
		request.level = 4;
		economy = runEconomy(*oracle, request);

		const auto trade = [&](int32_t itemId, int64_t count) {
			std::vector<std::string> arguments = {"m5c-trade", "--no-profile", "--npc", std::to_string(MERCHANT), "--item", std::to_string(itemId),
				"--count", std::to_string(count), "--race", "ELYOS"};
			for (const std::string& setting : oracleSettings) {
				arguments.push_back("--set");
				arguments.push_back(setting);
			}
			return parseTrade(oracle->run(arguments));
		};
		elixirs = trade(LIFE_ELIXIR, ELIXIRS_BOUGHT);
		potions = trade(LIFE_POTION, POTIONS_SOLD);
		juice = trade(JUICE, 1);
		tools = trade(EXTRACTION_TOOLS, 1);
		warrior = oracle->creation("ELYOS", "WARRIOR");
		mage = oracle->creation("ELYOS", "MAGE");
		masks = parseMaskFlags(oracle->run({"m5b3-item", "--item", std::to_string(JUICE), "--item", std::to_string(LIFE_POTION), "--item",
		                                    std::to_string(BANDAGE), "--item", std::to_string(MANASTONE)}));

		// X1's premise: the siege off leaves every influence at 0, so SM_PRICES is 125/100/113 for either race (§2.10)
		EXPECT_EQ(economy.smPrices.at("ELYOS"), (std::array<int32_t, 3>{125, 100, 113})) << "§2.10's SM_PRICES";
		EXPECT_EQ(elixirs.smPrices, economy.smPrices.at("ELYOS")) << "m5c-trade and m5c-economy must agree on SM_PRICES";
		// X2: the band spot of 798007 is inside the talk range only because of isInTalkRange's "+ 1" (PositionUtil.java:243-261, 306-309)
		const EconomyTalk& merchant = economy.talkOf(MERCHANT);
		EXPECT_TRUE(merchant.canInteract);
		EXPECT_TRUE(merchant.bandSpot.inTalkRange && !merchant.bandSpot.inRangeWithoutPlusOne && !merchant.bandSpot.inRangeCenterToCenter)
		  << "the X2 spot must lie in [talk + R_npc + R_player, talk + 1 + R_npc + R_player), " << merchant.bandSpot.distance << " m";
		EXPECT_FALSE(merchant.farSpot.inTalkRange) << "C3's far spot is out of range";
		for (int32_t npcId : {MERCHANT, POSTBOX, SERIL, FULLA, CUBE_NPC}) {
			const EconomyTalk& talk = economy.talkOf(npcId);
			EXPECT_TRUE(talk.nearSpot.inTalkRange) << npcId;
			EXPECT_TRUE(talk.bandSpot.otherNpcsInTalkRange.empty() && talk.nearSpot.otherNpcsInTalkRange.empty() && talk.farSpot.otherNpcsInTalkRange.empty())
			  << npcId << ": --direction " << SPOT_DIRECTION << " must leave every spot in no other npc's range (C3, §18)";
			EXPECT_TRUE(talk.startWindow.has_value()) << npcId;
		}
		ASSERT_TRUE(merchant.startWindow && merchant.startWindow->page && economy.talkOf(POSTBOX).startWindow);
		EXPECT_EQ(*merchant.startWindow->page, 10) << "a function npc's start page (DialogPage.getStartPageId)";
		EXPECT_EQ(economy.talkOf(POSTBOX).startWindow->page, std::optional<int32_t>(decoders::DIALOG_PAGE_MAIL));
		EXPECT_EQ(economy.talkOf(POSTBOX).startWindow->pageValue, decoders::MAILBOX_STATE_REGULAR);
		// X4-X8: the merchant's windows, prices, sell rewards and refusals
		EXPECT_EQ(elixirs.tradeTabs, (std::vector<int32_t>{132, 720})) << "X4's tabs";
		EXPECT_EQ(elixirs.tradeNpcTypeIndex, 1) << "TradeNpcType.NORMAL's constructor argument (TradeNpcType.java:12), never its ordinal";
		EXPECT_TRUE(elixirs.buyable);
		EXPECT_EQ(elixirs.buyKinah, ELIXIRS_BOUGHT * 352) << "§2.10: 250 x 125 % x 113 % = 352";
		EXPECT_FALSE(potions.buyable) << "C5's unlisted item";
		EXPECT_EQ(potions.buyFailure, std::optional<std::string>("STR_BUY_SELL_USER_BUY_FAILED"));
		EXPECT_TRUE(potions.sellAccepted);
		EXPECT_EQ(potions.soldCount, POTIONS_SOLD);
		EXPECT_EQ(potions.sellKinah, 500) << "X7: 250 x 20 % x 10";
		EXPECT_EQ(potions.repurchasePrice, std::optional<int64_t>(500)) << "X8's buy-back price is the sale's reward";
		EXPECT_FALSE(juice.sellAccepted);
		EXPECT_EQ(juice.sellFailure, std::optional<std::string>("STR_BUY_SELL_ITEM_CAN_NOT_BE_SELLED_TO_NPC"));
		EXPECT_EQ(tools.buyKinah, 1412) << "X27: 1,000 x 125 % x 113 %";
		// X9: the juice cannot be traded, the potion and the bandage can (Item.isTradeable over the template mask)
		EXPECT_FALSE(masks.at(JUICE).contains("TRADEABLE"));
		EXPECT_TRUE(masks.at(LIFE_POTION).contains("TRADEABLE") && masks.at(BANDAGE).contains("TRADEABLE"));
		// X13-X15, X25-X26
		ASSERT_EQ(economy.mail.size(), 2u);
		EXPECT_EQ(economy.mail[0].byRace.at("ELYOS").second, 251) << "X13: 5 potions and 200 kinah";
		EXPECT_EQ(economy.mail[1].byRace.at("ELYOS").second, 23) << "a 10-kinah letter";
		ASSERT_TRUE(economy.recovery && economy.recovery->price && economy.recovery->question);
		EXPECT_EQ(*economy.recovery->price, 249) << "X15";
		ASSERT_FALSE(economy.cube.empty());
		EXPECT_EQ(economy.cube[0].price, std::optional<int64_t>(1000)) << "X26";
		ASSERT_TRUE(economy.removalPrice);
		EXPECT_EQ(economy.removalPrice->at("ELYOS"), 917) << "X25";
		// X23-X24, X27: the items of C14's seed
		const EconomyItem& tunic = economy.item(TUNIC);
		const EconomyItem& sword = economy.item(PLAINSMAN_SWORD);
		EXPECT_TRUE(tunic.canTune);
		EXPECT_EQ(tunic.seedTuneCountForUnidentified, std::optional<int32_t>(-1)) << "D5: the row must be written with tune_count -1";
		EXPECT_TRUE(tunic.equipPasses) << "C15: a level-4 Mage wears the robe piece";
		ASSERT_TRUE(tunic.startExpOfRequiredLevel);
		EXPECT_EQ(*tunic.startExpOfRequiredLevel, 3820) << "§10.1: players.exp = 3,820 makes B level 4";
		ASSERT_FALSE(tunic.socketing.empty());
		EXPECT_TRUE(tunic.socketing[0].fits && tunic.socketing[0].certain.value_or(false)) << "X24: rate 200 socket without randomness (D6)";
		EXPECT_TRUE(sword.breakable);
		EXPECT_EQ(sword.breakCountRange, (std::optional<std::array<int32_t, 2>>(std::array<int32_t, 2>{2, 5}))) << "X27: a weapon breaks into 2-5";
		// B's kinah for C16-C18: exactly the removal, the cube and the tools (§10.1 "Seeds"), so the tools leave 0 (X27)
		seedKinahB = economy.removalPrice->at("ELYOS") + *economy.cube[0].price + tools.buyKinah;
		EXPECT_EQ(seedKinahB, 917 + 1000 + 1412);

		// the ledgers of §10.1: A's kinah must stay above C13's price until C13 (1,000 -> 296 -> 196 -> 696 -> 399)
		ledgerA = starterLedger(warrior);
		ledgerB = starterLedger(mage);
		EXPECT_EQ(ledgerA[KINAH_ITEM], 1000);
		EXPECT_EQ(ledgerB[KINAH_ITEM], 1000);
		EXPECT_GE(ledgerA[LIFE_POTION], 100) << "C6-C11 take potions from A's starter stack";
		EXPECT_GE(ledgerA[BANDAGE], 20);
		EXPECT_GE(ledgerB[BANDAGE], BANDAGES_EXCHANGED);
		EXPECT_GE(ledgerA[JUICE], 1);
		const int64_t beforeRecovery = 1000 - elixirs.buyKinah + potions.sellKinah - *potions.repurchasePrice - KINAH_EXCHANGED +
		                               STORE_PRICE * STORE_COUNT - economy.mail[0].byRace.at("ELYOS").second - 2 * economy.mail[1].byRace.at("ELYOS").second;
		EXPECT_EQ(beforeRecovery, 399) << "§10.2 C12's ledger";
		EXPECT_GE(beforeRecovery, *economy.recovery->price) << "C13 needs A to afford the soul healing";
		std::cout << "C0: merchant band spot " << merchant.bandSpot.distance << " m (limit " << merchant.limit << ", without + 1 " << merchant.limitWithoutPlusOne
		          << "), elixirs " << elixirs.buyKinah << ", 10 potions sold for " << potions.sellKinah << ", tools " << tools.buyKinah << ", mail "
		          << economy.mail[0].byRace.at("ELYOS").second << "/" << economy.mail[1].byRace.at("ELYOS").second << ", recovery "
		          << *economy.recovery->price << ", cube " << *economy.cube[0].price << ", removal " << economy.removalPrice->at("ELYOS")
		          << "; B's seed kinah " << seedKinahB << std::endl;
	});

	// ---- C1: login, create A and B, seed both at the X2 spot while disconnected (F-3), enter world, level ready ----
	std::array<float, 3> bandSpot{};
	runCase("C1", "login, create the Warrior A and the Mage B, seed both at the X2 spot, enter world, level ready", [&] {
		const EconomyTalk& merchant = economy.talkOf(MERCHANT);
		bandSpot = {merchant.bandSpot.x, merchant.bandSpot.y, merchant.bandSpot.z};
		for (ScenarioClient* client : {&a, &b}) {
			const decoders::CharacterList list = logIn(servers, *client);
			EXPECT_EQ(list.characterCount, 0) << client->label << ": a fresh account must have no character";
			NewCharacter character;
			character.name = client->name;
			character.asmodian = false;
			character.playerClassId = client->classId;
			client->game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(client->key.accountId, client->account, character, 1));
			EXPECT_EQ(decoders::decodeCreateCharacter(expectNext(*client->game, "SM_CREATE_CHARACTER", client->async).data).responseCode,
			          RESPONSE_OPEN_CREATION_WINDOW);
			client->game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(client->key.accountId, client->account, character, 0));
			const decoders::CreateCharacter created = decoders::decodeCreateCharacter(expectNext(*client->game, "SM_CREATE_CHARACTER", client->async).data);
			if (created.responseCode != RESPONSE_OK || !created.player)
				throw std::runtime_error(client->label + ": creating the character answered response code " + std::to_string(created.responseCode));
			client->playerId = created.player->playerId;
			// §10.1 "Seeds": the position is a `players` column, and the account's copy of it is loaded at connect and saved at logout, so the
			// seed is written with the account disconnected (m5c0-client-session.md F-3)
			disconnect(*client);
		}
		for (ScenarioClient* client : {&a, &b})
			database.execute(schema, "UPDATE players SET x = " + std::to_string(bandSpot[0]) + ", y = " + std::to_string(bandSpot[1]) + ", z = " +
			                           std::to_string(bandSpot[2]) + " WHERE id = " + std::to_string(client->playerId));
		for (ScenarioClient* client : {&a, &b}) {
			const auto [list, burst] = relogIn(servers, *client);
			ASSERT_EQ(list.characters.size(), 1u);
			EXPECT_NEAR(list.characters[0].x, bandSpot[0], 0.01) << client->label << ": the seeded position is not in the character list";
			EXPECT_NEAR(client->x, bandSpot[0], 0.01) << client->label << ": SM_PLAYER_SPAWN is not at the seeded X2 spot";
			EXPECT_NEAR(client->y, bandSpot[1], 0.01);
			// the model against the oracle's starter inventory: every m5a-creation item, equipped where it says so
			const OracleCreation& creation = client == &a ? warrior : mage;
			const std::vector<std::string> differences = ledgerDifferences(ledgerOf(client->model), starterLedger(creation));
			EXPECT_TRUE(differences.empty()) << client->label << ": the enter-world SM_INVENTORY_INFO against m5a-creation (have/want): "
			                                 << join(differences) << "; it lists " << client->model.describe();
			EXPECT_TRUE(client->model.decodeFailures.empty()) << join(client->model.decodeFailures, "\n  ");
			// P6-Q prologue: the first enter world of a new character in Poeta locks 1100 and the first level ready starts 1000 and plays its
			// movie, which the client ends before C3's walk (CM_MOVE is dropped while it plays)
			expectPrologueMissionLocked(burst, decoders::ELYOS_PROLOGUE, client->label + " C1 (P6-Q prologue)");
			endPrologue(*client->game, client->lastLevelReady, decoders::ELYOS_PROLOGUE, 0, client->async,
				[&] { return collectBurst(*client->game, client->async); }, client->label + " C1 the prologue (1000)");
			client->model.sync();
		}
		a.drain();
	});

	// ---- C2: X1, SM_PRICES of both enter-world bursts ----
	runCase("C2", "SM_PRICES in both enter-world bursts (X1)", [&] {
		for (ScenarioClient* client : {&a, &b}) {
			const std::vector<decoders::Prices> prices =
			  decodeAll<decoders::Prices>(client->lastEnterWorld, "SM_PRICES", decoders::decodePrices, "X1");
			ASSERT_EQ(prices.size(), 1u) << client->label << ": one SM_PRICES per enter world: " << join(namesOf(client->lastEnterWorld));
			const std::array<int32_t, 3>& want = economy.smPrices.at("ELYOS");
			EXPECT_EQ(prices[0].globalPrices, want[0]) << "X1 (" << client->label << "): PricesService.getGlobalPrices";
			EXPECT_EQ(prices[0].globalPricesModifier, want[1]) << "X1 (" << client->label << "): getGlobalPricesModifier";
			EXPECT_EQ(prices[0].taxes, want[2]) << "X1 (" << client->label << "): getTaxes";
		}
	});

	// ---- C2b: chat (lane A's chat job, 2026-10-05; m5j-plan.md §3.7): A and B stand together after the enter world ----
	// CM_CHAT_MESSAGE_PUBLIC NORMAL reaches the sender and the one beside him; a whisper between two level-1 characters is refused by
	// gameserver.chat.whisper.level (10, custom.properties:20: a non-Daeva is capped at 9, m5j-plan.md §3.6), and a whisper to a name that is
	// not online answers STR_NO_SUCH_USER first. Written from the Java (CM_CHAT_MESSAGE_PUBLIC.java:43-97, 110-113;
	// CM_CHAT_MESSAGE_WHISPER.java:58-76; SM_MESSAGE.java:135-149 through decoders::decodeMessage). No later case depends on it.
	runCase("C2b", "chat: A says hello to B; A cannot whisper B below the whisper level; a whisper to a name not online", [&] {
		constexpr int32_t CM_CHAT_MESSAGE_PUBLIC = 27;       // ClientPacketInfo.gen.inc:39
		constexpr int32_t CM_CHAT_MESSAGE_WHISPER = 28;      // :40
		constexpr uint8_t CHAT_NORMAL = 0;                   // ChatType.java
		constexpr int32_t STR_NO_SUCH_USER = 1300627;        // SM_SYSTEM_MESSAGE.java:12191-12192
		constexpr int32_t STR_CANT_WHISPER_LEVEL = 1310004;  // :15249-15250
		a.game->send(CM_CHAT_MESSAGE_PUBLIC, network::test::PacketWriter().C(CHAT_NORMAL).S("Hello there").data);
		for (ScenarioClient* client : {&a, &b}) {
			const decoders::Message said = decoders::decodeMessage(waitFor(*client->game, "SM_MESSAGE").data);
			EXPECT_EQ(said.chatType, CHAT_NORMAL) << client->label;
			EXPECT_EQ(said.senderRace, 1) << client->label << ": an Elyos sender (race id 0 + 1), neither side staff";
			EXPECT_EQ(said.senderObjectId, a.playerId) << client->label;
			EXPECT_EQ(said.senderName, a.name) << client->label;
			EXPECT_EQ(said.message, "Hello there") << client->label;
		}
		const size_t bBefore = b.mark();
		a.game->send(CM_CHAT_MESSAGE_WHISPER, network::test::PacketWriter().S(b.name).S("psst").data);
		EXPECT_EQ(decoders::decodeSystemMessageId(waitFor(*a.game, "SM_SYSTEM_MESSAGE").data), STR_CANT_WHISPER_LEVEL)
		  << "sender level 1 < 10 and the receiver is no staff";
		a.game->send(CM_CHAT_MESSAGE_WHISPER, network::test::PacketWriter().S("Nobodyhere").S("psst").data);
		EXPECT_EQ(decoders::decodeSystemMessageId(waitFor(*a.game, "SM_SYSTEM_MESSAGE").data), STR_NO_SUCH_USER);
		b.drain(1s);
		EXPECT_TRUE(ofName(b.since(bBefore), "SM_MESSAGE").empty()) << "B was whispered nothing";
		a.drain();
	});

	// object ids of the npcs, from the SM_NPC_INFO the clients were sent
	const auto npcOf = [&](ScenarioClient& client, int32_t npcId) -> int32_t {
		const EconomyTalk& talk = economy.talkOf(npcId);
		const std::optional<int32_t> object = npcObject(client, npcId, talk.x, talk.y);
		if (!object)
			throw std::runtime_error(client.label + " was never sent the SM_NPC_INFO of npc " + std::to_string(npcId) + " at (" + std::to_string(talk.x) +
			                         ", " + std::to_string(talk.y) + ")");
		return *object;
	};
	const auto walkToSpot = [&](ScenarioClient& client, const EconomySpot& spot, float offsetX = 0) { walkTo(client, spot.x + offsetX, spot.y, spot.z); };

	// ---- C3: talking (X2, X3) ----
	runCase("C3", "A talks to 798007 from the band spot and from 10 m; B opens the postbox (X2, X3)", [&] {
		const EconomyTalk& merchantTalk = economy.talkOf(MERCHANT);
		const int32_t merchant = npcOf(a, MERCHANT);
		b.drain();
		size_t from = a.mark();
		a.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(merchant));
		collectFor(*a.game, STEP);
		std::vector<Packet> window = news(a, b.playerId, a.since(from));
		std::vector<decoders::DialogWindow> windows = dialogWindows(window, "X2");
		EXPECT_EQ(windows.size(), 1u) << "X2: from the band spot (" << merchantTalk.bandSpot.distance << " m, inside only with isInTalkRange's + 1) "
		                              << "one SM_DIALOG_WINDOW: " << join(namesOf(window)) << " (messages " << joinNumbers(messageIds(window)) << ")";
		if (!windows.empty()) {
			EXPECT_EQ(windows[0].targetObjectId, merchant);
			EXPECT_EQ(windows[0].dialogPageId, *merchantTalk.startWindow->page) << "X2: DialogPage.getStartPageId of a function npc";
			EXPECT_EQ(windows[0].questId, merchantTalk.startWindow->questId);
			EXPECT_EQ(windows[0].pageValue, merchantTalk.startWindow->pageValue);
		}
		EXPECT_EQ(window.size(), 1u) << "X2: SM_DIALOG_WINDOW and nothing else: " << join(namesOf(window));

		walkToSpot(a, merchantTalk.farSpot);
		from = a.mark();
		a.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(merchant));
		collectFor(*a.game, SILENCE);
		window = news(a, b.playerId, a.since(from));
		EXPECT_EQ(messageIds(window), std::vector<int32_t>{*merchantTalk.outOfRangeMessageId})
		  << "X2: from " << merchantTalk.farSpot.distance << " m STR_DIALOG_TOO_FAR_TO_TALK (NpcController.onDialogRequest): " << join(namesOf(window));
		EXPECT_TRUE(dialogWindows(window, "X2").empty()) << "X2: no SM_DIALOG_WINDOW out of range";
		a.game->send(GameSession::CM_CLOSE_DIALOG, GameSession::buildCM_CLOSE_DIALOG(merchant));
		collectFor(*a.game, 500ms);

		// B at the postbox (X3): PostboxAI sets the mailbox state and answers page MAIL with it
		const EconomyTalk& postboxTalk = economy.talkOf(POSTBOX);
		const int32_t postbox = npcOf(b, POSTBOX);
		walkToSpot(b, postboxTalk.nearSpot);
		from = b.mark();
		b.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(postbox));
		collectFor(*b.game, STEP);
		windows = dialogWindows(b.since(from), "X3");
		ASSERT_EQ(windows.size(), 1u) << "X3: the postbox answers one SM_DIALOG_WINDOW: " << join(namesOf(b.since(from)));
		EXPECT_EQ(windows[0].targetObjectId, postbox);
		EXPECT_EQ(windows[0].dialogPageId, decoders::DIALOG_PAGE_MAIL) << "X3: DialogPage.MAIL (PostboxAI.handleDialogStart)";
		EXPECT_EQ(windows[0].pageValue, decoders::MAILBOX_STATE_REGULAR) << "X3: the last writeH is the mailbox state REGULAR";
		// back beside A at the merchant for C4-C10 (C8's exchange needs 5 m)
		walkToSpot(a, merchantTalk.nearSpot);
		walkToSpot(b, merchantTalk.nearSpot, 1.5f);
		a.drain();
	});

	// ---- C4: a shop's windows (X4) ----
	runCase("C4", "A opens the merchant's BUY and SELL windows (X4)", [&] {
		const int32_t merchant = npcOf(a, MERCHANT);
		size_t from = a.mark();
		a.game->send(GameSession::CM_DIALOG_SELECT, GameSession::buildCM_DIALOG_SELECT(merchant, DIALOG_BUY));
		collectFor(*a.game, STEP);
		std::vector<Packet> window = a.since(from);
		const std::vector<decoders::TradeList> lists = decodeAll<decoders::TradeList>(window, "SM_TRADELIST", decoders::decodeTradeList, "X4");
		EXPECT_EQ(lists.size(), 1u) << "X4: BUY answers one SM_TRADELIST: " << join(namesOf(window));
		if (!lists.empty()) {
			EXPECT_EQ(lists[0].npcObjectId, merchant);
			EXPECT_EQ(lists[0].tradeNpcType, elixirs.tradeNpcTypeIndex) << "X4: TradeNpcType.index() (NORMAL is 1), never ordinal() (0)";
			EXPECT_EQ(lists[0].buyPriceModifier, elixirs.buyPriceModifier) << "X4: VENDOR_BUY_MODIFIER x sell_price_rate / 100";
			EXPECT_EQ(lists[0].showBuyTab, elixirs.tradeShowBuyTab) << "X4: Npc.canSell (W-03)";
			EXPECT_EQ(lists[0].showSellTab, elixirs.tradeShowSellTab) << "X4: Npc.canBuy";
			EXPECT_EQ(lists[0].tabs, elixirs.tradeTabs) << "X4: the tabs the player's legion level may see";
			EXPECT_EQ(lists[0].limitedItems.size(), elixirs.limitedItems);
		}
		EXPECT_TRUE(ofName(window, "SM_SELL_ITEM").empty());

		from = a.mark();
		a.game->send(GameSession::CM_DIALOG_SELECT, GameSession::buildCM_DIALOG_SELECT(merchant, DIALOG_SELL));
		collectFor(*a.game, STEP);
		window = a.since(from);
		const std::vector<decoders::SellItemWindow> sells = decodeAll<decoders::SellItemWindow>(window, "SM_SELL_ITEM", decoders::decodeSellItem, "X4");
		EXPECT_EQ(sells.size(), 1u) << "X4: SELL answers one SM_SELL_ITEM: " << join(namesOf(window));
		EXPECT_TRUE(ofName(window, "SM_TRADELIST").empty()) << "X4: SELL is not answered with SM_TRADELIST";
		if (!sells.empty()) {
			EXPECT_EQ(sells[0].npcObjectId, merchant);
			EXPECT_EQ(sells[0].tradeNpcType, elixirs.sellNpcTypeIndex) << "X4: the NORMAL fallback's index() (SM_SELL_ITEM.java:30)";
			EXPECT_EQ(sells[0].buyPriceRate, elixirs.sellBuyPriceRate) << "X4: getVendorSellModifier without a purchase template";
			EXPECT_EQ(sells[0].showBuyTab, elixirs.sellShowBuyTab);
			EXPECT_EQ(sells[0].showSellTab, elixirs.sellShowSellTab);
			EXPECT_EQ(sells[0].tabs, elixirs.sellTabs);
		}
	});

	// ---- C5: buying (X5, X6) ----
	runCase("C5", "A buys two elixirs, then an unlisted item and an elixir it cannot afford (X5, X6)", [&] {
		const int32_t merchant = npcOf(a, MERCHANT);
		const std::optional<int32_t> kinah = a.kinahObject();
		ASSERT_TRUE(kinah) << "A's model has no kinah item: " << a.model.describe();
		EXPECT_EQ(a.kinah(), ledgerA[KINAH_ITEM]) << "A's starter kinah";
		// every kinah row below is a DELTA from the kinah the client was last told, so a wrong price fails its own row and not every later one
		// (the "must stay green" half of §10.4); the absolute amounts are X16's, through the ledger
		const int64_t before = a.kinah();
		size_t from = a.mark();
		const std::array<GameSession::BuyItemEntry, 1> buy{{{LIFE_ELIXIR, ELIXIRS_BOUGHT}}};
		a.game->send(GameSession::CM_BUY_ITEM, GameSession::buildCM_BUY_ITEM(merchant, GameSession::TRADE_BUY, buy));
		collectFor(*a.game, STEP);
		std::vector<Packet> window = a.since(from);
		ledgerA[KINAH_ITEM] -= elixirs.buyKinah;
		ledgerA[LIFE_ELIXIR] += ELIXIRS_BOUGHT;
		EXPECT_EQ(kinahUpdates(window, *kinah, "X5"), std::vector<int64_t>{before - elixirs.buyKinah})
		  << "X5: exactly one kinah update, to " << before << " - " << elixirs.buyKinah << ": " << join(namesOf(window));
		const std::vector<decoders::InventoryItem> added = addedItems(window, "X5");
		EXPECT_EQ(added.size(), 1u) << "X5: one SM_INVENTORY_ADD_ITEM: " << join(namesOf(window));
		if (!added.empty()) {
			EXPECT_EQ(added[0].templateId, LIFE_ELIXIR);
			EXPECT_EQ(added[0].general ? added[0].general->count : -1, ELIXIRS_BOUGHT) << "X5: ItemService.addItem(BUY) of the count";
		}

		from = a.mark();
		const std::array<GameSession::BuyItemEntry, 1> unlisted{{{LIFE_POTION, 1}}};
		a.game->send(GameSession::CM_BUY_ITEM, GameSession::buildCM_BUY_ITEM(merchant, GameSession::TRADE_BUY, unlisted));
		collectFor(*a.game, SILENCE);
		window = a.since(from);
		EXPECT_EQ(messageIds(window), std::vector<int32_t>{STR_BUY_SELL_USER_BUY_FAILED}) << "X6: validateBuyItems refuses an unlisted item";
		EXPECT_TRUE(itemPackets(window).empty()) << "X6: nothing changes: " << join(itemPackets(window));

		const int64_t left = a.kinah();
		if (left >= elixirs.buyKinah / ELIXIRS_BOUGHT) {
			ADD_FAILURE() << "C5's third buy must be one A cannot afford, and A has " << left << " kinah: X6's second refusal is not tried";
			return;
		}
		from = a.mark();
		const std::array<GameSession::BuyItemEntry, 1> unaffordable{{{LIFE_ELIXIR, 1}}};
		a.game->send(GameSession::CM_BUY_ITEM, GameSession::buildCM_BUY_ITEM(merchant, GameSession::TRADE_BUY, unaffordable));
		collectFor(*a.game, SILENCE);
		window = a.since(from);
		EXPECT_EQ(messageIds(window), std::vector<int32_t>{STR_MSG_NOT_ENOUGH_MONEY}) << "X6: calculateBuyListPrice refuses before any change";
		EXPECT_TRUE(itemPackets(window).empty()) << "X6: nothing changes: " << join(itemPackets(window));
		EXPECT_EQ(a.kinah(), left);
	});

	// ---- C6: selling (X7) ----
	int32_t potionStackA = 0;
	int64_t potionStackCountA = 0; // the stack's count before the sale, as A's model holds it (X8 buys the potions back into it)
	runCase("C6", "A sells 10 potions, then the juice (X7)", [&] {
		const int32_t merchant = npcOf(a, MERCHANT);
		const int32_t kinah = a.kinahObject().value_or(0);
		potionStackA = a.stackOf(LIFE_POTION);
		ASSERT_NE(potionStackA, 0) << "A has no potion stack: " << a.model.describe();
		const int64_t stackBefore = a.model.byObjectId(potionStackA)->count;
		potionStackCountA = stackBefore;
		const int64_t before = a.kinah();
		size_t from = a.mark();
		const std::array<GameSession::BuyItemEntry, 1> sell{{{potionStackA, POTIONS_SOLD}}};
		a.game->send(GameSession::CM_BUY_ITEM, GameSession::buildCM_BUY_ITEM(merchant, GameSession::TRADE_SELL, sell));
		collectFor(*a.game, STEP);
		std::vector<Packet> window = a.since(from);
		ledgerA[KINAH_ITEM] += potions.sellKinah;
		ledgerA[LIFE_POTION] -= POTIONS_SOLD;
		EXPECT_EQ(kinahUpdates(window, kinah, "X7"), std::vector<int64_t>{before + potions.sellKinah}) << "X7: +" << potions.sellKinah << " (getSellReward x 10)";
		const std::vector<decoders::InventoryUpdateItem> stack = updatesOf(window, potionStackA, "X7");
		EXPECT_EQ(stack.size(), 1u) << "X7: the stack is decreased by SM_INVENTORY_UPDATE_ITEM, not deleted: " << join(namesOf(window));
		if (!stack.empty())
			EXPECT_EQ(stack[0].item.general ? stack[0].item.general->count : -1, stackBefore - POTIONS_SOLD) << "X7: the stack less the ten sold";
		EXPECT_FALSE(contains(deletedObjects(window, "X7"), potionStackA)) << "X7: decreaseItemCount, not delete";

		const int32_t juiceObject = a.stackOf(JUICE);
		ASSERT_NE(juiceObject, 0);
		from = a.mark();
		const std::array<GameSession::BuyItemEntry, 1> juiceSale{{{juiceObject, 1}}};
		a.game->send(GameSession::CM_BUY_ITEM, GameSession::buildCM_BUY_ITEM(merchant, GameSession::TRADE_SELL, juiceSale));
		collectFor(*a.game, SILENCE);
		window = a.since(from);
		EXPECT_EQ(messageIds(window), std::vector<int32_t>{STR_BUY_SELL_ITEM_CAN_NOT_BE_SELLED_TO_NPC}) << "X7: Item.isSellable";
		EXPECT_TRUE(itemPackets(window).empty()) << "X7: the juice sale changes nothing: " << join(itemPackets(window));
	});

	// ---- C7: buying back (X8) ----
	runCase("C7", "A opens the buy-back list and buys the potions back (X8)", [&] {
		const int32_t merchant = npcOf(a, MERCHANT);
		const int32_t kinah = a.kinahObject().value_or(0);
		size_t from = a.mark();
		a.game->send(GameSession::CM_DIALOG_SELECT, GameSession::buildCM_DIALOG_SELECT(merchant, DIALOG_BUY_AGAIN));
		collectFor(*a.game, STEP);
		std::vector<Packet> window = a.since(from);
		const std::vector<decoders::Repurchase> lists = decodeAll<decoders::Repurchase>(window, "SM_REPURCHASE", decoders::decodeRepurchase, "X8");
		ASSERT_EQ(lists.size(), 1u) << "X8: BUY_AGAIN answers one SM_REPURCHASE: " << join(namesOf(window));
		EXPECT_EQ(lists[0].targetObjectId, merchant);
		ASSERT_EQ(lists[0].items.size(), 1u) << "X8: exactly the last sale (the refused juice is no sale; the set is replaced per sale)";
		const decoders::RepurchaseEntry& entry = lists[0].items[0];
		EXPECT_EQ(entry.item.templateId, LIFE_POTION);
		EXPECT_EQ(entry.item.general ? entry.item.general->count : -1, POTIONS_SOLD) << "X8: the ten potions sold";
		EXPECT_EQ(entry.repurchasePrice, *potions.repurchasePrice) << "X8: the sale's reward, not the template price";
		EXPECT_NE(entry.item.objectId, potionStackA) << "a part of a stack is sold as a NEW item (TradeService.java:229-231)";

		const int64_t before = a.kinah();
		const int64_t potionsBefore = a.countOf(LIFE_POTION);
		from = a.mark();
		const std::array<GameSession::BuyItemEntry, 1> buyBack{{{entry.item.objectId, POTIONS_SOLD}}};
		a.game->send(GameSession::CM_BUY_ITEM, GameSession::buildCM_BUY_ITEM(merchant, GameSession::TRADE_REPURCHASE, buyBack));
		collectFor(*a.game, STEP);
		window = a.since(from);
		ledgerA[KINAH_ITEM] -= *potions.repurchasePrice;
		ledgerA[LIFE_POTION] += POTIONS_SOLD;
		EXPECT_EQ(kinahUpdates(window, kinah, "X8"), std::vector<int64_t>{before - *potions.repurchasePrice}) << "X8: -" << *potions.repurchasePrice;
		EXPECT_EQ(a.countOf(LIFE_POTION), potionsBefore + POTIONS_SOLD) << "X8: the potions back: " << a.model.describe();
		const std::vector<decoders::InventoryUpdateItem> stack = updatesOf(window, potionStackA, "X8");
		EXPECT_FALSE(stack.empty()) << "X8: the bought-back potions merge into the stack (ItemService.addItem): " << join(namesOf(window));
		if (!stack.empty())
			EXPECT_EQ(stack.back().item.general ? stack.back().item.general->count : -1, potionStackCountA) << "X8: the stack back to its C6 count";
	});

	// the disjointness of X16, checked after every step of C8-C13
	std::vector<std::string> sharedObjects;
	const auto checkDisjoint = [&](std::string_view when) {
		a.model.sync();
		b.model.sync();
		for (const auto& [id, item] : a.model.items)
			if (b.model.items.contains(id))
				sharedObjects.push_back(std::string(when) + ": object " + std::to_string(id) + " (" + std::to_string(item.itemId) + ") in both");
	};
	const auto exchangeStep = [&](ScenarioClient& actor, ScenarioClient& partner, int32_t opcode, const std::vector<uint8_t>& body,
	                              std::string_view when) {
		const size_t actorFrom = actor.mark(), partnerFrom = partner.mark();
		actor.game->send(opcode, body);
		collectFor(*actor.game, 1000ms);
		collectFor(*partner.game, 500ms);
		checkDisjoint(when);
		return std::pair<std::vector<Packet>, std::vector<Packet>>{actor.since(actorFrom), partner.since(partnerFrom)};
	};
	/** A asks, B answers yes: both get SM_EXCHANGE_REQUEST with the other's name. @return B's question window */
	const auto openExchange = [&](std::string_view row) {
		const auto [askA, askB] = exchangeStep(a, b, GameSession::CM_EXCHANGE_REQUEST, GameSession::buildCM_EXCHANGE_REQUEST(b.playerId), row);
		const std::vector<decoders::QuestionWindow> questions = questionWindows(askB, row);
		if (questions.size() != 1)
			throw std::runtime_error(std::string(row) + ": B got " + std::to_string(questions.size()) + " SM_QUESTION_WINDOW for the request: " +
			                         join(namesOf(askB)));
		EXPECT_EQ(questions[0].code, QUESTION_EXCHANGE) << row << ": STR_EXCHANGE_DO_YOU_ACCEPT_EXCHANGE";
		EXPECT_EQ(questions[0].params[0], a.name) << row << ": the question names the requester";
		EXPECT_TRUE(contains(messageIds(askA), STR_EXCHANGE_ASKED_EXCHANGE_TO_HIM)) << row << ": " << join(namesOf(askA));
		const auto [yesB, yesA] = exchangeStep(b, a, GameSession::CM_QUESTION_RESPONSE,
		                                       GameSession::buildCM_QUESTION_RESPONSE(QUESTION_EXCHANGE, GameSession::ANSWER_YES), row);
		const std::vector<std::string> toA = decodeAll<std::string>(yesA, "SM_EXCHANGE_REQUEST", decoders::decodeExchangeRequest, row);
		const std::vector<std::string> toB = decodeAll<std::string>(yesB, "SM_EXCHANGE_REQUEST", decoders::decodeExchangeRequest, row);
		EXPECT_EQ(toA, std::vector<std::string>{b.name}) << row << ": registerExchange sends A the partner's name: " << join(namesOf(yesA));
		EXPECT_EQ(toB, std::vector<std::string>{a.name}) << row << ": and B the requester's: " << join(namesOf(yesB));
		return questions[0];
	};

	// ---- C8: an exchange (X9, X10) ----
	runCase("C8", "A and B exchange potions and kinah for bandages; the juice is refused (X9, X10)", [&] {
		b.drain();
		checkDisjoint("C8 start");
		// X10 is read as deltas of what each client was told before the exchange, so a wrong vendor price of C5-C7 cannot fail it
		const int64_t kinahBeforeA = a.kinah(), kinahBeforeB = b.kinah();
		const int64_t potionsBeforeA = a.countOf(LIFE_POTION), potionsBeforeB = b.countOf(LIFE_POTION);
		const int64_t bandagesBeforeA = a.countOf(BANDAGE), bandagesBeforeB = b.countOf(BANDAGE);
		const size_t fromA = a.mark(), fromB = b.mark();
		openExchange("X9");
		const int32_t potionStack = a.stackOf(LIFE_POTION);
		ASSERT_NE(potionStack, 0) << "A has no potion stack to offer: " << a.model.describe();
		const int64_t stackCount = a.model.byObjectId(potionStack)->count;
		// A adds 10 potions: the fake stack update to 90 (PUT_TO_EXCHANGE) and SM_EXCHANGE_ADD_ITEM to both
		auto [addA, addB] = exchangeStep(a, b, GameSession::CM_EXCHANGE_ADD_ITEM,
		                                 GameSession::buildCM_EXCHANGE_ADD_ITEM(potionStack, static_cast<int32_t>(POTIONS_EXCHANGED)), "C8 add potions");
		const std::vector<decoders::InventoryUpdateItem> fake = updatesOf(addA, potionStack, "X9");
		EXPECT_EQ(fake.size(), 1u) << "X9: A's fake stack update: " << join(namesOf(addA));
		if (!fake.empty())
			EXPECT_EQ(fake[0].item.general ? fake[0].item.general->count : -1, stackCount - POTIONS_EXCHANGED) << "X9: the stack shown at 90";
		std::vector<decoders::ExchangeAddItem> addedA = decodeAll<decoders::ExchangeAddItem>(addA, "SM_EXCHANGE_ADD_ITEM", decoders::decodeExchangeAddItem, "X9");
		std::vector<decoders::ExchangeAddItem> addedB = decodeAll<decoders::ExchangeAddItem>(addB, "SM_EXCHANGE_ADD_ITEM", decoders::decodeExchangeAddItem, "X9");
		ASSERT_EQ(addedA.size(), 1u) << join(namesOf(addA));
		ASSERT_EQ(addedB.size(), 1u) << join(namesOf(addB));
		EXPECT_EQ(addedA[0].action, decoders::EXCHANGE_SELF);
		EXPECT_EQ(addedA[0].item.templateId, LIFE_POTION);
		EXPECT_EQ(addedB[0].action, decoders::EXCHANGE_OTHER);
		EXPECT_EQ(addedB[0].item.templateId, LIFE_POTION);
		EXPECT_EQ(addedB[0].item.general ? addedB[0].item.general->count : -1, POTIONS_EXCHANGED);
		// A adds 100 kinah
		auto [kinahA, kinahB] = exchangeStep(a, b, GameSession::CM_EXCHANGE_ADD_KINAH, GameSession::buildCM_EXCHANGE_ADD_KINAH(KINAH_EXCHANGED),
		                                     "C8 add kinah");
		EXPECT_EQ(decodeAll<decoders::ExchangeAddKinah>(kinahA, "SM_EXCHANGE_ADD_KINAH", decoders::decodeExchangeAddKinah, "X9"),
		          (std::vector<decoders::ExchangeAddKinah>{{decoders::EXCHANGE_SELF, KINAH_EXCHANGED}}));
		EXPECT_EQ(decodeAll<decoders::ExchangeAddKinah>(kinahB, "SM_EXCHANGE_ADD_KINAH", decoders::decodeExchangeAddKinah, "X9"),
		          (std::vector<decoders::ExchangeAddKinah>{{decoders::EXCHANGE_OTHER, KINAH_EXCHANGED}}));
		// B adds 5 bandages
		const int32_t bandageStack = b.stackOf(BANDAGE);
		auto [bandB, bandA] = exchangeStep(b, a, GameSession::CM_EXCHANGE_ADD_ITEM,
		                                   GameSession::buildCM_EXCHANGE_ADD_ITEM(bandageStack, static_cast<int32_t>(BANDAGES_EXCHANGED)), "C8 add bandages");
		addedA = decodeAll<decoders::ExchangeAddItem>(bandA, "SM_EXCHANGE_ADD_ITEM", decoders::decodeExchangeAddItem, "X9");
		addedB = decodeAll<decoders::ExchangeAddItem>(bandB, "SM_EXCHANGE_ADD_ITEM", decoders::decodeExchangeAddItem, "X9");
		ASSERT_EQ(addedA.size(), 1u);
		ASSERT_EQ(addedB.size(), 1u);
		EXPECT_EQ(addedA[0].action, decoders::EXCHANGE_OTHER);
		EXPECT_EQ(addedA[0].item.templateId, BANDAGE);
		EXPECT_EQ(addedB[0].action, decoders::EXCHANGE_SELF);
		// A tries the juice: not tradeable, so nothing to either (ExchangeService.addItem's first check)
		auto [juiceA, juiceB] = exchangeStep(a, b, GameSession::CM_EXCHANGE_ADD_ITEM, GameSession::buildCM_EXCHANGE_ADD_ITEM(a.stackOf(JUICE), 1),
		                                     "C8 add juice");
		collectFor(*a.game, 400ms);
		collectFor(*b.game, 400ms);
		EXPECT_TRUE(ofName(juiceA, "SM_EXCHANGE_ADD_ITEM").empty() && ofName(juiceB, "SM_EXCHANGE_ADD_ITEM").empty())
		  << "X9: the untradeable juice appears to neither partner: " << join(namesOf(juiceA)) << " / " << join(namesOf(juiceB));
		for (ScenarioClient* client : {&a, &b})
			for (const decoders::ExchangeAddItem& added :
			     decodeAll<decoders::ExchangeAddItem>(client->since(client == &a ? fromA : fromB), "SM_EXCHANGE_ADD_ITEM", decoders::decodeExchangeAddItem, "X9"))
				EXPECT_NE(added.item.templateId, JUICE) << "X9: " << client->label << " was shown the juice (ExchangeService.addItem's isTradeable check)";
		EXPECT_TRUE(itemPackets(juiceA).empty()) << "X9: the juice is not taken out of A's cube: " << join(namesOf(juiceA));
		// both lock: (3) to the partner of each lock
		auto [lockA, lockAtB] = exchangeStep(a, b, GameSession::CM_EXCHANGE_LOCK, GameSession::buildCM_EXCHANGE_LOCK(), "C8 lock A");
		EXPECT_EQ(exchangeConfirmations(lockAtB, "X9"), std::vector<uint8_t>{decoders::EXCHANGE_CONFIRMATION_PARTNER_LOCKED}) << "X9: A's lock to B";
		EXPECT_TRUE(exchangeConfirmations(lockA, "X9").empty()) << "X9: nothing to the one who locked";
		auto [lockB, lockAtA] = exchangeStep(b, a, GameSession::CM_EXCHANGE_LOCK, GameSession::buildCM_EXCHANGE_LOCK(), "C8 lock B");
		EXPECT_EQ(exchangeConfirmations(lockAtA, "X9"), std::vector<uint8_t>{decoders::EXCHANGE_CONFIRMATION_PARTNER_LOCKED}) << "X9: B's lock to A";
		EXPECT_TRUE(exchangeConfirmations(lockB, "X9").empty());
		// A's OK: (2) to B only
		auto [okA, okAtB] = exchangeStep(a, b, GameSession::CM_EXCHANGE_OK, GameSession::buildCM_EXCHANGE_OK(), "C8 ok A");
		EXPECT_EQ(exchangeConfirmations(okAtB, "X9"), std::vector<uint8_t>{decoders::EXCHANGE_CONFIRMATION_PARTNER_CONFIRMED}) << "X9: A's OK to B";
		EXPECT_TRUE(exchangeConfirmations(okA, "X9").empty()) << "X9: (2) goes to the partner, never to self; performTrade waits for both OKs";
		EXPECT_TRUE(itemPackets(okA).empty() && itemPackets(okAtB).empty()) << "X9: no trade on the first OK";
		// B's OK: (2) to A, then the trade and (0) to both
		auto [okB, okAtA] = exchangeStep(b, a, GameSession::CM_EXCHANGE_OK, GameSession::buildCM_EXCHANGE_OK(), "C8 ok B");
		collectFor(*a.game, 400ms);
		collectFor(*b.game, 400ms);
		checkDisjoint("C8 trade");
		EXPECT_EQ(exchangeConfirmations(okAtA, "X9"),
		          (std::vector<uint8_t>{decoders::EXCHANGE_CONFIRMATION_PARTNER_CONFIRMED, decoders::EXCHANGE_CONFIRMATION_DONE}))
		  << "X9: B's OK sends A (2) before the partner check, then the trade (0) (ExchangeService.java:227-231, 254-255): " << join(namesOf(okAtA));
		EXPECT_EQ(exchangeConfirmations(okB, "X9"), std::vector<uint8_t>{decoders::EXCHANGE_CONFIRMATION_DONE}) << "X9: (0) to B: " << join(namesOf(okB));
		// X9 as a whole: every confirmation each partner got, in order
		EXPECT_EQ(exchangeConfirmations(a.since(fromA), "X9"),
		          (std::vector<uint8_t>{decoders::EXCHANGE_CONFIRMATION_PARTNER_LOCKED, decoders::EXCHANGE_CONFIRMATION_PARTNER_CONFIRMED,
		                                decoders::EXCHANGE_CONFIRMATION_DONE}));
		EXPECT_EQ(exchangeConfirmations(b.since(fromB), "X9"),
		          (std::vector<uint8_t>{decoders::EXCHANGE_CONFIRMATION_PARTNER_LOCKED, decoders::EXCHANGE_CONFIRMATION_PARTNER_CONFIRMED,
		                                decoders::EXCHANGE_CONFIRMATION_DONE}));

		// X10: the ledgers after the trade, from the packets
		ledgerA[LIFE_POTION] -= POTIONS_EXCHANGED;
		ledgerA[KINAH_ITEM] -= KINAH_EXCHANGED;
		ledgerA[BANDAGE] += BANDAGES_EXCHANGED;
		ledgerB[LIFE_POTION] += POTIONS_EXCHANGED;
		ledgerB[KINAH_ITEM] += KINAH_EXCHANGED;
		ledgerB[BANDAGE] -= BANDAGES_EXCHANGED;
		// the removals themselves: the fake PUT_TO_EXCHANGE updates (0x25) already showed 90 and 15, so only the trade's own decreaseItemCount
		// packets (ExchangeService.removeItemsFromInventory -> Storage.decreaseItemCount, DEC_ITEM_USE 0x16, Storage.java:126-147) tell a real
		// removal from a dupe that leaves the giver's stack whole - whichever OK the trade ran on
		const auto removal = [](const std::vector<Packet>& window, int32_t stack, int64_t count) {
			return std::ranges::any_of(updatesOf(window, stack, "X10"), [count](const decoders::InventoryUpdateItem& update) {
				return update.updateTypeMask == std::optional<uint16_t>(decoders::ITEM_UPDATE_DEC_ITEM_USE) && update.item.general &&
				       update.item.general->count == count;
			});
		};
		EXPECT_TRUE(removal(a.since(fromA), potionStack, stackCount - POTIONS_EXCHANGED)) << "X10: the trade decreases A's potion stack itself";
		EXPECT_TRUE(removal(b.since(fromB), bandageStack, bandagesBeforeB - BANDAGES_EXCHANGED)) << "X10: the trade decreases B's bandage stack itself";
		EXPECT_EQ(a.countOf(LIFE_POTION), potionsBeforeA - POTIONS_EXCHANGED) << "X10: A's potions (removeItemsFromInventory): " << a.model.describe();
		EXPECT_EQ(a.countOf(BANDAGE), bandagesBeforeA + BANDAGES_EXCHANGED) << "X10: A's bandages (putItemToInventory)";
		EXPECT_EQ(a.kinah(), kinahBeforeA - KINAH_EXCHANGED) << "X10: A's kinah";
		EXPECT_EQ(b.countOf(LIFE_POTION), potionsBeforeB + POTIONS_EXCHANGED) << "X10: B's potions: " << b.model.describe();
		EXPECT_EQ(b.countOf(BANDAGE), bandagesBeforeB - BANDAGES_EXCHANGED) << "X10: B's bandages";
		EXPECT_EQ(b.kinah(), kinahBeforeB + KINAH_EXCHANGED) << "X10: B's kinah";
	});

	// ---- C9: a cancelled exchange, and a partner who quits (X11) ----
	runCase("C9", "a cancelled exchange, one whose partner quits, and a new request after the partner's return (X11)", [&] {
		const int64_t kinahBeforeA = a.kinah(), kinahBeforeB = b.kinah();
		const int64_t potionsBeforeA = a.countOf(LIFE_POTION), potionsBeforeB = b.countOf(LIFE_POTION);
		openExchange("X11");
		const int32_t potionStack = a.stackOf(LIFE_POTION);
		ASSERT_NE(potionStack, 0) << a.model.describe();
		const int64_t stackCount = a.model.byObjectId(potionStack)->count;
		exchangeStep(a, b, GameSession::CM_EXCHANGE_ADD_ITEM, GameSession::buildCM_EXCHANGE_ADD_ITEM(potionStack, static_cast<int32_t>(POTIONS_EXCHANGED)),
		             "C9 add");
		EXPECT_EQ(a.model.byObjectId(potionStack)->count, stackCount - POTIONS_EXCHANGED) << "the fake update of the added part";
		auto [cancelB, cancelA] = exchangeStep(b, a, GameSession::CM_EXCHANGE_CANCEL, GameSession::buildCM_EXCHANGE_CANCEL(), "C9 cancel");
		collectFor(*a.game, 400ms);
		EXPECT_EQ(exchangeConfirmations(cancelA, "X11"), std::vector<uint8_t>{decoders::EXCHANGE_CONFIRMATION_CANCELLED})
		  << "X11: the partner of the cancel gets (1): " << join(namesOf(cancelA));
		const std::vector<decoders::InventoryUpdateItem> back = updatesOf(cancelA, potionStack, "X11");
		EXPECT_EQ(back.size(), 1u) << "X11: returnItems gives A its part back (GET_BACK): " << join(namesOf(cancelA));
		if (!back.empty())
			EXPECT_EQ(back[0].item.general ? back[0].item.general->count : -1, stackCount) << "X11: the whole stack again";
		EXPECT_TRUE(itemPackets(cancelB).empty() && exchangeConfirmations(cancelB, "X11").empty())
		  << "X11: B added nothing and gets nothing: " << join(namesOf(cancelB));
		EXPECT_EQ(a.countOf(LIFE_POTION), potionsBeforeA) << "X11: the ledgers are unchanged";
		EXPECT_EQ(a.kinah(), kinahBeforeA);
		EXPECT_EQ(b.countOf(LIFE_POTION), potionsBeforeB);
		EXPECT_EQ(b.kinah(), kinahBeforeB);

		// a third exchange; B quits the game mid-exchange (CM_QUIT(0)): the logout cancels it (PlayerLeaveWorldService.java:85)
		openExchange("X11");
		const size_t fromA = a.mark();
		disconnect(b);
		collectFor(*a.game, STEP);
		checkDisjoint("C9 quit");
		EXPECT_EQ(exchangeConfirmations(a.since(fromA), "X11"), std::vector<uint8_t>{decoders::EXCHANGE_CONFIRMATION_CANCELLED})
		  << "X11: the partner who quits cancels the exchange: " << join(namesOf(a.since(fromA)));
		relogIn(servers, b);
		EXPECT_EQ(b.countOf(LIFE_POTION), potionsBeforeB) << "B's re-entry inventory";
		EXPECT_EQ(b.kinah(), kinahBeforeB);
		a.drain();
		checkDisjoint("C9 re-entry");
		// X11: after B's return a new exchange opens - a stale entry in ExchangeService.exchanges would make isTrading refuse registerExchange
		openExchange("X11 (after the return)");
		auto [cancelA2, cancelAtB] = exchangeStep(a, b, GameSession::CM_EXCHANGE_CANCEL, GameSession::buildCM_EXCHANGE_CANCEL(), "C9 cancel 2");
		EXPECT_EQ(exchangeConfirmations(cancelAtB, "X11"), std::vector<uint8_t>{decoders::EXCHANGE_CONFIRMATION_CANCELLED});
		(void)cancelA2;
	});

	// ---- C10: a private store (X12) ----
	runCase("C10", "A opens a private store of 5 potions; B buys 3, then 2, and the store closes (X12)", [&] {
		a.drain();
		b.drain();
		const int32_t potionStack = a.stackOf(LIFE_POTION);
		const int64_t stackCount = a.model.byObjectId(potionStack)->count;
		const int32_t kinahA = a.kinahObject().value_or(0), kinahB = b.kinahObject().value_or(0);
		size_t fromA = a.mark(), fromB = b.mark();
		const std::array<GameSession::PrivateStoreItem, 1> store{{{potionStack, LIFE_POTION, STORE_COUNT, STORE_PRICE}}};
		a.game->send(GameSession::CM_PRIVATE_STORE, GameSession::buildCM_PRIVATE_STORE(store));
		collectFor(*a.game, STEP);
		a.game->send(GameSession::CM_PRIVATE_STORE_NAME, GameSession::buildCM_PRIVATE_STORE_NAME("m5c"));
		collectFor(*a.game, STEP);
		collectFor(*b.game, 500ms);
		for (ScenarioClient* client : {&a, &b}) {
			const std::vector<Packet> window = client->since(client == &a ? fromA : fromB);
			const std::vector<decoders::Emotion> opened = emotions(window, "X12");
			EXPECT_TRUE(std::ranges::any_of(opened, [&](const decoders::Emotion& e) {
				return e.senderObjectId == a.playerId && e.emotionType == EMOTION_OPEN_PRIVATESHOP;
			})) << "X12: " << client->label << " gets SM_EMOTION(A, OPEN_PRIVATESHOP): " << join(namesOf(window));
			const std::vector<decoders::PrivateStoreName> names =
			  decodeAll<decoders::PrivateStoreName>(window, "SM_PRIVATE_STORE_NAME", decoders::decodePrivateStoreName, "X12");
			EXPECT_EQ(names, (std::vector<decoders::PrivateStoreName>{{a.playerId, "m5c"}})) << "X12: " << client->label << " gets the store's name";
		}
		fromB = b.mark();
		b.game->send(GameSession::CM_DIALOG_SELECT, GameSession::buildCM_DIALOG_SELECT(a.playerId, DIALOG_BUY));
		collectFor(*b.game, STEP);
		const std::vector<decoders::PrivateStore> stores = decodeAll<decoders::PrivateStore>(b.since(fromB), "SM_PRIVATE_STORE", decoders::decodePrivateStore, "X12");
		ASSERT_EQ(stores.size(), 1u) << "X12: B's BUY on A answers SM_PRIVATE_STORE: " << join(namesOf(b.since(fromB)));
		EXPECT_TRUE(stores[0].present);
		EXPECT_EQ(stores[0].sellerObjectId, a.playerId);
		ASSERT_EQ(stores[0].items.size(), 1u);
		EXPECT_EQ(stores[0].items[0].itemObjectId, potionStack);
		EXPECT_EQ(stores[0].items[0].itemId, LIFE_POTION);
		EXPECT_EQ(stores[0].items[0].count, STORE_COUNT);
		EXPECT_EQ(stores[0].items[0].price, STORE_PRICE) << "X12: the price of ONE item";

		struct Purchase {
			std::vector<Packet> seller, buyer;
			int64_t sellerKinah = 0, buyerKinah = 0, buyerPotions = 0;
		};
		const auto buy = [&](int64_t count, std::string_view when) {
			Purchase purchase;
			purchase.sellerKinah = a.kinah();
			purchase.buyerKinah = b.kinah();
			purchase.buyerPotions = b.countOf(LIFE_POTION);
			const size_t atA = a.mark(), atB = b.mark();
			const std::array<GameSession::BuyItemEntry, 1> entries{{{0, count}}}; // the store's index 0, not an object id (PrivateStoreService.java:209)
			b.game->send(GameSession::CM_BUY_ITEM, GameSession::buildCM_BUY_ITEM(a.playerId, GameSession::TRADE_PRIVATE_STORE, entries));
			collectFor(*b.game, STEP);
			collectFor(*a.game, 600ms);
			checkDisjoint(when);
			ledgerA[LIFE_POTION] -= count;
			ledgerA[KINAH_ITEM] += count * STORE_PRICE;
			ledgerB[LIFE_POTION] += count;
			ledgerB[KINAH_ITEM] -= count * STORE_PRICE;
			purchase.seller = a.since(atA);
			purchase.buyer = b.since(atB);
			return purchase;
		};
		const Purchase first = buy(STORE_FIRST_BUY, "C10 buy 3");
		EXPECT_EQ(kinahUpdates(first.buyer, kinahB, "X12"), std::vector<int64_t>{first.buyerKinah - STORE_FIRST_BUY * STORE_PRICE})
		  << "X12: B -300 (the price times the count)";
		EXPECT_EQ(kinahUpdates(first.seller, kinahA, "X12"), std::vector<int64_t>{first.sellerKinah + STORE_FIRST_BUY * STORE_PRICE}) << "X12: A +300";
		EXPECT_EQ(b.countOf(LIFE_POTION), first.buyerPotions + STORE_FIRST_BUY) << "X12: B +3 potions";
		const std::vector<decoders::InventoryUpdateItem> sold = updatesOf(first.seller, potionStack, "X12");
		EXPECT_EQ(sold.size(), 1u) << "X12: the seller's stack is decreased: " << join(namesOf(first.seller));
		if (!sold.empty())
			EXPECT_EQ(sold[0].item.general ? sold[0].item.general->count : -1, stackCount - STORE_FIRST_BUY) << "X12: A -3";
		EXPECT_EQ(messageIds(first.seller), std::vector<int32_t>{STR_MSG_PERSONAL_SHOP_SELL_ITEM_MULTI}) << "X12: the seller's message for a count above 1";
		EXPECT_FALSE(std::ranges::any_of(emotions(first.seller, "X12"), [](const decoders::Emotion& e) { return e.emotionType == EMOTION_CLOSE_PRIVATESHOP; }))
		  << "X12: the store stays open with 2 left";

		const Purchase second = buy(STORE_SECOND_BUY, "C10 buy 2");
		EXPECT_EQ(kinahUpdates(second.buyer, kinahB, "X12"), std::vector<int64_t>{second.buyerKinah - STORE_SECOND_BUY * STORE_PRICE}) << "X12: B -200";
		EXPECT_EQ(kinahUpdates(second.seller, kinahA, "X12"), std::vector<int64_t>{second.sellerKinah + STORE_SECOND_BUY * STORE_PRICE}) << "X12: A +200";
		EXPECT_EQ(b.countOf(LIFE_POTION), second.buyerPotions + STORE_SECOND_BUY) << "X12: B +2 potions";
		EXPECT_TRUE(contains(messageIds(second.seller), STR_MSG_PERSONAL_SHOP_SELL_ITEM_MULTI) &&
		            !contains(messageIds(second.seller), STR_MSG_PERSONAL_SHOP_SELL_ITEM));
		for (const std::vector<Packet>* window : {&second.seller, &second.buyer})
			EXPECT_TRUE(std::ranges::any_of(emotions(*window, "X12"), [&](const decoders::Emotion& e) {
				return e.senderObjectId == a.playerId && e.emotionType == EMOTION_CLOSE_PRIVATESHOP;
			})) << "X12: the empty store closes: SM_EMOTION(A, CLOSE_PRIVATESHOP) to " << (window == &second.seller ? "A" : "B") << ": "
			    << join(namesOf(*window));
	});

	// ---- C11: mail, online (X3, X13) ----
	int32_t mailedPotionsObject = 0;
	runCase("C11", "B opens the postbox, A mails 5 potions and 200 kinah, B takes both and deletes the letter; a kinah letter (X3, X13)", [&] {
		const EconomyTalk& postboxTalk = economy.talkOf(POSTBOX);
		walkToSpot(b, postboxTalk.nearSpot);
		walkToSpot(a, postboxTalk.nearSpot, 1.0f);
		b.drain();
		const int32_t postbox = npcOf(b, POSTBOX);
		// B first: C9's relog gave B a new Mailbox at state 0 (MailDAO.java:34, Mailbox.java:25), which updateRecipientMailbox would not refresh
		size_t fromB = b.mark();
		b.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(postbox));
		collectFor(*b.game, STEP);
		std::vector<decoders::DialogWindow> windows = dialogWindows(b.since(fromB), "X3");
		ASSERT_EQ(windows.size(), 1u) << join(namesOf(b.since(fromB)));
		EXPECT_EQ(windows[0].pageValue, decoders::MAILBOX_STATE_REGULAR) << "X3: the reopened postbox after the relog";
		size_t fromA = a.mark();
		a.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(npcOf(a, POSTBOX)));
		collectFor(*a.game, STEP);
		windows = dialogWindows(a.since(fromA), "X3");
		ASSERT_EQ(windows.size(), 1u) << join(namesOf(a.since(fromA)));
		EXPECT_EQ(windows[0].pageValue, decoders::MAILBOX_STATE_REGULAR);

		const int32_t potionStack = a.stackOf(LIFE_POTION);
		const int64_t stackCount = a.model.byObjectId(potionStack)->count;
		const int32_t kinahA = a.kinahObject().value_or(0), kinahB = b.kinahObject().value_or(0);
		int64_t beforeA = a.kinah();
		fromA = a.mark();
		fromB = b.mark();
		a.game->send(GameSession::CM_SEND_MAIL, GameSession::buildCM_SEND_MAIL(b.name, "m5c", "gate", potionStack, MAILED_POTIONS, MAILED_KINAH));
		collectFor(*a.game, STEP);
		collectFor(*b.game, 600ms);
		checkDisjoint("C11 send");
		const int64_t firstCost = economy.mail[0].byRace.at("ELYOS").second;
		ledgerA[KINAH_ITEM] -= firstCost;
		ledgerA[LIFE_POTION] -= MAILED_POTIONS;
		std::vector<Packet> window = a.since(fromA);
		std::vector<decoders::MailService> mail = mailServices(window, "X13");
		ASSERT_EQ(mail.size(), 1u) << "X13: the sender gets one SM_MAIL_SERVICE: " << join(namesOf(window));
		EXPECT_EQ(mail[0].serviceId, decoders::MAIL_SERVICE_MESSAGE);
		EXPECT_EQ(mail[0].mailMessage, std::optional<uint8_t>(decoders::MAIL_MESSAGE_SEND_SUCCESS));
		EXPECT_EQ(kinahUpdates(window, kinahA, "X13"), std::vector<int64_t>{beforeA - firstCost}) << "X13: -" << firstCost << " (commission + service)";
		EXPECT_EQ(a.model.byObjectId(potionStack)->count, stackCount - MAILED_POTIONS) << "X13: the stack -5";
		// B, looking into its mailbox: the notice and the list refresh (SystemMailService.java:126-132)
		window = b.since(fromB);
		mail = mailServices(window, "X3");
		ASSERT_GE(mail.size(), 2u) << "X3: the first letter reaches B with a list refresh: " << join(namesOf(window));
		EXPECT_EQ(mail[0].serviceId, decoders::MAIL_SERVICE_MAILBOX_STATE);
		ASSERT_TRUE(mail[0].mailboxState);
		EXPECT_EQ(mail[0].mailboxState->total, 1) << "X13: total 1";
		EXPECT_EQ(mail[0].mailboxState->unread, 1) << "X13: unread 1";
		EXPECT_EQ(mail[1].serviceId, decoders::MAIL_SERVICE_LETTER_LIST) << "X3: the refresh follows the notice";

		// B's list, read, both attachments, the list again, delete
		fromB = b.mark();
		b.game->send(GameSession::CM_CHECK_MAIL_LIST, GameSession::buildCM_CHECK_MAIL_LIST(false));
		collectFor(*b.game, STEP);
		mail = mailServices(b.since(fromB), "X13");
		ASSERT_FALSE(mail.empty());
		ASSERT_TRUE(mail.back().letterList) << join(namesOf(b.since(fromB)));
		ASSERT_EQ(mail.back().letterList->letters.size(), 1u);
		const decoders::LetterListEntry letter = mail.back().letterList->letters[0];
		EXPECT_EQ(letter.senderName, a.name);
		EXPECT_EQ(letter.title, "m5c");
		EXPECT_FALSE(letter.read) << "X13: unread";
		EXPECT_EQ(letter.attachedItemTemplateId, LIFE_POTION);
		EXPECT_NE(letter.attachedItemObjectId, 0);
		EXPECT_EQ(letter.attachedKinah, MAILED_KINAH);
		EXPECT_EQ(letter.letterType, decoders::LETTER_TYPE_NORMAL);
		fromB = b.mark();
		b.game->send(GameSession::CM_READ_MAIL, GameSession::buildCM_READ_MAIL(letter.letterObjectId));
		collectFor(*b.game, STEP);
		mail = mailServices(b.since(fromB), "X13");
		ASSERT_EQ(mail.size(), 1u);
		ASSERT_TRUE(mail[0].letterRead) << "X13: read -> (3)";
		EXPECT_EQ(mail[0].letterRead->letterObjectId, letter.letterObjectId);
		EXPECT_EQ(mail[0].letterRead->message, "gate");
		ASSERT_TRUE(mail[0].letterRead->attachedItem);
		EXPECT_EQ(mail[0].letterRead->attachedItem->templateId, LIFE_POTION);
		EXPECT_EQ(mail[0].letterRead->attachedKinah, MAILED_KINAH);

		fromB = b.mark();
		b.game->send(GameSession::CM_GET_MAIL_ATTACHMENT, GameSession::buildCM_GET_MAIL_ATTACHMENT(letter.letterObjectId, GameSession::MAIL_ATTACHMENT_ITEM));
		collectFor(*b.game, STEP);
		checkDisjoint("C11 item attachment");
		ledgerB[LIFE_POTION] += MAILED_POTIONS;
		window = b.since(fromB);
		const std::vector<decoders::InventoryItem> taken = addedItems(window, "X13");
		ASSERT_EQ(taken.size(), 1u) << "X13: the item arrives as its own stack (Storage.add): " << join(namesOf(window));
		EXPECT_EQ(taken[0].templateId, LIFE_POTION);
		EXPECT_EQ(taken[0].general ? taken[0].general->count : -1, MAILED_POTIONS);
		mailedPotionsObject = taken[0].objectId;
		EXPECT_EQ(mailedPotionsObject, letter.attachedItemObjectId) << "the letter's item is the one B receives";
		mail = mailServices(window, "X13");
		ASSERT_EQ(mail.size(), 1u);
		EXPECT_EQ(mail[0].attachmentTaken, (std::optional<decoders::AttachmentTaken>(decoders::AttachmentTaken{letter.letterObjectId, decoders::MAIL_ATTACHMENT_ITEM})));

		const int64_t beforeB = b.kinah();
		fromB = b.mark();
		b.game->send(GameSession::CM_GET_MAIL_ATTACHMENT, GameSession::buildCM_GET_MAIL_ATTACHMENT(letter.letterObjectId, GameSession::MAIL_ATTACHMENT_KINAH));
		collectFor(*b.game, STEP);
		ledgerB[KINAH_ITEM] += MAILED_KINAH;
		window = b.since(fromB);
		EXPECT_EQ(kinahUpdates(window, kinahB, "X13"), std::vector<int64_t>{beforeB + MAILED_KINAH}) << "X13: +200";
		mail = mailServices(window, "X13");
		ASSERT_EQ(mail.size(), 1u);
		EXPECT_EQ(mail[0].attachmentTaken, (std::optional<decoders::AttachmentTaken>(decoders::AttachmentTaken{letter.letterObjectId, decoders::MAIL_ATTACHMENT_KINAH})));

		fromB = b.mark();
		b.game->send(GameSession::CM_CHECK_MAIL_LIST, GameSession::buildCM_CHECK_MAIL_LIST(false));
		collectFor(*b.game, STEP);
		mail = mailServices(b.since(fromB), "X13");
		ASSERT_FALSE(mail.empty());
		ASSERT_TRUE(mail.back().letterList);
		ASSERT_EQ(mail.back().letterList->letters.size(), 1u);
		EXPECT_TRUE(mail.back().letterList->letters[0].read) << "X13: the second list shows the letter read";
		EXPECT_EQ(mail.back().letterList->letters[0].attachedItemObjectId, 0) << "X13: the item left the letter (MailService.getAttachments)";
		EXPECT_EQ(mail.back().letterList->letters[0].attachedItemTemplateId, 0);
		EXPECT_EQ(mail.back().letterList->letters[0].attachedKinah, 0) << "X13: and the kinah";

		fromB = b.mark();
		const std::array<int32_t, 1> deleted{letter.letterObjectId};
		b.game->send(GameSession::CM_DELETE_MAIL, GameSession::buildCM_DELETE_MAIL(deleted));
		collectFor(*b.game, STEP);
		mail = mailServices(b.since(fromB), "X13");
		ASSERT_EQ(mail.size(), 1u);
		ASSERT_TRUE(mail[0].lettersDeleted) << "X13: delete -> (6)";
		EXPECT_EQ(mail[0].lettersDeleted->letterObjectIds, std::vector<int32_t>{letter.letterObjectId});
		EXPECT_EQ(mail[0].lettersDeleted->counts.total, 0) << "X13: 0 letters";

		// B closes the postbox (DialogService.onCloseDialog clears the state), then A's second letter: the notice only (X3)
		b.game->send(GameSession::CM_CLOSE_DIALOG, GameSession::buildCM_CLOSE_DIALOG(postbox));
		collectFor(*b.game, 500ms);
		beforeA = a.kinah();
		fromA = a.mark();
		fromB = b.mark();
		a.game->send(GameSession::CM_SEND_MAIL, GameSession::buildCM_SEND_MAIL(b.name, "m5c", "kinah", 0, 0, KINAH_LETTER));
		collectFor(*a.game, STEP);
		collectFor(*b.game, 600ms);
		const int64_t kinahCost = economy.mail[1].byRace.at("ELYOS").second;
		ledgerA[KINAH_ITEM] -= kinahCost;
		EXPECT_EQ(kinahUpdates(a.since(fromA), kinahA, "X13"), std::vector<int64_t>{beforeA - kinahCost}) << "the kinah letter costs " << kinahCost;
		mail = mailServices(b.since(fromB), "X3");
		// non-fatal (the review of 2026-09-28): a mutant of X3 alone must leave C12-C18 running, so the rows after it stay observable
		EXPECT_EQ(mail.size(), 1u) << "X3: after CM_CLOSE_DIALOG the letter is the notice only, no refresh: " << join(namesOf(b.since(fromB)));
		if (!mail.empty())
			EXPECT_EQ(mail[0].serviceId, decoders::MAIL_SERVICE_MAILBOX_STATE) << "X3: the notice";
		checkDisjoint("C11 end");
	});

	// ---- C12: mail, offline, and a wrong name (X14) ----
	runCase("C12", "B quits; A mails B offline and a name that does not exist; B re-enters (X14)", [&] {
		const decoders::CharacterList atSelect = quitToCharacterList(b);
		(void)atSelect;
		const std::string bId = std::to_string(b.playerId);
		const int64_t lettersBefore = database.queryLong(schema, "SELECT mailbox_letters FROM players WHERE id = " + bId).value_or(-1);
		const int64_t rowsBefore = database.queryLong(schema, "SELECT COUNT(*) FROM mail WHERE mail_recipient_id = " + bId).value_or(-1);
		const int32_t kinahA = a.kinahObject().value_or(0);
		const int64_t beforeA = a.kinah();
		size_t fromA = a.mark();
		a.game->send(GameSession::CM_SEND_MAIL, GameSession::buildCM_SEND_MAIL(b.name, "m5c", "offline", 0, 0, KINAH_LETTER));
		collectFor(*a.game, STEP);
		ledgerA[KINAH_ITEM] -= economy.mail[1].byRace.at("ELYOS").second;
		std::vector<decoders::MailService> mail = mailServices(a.since(fromA), "X14");
		ASSERT_EQ(mail.size(), 1u);
		EXPECT_EQ(mail[0].mailMessage, std::optional<uint8_t>(decoders::MAIL_MESSAGE_SEND_SUCCESS)) << "X14: an offline name is found (W-05)";
		EXPECT_EQ(kinahUpdates(a.since(fromA), kinahA, "X14"), std::vector<int64_t>{beforeA - economy.mail[1].byRace.at("ELYOS").second});
		EXPECT_EQ(database.queryLong(schema, "SELECT COUNT(*) FROM mail WHERE mail_recipient_id = " + bId).value_or(-1), rowsBefore + 1)
		  << "X14: the letter is stored";
		EXPECT_EQ(database.queryLong(schema, "SELECT mailbox_letters FROM players WHERE id = " + bId).value_or(-1), lettersBefore + 1)
		  << "X14: SystemMailService.updateRecipientMailbox's offline counter (updateOfflineMailCounter)";

		fromA = a.mark();
		a.game->send(GameSession::CM_SEND_MAIL, GameSession::buildCM_SEND_MAIL("Nobodyhere", "m5c", "nobody", 0, 0, KINAH_LETTER));
		collectFor(*a.game, SILENCE);
		mail = mailServices(a.since(fromA), "X14");
		ASSERT_EQ(mail.size(), 1u) << join(namesOf(a.since(fromA)));
		EXPECT_EQ(mail[0].mailMessage, std::optional<uint8_t>(decoders::MAIL_MESSAGE_NO_SUCH_CHARACTER_NAME)) << "X14: the wrong name";
		EXPECT_TRUE(kinahUpdates(a.since(fromA), kinahA, "X14").empty()) << "X14: no kinah change";
		checkDisjoint("C12 sends");

		// B's return: the character list flags unread mail (MailDAO.haveUnread), the enter world counts the two unread letters
		b.game->send(GameSession::CM_CHARACTER_LIST, GameSession::buildCM_CHARACTER_LIST(b.key.playOk2));
		const decoders::CharacterList list = decoders::decodeCharacterList(waitFor(*b.game, "SM_CHARACTER_LIST").data);
		ASSERT_EQ(list.characters.size(), 1u);
		EXPECT_EQ(list.characters[0].unreadMail, 1) << "X14: the character list's unread flag";
		const std::vector<Packet> burst = reenter(b);
		mail = mailServices(burst, "X14");
		ASSERT_FALSE(mail.empty()) << join(namesOf(burst));
		ASSERT_TRUE(mail[0].mailboxState);
		EXPECT_EQ(mail[0].mailboxState->unread, 2) << "X14: the second and the third letter (MailService.onPlayerLogin, loadPlayerMailbox)";
		EXPECT_EQ(mail[0].mailboxState->total, 2);
		checkDisjoint("C12 re-entry");
	});

	// ---- C13: soul healing (X15) ----
	runCase("C13", "A disconnects, recoverexp is seeded, A heals its soul at Fulla (X15)", [&] {
		const int64_t kinahBeforeQuit = a.kinah();
		disconnect(a);
		database.execute(schema, "UPDATE players SET recoverexp = " + std::to_string(RECOVERABLE_EXP) + " WHERE id = " + std::to_string(a.playerId));
		relogIn(servers, a);
		EXPECT_EQ(a.kinah(), kinahBeforeQuit) << "A's kinah after the relog";
		b.drain();
		checkDisjoint("C13 re-entry");
		const EconomyTalk& fullaTalk = economy.talkOf(FULLA);
		walkToSpot(a, fullaTalk.nearSpot);
		const int32_t fulla = npcOf(a, FULLA);
		const int32_t kinah = a.kinahObject().value_or(0);
		const int64_t before = a.kinah();
		size_t from = a.mark();
		a.game->send(GameSession::CM_DIALOG_SELECT, GameSession::buildCM_DIALOG_SELECT(fulla, DIALOG_RECOVERY));
		collectFor(*a.game, STEP);
		const std::vector<decoders::QuestionWindow> questions = questionWindows(a.since(from), "X15");
		ASSERT_EQ(questions.size(), 1u) << "X15: RECOVERY asks: " << join(namesOf(a.since(from)));
		const EconomyQuestion& want = *economy.recovery->question;
		EXPECT_EQ(questions[0].code, want.id) << "X15: STR_ASK_RECOVER_EXPERIENCE";
		EXPECT_EQ(questions[0].params, want.params) << "X15: the price as text";
		EXPECT_EQ(questions[0].senderId, want.senderId);
		EXPECT_EQ(questions[0].rangeOrCooldownSeconds, want.range);
		from = a.mark();
		a.game->send(GameSession::CM_QUESTION_RESPONSE, GameSession::buildCM_QUESTION_RESPONSE(want.id, GameSession::ANSWER_YES));
		collectFor(*a.game, STEP);
		const std::vector<Packet> window = a.since(from);
		ledgerA[KINAH_ITEM] += *economy.recovery->yesKinahDelta;
		std::vector<int32_t> wantMessages;
		for (const EconomyMessage& message : economy.recovery->yesMessages)
			wantMessages.push_back(message.id);
		std::vector<int32_t> gotMessages;
		for (int32_t id : messageIds(window))
			if (contains(wantMessages, id))
				gotMessages.push_back(id);
		EXPECT_EQ(gotMessages, wantMessages) << "X15: STR_GET_EXP2 then STR_SUCCESS_RECOVER_EXPERIENCE: " << joinNumbers(messageIds(window));
		EXPECT_EQ(kinahUpdates(window, kinah, "X15"), std::vector<int64_t>{before + *economy.recovery->yesKinahDelta}) << "X15: -" << -*economy.recovery->yesKinahDelta;
		const std::vector<decoders::StatUpdateExp> exp =
		  decodeAll<decoders::StatUpdateExp>(window, "SM_STATUPDATE_EXP", decoders::decodeStatUpdateExp, "X15");
		// non-fatal (the review of 2026-09-28): a mutant of X15 alone must leave C14-C18 running, so the rows after it stay observable
		EXPECT_FALSE(exp.empty()) << "X15: the exp update: " << join(namesOf(window));
		if (!exp.empty())
			EXPECT_EQ(exp.back().recoverableExp, *economy.recovery->yesRecoverableExpAfter) << "X15: resetRecoverableExp";
		checkDisjoint("C13 end");
	});

	// ---- C14: persistence (X16) ----
	std::map<int32_t, int32_t> seeded; // item id -> object id of B's seeds
	runCase("C14", "both disconnect; the database against the ledger; B's seeds; both re-enter (X16, X15)", [&] {
		EXPECT_TRUE(sharedObjects.empty()) << "X16: object ids in both clients' inventory models during C8-C13:\n  " << join(sharedObjects, "\n  ");
		// the models against the ledger before the quit
		for (ScenarioClient* client : {&a, &b}) {
			client->drain();
			const Ledger& want = client == &a ? ledgerA : ledgerB;
			const std::vector<std::string> differences = ledgerDifferences(ledgerOf(client->model), want);
			EXPECT_TRUE(differences.empty()) << "X16: " << client->label << "'s model against the ledger (have/want): " << join(differences);
		}
		disconnect(a);
		disconnect(b);
		std::this_thread::sleep_for(500ms);
		for (ScenarioClient* client : {&a, &b}) {
			const Ledger& want = client == &a ? ledgerA : ledgerB;
			Ledger stored;
			for (const auto& row : database.queryRows(schema, "SELECT item_id, SUM(item_count) FROM inventory WHERE item_owner = " +
			                                                    std::to_string(client->playerId) + " GROUP BY item_id",
			                                          2))
				stored[std::stoi(row[0].value_or("0"))] = std::stoll(row[1].value_or("0"));
			const std::vector<std::string> differences = ledgerDifferences(stored, want);
			EXPECT_TRUE(differences.empty()) << "X16: " << client->label << "'s `inventory` rows against the ledger (have/want): " << join(differences);
		}
		// per item id over both characters: every transfer conserved (the starters plus the vendors' net)
		Ledger bothStored, bothWant;
		for (const auto& row : database.queryRows(schema, "SELECT item_id, SUM(item_count) FROM inventory WHERE item_owner IN (" + std::to_string(a.playerId) +
		                                                    ", " + std::to_string(b.playerId) + ") GROUP BY item_id",
		                                          2))
			bothStored[std::stoi(row[0].value_or("0"))] = std::stoll(row[1].value_or("0"));
		for (const Ledger* ledger : {&ledgerA, &ledgerB})
			for (const auto& [id, count] : *ledger)
				bothWant[id] += count;
		EXPECT_TRUE(ledgerDifferences(bothStored, bothWant).empty()) << "X16: per item id over A and B: " << join(ledgerDifferences(bothStored, bothWant));
		EXPECT_EQ(database.queryLong(schema, "SELECT item_owner FROM inventory WHERE item_unique_id = " + std::to_string(mailedPotionsObject)).value_or(-1),
		          b.playerId)
		  << "X16: the mailed potions' row belongs to B";
		const auto letters = database.queryRows(schema, "SELECT mail_recipient_id, attached_item_id, attached_kinah_count, unread FROM mail", 4);
		EXPECT_EQ(letters.size(), 2u) << "X16: `mail` holds only the two unread kinah letters";
		for (const auto& row : letters) {
			EXPECT_EQ(row[0].value_or(""), std::to_string(b.playerId));
			EXPECT_EQ(row[1].value_or(""), "0");
			EXPECT_EQ(row[2].value_or(""), std::to_string(KINAH_LETTER));
			EXPECT_EQ(row[3].value_or(""), "1");
		}
		// X15's persistence: the exp came back, nothing is recoverable any more
		const auto aRow = database.queryRows(schema, "SELECT exp, recoverexp FROM players WHERE id = " + std::to_string(a.playerId), 2);
		ASSERT_EQ(aRow.size(), 1u);
		// P6-Q prologue: A holds quest 1000's reward from C1 (decoders::PROLOGUE_EXP) under the recovered exp
		EXPECT_EQ(aRow[0][0].value_or(""), std::to_string(decoders::PROLOGUE_EXP + *economy.recovery->yesExpDelta))
		  << "X15: exp +1,000 over A's prologue exp";
		EXPECT_EQ(aRow[0][1].value_or(""), std::to_string(*economy.recovery->yesRecoverableExpAfter)) << "X15: players.recoverexp = 0";
		ASSERT_NE(servers.gameServer(), nullptr);
		std::vector<std::string> daoErrors;
		for (const std::string& line : servers.gameServer()->findLogLines(" ERROR "))
			if (line.find("InventoryDAO") != std::string::npos || line.find("MailDAO") != std::string::npos)
				daoErrors.push_back(line);
		EXPECT_TRUE(daoErrors.empty()) << "X16: ERROR lines of InventoryDAO or MailDAO (a duplicate item_unique_id fails there):\n" << join(daoErrors, "\n");

		// §10.1 "Seeds" for C15-C18, with B disconnected (F-3): level 4, the removal + cube + tools kinah, the unidentified tunic, the sword, the stone
		const std::string bId = std::to_string(b.playerId);
		database.execute(schema, "UPDATE players SET exp = " + std::to_string(*economy.item(TUNIC).startExpOfRequiredLevel) + " WHERE id = " + bId);
		database.execute(schema, "UPDATE inventory SET item_count = " + std::to_string(seedKinahB) + " WHERE item_owner = " + bId + " AND item_id = " +
		                           std::to_string(KINAH_ITEM));
		for (int32_t itemId : {TUNIC, PLAINSMAN_SWORD, MANASTONE})
			seeded[itemId] = database.seedInventoryItem(schema, {b.playerId, itemId, 1, ModelItem::CUBE, 65535});
		database.execute(schema, "UPDATE inventory SET tune_count = " + std::to_string(*economy.item(TUNIC).seedTuneCountForUnidentified) +
		                           " WHERE item_unique_id = " + std::to_string(seeded[TUNIC]));
		ledgerB[KINAH_ITEM] = seedKinahB;
		ledgerB[TUNIC] += 1;
		ledgerB[PLAINSMAN_SWORD] += 1;
		ledgerB[MANASTONE] += 1;

		// both re-enter: SM_INVENTORY_INFO against the same ledger
		for (ScenarioClient* client : {&a, &b}) {
			relogIn(servers, *client);
			const Ledger& want = client == &a ? ledgerA : ledgerB;
			const std::vector<std::string> differences = ledgerDifferences(ledgerOf(client->model), want);
			EXPECT_TRUE(differences.empty()) << "X16: " << client->label << "'s re-entry SM_INVENTORY_INFO against the ledger (have/want): " << join(differences);
		}
		a.drain();
		const std::optional<decoders::StatsInfo> stats = [&]() -> std::optional<decoders::StatsInfo> {
			std::optional<decoders::StatsInfo> last;
			for (const Packet& packet : b.game->recorded())
				if (packet.name == "SM_STATS_INFO")
					last = decoders::decodeStatsInfo(packet.data);
			return last;
		}();
		ASSERT_TRUE(stats);
		EXPECT_EQ(stats->level, economy.item(TUNIC).requiredLevel) << "B is level 4 after the exp seed (onLevelChange at the enter world, W-20)";
	});

	// ---- C15: identification (X23) ----
	const auto armourOf = [&]() -> std::optional<ModelItem> {
		b.model.sync();
		return b.model.byObjectId(seeded[TUNIC]);
	};
	runCase("C15", "B identifies the tunic, equips and unequips it (X23)", [&] {
		const EconomyItem& tunic = economy.item(TUNIC);
		const int32_t armour = seeded[TUNIC];
		ASSERT_TRUE(armourOf()) << "the seeded tunic " << armour << " was not loaded: " << b.model.describe();
		// the seed loads unidentified: EnchantInfoBlobEntry writes -1 for the sockets and the bonus of an unidentified item
		const std::vector<decoders::InventoryInfo> infos =
		  decodeAll<decoders::InventoryInfo>(b.lastEnterWorld, "SM_INVENTORY_INFO", decoders::decodeInventoryInfo, "X23");
		bool unidentified = false;
		for (const decoders::InventoryInfo& info : infos)
			for (const decoders::InventoryItem& item : info.items)
				if (item.objectId == armour && item.enchant)
					unidentified = item.enchant->optionalSockets == -1 && item.enchant->enchantBonus == -1;
		EXPECT_TRUE(unidentified) << "X23: the seeded tunic (tune_count -1) must load unidentified (D5)";

		ASSERT_TRUE(tunic.identifyAnimation);
		const size_t from = b.mark();
		const auto sentAt = std::chrono::steady_clock::now();
		b.game->send(GameSession::CM_TUNE, GameSession::buildCM_TUNE(armour, 0));
		const std::optional<size_t> done = readUntil(
		  *b.game,
		  [&](const Packet& packet) {
			  if (packet.name != "SM_SYSTEM_MESSAGE")
				  return false;
			  try {
				  return decoders::decodeSystemMessageId(packet.data) == tunic.identifyMessageId.value_or(0);
			  } catch (const DecodeError&) {
				  return false;
			  }
		  },
		  15s);
		collectFor(*b.game, 500ms);
		const std::vector<Packet> window = b.since(from);
		ASSERT_TRUE(done) << "X23: no STR_MSG_ITEM_IDENTIFY_SUCCEED within 15 s: " << join(namesOf(window));
		const std::vector<decoders::ItemUsageAnimation> animations = usageAnimations(window, "X23");
		ASSERT_EQ(animations.size(), 2u) << "X23: the start and the end animation: " << join(namesOf(window));
		const EconomyAnimation& want = *tunic.identifyAnimation;
		EXPECT_EQ(animations[0].playerObjectId, b.playerId);
		EXPECT_EQ(animations[0].itemObjectId, armour);
		EXPECT_EQ(animations[0].itemId, TUNIC);
		EXPECT_EQ(animations[0].time, want.time) << "X23: 5,000 ms";
		EXPECT_EQ(animations[0].end, want.start) << "X23: the start action 9";
		EXPECT_EQ(animations[1].time, 0);
		EXPECT_EQ(animations[1].end, want.end) << "X23: the end action 10";
		const std::vector<Packet> animationPackets = ofName(window, "SM_ITEM_USAGE_ANIMATION");
		const int64_t millis = millisBetween(animationPackets.front().receivedAt, animationPackets.back().receivedAt);
		EXPECT_GE(millis, want.time - 300) << "X23: the task runs " << want.time << " ms after the start animation, not with delay 0";
		EXPECT_LE(millisBetween(sentAt, animationPackets.front().receivedAt), 2000) << "X23: the start animation comes at once";
		const std::vector<decoders::InventoryUpdateItem> updates = updatesOf(window, armour, "X23");
		ASSERT_EQ(updates.size(), 1u) << "X23: one SM_INVENTORY_UPDATE_ITEM of the tunic: " << join(namesOf(window));
		ASSERT_TRUE(updates[0].item.enchant) << "X23: the full blob";
		ASSERT_TRUE(tunic.optionalSocketsRange && tunic.enchantBonusRange);
		EXPECT_GE(updates[0].item.enchant->optionalSockets, (*tunic.optionalSocketsRange)[0]) << "X23: Rnd.get(0, option_slot_bonus)";
		EXPECT_LE(updates[0].item.enchant->optionalSockets, (*tunic.optionalSocketsRange)[1]);
		EXPECT_GE(updates[0].item.enchant->enchantBonus, (*tunic.enchantBonusRange)[0]) << "X23: Rnd.get(0, max_enchant_bonus)";
		EXPECT_LE(updates[0].item.enchant->enchantBonus, (*tunic.enchantBonusRange)[1]);
		EXPECT_TRUE(contains(messageIds(window), *tunic.identifyMessageId));

		// identified, B can wear it (Equipment.java:163-167 refuses an unidentified item); then it comes off again for C16's removal
		size_t equipFrom = b.mark();
		b.game->send(GameSession::CM_EQUIP_ITEM, GameSession::buildCM_EQUIP_ITEM(GameSession::EQUIP, TORSO, armour));
		collectFor(*b.game, STEP);
		std::vector<decoders::InventoryUpdateItem> equip = updatesOf(b.since(equipFrom), armour, "X23");
		const bool equipped = !equip.empty() && equip.back().item.equippedSlotBlob == std::optional<int64_t>(TORSO);
		EXPECT_TRUE(equipped) << "X23: the identified tunic can be equipped (A-05's equip packets): " << join(namesOf(b.since(equipFrom)));
		const std::vector<decoders::UpdatePlayerAppearance> appearance =
		  decodeAll<decoders::UpdatePlayerAppearance>(b.since(equipFrom), "SM_UPDATE_PLAYER_APPEARANCE", decoders::decodeUpdatePlayerAppearance, "X23");
		EXPECT_TRUE(std::ranges::any_of(appearance, [&](const decoders::UpdatePlayerAppearance& update) { return update.playerObjectId == b.playerId; }))
		  << "X23: CM_EQUIP_ITEM's SM_UPDATE_PLAYER_APPEARANCE";
		if (equipped) {
			// back into the cube, where C16's removal looks for it (ItemSocketService.removeManastone reads the inventory only)
			equipFrom = b.mark();
			b.game->send(GameSession::CM_EQUIP_ITEM, GameSession::buildCM_EQUIP_ITEM(GameSession::UNEQUIP, 0, armour));
			collectFor(*b.game, STEP);
			equip = updatesOf(b.since(equipFrom), armour, "X23");
			EXPECT_TRUE(!equip.empty() && equip.back().item.equippedSlotBlob == std::optional<int64_t>(0))
			  << "the tunic back in the cube: " << join(namesOf(b.since(equipFrom)));
		}
	});

	// ---- C16: a manastone, and Seril (X24, X25) ----
	runCase("C16", "B sockets the manastone, relogs, and has it removed at Seril (X24, X25)", [&] {
		const EconomyItem& tunic = economy.item(TUNIC);
		const EconomySocketing& socketing = tunic.socketing.at(0);
		const int32_t armour = seeded[TUNIC], stone = seeded[MANASTONE];
		size_t from = b.mark();
		b.game->send(GameSession::CM_MANASTONE, GameSession::buildCM_MANASTONE(MANASTONE_ADD, 1, armour, stone, 0));
		const std::optional<size_t> done = readUntil(
		  *b.game,
		  [&](const Packet& packet) {
			  if (packet.name != "SM_ITEM_USAGE_ANIMATION")
				  return false;
			  try {
				  const decoders::ItemUsageAnimation animation = decoders::decodeItemUsageAnimation(packet.data);
				  return animation.time == 0 && animation.end != 0;
			  } catch (const DecodeError&) {
				  return true;
			  }
		  },
		  10s);
		collectFor(*b.game, 500ms);
		std::vector<Packet> window = b.since(from);
		ASSERT_TRUE(done) << "X24: the socketing never ended: " << join(namesOf(window));
		ledgerB[MANASTONE] -= 1;
		EXPECT_TRUE(contains(deletedObjects(window, "X24"), stone)) << "X24: the stone is consumed: " << join(namesOf(window));
		const std::vector<decoders::InventoryUpdateItem> socketed = updatesOf(window, armour, "X24");
		ASSERT_FALSE(socketed.empty()) << "X24: the tunic's update: " << join(namesOf(window));
		ASSERT_TRUE(socketed.back().item.enchant);
		EXPECT_EQ(socketed.back().item.enchant->manaStones[0], MANASTONE) << "X24: the stone in slot 0";
		EXPECT_EQ(socketed.back().item.enchant->manaStones[1], 0) << "X24: and nowhere else";
		EXPECT_TRUE(contains(messageIds(window), *socketing.successMessageId)) << "X24: STR_GIVE_ITEM_OPTION_SUCCEED (chance 200, no randomness, D6)";
		EXPECT_FALSE(contains(messageIds(window), *socketing.failureMessageId));
		const std::vector<decoders::ItemUsageAnimation> animations = usageAnimations(window, "X24");
		ASSERT_FALSE(animations.empty());
		EXPECT_EQ(animations.front().time, *socketing.animationMillis);
		EXPECT_EQ(animations.back().end, 1) << "X24: the closing animation says success";

		// the socket is stored at the quit as an item_stones row, and comes back at the re-entry
		quitToCharacterList(b);
		const auto stones = database.queryRows(schema, "SELECT item_id, slot, category FROM item_stones WHERE item_unique_id = " + std::to_string(armour), 3);
		ASSERT_EQ(stones.size(), 1u) << "X24: one item_stones row for the tunic after the quit";
		EXPECT_EQ(stones[0][0].value_or(""), std::to_string(MANASTONE));
		EXPECT_EQ(stones[0][1].value_or(""), "0");
		EXPECT_EQ(stones[0][2].value_or(""), std::to_string(ITEM_STONE_CATEGORY_MANASTONE));
		const std::vector<Packet> burst = reenter(b);
		bool reloaded = false;
		for (const decoders::InventoryInfo& info : decodeAll<decoders::InventoryInfo>(burst, "SM_INVENTORY_INFO", decoders::decodeInventoryInfo, "X24"))
			for (const decoders::InventoryItem& item : info.items)
				if (item.objectId == armour && item.enchant)
					reloaded = item.enchant->manaStones[0] == MANASTONE;
		EXPECT_TRUE(reloaded) << "X24: the re-entry shows the stone in the tunic's item info";

		// Seril: target, talk, REMOVE_ITEM_OPTION's page, the removal
		const EconomyTalk& serilTalk = economy.talkOf(SERIL);
		walkToSpot(b, serilTalk.nearSpot);
		const int32_t seril = npcOf(b, SERIL);
		b.game->send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(seril));
		collectFor(*b.game, 500ms);
		from = b.mark();
		b.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(seril));
		collectFor(*b.game, STEP);
		std::vector<decoders::DialogWindow> windows = dialogWindows(b.since(from), "X25");
		ASSERT_EQ(windows.size(), 1u) << join(namesOf(b.since(from)));
		EXPECT_EQ(windows[0].dialogPageId, *serilTalk.startWindow->page) << "X25: Seril's start page";
		from = b.mark();
		b.game->send(GameSession::CM_DIALOG_SELECT, GameSession::buildCM_DIALOG_SELECT(seril, DIALOG_REMOVE_ITEM_OPTION));
		collectFor(*b.game, STEP);
		windows = dialogWindows(b.since(from), "X25");
		ASSERT_EQ(windows.size(), 1u) << "X25: REMOVE_ITEM_OPTION answers a window (W-18): " << join(namesOf(b.since(from)));
		const auto removeArm = std::ranges::find_if(serilTalk.functions, [](const EconomyFunction& arm) { return arm.action == DIALOG_REMOVE_ITEM_OPTION; });
		ASSERT_NE(removeArm, serilTalk.functions.end());
		EXPECT_EQ(windows[0].dialogPageId, removeArm->page.value_or(-1)) << "X25: DialogPage.REMOVE_MANASTONE (20), not the action id 42";

		const int32_t kinah = b.kinahObject().value_or(0);
		const int64_t before = b.kinah();
		from = b.mark();
		GameSession::ManastoneRequest removal;
		removal.actionType = GameSession::MANASTONE_REMOVE;
		removal.targetFusedSlot = 1;
		removal.targetItemUniqueId = armour;
		removal.slotNum = 0;
		removal.npcObjId = seril;
		b.game->send(GameSession::CM_MANASTONE, GameSession::buildCM_MANASTONE(removal));
		collectFor(*b.game, STEP);
		window = b.since(from);
		ledgerB[KINAH_ITEM] -= economy.removalPrice->at("ELYOS");
		EXPECT_EQ(kinahUpdates(window, kinah, "X25"), std::vector<int64_t>{before - economy.removalPrice->at("ELYOS")})
		  << "X25: -" << economy.removalPrice->at("ELYOS") << " (PricesService.getPriceForService(650), with the taxes)";
		EXPECT_TRUE(contains(messageIds(window), *economy.removalSucceedMessageId)) << "X25: STR_REMOVE_ITEM_OPTION_SUCCEED: " << joinNumbers(messageIds(window));
		const std::vector<decoders::InventoryUpdateItem> removed = updatesOf(window, armour, "X25");
		ASSERT_FALSE(removed.empty()) << join(namesOf(window));
		ASSERT_TRUE(removed.back().item.enchant);
		EXPECT_EQ(removed.back().item.enchant->manaStones[0], 0) << "X25: the tunic without its stone";
		EXPECT_EQ(database.queryLong(schema, "SELECT COUNT(*) FROM item_stones WHERE item_unique_id = " + std::to_string(armour)).value_or(-1), 0)
		  << "X25: the removal writes at once (ItemStoneListDAO.storeManaStones with DELETED)";
	});

	// ---- C17: the cube (X26) ----
	runCase("C17", "B expands the cube at 798008 (X26)", [&] {
		const EconomyTalk& cubeTalk = economy.talkOf(CUBE_NPC);
		const EconomyCube& cube = economy.cube.at(0);
		walkToSpot(b, cubeTalk.nearSpot);
		const int32_t npc = npcOf(b, CUBE_NPC);
		b.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(npc));
		collectFor(*b.game, STEP);
		size_t from = b.mark();
		b.game->send(GameSession::CM_DIALOG_SELECT, GameSession::buildCM_DIALOG_SELECT(npc, DIALOG_EXTEND_INVENTORY));
		collectFor(*b.game, STEP);
		const std::vector<decoders::QuestionWindow> questions = questionWindows(b.since(from), "X26");
		ASSERT_EQ(questions.size(), 1u) << "X26: EXTEND_INVENTORY asks: " << join(namesOf(b.since(from)));
		ASSERT_TRUE(cube.question);
		EXPECT_EQ(questions[0].code, cube.question->id) << "X26: STR_WAREHOUSE_EXPAND_WARNING (the cube reuses the warehouse question)";
		EXPECT_EQ(questions[0].params, cube.question->params) << "X26: the price";
		const int32_t kinah = b.kinahObject().value_or(0);
		const int64_t before = b.kinah();
		from = b.mark();
		b.game->send(GameSession::CM_QUESTION_RESPONSE, GameSession::buildCM_QUESTION_RESPONSE(cube.question->id, GameSession::ANSWER_YES));
		collectFor(*b.game, STEP);
		const std::vector<Packet> window = b.since(from);
		ledgerB[KINAH_ITEM] += *cube.yesKinahDelta;
		EXPECT_EQ(kinahUpdates(window, kinah, "X26"), std::vector<int64_t>{before + *cube.yesKinahDelta}) << "X26: -1,000 (the raw template price)";
		ASSERT_TRUE(cube.yesMessage && cube.smCubeUpdate);
		EXPECT_TRUE(contains(messageIds(window), cube.yesMessage->id)) << "X26: STR_EXTEND_INVENTORY_SIZE_EXTENDED";
		const std::vector<decoders::CubeUpdate> updates = decodeAll<decoders::CubeUpdate>(window, "SM_CUBE_UPDATE", decoders::decodeCubeUpdate, "X26");
		ASSERT_FALSE(updates.empty()) << join(namesOf(window));
		EXPECT_EQ(updates.back().action, cube.smCubeUpdate->action);
		EXPECT_EQ(updates.back().actionValue, cube.smCubeUpdate->storage);
		EXPECT_EQ(updates.back().npcExpands, cube.smCubeUpdate->npcExpands) << "X26: the npc expansion count 1 (npcExpand)";
		EXPECT_EQ(updates.back().questExpands, cube.smCubeUpdate->questExpands);
		EXPECT_EQ(updates.back().itemExpands, cube.smCubeUpdate->itemExpands);
	});

	// ---- C18: extraction and enchanting (X27, X28) ----
	int32_t enchantSeen = -1;
	runCase("C18", "B buys the tools with its last kinah, breaks the sword and enchants the tunic (X27, X28)", [&] {
		const EconomyItem& sword = economy.item(PLAINSMAN_SWORD);
		const int32_t armour = seeded[TUNIC], weapon = seeded[PLAINSMAN_SWORD];
		const EconomyTalk& merchantTalk = economy.talkOf(MERCHANT);
		walkToSpot(b, merchantTalk.nearSpot);
		const int32_t merchant = npcOf(b, MERCHANT);
		b.game->send(GameSession::CM_DIALOG_SELECT, GameSession::buildCM_DIALOG_SELECT(merchant, DIALOG_BUY));
		collectFor(*b.game, STEP);
		const int32_t kinah = b.kinahObject().value_or(0);
		EXPECT_EQ(b.kinah(), tools.buyKinah) << "C14's seed minus X25 and X26 leaves exactly the tools' price";
		size_t from = b.mark();
		const std::array<GameSession::BuyItemEntry, 1> buy{{{EXTRACTION_TOOLS, 1}}};
		b.game->send(GameSession::CM_BUY_ITEM, GameSession::buildCM_BUY_ITEM(merchant, GameSession::TRADE_BUY, buy));
		collectFor(*b.game, STEP);
		std::vector<Packet> window = b.since(from);
		ledgerB[KINAH_ITEM] -= tools.buyKinah;
		EXPECT_EQ(kinahUpdates(window, kinah, "X27"), std::vector<int64_t>{0}) << "X27: the tools take the last kinah (>= in the kinah checks): "
		                                                                       << joinNumbers(messageIds(window));
		const std::vector<decoders::InventoryItem> bought = addedItems(window, "X27");
		ASSERT_EQ(bought.size(), 1u) << "X27: " << join(namesOf(window));
		EXPECT_EQ(bought[0].templateId, EXTRACTION_TOOLS);
		const int32_t toolsObject = bought[0].objectId;

		from = b.mark();
		b.game->send(GameSession::CM_USE_ITEM, GameSession::buildCM_USE_ITEM(toolsObject, USE_ITEM_ON_ITEM, weapon));
		const std::optional<size_t> done = readUntil(
		  *b.game,
		  [&](const Packet& packet) {
			  if (packet.name != "SM_ITEM_USAGE_ANIMATION")
				  return false;
			  try {
				  const decoders::ItemUsageAnimation animation = decoders::decodeItemUsageAnimation(packet.data);
				  return animation.time == 0 && animation.end != 0;
			  } catch (const DecodeError&) {
				  return true;
			  }
		  },
		  15s);
		collectFor(*b.game, 500ms);
		window = b.since(from);
		ASSERT_TRUE(done) << "X27: the extraction never ended: " << join(namesOf(window));
		const std::vector<decoders::ItemUsageAnimation> animations = usageAnimations(window, "X27");
		ASSERT_GE(animations.size(), 2u);
		EXPECT_EQ(animations.front().itemObjectId, toolsObject);
		EXPECT_EQ(animations.front().time, 5000) << "X27: ExtractAction's 5,000 ms (ExtractAction.java)";
		EXPECT_EQ(animations.back().end, 1) << "X27: the animation's result 1";
		const std::vector<int32_t> deleted = deletedObjects(window, "X27");
		EXPECT_TRUE(contains(deleted, weapon)) << "X27: the sword is deleted (breakItem): " << join(namesOf(window));
		EXPECT_TRUE(contains(deleted, toolsObject)) << "X27: the tools are used up";
		EXPECT_TRUE(contains(messageIds(window), *sword.breakMessageId)) << "X27: STR_DECOMPOSE_ITEM_SUCCEED";
		const std::vector<decoders::InventoryItem> stones = addedItems(window, "X27");
		ASSERT_EQ(stones.size(), 1u) << "X27: ONE stone stack: " << join(namesOf(window));
		EXPECT_TRUE(std::ranges::any_of(sword.breakStones, [&](const auto& stone) { return stone.first == stones[0].templateId; }))
		  << "X27: " << stones[0].templateId << " is not in the oracle's grade set";
		const int64_t stoneCount = stones[0].general ? stones[0].general->count : 0;
		EXPECT_GE(stoneCount, (*sword.breakCountRange)[0]) << "X27: a weapon breaks into [2, 5] (EnchantService.java:52-74)";
		EXPECT_LE(stoneCount, (*sword.breakCountRange)[1]);
		ledgerB[PLAINSMAN_SWORD] -= 1;
		ledgerB[stones[0].templateId] += stoneCount;
		const int32_t enchantStone = stones[0].objectId;

		// the enchantment: exactly one of Java's two outcomes (the chance is capped at 80 %, EnchantService.java:126-127)
		from = b.mark();
		b.game->send(GameSession::CM_MANASTONE, GameSession::buildCM_MANASTONE(MANASTONE_ENCHANT, 1, armour, enchantStone, 0));
		const std::optional<size_t> ended = readUntil(
		  *b.game,
		  [&](const Packet& packet) {
			  if (packet.name != "SM_ITEM_USAGE_ANIMATION")
				  return false;
			  try {
				  const decoders::ItemUsageAnimation animation = decoders::decodeItemUsageAnimation(packet.data);
				  return animation.time == 0 && animation.end != 0;
			  } catch (const DecodeError&) {
				  return true;
			  }
		  },
		  10s);
		collectFor(*b.game, 500ms);
		window = b.since(from);
		ASSERT_TRUE(ended) << "X28: the enchantment never ended: " << join(namesOf(window));
		ledgerB[stones[0].templateId] -= 1;
		const std::vector<decoders::InventoryUpdateItem> stoneUpdates = updatesOf(window, enchantStone, "X28");
		EXPECT_TRUE((!stoneUpdates.empty() && stoneUpdates.back().item.general && stoneUpdates.back().item.general->count == stoneCount - 1) ||
		            (stoneCount == 1 && contains(deletedObjects(window, "X28"), enchantStone)))
		  << "X28: the stone -1 on either outcome: " << join(namesOf(window));
		const std::vector<int32_t> messages = messageIds(window);
		const bool success = contains(messages, STR_MSG_ENCHANT_ITEM_SUCCEED_NEW);
		const bool failure = contains(messages, STR_ENCHANT_ITEM_FAILED);
		EXPECT_NE(success, failure) << "X28: exactly one of the two outcomes: " << joinNumbers(messages);
		const std::vector<decoders::InventoryUpdateItem> armourUpdates = updatesOf(window, armour, "X28");
		ASSERT_FALSE(armourUpdates.empty()) << "X28: setEnchantLevel updates the tunic: " << join(namesOf(window));
		ASSERT_TRUE(armourUpdates.back().item.enchant);
		enchantSeen = armourUpdates.back().item.enchant->enchantLevel;
		if (success) {
			EXPECT_GE(enchantSeen, 1) << "X28: +1, +2 or +3";
			EXPECT_LE(enchantSeen, 3);
		} else {
			EXPECT_EQ(enchantSeen, 0) << "X28: a failure at +0 stays at 0";
			EXPECT_FALSE(contains(deletedObjects(window, "X28"), armour)) << "X28: the tunic kept (enchant type 0)";
		}
		EXPECT_EQ(usageAnimations(window, "X28").back().end, success ? 1 : 2) << "X28: the closing animation's result";
		std::cout << "X27/X28: " << stoneCount << " x " << stones[0].templateId << "; the enchantment " << (success ? "succeeded" : "failed") << " at +"
		          << enchantSeen << std::endl;
	});

	// ---- C19: crafting in Sanctum (X17-X21a) ----
	// A's account disconnects and A is seeded as a level-10 Gladiator (quest 1006 COMPLETE, exp 126,069) beside Hestia in Sanctum, with exactly
	// the kinah of the learn and the Salt and the recipe's Inina plus SURPLUS_ININA (§10.1 "Seeds", the surplus since the review of 2026-09-28;
	// the `players` row is loaded at connect, so the account is disconnected while it is written, F-3); A enters Sanctum (the enter world's
	// onLevelChange(old_level, 10), W-20), learns Cooking, buys the Salt, sends CM_CRAFT from 7 m, 12 m and 3 m, waits for the end, deletes the
	// recipe and quits. The expectations are m5c-economy's --daeva and --craft blocks, asked after A's quit with the old level that quit stored.
	runCase("C19", "A as a level-10 Gladiator in Sanctum: learns Cooking, buys Salt, crafts Roast Inina, deletes the recipe (X17-X21a)", [&] {
		b.drain();
		const std::string aId = std::to_string(a.playerId);
		// A's level as the gate saw it last: the level of the last SM_STATS_INFO of A's connection
		a.drain();
		std::optional<int32_t> levelSeen;
		for (const decoders::StatsInfo& info : decodeAll<decoders::StatsInfo>(a.game->recorded(), "SM_STATS_INFO", decoders::decodeStatsInfo, "C19"))
			levelSeen = info.level;
		disconnect(a);
		// the level A's last quit stored (PlayerLeaveWorldService.java:148), from which the enter world learns (PlayerEnterWorldService.java:204)
		const std::optional<int64_t> oldLevel = database.queryLong(schema, "SELECT old_level FROM players WHERE id = " + aId);
		ASSERT_TRUE(oldLevel) << "A has no players row";
		// the review of 2026-09-28: the oracle models the stored skills from this value, so a quit that stored a lower level (or none) would pass
		// X21a unseen - the quit must store the level A had
		EXPECT_EQ(*oldLevel, levelSeen.value_or(-1))
		  << "X21a: storeOldCharacterLevel(player.getLevel()) at the quit (PlayerLeaveWorldService.java:148); -1: A's connection had no SM_STATS_INFO";

		EconomyRequest request;
		request.noProfile = true;
		request.settings = oracleSettings;
		request.race = "ELYOS";
		request.direction = CRAFT_DIRECTION;
		request.daevaClass = std::string(DAEVA_CLASS);
		request.daevaOldLevel = static_cast<int32_t>(*oldLevel);
		request.craftRecipe = CRAFT_RECIPE;
		request.craftTool = OVEN;
		request.craftDistances = {CRAFT_NEAR, CRAFT_TOO_FAR, CRAFT_OUT_OF_PACKET_RANGE};
		const std::string answer = oracle->run(economyArguments(request));
		const EconomyAnswer sanctum = parseEconomy(answer);
		const LearnYes learnYes = parseLearnYes(answer);
		const C19Extras extras = parseC19Extras(answer);
		ASSERT_TRUE(sanctum.daeva && sanctum.craft) << "m5c-economy answered no daeva or craft block";
		const EconomyDaeva& daeva = *sanctum.daeva;
		const EconomyCraft& craft = *sanctum.craft;

		// the plan's premises (§2.10, §10.1), re-derived by the oracle
		EXPECT_EQ(daeva.level, 10) << "§2.10: a Gladiator whose quest 1006 is COMPLETE loads at level 10";
		EXPECT_EQ(daeva.levelWithoutQuest, 9) << "F-1: without the quest the same exp is level 9, and Hestia refuses silently";
		EXPECT_EQ(daeva.questStatus, "COMPLETE");
		EXPECT_TRUE(contains(craft.recipesLearnedWithTheSkill, CRAFT_RECIPE)) << "the recipe C19 crafts is the one learning Cooking teaches";
		EXPECT_EQ(learnYes.skillId, craft.skillId);
		EXPECT_EQ(learnYes.kinahDelta, -craft.learnCost);
		ASSERT_EQ(craft.seedItems.size(), 1u) << "§2.10: only Inina, which no spawned npc sells, is seeded";
		const EconomyComponent* bought = nullptr;
		for (const EconomyComponent& component : craft.components)
			if (component.vendor) {
				ASSERT_EQ(bought, nullptr) << "§2.10: one component (Salt) is bought";
				bought = &component;
			}
		ASSERT_NE(bought, nullptr) << "§2.10: the Salt is sold in Sanctum";
		const auto vendorOf = std::ranges::find_if(bought->vendors, [&](const EconomyVendor& vendor) { return vendor.npcId == *bought->vendor; });
		ASSERT_NE(vendorOf, bought->vendors.end());
		const EconomyVendor& vendor = *vendorOf;
		EXPECT_EQ(craft.exactKinah, craft.learnCost + vendor.kinah) << "§2.10: the learn and the Salt spend the seed exactly, so the Salt takes the last kinah";
		const auto spotAt = [&](double distance) -> const EconomyCraftSpot& {
			for (const EconomyCraftSpot& spot : craft.spots)
				if (spot.distance == distance)
					return spot;
			throw std::runtime_error("m5c-economy answered no craft spot at " + std::to_string(distance) + " m");
		};
		const EconomyCraftSpot& nearSpot = spotAt(CRAFT_NEAR);
		const EconomyCraftSpot& tooFarSpot = spotAt(CRAFT_TOO_FAR);
		const EconomyCraftSpot& outOfRangeSpot = spotAt(CRAFT_OUT_OF_PACKET_RANGE);
		EXPECT_TRUE(nearSpot.inPacketRange && nearSpot.inCheckCraftRange) << "X19: " << nearSpot.outcome;
		EXPECT_TRUE(tooFarSpot.inPacketRange && !tooFarSpot.inCheckCraftRange) << "X18: 7 m passes CM_CRAFT's 10 m and fails checkCraft's 5 m + radii";
		EXPECT_TRUE(tooFarSpot.otherToolsInCheckCraftRange.empty()) << "--direction " << CRAFT_DIRECTION;
		EXPECT_FALSE(outOfRangeSpot.inPacketRange) << "X18: 12 m is outside CM_CRAFT's centre-to-centre 10 m";

		// §10.1 "Seeds" for C19, with A's account disconnected (F-3): the Daeva (class, exp, the ascension quest), Sanctum, the exact kinah, one Inina
		database.execute(schema, "UPDATE players SET player_class = '" + daeva.playerClass + "', exp = " + std::to_string(daeva.exp) + ", world_id = " +
		                           std::to_string(craft.seedWorldId) + ", x = " + std::to_string(craft.seedX) + ", y = " + std::to_string(craft.seedY) +
		                           ", z = " + std::to_string(craft.seedZ) + ", heading = 0 WHERE id = " + aId);
		database.execute(schema, "INSERT INTO player_quests (player_id, quest_id, status, complete_count) VALUES (" + aId + ", " +
		                           std::to_string(daeva.questId) + ", '" + daeva.questStatus + "', 1)");
		database.execute(schema, "UPDATE inventory SET item_count = " + std::to_string(craft.exactKinah) + " WHERE item_owner = " + aId +
		                           " AND item_id = " + std::to_string(KINAH_ITEM));
		// the seed item (Inina) with SURPLUS_ININA more than the recipe takes, so that a second consumption would show (X19)
		std::map<int32_t, int32_t> seededA; // item id -> object id
		for (const auto& [itemId, count] : craft.seedItems) {
			seededA[itemId] = database.seedInventoryItem(schema, {a.playerId, itemId, count + SURPLUS_ININA, ModelItem::CUBE, 65535});
			ledgerA[itemId] += count + SURPLUS_ININA;
		}
		ledgerA[KINAH_ITEM] = craft.exactKinah;

		// ---- X21a: the enter world of the seeded Daeva ----
		const std::vector<Packet> burst = relogIn(servers, a).second;
		const std::vector<Packet> entered = a.game->recorded(); // the login, the enter world and the level ready of the new connection
		const Packet* spawnPacket = firstOfName(burst, "SM_PLAYER_SPAWN");
		ASSERT_NE(spawnPacket, nullptr);
		EXPECT_EQ(decoders::decodePlayerSpawn(spawnPacket->data).worldId, craft.seedWorldId) << "A enters Sanctum";
		EXPECT_NEAR(a.x, craft.seedX, 0.01) << "A spawns at the seeded spot beside Hestia";
		EXPECT_NEAR(a.y, craft.seedY, 0.01);
		std::optional<decoders::StatsInfo> stats;
		for (const decoders::StatsInfo& info : decodeAll<decoders::StatsInfo>(entered, "SM_STATS_INFO", decoders::decodeStatsInfo, "X21a"))
			stats = info;
		ASSERT_TRUE(stats) << "X21a: no SM_STATS_INFO at the enter world: " << join(namesOf(entered));
		EXPECT_EQ(stats->level, daeva.level) << "X21a: updateDaeva finds quest 1006 COMPLETE (PlayerCommonData.java:276-281, 588-610); level "
		                                     << daeva.levelWithoutQuest << " is F-1";
		std::set<int32_t> skills;
		std::map<int32_t, decoders::SkillEntry> shownSkills;
		for (const decoders::SkillList& skillList : decodeAll<decoders::SkillList>(entered, "SM_SKILL_LIST", decoders::decodeSkillList, "X21a"))
			if (skillList.messageId == 0)
				for (const decoders::SkillEntry& entry : skillList.skills) {
					skills.insert(entry.skillId);
					shownSkills[entry.skillId] = entry;
				}
		const std::set<int32_t> wantSkills(daeva.skills.begin(), daeva.skills.end());
		std::vector<int32_t> missing, extra;
		std::ranges::set_difference(wantSkills, skills, std::back_inserter(missing));
		std::ranges::set_difference(skills, wantSkills, std::back_inserter(extra));
		EXPECT_TRUE(missing.empty() && extra.empty()) << "X21a: the enter world's SM_SKILL_LIST against the oracle's (learnNewSkills(" << daeva.learnNewSkills[0]
		                                              << ", " << daeva.learnNewSkills[1] << ") over the Warrior's and the Gladiator's rows, and the Daeva swap): "
		                                              << "missing " << joinNumbers(missing) << ", extra " << joinNumbers(extra);
		ASSERT_TRUE(daeva.swapRemoved && daeva.swapAdded) << "the oracle's Daeva swap 30001 -> 30002";
		EXPECT_FALSE(skills.contains(*daeva.swapRemoved)) << "X21a: " << *daeva.swapRemoved << " is swapped out (PlayerController.upgradePlayer)";
		EXPECT_TRUE(skills.contains(*daeva.swapAdded)) << "X21a: " << *daeva.swapAdded << " is swapped in";
		for (const EconomyLearnedSkill& learned : daeva.learnedSkills)
			if (learned.level == 10)
				EXPECT_TRUE(skills.contains(learned.skillId)) << "X21a: the level-10 skill " << learned.skillId << " of the " << learned.playerClass;
		// the levels (the review of 2026-09-28): each skill at the oracle's level - its template's lvl (SkillLearnTemplate.getSkillLevel), 30002 at
		// 30001's - as SM_SKILL_LIST shows it: 1 for a normal skill (SkillEntryWriter.java:27); player_skills after the quit holds every level
		EXPECT_EQ(extras.skillLevels.size(), daeva.skills.size()) << "the oracle's skillLevels name its skills";
		for (const auto& [skillId, level] : extras.skillLevels) {
			const auto shown = shownSkills.find(skillId);
			if (shown == shownSkills.end())
				continue; // the set comparison above names it
			const bool normal = shown->second.skillType == 0 && skillId < NORMAL_SKILL_ID_LIMIT;
			EXPECT_EQ(shown->second.skillLevel, normal ? 1 : level)
			  << "X21a: skill " << skillId << " in the enter world's SM_SKILL_LIST (the oracle's level " << level
			  << (normal ? ", a normal skill shows 1)" : ")");
		}
		// the Daeva swap's removal (learnNewSkills -> removeSkill, SkillLearnService.java:73, 108): exactly one SM_SKILL_REMOVE, of 30001
		EXPECT_TRUE(extras.smSkillRemove.has_value()) << "the oracle's Daeva swap names an SM_SKILL_REMOVE";
		std::vector<SkillRemove> wantRemoved;
		if (extras.smSkillRemove)
			wantRemoved.push_back({static_cast<uint16_t>(*extras.smSkillRemove), SKILL_REMOVE_TAPPING_FLAG, 0});
		EXPECT_EQ(decodeAll<SkillRemove>(entered, "SM_SKILL_REMOVE", decodeSkillRemove, "X21a"), wantRemoved)
		  << "X21a: the swap's SM_SKILL_REMOVE(30001, its profession flag, skill type 0) during the enter world's onLevelChange";
		std::vector<int32_t> learnedRecipes = decodeAll<int32_t>(entered, "SM_LEARN_RECIPE", decoders::decodeLearnRecipe, "X21a");
		std::vector<int32_t> wantRecipes = daeva.learnedRecipes;
		std::ranges::sort(learnedRecipes);
		std::ranges::sort(wantRecipes);
		EXPECT_EQ(learnedRecipes, wantRecipes) << "X21a: exactly the three Elyos morph recipes, one SM_LEARN_RECIPE each (onLearnSkill -> autoLearnRecipes "
		                                       << "for the morph skill 40009, W-06): " << joinNumbers(learnedRecipes);
		std::set<int32_t> recipeList;
		for (const std::vector<int32_t>& recipes : decodeAll<std::vector<int32_t>>(entered, "SM_RECIPE_LIST", decoders::decodeRecipeList, "X21a"))
			recipeList.insert(recipes.begin(), recipes.end());
		EXPECT_EQ(recipeList, std::set<int32_t>(wantRecipes.begin(), wantRecipes.end())) << "X21a: the enter world's SM_RECIPE_LIST";
		const std::vector<std::string> enteredDifferences = ledgerDifferences(ledgerOf(a.model), ledgerA);
		EXPECT_TRUE(enteredDifferences.empty()) << "X16: A's Sanctum SM_INVENTORY_INFO against the ledger (have/want): " << join(enteredDifferences);

		// ---- X17: Cooking from Hestia, then the Salt from the vendor with the last kinah ----
		const std::optional<int32_t> hestia = npcObject(a, craft.masterNpcId, craft.master.x, craft.master.y);
		ASSERT_TRUE(hestia) << "A was never sent the SM_NPC_INFO of " << craft.masterNpcId;
		size_t from = a.mark();
		a.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(*hestia));
		collectFor(*a.game, STEP);
		EXPECT_EQ(dialogWindows(a.since(from), "C19").size(), 1u) << "C19: Hestia's window: " << join(namesOf(a.since(from)));
		from = a.mark();
		a.game->send(GameSession::CM_DIALOG_SELECT, GameSession::buildCM_DIALOG_SELECT(*hestia, static_cast<uint16_t>(craft.dialogAction)));
		collectFor(*a.game, STEP);
		const std::vector<decoders::QuestionWindow> questions = questionWindows(a.since(from), "X17");
		ASSERT_EQ(questions.size(), 1u) << "X17: COMBINE_SKILL_LEVELUP asks (a level-9 character is refused without a packet, "
		                                << "CraftSkillUpdateService.java:83-84): " << join(namesOf(a.since(from)));
		EXPECT_EQ(questions[0].code, craft.learnQuestion.id) << "X17: STR_CRAFT_ADDSKILL_CONFIRM";
		for (size_t i = 0; i < questions[0].params.size(); i++) {
			const auto l10n = craft.learnQuestion.l10nParams.find(i);
			EXPECT_EQ(questions[0].params[i], l10n == craft.learnQuestion.l10nParams.end() ? craft.learnQuestion.params[i] : l10nText(l10n->second))
			  << "X17: parameter " << i << " (the profession's l10n name, then Profession.getUpgradeCost(0))";
		}
		EXPECT_EQ(questions[0].senderId, craft.learnQuestion.senderId);
		EXPECT_EQ(questions[0].rangeOrCooldownSeconds, craft.learnQuestion.range);
		const int32_t kinah = a.kinahObject().value_or(0);
		const int64_t seedKinah = a.kinah();
		EXPECT_EQ(seedKinah, craft.exactKinah) << "C19's kinah seed";
		from = a.mark();
		a.game->send(GameSession::CM_QUESTION_RESPONSE, GameSession::buildCM_QUESTION_RESPONSE(craft.learnQuestion.id, GameSession::ANSWER_YES));
		collectFor(*a.game, STEP);
		std::vector<Packet> window = a.since(from);
		ledgerA[KINAH_ITEM] += learnYes.kinahDelta;
		EXPECT_EQ(kinahUpdates(window, kinah, "X17"), std::vector<int64_t>{seedKinah + learnYes.kinahDelta})
		  << "X17: " << learnYes.kinahDelta << " (tryDecreaseKinah of the price): " << joinNumbers(messageIds(window));
		std::vector<decoders::SkillList> skillUpdates = decodeAll<decoders::SkillList>(window, "SM_SKILL_LIST", decoders::decodeSkillList, "X17");
		EXPECT_EQ(skillUpdates.size(), 1u) << "X17: one SM_SKILL_LIST for the new skill: " << join(namesOf(window));
		if (!skillUpdates.empty()) {
			ASSERT_EQ(skillUpdates[0].skills.size(), 1u);
			EXPECT_EQ(skillUpdates[0].skills[0].skillId, learnYes.skillId) << "X17: addSkill(skillId, skillLevel + 1)";
			EXPECT_EQ(skillUpdates[0].skills[0].skillLevel, learnYes.skillLevel);
			EXPECT_EQ(skillUpdates[0].messageId, SKILL_LIST_CRAFT_LEARNED) << "X17: a new crafting skill's message (SkillLearnService.java:49)";
		}
		EXPECT_EQ(decodeAll<int32_t>(window, "SM_LEARN_RECIPE", decoders::decodeLearnRecipe, "X17"), craft.recipesLearnedWithTheSkill)
		  << "X17: exactly one SM_LEARN_RECIPE, the Elyos autolearn recipe (onLearnSkill -> autoLearnRecipes and its race filter, W-06)";
		const auto animationsOfA = [&](const std::vector<std::pair<uint16_t, int32_t>>& wanted) {
			std::vector<ActionAnimationPacket> packets;
			for (const auto& [animation, levelOrObjectId] : wanted)
				packets.push_back({a.playerId, animation, levelOrObjectId});
			return packets;
		};
		EXPECT_EQ(decodeAll<ActionAnimationPacket>(window, "SM_ACTION_ANIMATION", decodeActionAnimation, "X17"), animationsOfA(extras.learnAnimations))
		  << "X17: onLearnSkill's CRAFT_LEVEL_UP at a crafting skill's level 1, broadcast to A too (SkillLearnService.java:24-28)";

		const EconomyTalk& vendorTalk = vendor.talk;
		walkTo(a, vendorTalk.nearSpot.x, vendorTalk.nearSpot.y, vendorTalk.nearSpot.z);
		const std::optional<int32_t> seller = npcObject(a, vendor.npcId, vendorTalk.x, vendorTalk.y);
		ASSERT_TRUE(seller) << "A was never sent the SM_NPC_INFO of " << vendor.npcId;
		a.game->send(GameSession::CM_DIALOG_SELECT, GameSession::buildCM_DIALOG_SELECT(*seller, DIALOG_BUY));
		collectFor(*a.game, STEP);
		EXPECT_EQ(a.kinah(), vendor.kinah) << "the learn leaves exactly the Salt's price";
		from = a.mark();
		const std::array<GameSession::BuyItemEntry, 1> salt{{{bought->itemId, bought->quantity}}};
		a.game->send(GameSession::CM_BUY_ITEM, GameSession::buildCM_BUY_ITEM(*seller, GameSession::TRADE_BUY, salt));
		collectFor(*a.game, STEP);
		window = a.since(from);
		ledgerA[KINAH_ITEM] -= vendor.kinah;
		ledgerA[bought->itemId] += bought->quantity;
		EXPECT_EQ(kinahUpdates(window, kinah, "X17"), std::vector<int64_t>{0})
		  << "X17: the Salt takes the last kinah (>= in calculateBuyListPrice and tryDecreaseKinah): messages " << joinNumbers(messageIds(window));
		const std::vector<decoders::InventoryItem> saltAdded = addedItems(window, "X17");
		EXPECT_EQ(saltAdded.size(), 1u) << "X17: " << join(namesOf(window));
		if (!saltAdded.empty()) {
			EXPECT_EQ(saltAdded[0].templateId, bought->itemId);
			EXPECT_EQ(saltAdded[0].general ? saltAdded[0].general->count : -1, bought->quantity);
		}

		// ---- X18: CM_CRAFT from 7 m (checkCraft refuses) and from 12 m (the packet returns) ----
		const auto chosenTool = std::ranges::find(craft.tools, craft.chosenToolStaticId, &EconomyTool::staticId);
		ASSERT_NE(chosenTool, craft.tools.end()) << "the oracle's chosen oven " << craft.chosenToolStaticId << " is one of its tools";
		const std::optional<int32_t> oven = staticObject(a, craft.toolTemplateId, *chosenTool);
		ASSERT_TRUE(oven) << "A was never sent the SM_GATHERABLE_INFO of the oven with static id " << craft.chosenToolStaticId << " at ("
		                  << chosenTool->x << ", " << chosenTool->y << ", " << chosenTool->z << ")";
		std::vector<GameSession::CraftMaterial> materials;
		for (const auto& [itemId, count] : craft.recipeComponents)
			materials.push_back({itemId, count});
		const auto sendCraft = [&] {
			a.game->send(GameSession::CM_CRAFT, GameSession::buildCM_CRAFT(0, craft.toolTemplateId, CRAFT_RECIPE, *oven, materials, 0));
		};
		const auto componentCounts = [&] {
			std::map<int32_t, int64_t> counts;
			for (const auto& [itemId, count] : craft.recipeComponents)
				counts[itemId] = a.countOf(itemId);
			return counts;
		};
		std::map<int32_t, int64_t> wantComponents;
		for (const auto& [itemId, count] : craft.recipeComponents)
			wantComponents[itemId] = count + (seededA.contains(itemId) ? SURPLUS_ININA : 0);
		EXPECT_EQ(componentCounts(), wantComponents) << "the components of one craft and the surplus Inina, no more";

		walkTo(a, tooFarSpot.x, tooFarSpot.y, tooFarSpot.z);
		from = a.mark();
		sendCraft();
		collectFor(*a.game, SILENCE);
		window = a.since(from);
		EXPECT_TRUE(contains(messageIds(window), STR_COMBINE_TOO_FAR_FROM_TOOL))
		  << "X18: from " << tooFarSpot.distance << " m STR_COMBINE_TOO_FAR_FROM_TOOL (checkCraft's isInRange(player, target, 5, false)): "
		  << joinNumbers(messageIds(window));
		const std::vector<decoders::CraftUpdate> cancelled = decodeAll<decoders::CraftUpdate>(window, "SM_CRAFT_UPDATE", decoders::decodeCraftUpdate, "X18");
		EXPECT_EQ(cancelled.size(), 1u) << "X18: sendCancelCraft's one SM_CRAFT_UPDATE: " << join(namesOf(window));
		if (!cancelled.empty()) {
			EXPECT_EQ(cancelled[0].action, decoders::CRAFT_UPDATE_CANCELLED) << "X18: SM_CRAFT_UPDATE(action 4)";
			EXPECT_EQ(cancelled[0].skillId, craft.skillId);
			EXPECT_EQ(cancelled[0].itemId, craft.productId);
			// the review of 2026-09-28: the bars, the speed and the delay too (CraftService.java:236, SM_CRAFT_UPDATE(..., 0, 0, 4, 0, 0))
			expectCraftUpdate(cancelled[0], extras.cancel, "X18: sendCancelCraft's update");
		}
		EXPECT_EQ(decodeAll<decoders::CraftAnimation>(window, "SM_CRAFT_ANIMATION", decoders::decodeCraftAnimation, "X18"),
		          (std::vector<decoders::CraftAnimation>{{a.playerId, *oven, 0, CRAFT_ANIMATION_END}}))
		  << "X18: sendCancelCraft's SM_CRAFT_ANIMATION(player, target, 0, 2)";
		EXPECT_TRUE(itemPackets(window).empty()) << "X18: checkCraft consumes the materials only at its end: " << join(itemPackets(window));
		EXPECT_EQ(componentCounts(), wantComponents) << "X18: Inina and Salt unchanged";

		walkTo(a, outOfRangeSpot.x, outOfRangeSpot.y, outOfRangeSpot.z);
		from = a.mark();
		sendCraft();
		collectFor(*a.game, SILENCE);
		window = a.since(from);
		EXPECT_TRUE(ofName(window, "SM_CRAFT_UPDATE").empty() && ofName(window, "SM_CRAFT_ANIMATION").empty() && messageIds(window).empty() &&
		            itemPackets(window).empty())
		  << "X18: from " << outOfRangeSpot.distance << " m nothing (CM_CRAFT's isInRange(player, staticObject, 10)): " << join(namesOf(window));
		EXPECT_EQ(componentCounts(), wantComponents) << "X18: Inina and Salt unchanged";

		// ---- X19, X20: CM_CRAFT from 3 m, to the end ----
		walkTo(a, nearSpot.x, nearSpot.y, nearSpot.z);
		std::map<int32_t, int32_t> componentObjects; // object id -> item id
		a.model.sync();
		for (const auto& [itemId, count] : craft.recipeComponents)
			for (const ModelItem& item : a.model.byItemId(itemId))
				componentObjects[item.objectId] = itemId;
		int64_t expShownBefore = 0;
		for (const Packet& packet : a.game->recorded()) {
			try {
				if (packet.name == "SM_STATUPDATE_EXP")
					expShownBefore = decoders::decodeStatUpdateExp(packet.data).currentExp;
				else if (packet.name == "SM_STATS_INFO")
					expShownBefore = decoders::decodeStatsInfo(packet.data).expShown;
			} catch (const DecodeError&) {
				// the rows below fail on the missing update
			}
		}
		// the wait is five times the oracle's longest craft, so that a craft that runs long is counted by X20 instead of being cut off here
		// (the step without its 70 minimum takes about 35 ticks on average, §10.4)
		from = a.mark();
		sendCraft();
		const std::optional<size_t> ended = readUntil(
		  *a.game,
		  [](const Packet& packet) {
			  if (packet.name != "SM_CRAFT_UPDATE")
				  return false;
			  try {
				  return decoders::decodeCraftUpdate(packet.data).action >= decoders::CRAFT_UPDATE_CANCELLED;
			  } catch (const DecodeError&) {
				  return true;
			  }
		  },
		  std::chrono::milliseconds(5 * craft.mostMillis) + 15s);
		collectFor(*a.game, STEP); // finishCrafting's packets follow the (5) in the same tick
		window = a.since(from);
		ASSERT_TRUE(ended) << "X19: the craft did not end within 5 x " << craft.mostMillis << " ms + 15 s: " << join(namesOf(window));
		ledgerA[craft.productId] += craft.productQuantity;
		for (const auto& [itemId, count] : craft.recipeComponents)
			ledgerA[itemId] -= count;
		std::vector<std::pair<decoders::CraftUpdate, std::chrono::steady_clock::time_point>> updates;
		for (const Packet& packet : ofName(window, "SM_CRAFT_UPDATE")) {
			try {
				updates.emplace_back(decoders::decodeCraftUpdate(packet.data), packet.receivedAt);
			} catch (const DecodeError& error) {
				ADD_FAILURE() << "X19: SM_CRAFT_UPDATE does not decode: " << error.what();
			}
		}
		ASSERT_GE(updates.size(), 3u) << "X19: the start pair and the end: " << join(namesOf(window));
		const decoders::CraftUpdate& init = updates.front().first;
		EXPECT_EQ(init.action, decoders::CRAFT_UPDATE_INIT) << "X19: SM_CRAFT_UPDATE(0) (CraftingTask.onInteractionStart)";
		EXPECT_EQ(init.skillId, craft.skillId);
		EXPECT_EQ(init.itemId, craft.productId) << "X19: the base product, not a combo product (crits off, D6)";
		EXPECT_EQ(init.success, 1000) << "X19: the full bars (AbstractCraftTask.fullBarValue)";
		EXPECT_EQ(init.failure, 1000);
		EXPECT_EQ(updates[1].first.action, decoders::CRAFT_UPDATE_NORMAL) << "X19: then SM_CRAFT_UPDATE(1) with empty bars";
		EXPECT_EQ(updates[1].first.success, 0);
		EXPECT_EQ(updates[1].first.failure, 0);
		const decoders::CraftUpdate& last = updates.back().first;
		EXPECT_EQ(last.action, decoders::CRAFT_UPDATE_SUCCESS) << "X19: SM_CRAFT_UPDATE(5) (CraftingTask.onSuccessFinish)";
		EXPECT_EQ(last.itemId, craft.productId);
		EXPECT_EQ(last.success, 1000);
		// the review of 2026-09-28: the speed and the delay of the start pair and of the end (CraftingTask.java onInteractionStart, onSuccessFinish)
		expectCraftUpdate(init, extras.init, "X19: onInteractionStart's first update");
		expectCraftUpdate(updates[1].first, extras.start, "X19: onInteractionStart's second update");
		expectCraftUpdate(last, extras.success, "X19: onSuccessFinish's update");
		// X20: the analyze ticks between the start pair and the end, each a progress update of a growing success bar that ends full, each with
		// the bar's executionSpeed and showBarDelay (CraftingTask.sendInteractionUpdate; analyzeInteraction, CraftingTask.java:170-172, as m5c-craft)
		const size_t progress = updates.size() - 3;
		int32_t previous = 0;
		for (size_t i = 2; i + 1 < updates.size(); i++) {
			const decoders::CraftUpdate& update = updates[i].first;
			EXPECT_TRUE(update.action == decoders::CRAFT_UPDATE_NORMAL || update.action == decoders::CRAFT_UPDATE_CRIT_BLUE)
			  << "X20: progress update " << i - 1 << " has action " << static_cast<int32_t>(update.action);
			EXPECT_GT(update.success, previous) << "X20: the success bar grows by at least the 70 minimum step";
			EXPECT_EQ(update.failure, 0) << "X20: gameserver.craft.fail.chance = 0 (D6)";
			EXPECT_EQ(update.executionSpeed, extras.executionSpeed) << "X20: progress update " << i - 1 << "'s executionSpeed";
			EXPECT_EQ(update.delay, extras.showBarDelay) << "X20: progress update " << i - 1 << "'s showBarDelay";
			previous = update.success;
		}
		EXPECT_EQ(previous, 1000) << "X20: the last progress update fills the bar";
		EXPECT_GE(progress, static_cast<size_t>(craft.fewestSteps)) << "X20: between " << craft.fewestSteps << " and " << craft.mostSteps << " progress updates";
		EXPECT_LE(progress, static_cast<size_t>(craft.mostSteps)) << "X20: the step without its 70 minimum takes more";
		// the ticks (AbstractInteractionTask.start: scheduleAtFixedRate(delay, interval) right after onInteractionStart): the first progress update
		// firstTickDelay after the start pair, every later update and the end one interval (2,500 - 60 x the level difference, CraftService.
		// startCrafting) after the update before it - gap by gap (the review of 2026-09-28: a total alone let a wrong first delay pass)
		const int64_t firstGap = millisBetween(updates[0].second, updates[2].second);
		EXPECT_NEAR(static_cast<double>(firstGap), static_cast<double>(craft.firstTickDelay), TICK_TOLERANCE_MS)
		  << "X20: from the start pair to the first progress update";
		int64_t largestGapError = std::abs(firstGap - craft.firstTickDelay);
		for (size_t i = 3; i < updates.size(); i++) {
			const int64_t gap = millisBetween(updates[i - 1].second, updates[i].second);
			largestGapError = std::max(largestGapError, std::abs(gap - craft.interval));
			EXPECT_NEAR(static_cast<double>(gap), static_cast<double>(craft.interval), TICK_TOLERANCE_MS)
			  << "X20: from update " << i - 2 << " to " << (i + 1 < updates.size() ? "the next" : "the end");
		}
		const int64_t elapsed = millisBetween(updates.front().second, updates.back().second);
		const int64_t expected = craft.firstTickDelay + static_cast<int64_t>(progress) * craft.interval;
		EXPECT_NEAR(static_cast<double>(elapsed), static_cast<double>(expected), TICK_TOLERANCE_MS)
		  << "X20: " << progress << " progress updates and the end in " << elapsed << " ms, at " << craft.interval << " ms intervals after "
		  << craft.firstTickDelay << " ms (the morph's 200 would take " << craft.firstTickDelay + static_cast<int64_t>(progress) * 200 << " ms)";
		EXPECT_GE(static_cast<double>(elapsed), static_cast<double>(craft.fewestMillis) - TICK_TOLERANCE_MS)
		  << "X20: the craft takes " << craft.fewestMillis << "-" << craft.mostMillis << " ms";
		EXPECT_LE(static_cast<double>(elapsed), static_cast<double>(craft.mostMillis) + TICK_TOLERANCE_MS)
		  << "X20: the craft takes " << craft.fewestMillis << "-" << craft.mostMillis << " ms";
		EXPECT_EQ(decodeAll<decoders::CraftAnimation>(window, "SM_CRAFT_ANIMATION", decoders::decodeCraftAnimation, "X19"),
		          (std::vector<decoders::CraftAnimation>{{a.playerId, *oven, static_cast<uint16_t>(craft.skillId), CRAFT_ANIMATION_START},
		                                                 {a.playerId, *oven, static_cast<uint16_t>(craft.skillId), CRAFT_ANIMATION_PROGRESS},
		                                                 {a.playerId, *oven, 0, CRAFT_ANIMATION_END}}))
		  << "X19: SM_CRAFT_ANIMATION (…, 40001, 0) and (…, 1) at the start, (…, 0, 2) at the end";
		// the materials once (checkCraft's decreaseByItemId per component, CraftService.java:222-230), the product, the skill level and the exp
		// (CraftService.finishCrafting): the Salt's stack is used up, the Inina's keeps the surplus - a second consumption would take it too
		std::map<int32_t, int64_t> leftComponents;
		for (const auto& [itemId, count] : craft.recipeComponents)
			leftComponents[itemId] = wantComponents[itemId] - count;
		const std::vector<int32_t> deleted = deletedObjects(window, "X19");
		for (const auto& [object, itemId] : componentObjects) {
			if (leftComponents[itemId] == 0) {
				EXPECT_TRUE(contains(deleted, object))
				  << "X19: the component stack " << object << " of " << itemId << " is used up: " << join(namesOf(window));
				continue;
			}
			EXPECT_FALSE(contains(deleted, object)) << "X19: the stack " << object << " of " << itemId << " keeps " << leftComponents[itemId];
			const std::vector<decoders::InventoryUpdateItem> stack = updatesOf(window, object, "X19");
			EXPECT_TRUE(!stack.empty() && stack.back().item.general && stack.back().item.general->count == leftComponents[itemId])
			  << "X19: SM_INVENTORY_UPDATE_ITEM of the stack " << object << " of " << itemId << " to " << leftComponents[itemId] << ": "
			  << join(namesOf(window));
		}
		EXPECT_EQ(componentCounts(), leftComponents) << "X19: Inina -1 (the surplus left), Salt -2";
		const std::vector<decoders::InventoryItem> products = addedItems(window, "X19");
		EXPECT_EQ(products.size(), 1u) << "X19: " << join(namesOf(window));
		if (!products.empty()) {
			EXPECT_EQ(products[0].templateId, craft.productId) << "X19: the product, not getComboProduct";
			EXPECT_EQ(products[0].general ? products[0].general->count : -1, craft.productQuantity);
		}
		skillUpdates = decodeAll<decoders::SkillList>(window, "SM_SKILL_LIST", decoders::decodeSkillList, "X19");
		EXPECT_EQ(skillUpdates.size(), 1u) << "X19: the skill level-up: " << join(namesOf(window));
		if (!skillUpdates.empty()) {
			ASSERT_EQ(skillUpdates[0].skills.size(), 1u);
			EXPECT_EQ(skillUpdates[0].skills[0].skillId, craft.skillId);
			EXPECT_EQ(skillUpdates[0].skills[0].skillLevel, craft.skillLevelAfter) << "X19: " << craft.xpReward << " skill xp over the level-1 threshold";
			EXPECT_EQ(skillUpdates[0].messageId, SKILL_LIST_CRAFT_LEVEL_UP) << "X19: a crafting skill's level-up message (SkillLearnService.java:49)";
		}
		EXPECT_EQ(decodeAll<ActionAnimationPacket>(window, "SM_ACTION_ANIMATION", decodeActionAnimation, "X19"), animationsOfA(extras.skillUpAnimations))
		  << "X19: the level-up to " << craft.skillLevelAfter << " is no CRAFT_LEVEL_UP level (SkillLearnService.java:24-28)";
		const std::vector<decoders::StatUpdateExp> exp =
		  decodeAll<decoders::StatUpdateExp>(window, "SM_STATUPDATE_EXP", decoders::decodeStatUpdateExp, "X19");
		EXPECT_FALSE(exp.empty()) << "X19: the exp update: " << join(namesOf(window));
		if (!exp.empty())
			EXPECT_EQ(exp.back().currentExp, expShownBefore + craft.playerExp) << "X19: exp +" << craft.playerExp << " (addExp(xpReward, XP_CRAFTING))";

		// ---- X21: the recipe deleted ----
		from = a.mark();
		a.game->send(GameSession::CM_RECIPE_DELETE, GameSession::buildCM_RECIPE_DELETE(CRAFT_RECIPE));
		collectFor(*a.game, STEP);
		EXPECT_EQ(decodeAll<int32_t>(a.since(from), "SM_RECIPE_DELETE", decoders::decodeRecipeDelete, "X21"), std::vector<int32_t>{CRAFT_RECIPE})
		  << "X21: RecipeList.deleteRecipe: " << join(namesOf(a.since(from)));

		// the quit, and the rows it stored
		a.drain();
		const std::vector<std::string> quitDifferences = ledgerDifferences(ledgerOf(a.model), ledgerA);
		EXPECT_TRUE(quitDifferences.empty()) << "X16: A's model against the ledger after C19 (have/want): " << join(quitDifferences);
		disconnect(a);
		std::this_thread::sleep_for(500ms);
		std::set<int32_t> storedRecipes;
		for (const auto& row : database.queryRows(schema, "SELECT recipe_id FROM player_recipes WHERE player_id = " + aId, 1))
			storedRecipes.insert(std::stoi(row[0].value_or("0")));
		EXPECT_FALSE(storedRecipes.contains(CRAFT_RECIPE)) << "X21: PlayerRecipesDAO.delRecipe";
		EXPECT_EQ(storedRecipes, std::set<int32_t>(wantRecipes.begin(), wantRecipes.end())) << "X21a: player_recipes holds the three morph recipes";
		EXPECT_EQ(database.queryLong(schema, "SELECT skill_level FROM player_skills WHERE player_id = " + aId + " AND skill_id = " +
		                                       std::to_string(craft.skillId))
		            .value_or(-1),
		          craft.skillLevelAfter)
		  << "X21: player_skills holds Cooking at its new level";
		// the review of 2026-09-28: every stored level, which SM_SKILL_LIST shows as 1 for a normal skill
		std::map<int32_t, int32_t> storedSkills;
		for (const auto& row : database.queryRows(schema, "SELECT skill_id, skill_level FROM player_skills WHERE player_id = " + aId, 2))
			storedSkills[std::stoi(row[0].value_or("0"))] = std::stoi(row[1].value_or("0"));
		std::map<int32_t, int32_t> wantStoredSkills = extras.skillLevels;
		wantStoredSkills[craft.skillId] = craft.skillLevelAfter;
		EXPECT_EQ(storedSkills, wantStoredSkills) << "X21a: player_skills after the quit: the oracle's skills at their levels (30002 at 30001's, no "
		                                          << "30001) and Cooking";
		EXPECT_EQ(database.queryLong(schema, "SELECT exp FROM players WHERE id = " + aId).value_or(-1), daeva.exp + craft.playerExp)
		  << "X19: players.exp";
		b.drain();
		std::cout << "C19: old level " << *oldLevel << ", " << progress << " progress updates in " << elapsed << " ms (the largest gap off by "
		          << largestGapError << " ms), " << skills.size()
		          << " skills and recipes " << joinNumbers(learnedRecipes) << " at the enter world" << std::endl;
	});

	// ---- C20: reports and shutdown (X22, and the last quit's rows of X23, X26, X28) ----
	// C20a always runs, whatever failed before it: the characters must be out of the world for X22's bar (nobody online at the end)
	cases.run("C20a", "both quit; the rows of the last quit (X16, X23, X26, X28)", [&] {
		for (ScenarioClient* client : {&a, &b}) {
			try {
				client->drain();
				disconnect(*client);
			} catch (const std::exception& exception) {
				ADD_FAILURE() << client->label << " could not quit: " << exception.what();
				client->game.reset();
				client->login.reset();
			}
		}
		std::this_thread::sleep_for(500ms);
		if (!ok) {
			std::cout << "C20a: an earlier case ended the script, so the rows of the last quit are not the script's and are not read" << std::endl;
			return;
		}
		// X16 "and every later quit": both characters against their ledgers, C15-C18 included
		for (ScenarioClient* client : {&a, &b}) {
			Ledger stored;
			for (const auto& row : database.queryRows(schema, "SELECT item_id, SUM(item_count) FROM inventory WHERE item_owner = " +
			                                                    std::to_string(client->playerId) + " GROUP BY item_id",
			                                          2))
				stored[std::stoi(row[0].value_or("0"))] = std::stoll(row[1].value_or("0"));
			const std::vector<std::string> differences = ledgerDifferences(stored, client == &a ? ledgerA : ledgerB);
			EXPECT_TRUE(differences.empty()) << "X16 (the last quit): " << client->label << "'s `inventory` rows against the ledger (have/want): "
			                                 << join(differences);
		}
		EXPECT_EQ(database.queryLong(schema, "SELECT COUNT(*) FROM inventory WHERE item_unique_id = " + std::to_string(seeded[PLAINSMAN_SWORD])).value_or(-1), 0)
		  << "X16/X27: the broken sword's row is gone";
		const auto row = database.queryRows(schema, "SELECT tune_count, enchant FROM inventory WHERE item_unique_id = " + std::to_string(seeded[TUNIC]), 2);
		ASSERT_EQ(row.size(), 1u);
		EXPECT_EQ(row[0][0].value_or(""), std::to_string(*economy.item(TUNIC).tuneCountAfter)) << "X23: inventory.tune_count after the identification";
		EXPECT_EQ(row[0][1].value_or(""), std::to_string(enchantSeen)) << "X28: inventory.enchant is the level the client saw";
		EXPECT_EQ(database.queryLong(schema, "SELECT npc_expands FROM players WHERE id = " + std::to_string(b.playerId)).value_or(-1),
		          *economy.cube.at(0).npcExpandsAfter)
		  << "X26: players.npc_expands";
	});

	std::optional<int32_t> gameServerExit;
	const int64_t connectionsAtShutdown = (a.game && !a.game->client.socket.isClosed() ? 1 : 0) + (b.game && !b.game->client.socket.isClosed() ? 1 : 0);
	gameServerExit = servers.stopGameServer();
	const std::chrono::system_clock::time_point serverUpTo = std::chrono::system_clock::now();
	for (ScenarioClient* client : {&a, &b})
		if (client->game)
			client->game->waitClosed(60s);
	const std::optional<int32_t> loginServerExit = servers.stopLoginServer();

	cases.run("C20", "reports: the Q8 bar, the allow-list and the transfer classes (X22)", [&] {
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
		  << "X22: the game server wrote no check output in " << servers.checkOutputDir();

		// ---- X22: the M5a Q8 bar ----
		const std::vector<std::string> unported = servers.readReportLines("unported_trace.txt");
		const bool legionDominionCron = crossesWednesdayNine(serverUpFrom, serverUpTo);
		std::string cronNote;
		for (const std::string& line : unported)
			if (line.find("LegionDominionService") != std::string::npos && legionDominionCron)
				cronNote = "\n  NOTE: the server was up at a Wednesday 09:00 local time, when CronJobService's hard-coded LegionDominion job fires into the "
				           "unported LegionDominionService::startWeeklyCalculation (CronJobService.cpp:185-187, m5c-plan.md G-07): that hit is the cron's, "
				           "not the script's - rerun the gate (P5-SC.md \"M5c stage 1 integration\")";
		EXPECT_TRUE(unported.empty()) << "X22: AION_UNPORTED sites were reached on the economy path:\n" << join(unported, "\n") << cronNote;
		const std::vector<AllowlistEntry> allowlist = readAllowlist();
		ASSERT_FALSE(allowlist.empty()) << "X22: tests/scenario/m5c_partial_allowlist.txt is empty or missing";
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
			EXPECT_TRUE(allowed) << "X22: the AION_PARTIAL site " << hit.site << " is not in tests/scenario/m5c_partial_allowlist.txt (" << hit.line << ")";
		}
		for (const AllowlistEntry& entry : allowlist) {
			if (entry.section == AllowlistSection::HitAtLeastOnce)
				EXPECT_GT(hitsByEntry[entry.site], 0) << "X22: the section A row " << entry.site << " was never hit";
			else if (entry.section == AllowlistSection::HitNever)
				EXPECT_EQ(hitsByEntry[entry.site], 0) << "X22: the section B row " << entry.site << " was hit " << hitsByEntry[entry.site] << " times";
		}
		std::cout << "X22: AION_PARTIAL hits by allow-list row (hits, section, site):\n";
		for (const AllowlistEntry& entry : allowlist)
			std::cout << "  " << hitsByEntry[entry.site] << "\t" << sectionName(entry.section) << "\t" << entry.site << "\n";
		std::cout << std::flush;

		const std::vector<std::string> census = servers.readReportLines("census.txt");
		EXPECT_TRUE(census.empty()) << "X22: the final census reports leaks:\n" << join(census, "\n");
		EXPECT_TRUE(servers.readReportLines("lockdep.txt").empty()) << "X22: the lock order validator reported:\n"
		                                                            << join(servers.readReportLines("lockdep.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("watchdog.txt").empty()) << "X22: the watchdog dumped:\n" << join(servers.readReportLines("watchdog.txt"), "\n");
		const std::map<std::string, std::vector<std::string>> summary = servers.readSummary();
		const auto value = [&](std::string_view key) -> std::string {
			const auto found = summary.find(std::string(key));
			return found == summary.end() || found->second.empty() ? std::string() : found->second[0];
		};
		EXPECT_EQ(value("started"), "true");
		EXPECT_EQ(value("exitCode"), "0");
		EXPECT_EQ(value("knownListNotifyFailures"), "0");
		EXPECT_EQ(value("liveCountsEnabled"), "true") << "X22: a release build counts nothing: build it checked";
		EXPECT_EQ(value("liveLeaks"), "0") << "X22: a strict class is alive after the logouts: " << join(summary.contains("liveLeak") ? summary.at("liveLeak") : std::vector<std::string>{});
		EXPECT_EQ(value("zombieCuts"), "0");
		const auto notPorted = summary.find("notPortedClientPacket");
		if (notPorted != summary.end())
			ADD_FAILURE() << "X22: the scripted path sent client packets that are not ported: " << join(notPorted->second);
		std::vector<std::string> errors;
		ASSERT_NE(servers.gameServer(), nullptr);
		for (const std::string& line : servers.gameServer()->findLogLines(" ERROR "))
			errors.push_back("game server: " + line);
		if (servers.loginServer() != nullptr)
			for (const std::string& line : servers.loginServer()->findLogLines(" ERROR "))
				errors.push_back("login server: " + line);
		EXPECT_TRUE(errors.empty()) << "X22: ERROR lines in the server logs:\n" << join(errors, "\n") << cronNote;
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
			EXPECT_TRUE(fileErrors.empty()) << "X22: " << errorLog << " is not empty:\n" << join(fileErrors, "\n");
		} else {
			ADD_FAILURE() << "X22: the game server wrote no " << errorLog;
		}
		EXPECT_TRUE(servers.gameServer()->findLogLines("did not leave world cleanly", 5).empty());
		EXPECT_TRUE(servers.gameServer()->findLogLines("stale pin", 5).empty());
		EXPECT_TRUE(servers.gameServer()->findLogLines("objects removed from the world are still alive", 5).empty())
		  << "X22: " << join(servers.gameServer()->findLogLines("objects removed from the world are still alive", 5), "\n");

		// ---- X22: the transfer classes (G-06): live 0 with created > 0 - every transfer object of the run was reclaimed ----
		const auto liveCount = [&](std::string_view name) -> std::optional<LiveCount> {
			const auto rows = summary.find("liveCount");
			if (rows == summary.end())
				return std::nullopt;
			for (const std::string& row : rows->second) {
				std::istringstream in(row);
				std::string key;
				LiveCount count;
				if (!(in >> key >> count.live >> count.created) || key != name)
					continue;
				count.line = row;
				return count;
			}
			return std::nullopt;
		};
		// C19 adds the craft's two: the CraftingTask of the 3 m craft and the handler of Hestia's question (m5c-plan.md G-03 part 2)
		for (const std::string_view name : {"model::trade::Exchange", "model::trade::ExchangeItem", "model::trade::TradeList", "model::trade::TradeItem",
		                                    "model::trade::RepurchaseList", "model::trade::TradePSItem", "model::gameobjects::Letter",
		                                    "CM_EXCHANGE_REQUEST_RequestResponseHandler", "DialogService_RequestResponseHandler",
		                                    "CubeExpandService_RequestResponseHandler", "skillengine::task::CraftingTask",
		                                    "CraftSkillUpdateService_RequestResponseHandler"}) {
			const std::optional<LiveCount> count = liveCount(name);
			if (!count) {
				ADD_FAILURE() << "X22: m5a_summary.txt has no liveCount row for " << name << " (CheckOutput, G-06)";
				continue;
			}
			EXPECT_EQ(count->live, 0) << "X22: " << count->line << " - a transfer object outlived the logouts";
			EXPECT_GT(count->created, 0) << "X22: " << count->line << " - the gate's path never created one, so its 0 live proves nothing";
		}
		// nobody is online at the end, so no per-connection object survives either (the M5a bound, here 0)
		const std::set<std::string> perConnection = {"Account", "AccountTime", "ConnectionAliveChecker", "PlayerAccountData", "PlayerCommonData",
			"PlayerAppearance"};
		for (const auto& [name, count] : readLiveCounts(servers, "live_counts.txt")) {
			if (perConnection.contains(name) || name.ends_with("Storage"))
				EXPECT_LE(count.live, connectionsAtShutdown) << "X22: live instances left: " << count.line;
			if (name == "Player" || name == "Item" || name == "Mailbox")
				EXPECT_EQ(count.live, 0) << "X22: live instances left: " << count.line;
		}
		EXPECT_EQ(connectionsAtShutdown, 0) << "both characters quit before the stop (C20a)";
	});

	finishRun(servers, outputDir, testName);
}

} // namespace

// ---- the gate ----------------------------------------------------------------------------------------------------------------------------

/** `gs.scenario.m5c` (G-03): the cases of §10.2, C0-C20 (C19, crafting in Sanctum, is part 2), two accounts online at once; no geo variant (D12) */
TEST(M5cScenario, Run) {
	runM5cGate();
}

} // namespace aion::gameserver::scenario
