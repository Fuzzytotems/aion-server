// The M5b-3 scenario gate (m5b3-plan.md G-03 and G-04, §10): one login server and one game server as child processes on their own test
// schemas, one account with the M5b-1 Elyos Warrior, and the loot and item paths of stage 1 at gameserver.rates.drop = 1000000: two kills of
// npc 210663 and one of npc 210133 looted empty, kinah, the sword unequipped and re-equipped, a godstone socketed and seen to proc, a potion,
// the cube and warehouse moves, a split, a destroy, a swap and the inventory read back from the database after a quit - then the reports the
// server writes at shutdown.
//
// Every expectation is independent of the C++ server code, exactly as the earlier gates are: server packets are read with the decoders of
// tests/scenario/decoders (ItemDecoders.h for the nine item packets, CombatDecoders.h, SkillDecoders.h and PacketDecoders.h, all written from
// the Java writeImpl methods, m5a-plan.md D9), and every number comes from `tools/oracle/oracle.py` - m5b3-drops (the rules, candidate sets,
// count ranges, loot effects and the cube budget of each corpse), m5b3-item (the potion, the event potion, the godstone and the sword),
// m5b3-material (the camp fire and where to stand on it, geo run only), m5a-creation and m5b-monster - or from the Java arithmetic of the
// method an assertion is about, cited at the line.
//
// **This file deliberately does not share M5b2ScenarioTest.cpp's helpers**, for the reason that file gives: each gate owns one pair of server
// processes, their helpers live in anonymous namespaces, and lifting them into a shared header is a follow-up. What is duplicated is
// scaffolding (the case log, the burst collector, the login conversation, the report readers), never an assertion.
//
// It holds TWO gates: M5b3Scenario.Run (gs.scenario.m5b3, geo off) and M5b3ScenarioGeo.Run (gs.scenario.m5b3_geo, geo on), the same script
// through one shared body; the geo run adds the camp fire (§10.5, Y15). The comment above TEST(M5b3ScenarioGeo, Run) says what it adds.
//
// **The inventory model.** Every item packet the character receives is applied, in arrival order, to a model of its cube, equipment and
// regular warehouse (InventoryModel below): SM_INVENTORY_INFO and SM_WAREHOUSE_INFO at enter world, then SM_INVENTORY_ADD_ITEM,
// SM_INVENTORY_UPDATE_ITEM, SM_DELETE_ITEM and their warehouse twins. The model is what the gate expects a loot to merge into (Y3), what the
// cube budget is asked about (L3b), the count before a potion or a split (Y8, Y10), and what the database must hold after the quit (Y12) -
// "the count of the last packet the client got about it".

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

#include <nlohmann/json.hpp>

#include "AsyncAllowed.h"
#include "FakeLoginClient.h"
#include "GameSession.h"
#include "Oracle.h"
#include "PacketSequence.h"
#include "ScenarioDatabase.h"
#include "ScenarioServers.h"
#include "decoders/CombatDecoders.h"
#include "decoders/ItemDecoders.h"
#include "decoders/PacketDecoders.h"
#include "decoders/SkillDecoders.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {
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
constexpr int32_t CLASS_WARRIOR = 0;

/** the Elyos start map and the two monsters of §10.1: 210663 (loot, the godstone proc) and 210133 (kinah) */
constexpr int32_t ELYOS_START_MAP = 210010000;
constexpr int32_t LOOT_NPC_ID = 210663;
constexpr int32_t KINAH_NPC_ID = 210133;
/** §10.1 D3: every applicable rule fires (m5b3.properties.example) */
constexpr std::string_view DROP_RATE = "1000000";

/** the items §10.2 names: the starter potion (L7-L9), the starter event potion that is not storable in a warehouse (L8), the seeded godstone */
constexpr int32_t LIFE_POTION = 162000002;
constexpr int32_t MANA_POTION = 162000007;
constexpr int32_t EVENT_POTION = 164002116;
constexpr int32_t GODSTONE = 168000116;
/** ItemId.KINAH (the kinah item of every storage) and the starter weapon of the Warrior (m5a-creation: equipped in MAIN_HAND) */
constexpr int32_t KINAH_ITEM = 182400001;
constexpr int32_t TRAINING_SWORD = 100000094;
/** ItemSlot.MAIN_HAND.getSlotIdMask() (ItemSlot.java), the slot CM_EQUIP_ITEM re-equips the sword in */
constexpr int64_t MAIN_HAND = 1;
/** ItemStone.ItemStoneType.GODSTONE.ordinal(): the `category` ItemStoneListDAO stores (ItemStoneListDAO.java:130, ItemStone.java:22-27) */
constexpr int32_t ITEM_STONE_CATEGORY_GODSTONE = 1;

/** EmotionType.START_LOOT / END_LOOT (model/EmotionType.java:48-49) */
constexpr uint8_t EMOTION_START_LOOT = 40;
constexpr uint8_t EMOTION_END_LOOT = 41;

/**
 * SM_ATTACK_STATUS.TYPE.HP and DAMAGE share the byte 7 (SM_ATTACK_STATUS.java:30-31); DAMAGE is written negated and HP as it is
 * (:125-133 and the default arm), so a potion's heal reaches the wire as +value and a proc's or a fire's damage as -damage
 */
constexpr uint8_t ATTACK_STATUS_TYPE_HP_OR_DAMAGE = 7;
/** SM_ATTACK_STATUS.LOG.PROCATKINSTANT ("changed in 4.5", SM_ATTACK_STATUS.java:77): DamageEffect's PROVOKED arm (DamageEffect.java:29-31) */
constexpr uint8_t ATTACK_STATUS_LOG_PROCATKINSTANT = 93;

/** SM_SYSTEM_MESSAGE ids (SM_SYSTEM_MESSAGE.java) */
constexpr int32_t STR_UI_INVENTORY_FULL = 1300042;
constexpr int32_t STR_WAREHOUSE_CANT_DEPOSIT_ITEM = 1300418;
constexpr int32_t STR_ITEM_CANT_USE_UNTIL_DELAY_TIME = 1300494;
constexpr int32_t STR_GIVE_ITEM_PROC_ENCHANTED_TARGET_ITEM = 1300508;
constexpr int32_t STR_SKILL_PROC_EFFECT_OCCURRED = 1301062;

/** the melee distance of m5b-plan.md K4b: inside the Warrior's attack range */
constexpr double MELEE_DISTANCE = 2.0;
/** the HP the potion case seeds (§10.1 "Character", L7): far enough below the Warrior's maximum that the 37 of the instant heal is not capped */
constexpr int32_t POTION_CASE_HP = 100;
/** the cube slots L8 moves the two starter potion stacks to inside the cube (the same-storage arm), and the slot L9 splits into */
constexpr int16_t LIFE_POTION_SLOT = 21;
constexpr int16_t MANA_POTION_SLOT = 22;
constexpr int16_t SPLIT_SLOT = 23;
/** how many potions L9 splits off */
constexpr int64_t SPLIT_COUNT = 10;

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

/** the packets [from, to) of a session's recording */
std::vector<Packet> slice(const GameSession& session, size_t from, std::optional<size_t> to = std::nullopt) {
	const std::vector<Packet>& packets = session.recorded();
	const size_t end = std::min(packets.size(), to.value_or(packets.size()));
	if (from >= end)
		return {};
	return std::vector<Packet>(packets.begin() + static_cast<std::ptrdiff_t>(from), packets.begin() + static_cast<std::ptrdiff_t>(end));
}

/** AttackStatus.getBaseStatus (AttackStatus.java): the four DODGE and the four RESIST ids of SM_ATTACK's status byte */
bool isDodgeOrResist(int8_t attackStatusId) {
	switch (attackStatusId) {
		case decoders::ATTACK_STATUS_DODGE:
		case decoders::ATTACK_STATUS_OFFHAND_DODGE:
		case decoders::ATTACK_STATUS_CRITICAL_DODGE:
		case decoders::ATTACK_STATUS_OFFHAND_CRITICAL_DODGE:
		case decoders::ATTACK_STATUS_RESIST:
		case decoders::ATTACK_STATUS_OFFHAND_RESIST:
		case decoders::ATTACK_STATUS_CRITICAL_RESIST:
		case decoders::ATTACK_STATUS_OFFHAND_CRITICAL_RESIST:
			return true;
		default:
			return false;
	}
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
 * @return whether the NEXT case can run: false after an exception or a fatal (ASSERT_*) failure, which leave the character's state unknown;
 * true after non-fatal (EXPECT_*) failures only, which leave it as the script intended - so a mutant that breaks one row still shows every
 * later row of the gate (the "others pass" half of a mutation run), where M5b-2's gate stopped at the first failed case
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
void expectSequence(const std::vector<Packet>& packets, std::string_view pattern, const AsyncAllowed& async) {
	const PacketSequence sequence = PacketSequence::parse(pattern);
	const std::vector<std::string> names = namesOf(packets);
	const PacketSequence::Result result = sequence.match(names, async.predicate(packets));
	EXPECT_TRUE(result.matched) << result.message << "\n  expected: " << sequence.toString() << "\n  got (" << names.size() << "): " << join(names);
}

/**
 * The CM_ENTER_WORLD part of m5a-plan.md §5.8, as M5b2ScenarioTest.cpp replays it - with SM_INVENTORY_INFO and SM_WAREHOUSE_INFO as `+`: this
 * gate's cube and warehouse change between entries, and their CONTENT is what it asserts (Y12), not the packet count the M5a gate owns
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

/** The CM_LEVEL_READY part of m5a-plan.md §5.8 (#33 to #44) */
std::string levelReadyPattern() {
	return "SM_PLAYER_INFO, SM_PLAYER_STATE, SM_ACCOUNT_PROPERTIES, SM_MOTION, "
	       "SM_WINDSTREAM_ANNOUNCE*, "
	       "(SM_NPC_INFO | SM_GATHERABLE_INFO)+, "
	       "SM_RIFT_ANNOUNCE, "
	       "SM_NEARBY_QUESTS, [SM_QUEST_REPEAT], [SM_WEATHER], "
	       "SM_ABNORMAL_STATE, SM_CUBE_UPDATE";
}

// ---- the inventory model ----------------------------------------------------------------------------------------------------------------

/** StorageType ids of the two storages this gate touches (StorageType.java:7-8), the `item_location` column */
constexpr int32_t LOCATION_CUBE = decoders::STORAGE_CUBE;
constexpr int32_t LOCATION_WAREHOUSE = decoders::STORAGE_REGULAR_WAREHOUSE;

struct ModelItem {
	int32_t objectId = 0;
	int32_t itemId = 0;
	int64_t count = 0;
	int32_t location = LOCATION_CUBE;
	/** the EQUIPPED_SLOT blob entry / the equipped state of SM_INVENTORY_INFO: the slot mask, 0 when the item is not equipped */
	int64_t equippedSlot = 0;
	/** the low 16 bits of Item.getEquipmentSlot() the last ADD/INFO packet carried (0xFFFF for -1 and 65535) */
	uint16_t slot = 0xFFFF;
	int32_t godStoneId = 0;
};

/**
 * The character's items as its client knows them: every item packet applied in arrival order. Nothing here reads a C++ server class; the
 * packets are decoded with ItemDecoders.h / PacketDecoders.h.
 */
class InventoryModel {
public:
	std::map<int32_t, ModelItem> items;
	std::vector<std::string> decodeFailures;
	/** the last SM_CUBE_UPDATE(cubeSize) item count per StorageType, for the message of a mismatch */
	std::map<int32_t, int32_t> lastCubeUpdateCount;

	void follow(const GameSession* next) {
		session = next;
		scanned = 0;
		items.clear();
	}

	/** applies the packets recorded since the last call */
	void sync() {
		if (session == nullptr)
			return;
		const std::vector<Packet>& packets = session->recorded();
		for (; scanned < packets.size(); scanned++)
			apply(packets[scanned], scanned);
	}

	/** the stacks that take a cube slot: in the cube, not equipped, not the kinah (Storage keeps the kinah item apart, Storage.java:172-173) */
	std::vector<ModelItem> cubeStacks() const {
		std::vector<ModelItem> stacks;
		for (const auto& [id, item] : items)
			if (item.location == LOCATION_CUBE && item.equippedSlot == 0 && item.itemId != KINAH_ITEM)
				stacks.push_back(item);
		return stacks;
	}

	std::vector<ModelItem> byItemId(int32_t itemId, int32_t location = LOCATION_CUBE) const {
		std::vector<ModelItem> found;
		for (const auto& [id, item] : items)
			if (item.itemId == itemId && item.location == location && item.equippedSlot == 0)
				found.push_back(item);
		return found;
	}

	std::optional<ModelItem> byObjectId(int32_t objectId) const {
		const auto found = items.find(objectId);
		if (found == items.end())
			return std::nullopt;
		return found->second;
	}

	std::optional<ModelItem> equipped(int32_t itemId) const {
		for (const auto& [id, item] : items)
			if (item.itemId == itemId && item.equippedSlot != 0)
				return item;
		return std::nullopt;
	}

	int64_t kinah() const {
		for (const auto& [id, item] : items)
			if (item.itemId == KINAH_ITEM && item.location == LOCATION_CUBE)
				return item.count;
		return 0;
	}

	std::string describe() const {
		std::vector<std::string> lines;
		for (const auto& [id, item] : items)
			lines.push_back(std::to_string(id) + ":" + std::to_string(item.itemId) + "x" + std::to_string(item.count) + "@" + std::to_string(item.location) +
			                (item.equippedSlot != 0 ? "(equipped " + std::to_string(item.equippedSlot) + ")" : "") + " slot " + std::to_string(item.slot));
		return join(lines, "; ");
	}

private:
	void put(const decoders::InventoryItem& item, int32_t location) {
		ModelItem& model = items[item.objectId];
		model.objectId = item.objectId;
		if (item.templateId != 0)
			model.itemId = item.templateId;
		model.location = location;
		if (item.general)
			model.count = item.general->count;
		model.equippedSlot = item.equippedSlotBlob.value_or(0);
		model.slot = item.equipmentSlot;
		if (item.enchant)
			model.godStoneId = item.enchant->godStoneId;
	}

	void update(const decoders::InventoryItem& item) {
		const auto found = items.find(item.objectId);
		if (found == items.end()) {
			decodeFailures.push_back("an update of object " + std::to_string(item.objectId) + ", which the client does not have");
			return;
		}
		if (item.general)
			found->second.count = item.general->count;
		if (item.equippedSlotBlob)
			found->second.equippedSlot = *item.equippedSlotBlob;
		if (item.enchant)
			found->second.godStoneId = item.enchant->godStoneId;
	}

	void apply(const Packet& packet, size_t index) {
		try {
			if (packet.name == "SM_INVENTORY_INFO") {
				const decoders::InventoryInfo info = decoders::decodeInventoryInfo(packet.data);
				if (info.firstPacket)
					std::erase_if(items, [](const auto& entry) { return entry.second.location == LOCATION_CUBE; });
				for (const decoders::InventoryItem& item : info.items)
					put(item, LOCATION_CUBE);
			} else if (packet.name == "SM_WAREHOUSE_INFO") {
				const decoders::WarehouseInfo info = decoders::decodeWarehouseInfo(packet.data);
				if (info.warehouseType != LOCATION_WAREHOUSE)
					return;
				if (info.firstPacket)
					std::erase_if(items, [](const auto& entry) { return entry.second.location == LOCATION_WAREHOUSE; });
				for (const decoders::InventoryItem& item : info.items)
					put(item, LOCATION_WAREHOUSE);
			} else if (packet.name == "SM_INVENTORY_ADD_ITEM") {
				for (const decoders::InventoryItem& item : decoders::decodeInventoryAddItem(packet.data).items)
					put(item, LOCATION_CUBE);
			} else if (packet.name == "SM_INVENTORY_UPDATE_ITEM") {
				update(decoders::decodeInventoryUpdateItem(packet.data).item);
			} else if (packet.name == "SM_DELETE_ITEM") {
				items.erase(decoders::decodeDeleteItem(packet.data).objectId);
			} else if (packet.name == "SM_WAREHOUSE_ADD_ITEM") {
				const decoders::WarehouseAddItem add = decoders::decodeWarehouseAddItem(packet.data);
				for (const decoders::InventoryItem& item : add.items)
					put(item, add.warehouseType);
			} else if (packet.name == "SM_WAREHOUSE_UPDATE_ITEM") {
				update(decoders::decodeWarehouseUpdateItem(packet.data).item);
			} else if (packet.name == "SM_DELETE_WAREHOUSE_ITEM") {
				items.erase(decoders::decodeDeleteWarehouseItem(packet.data).objectId);
			} else if (packet.name == "SM_CUBE_UPDATE") {
				const decoders::CubeUpdate cube = decoders::decodeCubeUpdate(packet.data);
				if (cube.action == 0)
					lastCubeUpdateCount[cube.actionValue] = cube.itemsCount;
			}
		} catch (const DecodeError& error) {
			decodeFailures.push_back(packet.name + " at " + std::to_string(index) + ": " + error.what());
		}
	}

	const GameSession* session = nullptr;
	size_t scanned = 0;
};

// ---- the oracle answers this gate reads (tools/oracle/m5b3/*.py) -------------------------------------------------------------------------

struct DropCandidate {
	int32_t itemId = 0;
	int64_t minCount = 0, maxCount = 0;
	bool kinah = false;
	int32_t lootEffectId = 0;
	int64_t maxStackCount = 1;
};

struct DropRule {
	int32_t ruleIndex = 0;
	std::string name;
	bool certain = false;
	int32_t entriesIfFired = 0;
	std::vector<DropCandidate> candidates;

	const DropCandidate* candidate(int32_t itemId) const {
		for (const DropCandidate& c : candidates)
			if (c.itemId == itemId)
				return &c;
		return nullptr;
	}
};

/** oracle.py m5b3-drops --npc N --drop-rate R [--inventory ...]: the rules of one corpse, its entry count and its cube budget */
struct OracleDrops {
	int32_t npcId = 0;
	int32_t entriesMin = 0, entriesMax = 0;
	bool deterministic = false;
	std::vector<DropRule> rules;
	bool registerDrop = false;
	// the `cube` section, present when the oracle was given the inventory
	int32_t cubeLimit = 0, worstCaseNewSlots = 0, slotsUsed = 0;
	std::set<int32_t> deterministicMergeItems;

	const DropCandidate* candidate(int32_t itemId) const {
		for (const DropRule& rule : rules)
			if (const DropCandidate* c = rule.candidate(itemId))
				return c;
		return nullptr;
	}
};

OracleDrops parseDrops(const std::string& text) {
	const json answer = json::parse(text);
	OracleDrops drops;
	drops.npcId = answer.at("npcId").get<int32_t>();
	drops.registerDrop = answer.at("registerDrop").get<bool>();
	const json& entries = answer.at("entries");
	drops.entriesMin = entries.at("min").get<int32_t>();
	drops.entriesMax = entries.at("max").get<int32_t>();
	drops.deterministic = entries.at("deterministic").get<bool>();
	for (const json& node : answer.at("rules")) {
		DropRule rule;
		rule.ruleIndex = node.at("ruleIndex").get<int32_t>();
		rule.name = node.at("ruleName").get<std::string>();
		rule.certain = node.at("certain").get<bool>();
		rule.entriesIfFired = node.at("entriesIfFired").get<int32_t>();
		for (const json& c : node.at("candidates")) {
			DropCandidate candidate;
			candidate.itemId = c.at("itemId").get<int32_t>();
			candidate.minCount = c.at("countRange").at(0).get<int64_t>();
			candidate.maxCount = c.at("countRange").at(1).get<int64_t>();
			candidate.kinah = c.at("kinah").get<bool>();
			candidate.lootEffectId = c.at("lootEffectId").get<int32_t>();
			candidate.maxStackCount = c.at("maxStackCount").get<int64_t>();
			rule.candidates.push_back(candidate);
		}
		drops.rules.push_back(rule);
	}
	const auto cube = answer.find("cube");
	if (cube != answer.end() && cube->is_object()) {
		drops.cubeLimit = cube->at("limit").get<int32_t>();
		drops.worstCaseNewSlots = cube->at("worstCaseNewSlots").get<int32_t>();
		drops.slotsUsed = cube->at("slotsUsed").get<int32_t>();
		for (const json& merge : cube->at("deterministicMerges"))
			for (const json& id : merge.at("itemIds"))
				drops.deterministicMergeItems.insert(id.get<int32_t>());
	}
	return drops;
}

/** the parts of one oracle.py m5b3-item entry the gate reads */
struct OracleItemInfo {
	int32_t itemId = 0;
	int64_t maxStackCount = 1;
	std::set<std::string> maskFlags;
	int32_t useDelayMillis = 0;
	/** the skill of the first `skilluse` action, and its effects' classes and values at the action's level */
	int32_t skillId = 0;
	std::vector<std::pair<std::string, int32_t>> effects;
	/**
	 * EffectTemplate.getDuration2() per effect class with a `duration2` attribute: the attribute, plus 1000 for an AbstractOverTimeEffect ("on
	 * retail these effects last one sec more than their template value", AbstractOverTimeEffect.java:64-67) - what Effect.calculateTemplateDuration
	 * makes the lifetime of (Effect.java:899-910)
	 */
	std::map<std::string, int32_t> effectDuration2;
	/** a godstone's proc skill, its probability (per mille) and its breakprob */
	int32_t godstoneSkillId = 0, godstoneProbability = 0, godstoneBreakProbability = 0;
	std::vector<std::string> godstoneEffectClasses;
};

std::map<int32_t, OracleItemInfo> parseItems(const std::string& text) {
	const json answer = json::parse(text);
	std::map<int32_t, OracleItemInfo> items;
	for (const json& node : answer.at("items")) {
		OracleItemInfo info;
		info.itemId = node.at("itemId").get<int32_t>();
		info.maxStackCount = node.at("maxStackCount").get<int64_t>();
		for (const json& flag : node.at("maskFlags"))
			info.maskFlags.insert(flag.get<std::string>());
		if (node.at("cooldown").is_object())
			info.useDelayMillis = node.at("cooldown").at("useDelayMillis").get<int32_t>();
		for (const json& action : node.at("actions")) {
			if (action.at("tag").get<std::string>() != "skilluse" || info.skillId != 0)
				continue;
			const json& skill = action.at("skill");
			info.skillId = skill.at("skillId").get<int32_t>();
			for (const json& effect : skill.at("effects")) {
				info.effects.emplace_back(effect.at("class").get<std::string>(), effect.at("valueAtLevel").get<int32_t>());
				const json& attributes = effect.at("attributes");
				if (attributes.contains("duration2")) {
					const std::vector<std::string> chain = effect.at("classChain").get<std::vector<std::string>>();
					const bool overTime = std::ranges::find(chain, "AbstractOverTimeEffect") != chain.end();
					info.effectDuration2[effect.at("class").get<std::string>()] = std::stoi(attributes.at("duration2").get<std::string>()) + (overTime ? 1000 : 0);
				}
			}
		}
		if (node.at("godstone").is_object()) {
			const json& godstone = node.at("godstone");
			info.godstoneSkillId = godstone.at("skill").at("skillId").get<int32_t>();
			info.godstoneProbability = std::stoi(godstone.at("attributes").at("probability").get<std::string>());
			info.godstoneBreakProbability = std::stoi(godstone.at("attributes").value("breakprob", std::string("0")));
			for (const json& effect : godstone.at("skill").at("effects"))
				info.godstoneEffectClasses.push_back(effect.at("class").get<std::string>());
		}
		items[info.itemId] = info;
	}
	return items;
}

/** oracle.py m5b3-material --stand (G-04): the fire to stand on and where */
struct OracleCampFire {
	std::string zoneName;
	int32_t materialId = 0;
	int32_t skillId = 0, frequency = 0;
	float x = 0, y = 0, z = 0;
	float offX = 0, offY = 0, offZ = 0;
	/** inside the fire's zone alone, every TOUCH ray missing (§18.2): the negative case of the ray */
	float missX = 0, missY = 0, missZ = 0;
	double missMargin = 0, missHeight = 0;
	std::vector<std::string> missInside, missTouched;
	double clearance = 0, distanceFromSpawn = 0;
	std::vector<std::string> insideZones, touchedZones, nearbyZones;
};

OracleCampFire parseCampFire(const std::string& text) {
	const json answer = json::parse(text);
	const json& fire = answer.at("nearestUnconditional");
	const json& stand = answer.at("stand");
	if (!fire.is_object() || !stand.is_object() || !stand.at("point").is_object() || !stand.at("stepOff").is_object())
		throw std::runtime_error("m5b3-material --stand found no point to stand on the nearest unconditional fire");
	if (!stand.at("untouched").is_object())
		throw std::runtime_error("m5b3-material --stand found no point inside the fire's zone alone where every TOUCH ray misses");
	OracleCampFire result;
	result.zoneName = fire.at("zoneName").get<std::string>();
	result.materialId = fire.at("materialId").get<int32_t>();
	result.distanceFromSpawn = fire.at("distance").get<double>();
	const json& skill = fire.at("skills").at(0);
	result.skillId = skill.at("skillId").get<int32_t>();
	result.frequency = skill.at("frequency").get<int32_t>();
	const json& point = stand.at("point");
	result.x = point.at("x").get<float>();
	result.y = point.at("y").get<float>();
	result.z = point.at("z").get<float>();
	result.clearance = point.at("clearance").get<double>();
	for (const json& name : point.at("inside"))
		result.insideZones.push_back(name.get<std::string>());
	for (const json& name : point.at("touched"))
		result.touchedZones.push_back(name.get<std::string>());
	for (const json& zone : stand.at("nearbyZones"))
		result.nearbyZones.push_back(zone.at("zoneName").get<std::string>() + " (material " + std::to_string(zone.at("materialId").get<int32_t>()) + ")");
	const json& off = stand.at("stepOff");
	result.offX = off.at("x").get<float>();
	result.offY = off.at("y").get<float>();
	result.offZ = off.at("z").get<float>();
	const json& miss = stand.at("untouched");
	result.missX = miss.at("x").get<float>();
	result.missY = miss.at("y").get<float>();
	result.missZ = miss.at("z").get<float>();
	result.missMargin = miss.at("margin").get<double>();
	result.missHeight = miss.at("height").get<double>();
	for (const json& name : miss.at("inside"))
		result.missInside.push_back(name.get<std::string>());
	for (const json& name : miss.at("touched"))
		result.missTouched.push_back(name.get<std::string>());
	return result;
}

// ---- the scenario client ---------------------------------------------------------------------------------------------------------------

struct ScenarioClient {
	std::string account;
	std::string password = "m5b3Password1";
	std::unique_ptr<FakeLoginClient> login;
	std::unique_ptr<GameSession> game;
	FakeLoginClient::SessionKey key;
	int32_t warriorId = 0;
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

// ---- the check output reports (Y13, Y14) ----------------------------------------------------------------------------------------------

/** One section of m5b3_partial_allowlist.txt: §A hit at least once, §B hit exactly zero times, §C counted but not pinned */
enum class AllowlistSection { HitAtLeastOnce, HitNever, NotPinned };

struct AllowlistEntry {
	std::string site;
	AllowlistSection section = AllowlistSection::NotPinned;
};

/** Reads tests/scenario/m5b3_partial_allowlist.txt with its three sections ("# --- SECTION A/B/C" marker lines, as the M5b lists) */
std::vector<AllowlistEntry> readAllowlist() {
	std::vector<AllowlistEntry> entries;
	std::ifstream in(AION_SCENARIO_M5B3_PARTIAL_ALLOWLIST, std::ios::binary);
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

/** "<live>\t<created>\t<qualified class name>" rows of live_counts.txt */
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

/**
 * A maximum matching between a corpse's entries and the oracle's applicable rules by candidate membership (Kuhn's augmenting paths): Y2's
 * "bijection between entries and applicable rules". @return the rule index matched to each entry (-1 unmatched)
 */
std::vector<int32_t> matchEntriesToRules(const std::vector<decoders::LootItem>& entries, const std::vector<DropRule>& rules) {
	std::vector<int32_t> ruleOfEntry(entries.size(), -1);
	std::vector<int32_t> entryOfRule(rules.size(), -1);
	std::function<bool(size_t, std::vector<bool>&)> augment = [&](size_t entry, std::vector<bool>& seen) {
		for (size_t rule = 0; rule < rules.size(); rule++) {
			if (seen[rule] || rules[rule].candidate(entries[entry].itemId) == nullptr)
				continue;
			seen[rule] = true;
			if (entryOfRule[rule] < 0 || augment(static_cast<size_t>(entryOfRule[rule]), seen)) {
				entryOfRule[rule] = static_cast<int32_t>(entry);
				ruleOfEntry[entry] = static_cast<int32_t>(rule);
				return true;
			}
		}
		return false;
	};
	for (size_t entry = 0; entry < entries.size(); entry++) {
		std::vector<bool> seen(rules.size(), false);
		augment(entry, seen);
	}
	return ruleOfEntry;
}

/**
 * Y5 (rev 2): the SM_STATS_INFO fields that are STATS - the current attributes and resistances (SM_STATS_INFO.java:36-47), the current values
 * from max HP to spell fortitude (:64-117) without the regenerating current HP, MP, DP and FP (:65, :68, :71, :74), the fly state and movement
 * mask, the class, and the whole base block (from :152). Not the game time, the level's experience (:58-60), the inventory size (:119, the
 * cube changes with an unequip) or repose and salvation (:128-130). @return one "name a/b" line per field that differs
 */
std::vector<std::string> statFieldDifferences(const decoders::StatsInfo& a, const decoders::StatsInfo& b) {
	std::vector<std::string> differences;
	const auto field = [&](std::string_view name, auto x, auto y) {
		if (x != y) {
			std::ostringstream line;
			line << name << " " << +x << "/" << +y;
			differences.push_back(line.str());
		}
	};
	field("power", a.power, b.power);
	field("health", a.health, b.health);
	field("accuracy", a.accuracy, b.accuracy);
	field("agility", a.agility, b.agility);
	field("knowledge", a.knowledge, b.knowledge);
	field("will", a.will, b.will);
	field("waterResistance", a.waterResistance, b.waterResistance);
	field("windResistance", a.windResistance, b.windResistance);
	field("earthResistance", a.earthResistance, b.earthResistance);
	field("fireResistance", a.fireResistance, b.fireResistance);
	field("lightResistance", a.lightResistance, b.lightResistance);
	field("darkResistance", a.darkResistance, b.darkResistance);
	field("level", a.level, b.level);
	field("maxHp", a.maxHp, b.maxHp);
	field("maxMp", a.maxMp, b.maxMp);
	field("maxDp", a.maxDp, b.maxDp);
	field("maxFlyTime", a.maxFlyTime, b.maxFlyTime);
	field("mainHandPAttack", a.mainHandPAttack, b.mainHandPAttack);
	field("offHandPAttack", a.offHandPAttack, b.offHandPAttack);
	field("pDef", a.pDef, b.pDef);
	field("mainHandMAttack", a.mainHandMAttack, b.mainHandMAttack);
	field("offHandMAttack", a.offHandMAttack, b.offHandMAttack);
	field("mDef", a.mDef, b.mDef);
	field("mResist", a.mResist, b.mResist);
	field("attackRange", a.attackRange, b.attackRange);
	field("attackSpeed", a.attackSpeed, b.attackSpeed);
	field("evasion", a.evasion, b.evasion);
	field("parry", a.parry, b.parry);
	field("block", a.block, b.block);
	field("mainHandPCritical", a.mainHandPCritical, b.mainHandPCritical);
	field("offHandPCritical", a.offHandPCritical, b.offHandPCritical);
	field("mainHandPAccuracy", a.mainHandPAccuracy, b.mainHandPAccuracy);
	field("offHandPAccuracy", a.offHandPAccuracy, b.offHandPAccuracy);
	field("mAccuracy", a.mAccuracy, b.mAccuracy);
	field("mCritical", a.mCritical, b.mCritical);
	field("castingSpeed", a.castingSpeed, b.castingSpeed);
	field("concentration", a.concentration, b.concentration);
	field("mBoost", a.mBoost, b.mBoost);
	field("mbResist", a.mbResist, b.mbResist);
	field("healBoost", a.healBoost, b.healBoost);
	field("pcr", a.pcr, b.pcr);
	field("mcr", a.mcr, b.mcr);
	field("physicalCriticalDamageReduce", a.physicalCriticalDamageReduce, b.physicalCriticalDamageReduce);
	field("magicalCriticalDamageReduce", a.magicalCriticalDamageReduce, b.magicalCriticalDamageReduce);
	field("inventoryLimit", a.inventoryLimit, b.inventoryLimit);
	field("classId", a.classId, b.classId);
	field("basePower", a.basePower, b.basePower);
	field("baseHealth", a.baseHealth, b.baseHealth);
	field("baseAccuracy", a.baseAccuracy, b.baseAccuracy);
	field("baseAgility", a.baseAgility, b.baseAgility);
	field("baseKnowledge", a.baseKnowledge, b.baseKnowledge);
	field("baseWill", a.baseWill, b.baseWill);
	field("baseWaterResistance", a.baseWaterResistance, b.baseWaterResistance);
	field("baseWindResistance", a.baseWindResistance, b.baseWindResistance);
	field("baseEarthResistance", a.baseEarthResistance, b.baseEarthResistance);
	field("baseFireResistance", a.baseFireResistance, b.baseFireResistance);
	field("baseLightResistance", a.baseLightResistance, b.baseLightResistance);
	field("baseDarkResistance", a.baseDarkResistance, b.baseDarkResistance);
	field("baseMaxHp", a.baseMaxHp, b.baseMaxHp);
	field("baseMaxMp", a.baseMaxMp, b.baseMaxMp);
	field("baseMaxDp", a.baseMaxDp, b.baseMaxDp);
	field("baseFlyTime", a.baseFlyTime, b.baseFlyTime);
	field("baseMainHandPAttack", a.baseMainHandPAttack, b.baseMainHandPAttack);
	field("baseOffHandPAttack", a.baseOffHandPAttack, b.baseOffHandPAttack);
	field("baseMainHandMAttack", a.baseMainHandMAttack, b.baseMainHandMAttack);
	field("baseOffHandMAttack", a.baseOffHandMAttack, b.baseOffHandMAttack);
	field("basePDef", a.basePDef, b.basePDef);
	field("baseMDef", a.baseMDef, b.baseMDef);
	field("baseMResist", a.baseMResist, b.baseMResist);
	field("baseAttackRange", a.baseAttackRange, b.baseAttackRange);
	field("baseEvasion", a.baseEvasion, b.baseEvasion);
	field("baseParry", a.baseParry, b.baseParry);
	field("baseBlock", a.baseBlock, b.baseBlock);
	field("baseMainHandPCritical", a.baseMainHandPCritical, b.baseMainHandPCritical);
	field("baseOffHandPCritical", a.baseOffHandPCritical, b.baseOffHandPCritical);
	field("baseMCritical", a.baseMCritical, b.baseMCritical);
	field("baseMainHandPAccuracy", a.baseMainHandPAccuracy, b.baseMainHandPAccuracy);
	field("baseOffHandPAccuracy", a.baseOffHandPAccuracy, b.baseOffHandPAccuracy);
	field("baseMAccuracy", a.baseMAccuracy, b.baseMAccuracy);
	field("baseConcentration", a.baseConcentration, b.baseConcentration);
	field("baseMBoost", a.baseMBoost, b.baseMBoost);
	field("baseMBResist", a.baseMBResist, b.baseMBResist);
	field("baseHealBoost", a.baseHealBoost, b.baseHealBoost);
	field("basePcr", a.basePcr, b.basePcr);
	field("baseMcr", a.baseMcr, b.baseMcr);
	field("basePhysicalCriticalDamageReduce", a.basePhysicalCriticalDamageReduce, b.basePhysicalCriticalDamageReduce);
	field("baseMagicalCriticalDamageReduce", a.baseMagicalCriticalDamageReduce, b.baseMagicalCriticalDamageReduce);
	return differences;
}

/** One kill of the character: the corpse (an npc's object id stays its corpse's, DropRegistrationService keys the drop by it) and when */
struct KillRecord {
	int32_t templateId = 0;
	int32_t corpseId = 0;
	/** the first packet of the fight and the index of the npc's SM_EMOTION(DIE) */
	size_t fightFrom = 0, dieIndex = 0;
};

/** What a corpse listed when it was opened (L2, L4, L6c): Y1 reads its loot effect from these entries */
struct LootRecord {
	KillRecord kill;
	std::vector<decoders::LootItem> entries;
	bool opened = false;
	bool emptied = false;
};

// ---- the gate ---------------------------------------------------------------------------------------------------------------------------

/** What separates gs.scenario.m5b3 from gs.scenario.m5b3_geo */
struct GateVariant {
	bool geodata = false;
	std::string testName;      // gs.scenario.m5b3
	std::string outputSubdir;  // m5b3
	std::string schemaPrefix;  // m5b3
	std::string accountPrefix; // m5b3a
};

void runM5b3Gate(const GateVariant& variant) {
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

	// ---- §10.1 processes, databases and profile: game-server/config/m5b3.properties.example, key by key ----
	// The M5a set comes from ScenarioServers::m5aProfile (events disabled with `*`, so no event drop rule applies, §2.4). The M5b-1 and M5b-2
	// keys follow, then what M5b-3 adds: the drop rate that makes every applicable rule certain (D3), and the potion, godstone and announce keys
	// written out at their defaults (CustomConfig.java:243-244, 273-277; drop.properties). Geodata is the one key that separates the variants.
	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.schemaPrefix = variant.schemaPrefix;
	config.gameServerProperties["gameserver.geodata.enable"] = variant.geodata ? "true" : "false";
	config.gameServerProperties["gameserver.npcshouts.enable"] = "false";
	config.gameServerProperties["gameserver.rates.xp.solo"] = "1.0, 2.0";
	config.gameServerProperties["gameserver.soulsickness.disable"] = "10";
	config.gameServerProperties["gameserver.rates.drop"] = std::string(DROP_RATE);
	config.gameServerProperties["gameserver.items.ignore_potions_at_full_health"] = "false";
	config.gameServerProperties["gameserver.rates.godstone.activation.rate"] = "1.0";
	config.gameServerProperties["gameserver.rates.godstone.evaluation.cooldown_millis"] = "750";
	config.gameServerProperties["gameserver.drop.announce_quality"] = "MYTHIC";
	config.startupTimeout = variant.geodata ? 25min : 10min;
	config.stopTimeout = 3min;
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
	InventoryModel model;
	a.account = variant.accountPrefix + servers.gameSchema().substr(servers.gameSchema().size() - 8);
	const std::string warriorName = "Scenariolooter";

	// ---- L0: the oracles answer, and the plan's premises are re-derived from them (§10.2 L0, G-01) ----
	OracleCreation creation;
	OracleMonster lootMonster, kinahMonster;
	OracleDrops lootDrops, kinahDrops;
	std::map<int32_t, OracleItemInfo> itemInfo;
	// the single candidates of the two JUNK_* rules (L6c merges the first, L8 moves it, L11 swaps it; L10 destroys the second) and of the shard rule
	int32_t lootJunk = 0, kinahJunk = 0, shardItem = 0;
	runCase("L0", "the oracles answer and the plan's premises hold (m5b3-drops, m5b3-item, m5a-creation, m5b-monster)", [&] {
		creation = oracle->creation("ELYOS", "WARRIOR");
		ASSERT_EQ(creation.mapId, ELYOS_START_MAP);
		bool sword = false;
		for (const OracleItem& item : creation.items)
			sword = sword || (item.itemId == TRAINING_SWORD && item.equipped && item.slot == MAIN_HAND);
		ASSERT_TRUE(sword) << "the fresh Warrior's main hand is not the Training Sword; L5 and L6 unequip it";
		ASSERT_TRUE(std::ranges::any_of(creation.items, [](const OracleItem& item) { return item.itemId == LIFE_POTION && !item.equipped; }))
		  << "the starter inventory has no Minor Life Potion (L7-L9)";
		ASSERT_TRUE(std::ranges::any_of(creation.items, [](const OracleItem& item) { return item.itemId == MANA_POTION && !item.equipped; }));
		ASSERT_TRUE(std::ranges::any_of(creation.items, [](const OracleItem& item) { return item.itemId == EVENT_POTION && !item.equipped; }))
		  << "the starter inventory has no [Event] Rx: Accelerox (L8)";

		lootMonster = oracle->monster(ELYOS_START_MAP, LOOT_NPC_ID, 1);
		kinahMonster = oracle->monster(ELYOS_START_MAP, KINAH_NPC_ID, 1);
		ASSERT_TRUE(lootMonster.nearestPlainSpot && kinahMonster.nearestPlainSpot) << "no plain spot of the two monsters (m5b-monster)";
		EXPECT_EQ(lootMonster.tribe, "MONSTER") << "a MONSTER-tribe npc never starts a fight (m5b-plan.md A5a): the gate pulls each one";
		EXPECT_EQ(kinahMonster.tribe, "MONSTER");

		const std::string rate = std::string(DROP_RATE);
		lootDrops = parseDrops(oracle->run({"m5b3-drops", "--npc", std::to_string(LOOT_NPC_ID), "--drop-rate", rate}));
		kinahDrops = parseDrops(oracle->run({"m5b3-drops", "--npc", std::to_string(KINAH_NPC_ID), "--drop-rate", rate}));
		for (const OracleDrops* drops : {&lootDrops, &kinahDrops}) {
			EXPECT_TRUE(drops->registerDrop) << "npc " << drops->npcId << ": registerDrop does not run for its kill";
			EXPECT_TRUE(drops->deterministic) << "npc " << drops->npcId << ": the entry count is not certain at rate " << rate << " (§10.3 Y2)";
			EXPECT_EQ(drops->entriesMin, drops->entriesMax);
			EXPECT_EQ(static_cast<int32_t>(drops->rules.size()), drops->entriesMin) << "npc " << drops->npcId << ": one entry per applicable rule";
			for (const DropRule& rule : drops->rules) {
				EXPECT_TRUE(rule.certain) << "npc " << drops->npcId << " rule " << rule.name;
				EXPECT_EQ(rule.entriesIfFired, 1) << "npc " << drops->npcId << " rule " << rule.name << ": Y2 matches one entry to one rule";
			}
		}
		// §10.2's premises: 10 entries each; 210133's kinah entry in [5, 25]; one certain shard rule for both (the merge of Y3); one junk rule each
		EXPECT_EQ(lootDrops.entriesMin, 10) << "§10.3 Y2's number for npc 210663";
		EXPECT_EQ(kinahDrops.entriesMin, 10) << "§10.3 Y2's number for npc 210133";
		int32_t kinahRules = 0;
		for (const DropRule& rule : kinahDrops.rules)
			for (const DropCandidate& candidate : rule.candidates)
				if (candidate.kinah) {
					kinahRules++;
					EXPECT_EQ(candidate.itemId, KINAH_ITEM);
					EXPECT_EQ(rule.candidates.size(), 1u);
				}
		EXPECT_EQ(kinahRules, 1) << "Y4: npc 210133 has exactly one kinah rule";
		EXPECT_TRUE(std::ranges::none_of(lootDrops.rules, [](const DropRule& rule) { return rule.candidate(KINAH_ITEM) != nullptr; }))
		  << "npc 210663 is a BEAST, which the Kinah rule excludes (§2.4)";
		for (const DropRule& rule : lootDrops.rules)
			if (rule.name == "Power Shards" && rule.candidates.size() == 1)
				shardItem = rule.candidates[0].itemId;
		EXPECT_NE(shardItem, 0) << "Y3's certain merge: npc 210663 has no single-candidate Power Shards rule";
		EXPECT_TRUE(kinahDrops.candidate(shardItem) != nullptr) << "Y3's certain merge at L4: npc 210133 drops the same shard";
		for (const DropRule& rule : lootDrops.rules)
			if (rule.name.starts_with("JUNK_") && rule.candidates.size() == 1)
				lootJunk = rule.candidates[0].itemId;
		for (const DropRule& rule : kinahDrops.rules)
			if (rule.name.starts_with("JUNK_") && rule.candidates.size() == 1)
				kinahJunk = rule.candidates[0].itemId;
		ASSERT_NE(lootJunk, 0) << "npc 210663 has no single-candidate junk rule (L6c's second certain merge, L8, L11)";
		ASSERT_NE(kinahJunk, 0) << "npc 210133 has no single-candidate junk rule (L10's destroy)";
		ASSERT_NE(lootJunk, kinahJunk);

		itemInfo = parseItems(oracle->run({"m5b3-item", "--item", std::to_string(LIFE_POTION), "--item", std::to_string(MANA_POTION), "--item",
		                                   std::to_string(EVENT_POTION), "--item", std::to_string(GODSTONE), "--item", std::to_string(TRAINING_SWORD),
		                                   "--item", std::to_string(lootJunk), "--item", std::to_string(kinahJunk)}));
		const OracleItemInfo& potion = itemInfo.at(LIFE_POTION);
		ASSERT_NE(potion.skillId, 0) << "162000002 has no skilluse action";
		ASSERT_FALSE(potion.effects.empty());
		EXPECT_EQ(potion.effects.front().first, "ProcHealInstantEffect") << "Y8: the potion's instant heal is its first template";
		EXPECT_GT(potion.useDelayMillis, 2000) << "Y8's refusal 1 s later needs a use delay longer than that";
		EXPECT_TRUE(potion.maskFlags.contains("CAN_SPLIT") && potion.maskFlags.contains("STORABLE_IN_WH")) << "L9 splits it";
		EXPECT_FALSE(itemInfo.at(EVENT_POTION).maskFlags.contains("STORABLE_IN_WH")) << "L8's refused move needs an item not storable in a warehouse";
		EXPECT_TRUE(itemInfo.at(MANA_POTION).maskFlags.contains("STORABLE_IN_WH")) << "L11 swaps the mana potion stack into the warehouse";
		EXPECT_TRUE(itemInfo.at(lootJunk).maskFlags.contains("STORABLE_IN_WH")) << "L8 moves the sparkie junk into the warehouse";
		EXPECT_TRUE(itemInfo.at(kinahJunk).maskFlags.contains("BREAKABLE")) << "L10 destroys the kerub junk (CM_DELETE_ITEM refuses an unbreakable item)";
		const OracleItemInfo& godstone = itemInfo.at(GODSTONE);
		ASSERT_NE(godstone.godstoneSkillId, 0) << "168000116 is not a godstone";
		EXPECT_EQ(godstone.godstoneProbability, 1000) << "Y7: probability 1000 makes every evaluation a proc (GodStone.tryActivate)";
		EXPECT_EQ(godstone.godstoneBreakProbability, 0) << "Y7: a breakprob would take the godstone away mid fight";
		EXPECT_EQ(godstone.godstoneEffectClasses, std::vector<std::string>{"ProcAtkInstantEffect"}) << "Y7: the proc is one ProcAtkInstantEffect";
		EXPECT_TRUE(itemInfo.at(TRAINING_SWORD).maskFlags.contains("CAN_PROC_ENCHANT")) << "Item.canSocketGodstone (Item.java:659-661)";
		std::cout << "L0: npc 210663 at (" << lootMonster.nearestPlainSpot->x << ", " << lootMonster.nearestPlainSpot->y << ") " << lootMonster.nearestPlainSpot->distance
		          << " m, " << lootDrops.entriesMin << " entries; npc 210133 at (" << kinahMonster.nearestPlainSpot->x << ", " << kinahMonster.nearestPlainSpot->y
		          << ") " << kinahMonster.nearestPlainSpot->distance << " m, " << kinahDrops.entriesMin << " entries; shard " << shardItem << ", junk " << lootJunk
		          << " / " << kinahJunk << "; potion skill " << potion.skillId << " (" << potion.effects.front().first << " " << potion.effects.front().second
		          << "), godstone skill " << godstone.godstoneSkillId << std::endl;
	});

	AsyncAllowed async = AsyncAllowed::m5aDefault();

	/** CM_ENTER_WORLD, the §5.8 sequence and the model's reload; @return the burst */
	const auto enterWorld = [&](bool firstEnter) {
		async = AsyncAllowed::m5aDefault();
		async.selfPlayerState(a.warriorId);
		announcedNpcs.follow(a.game.get());
		async.npcActivity(announcedNpcs.predicate());
		a.game->send(GameSession::CM_ENTER_WORLD, GameSession::buildCM_ENTER_WORLD(a.warriorId));
		const std::vector<Packet> burst = collectBurst(*a.game, async);
		if (burst.empty())
			throw std::runtime_error("no packet after CM_ENTER_WORLD");
		expectSequence(burst, enterWorldPattern(firstEnter), async);
		model.sync();
		return burst;
	};
	const auto levelReady = [&](bool assertSequence) {
		a.game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
		const std::vector<Packet> burst = collectBurst(*a.game, async);
		if (burst.empty())
			throw std::runtime_error("no packet after CM_LEVEL_READY");
		if (assertSequence)
			expectSequence(burst, levelReadyPattern(), async);
		model.sync();
		return burst;
	};
	const auto lastStats = [&](size_t from = 0) -> std::optional<decoders::StatsInfo> {
		std::optional<decoders::StatsInfo> stats;
		const std::vector<Packet>& packets = a.game->recorded();
		for (size_t i = from; i < packets.size(); i++)
			if (packets[i].name == "SM_STATS_INFO")
				stats = decoders::decodeStatsInfo(packets[i].data);
		return stats;
	};

	/** the object id of the npc of `templateId` standing on `spot`, from every SM_NPC_INFO recorded so far (the latest wins) */
	const auto objectAt = [&](const OracleMonsterSpot& spot, int32_t templateId, std::optional<int32_t> excluding) -> std::optional<int32_t> {
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
	const auto waitForNpcAt = [&](const OracleMonsterSpot& spot, int32_t templateId, std::optional<int32_t> excluding,
	                              std::chrono::milliseconds timeout) -> std::optional<int32_t> {
		if (std::optional<int32_t> known = objectAt(spot, templateId, excluding))
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

	// the walk cursor and the walk of the M5b gates: 5 m steps, each followed by a short read
	float atX = creation.x, atY = creation.y, atZ = creation.z;
	const auto walkTo = [&](float toX, float toY, float toZ) {
		const double total = distance2d(atX, atY, toX, toY);
		const int32_t steps = std::max(1, static_cast<int32_t>(total / 5.0));
		const float fromX = atX, fromY = atY, fromZ = atZ;
		for (int32_t step = 1; step <= steps; step++) {
			const float t = static_cast<float>(step) / static_cast<float>(steps);
			a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(fromX + (toX - fromX) * t, fromY + (toY - fromY) * t, fromZ + (toZ - fromZ) * t, 0,
			                                                             static_cast<int8_t>(0xE0), toX, toY, toZ));
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

	// ---- C1-C3: login, create, enter world, level ready (as M5b-1) ----
	int32_t lifePotionObject = 0, manaPotionObject = 0, eventPotionObject = 0;
	runCase("C1-C3", "login, create the Elyos Warrior, enter world, level ready", [&] {
		const decoders::CharacterList list = logIn(servers, a, async);
		EXPECT_EQ(list.characterCount, 0) << "a fresh account must have no character";
		NewCharacter character;
		character.name = warriorName;
		character.asmodian = false;
		character.playerClassId = CLASS_WARRIOR;
		a.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(a.key.accountId, a.account, character, 1));
		EXPECT_EQ(decoders::decodeCreateCharacter(expectNext(*a.game, "SM_CREATE_CHARACTER", async).data).responseCode, RESPONSE_OPEN_CREATION_WINDOW);
		a.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(a.key.accountId, a.account, character, 0));
		const decoders::CreateCharacter created = decoders::decodeCreateCharacter(expectNext(*a.game, "SM_CREATE_CHARACTER", async).data);
		if (created.responseCode != RESPONSE_OK || !created.player)
			throw std::runtime_error("creating the Warrior answered response code " + std::to_string(created.responseCode));
		a.warriorId = created.player->playerId;
		model.follow(a.game.get());
		enterWorld(true);
		levelReady(true);
		// the model against the oracle's starter inventory: every m5a-creation item, equipped where it says so
		std::vector<std::string> missing;
		for (const OracleItem& item : creation.items) {
			bool found = false;
			for (const auto& [id, known] : model.items)
				found = found || (known.itemId == item.itemId && known.count == item.count && (known.equippedSlot != 0) == item.equipped);
			if (!found)
				missing.push_back(std::to_string(item.itemId) + "x" + std::to_string(item.count));
		}
		EXPECT_TRUE(missing.empty()) << "the enter-world SM_INVENTORY_INFO lacks the starter items " << join(missing) << "; it lists " << model.describe();
		EXPECT_TRUE(model.decodeFailures.empty()) << join(model.decodeFailures, "\n  ");
		// the starter stacks the later cases use, by object: a loot may add a second stack of the same id (a port that does not merge), and each
		// case must then still fail at its own row, not at the next case's lookup
		const std::array<std::pair<int32_t*, int32_t>, 3> starters{
		  {{&lifePotionObject, LIFE_POTION}, {&manaPotionObject, MANA_POTION}, {&eventPotionObject, EVENT_POTION}}};
		for (const auto& [object, item] : starters) {
			const std::vector<ModelItem> stacks = model.byItemId(item);
			ASSERT_EQ(stacks.size(), 1u) << "the starter inventory holds one stack of " << item << ": " << model.describe();
			*object = stacks.front().objectId;
		}
	});

	// ---- the kill, the corpse and the loot (L1-L3, L4, L6b-L6c) ----
	std::vector<KillRecord> kills;
	std::vector<LootRecord> loots;
	std::set<int32_t> lootedItemIds; // every item id a loot added, for L3b's choice of what to delete
	std::set<int32_t> deletedObjects; // what Y12 must not find in `inventory`: L3b's deletes, the consumed godstone, L10's junk

	/** walks to 2 m from `spot`, waits for the npc there (not `excluding`), selects it and fights it until it dies */
	const auto killAt = [&](const OracleMonsterSpot& spot, int32_t templateId, std::optional<int32_t> excluding,
	                        std::chrono::milliseconds respawnWait) -> KillRecord {
		const std::array<float, 3> melee = pointNear(spot, MELEE_DISTANCE, atX, atY);
		walkTo(melee[0], melee[1], melee[2]);
		const std::optional<int32_t> npc = waitForNpcAt(spot, templateId, excluding, respawnWait);
		if (!npc)
			throw std::runtime_error("no SM_NPC_INFO for npc " + std::to_string(templateId) + " at (" + std::to_string(spot.x) + ", " +
			                         std::to_string(spot.y) + ")");
		a.game->send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(*npc));
		waitFor(*a.game, "SM_TARGET_SELECTED", 10s);
		const std::optional<decoders::StatsInfo> stats = lastStats();
		if (!stats)
			throw std::runtime_error("no SM_STATS_INFO recorded");
		KillRecord kill;
		kill.templateId = templateId;
		kill.corpseId = *npc;
		bool died = false, warriorDied = false;
		const GameSession::FightOutcome outcome = a.game->fightUntil(
		  *npc, std::chrono::milliseconds(stats->attackSpeed),
		  [&](const Packet& packet) {
			  if (packet.name != "SM_EMOTION")
				  return false;
			  try {
				  const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
				  if (emotion.emotionType != decoders::EMOTION_DIE)
					  return false;
				  died = died || emotion.senderObjectId == *npc;
				  warriorDied = warriorDied || emotion.senderObjectId == a.warriorId;
				  return died || warriorDied;
			  } catch (const DecodeError&) {
				  return false;
			  }
		  },
		  120s, 80);
		kill.fightFrom = outcome.firstPacket;
		kill.dieIndex = a.game->recorded().size() - 1;
		if (warriorDied)
			throw std::runtime_error("npc " + std::to_string(templateId) + " killed the Warrior");
		if (!died)
			throw std::runtime_error("npc " + std::to_string(templateId) + " did not die within " + std::to_string(outcome.elapsed.count()) + " ms");
		kills.push_back(kill);
		return kill;
	};

	/**
	 * L2 and L3 (and the loot halves of L4 and L6c): CM_START_LOOT(corpse, 0), then CM_LOOT_ITEM for every listed index in list order, with Y2
	 * on the list and Y3 (and Y4 for kinah) on every loot. `certainMerges` are the item ids the oracle's cube budget calls deterministic merges.
	 */
	const auto lootCorpse = [&](const KillRecord& kill, const OracleDrops& drops, const std::set<int32_t>& certainMerges) {
		loots.emplace_back();
		LootRecord& record = loots.back(); // nothing else appends to `loots` while this runs
		record.kill = kill;
		// the corpse is announced as lootable by registerDrop's SM_LOOT_STATUS(LOOT_ENABLE), which follows the kill at once (NpcController.onDie);
		// Y1 counts them over the whole run at L13, this only waits for it
		const auto isLootEnable = [&](const Packet& packet) {
			if (packet.name != "SM_LOOT_STATUS")
				return false;
			const decoders::LootStatus status = decoders::decodeLootStatus(packet.data);
			return status.targetObjectId == kill.corpseId && status.status == decoders::LOOT_STATUS_LOOT_ENABLE;
		};
		bool enabled = false;
		for (size_t i = kill.fightFrom; i < a.game->recorded().size() && !enabled; i++)
			enabled = isLootEnable(a.game->recorded()[i]);
		if (!enabled && !readUntil(*a.game, isLootEnable, 5s))
			ADD_FAILURE() << "Y1: no SM_LOOT_STATUS(LOOT_ENABLE) for the corpse " << kill.corpseId << " within 5 s of its death";
		collectFor(*a.game, 500ms);
		model.sync();

		// ---- L2: open the corpse (Y2) ----
		const size_t openFrom = a.game->recorded().size();
		a.game->send(GameSession::CM_START_LOOT, GameSession::buildCM_START_LOOT(kill.corpseId, GameSession::LOOT_OPEN));
		const std::optional<size_t> emoted = readUntil(
		  *a.game,
		  [&](const Packet& packet) {
			  if (packet.name != "SM_EMOTION")
				  return false;
			  const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
			  return emotion.emotionType == EMOTION_START_LOOT && emotion.senderObjectId == a.warriorId;
		  },
		  5s);
		const std::vector<Packet> opened = slice(*a.game, openFrom);
		std::optional<decoders::LootItemList> list;
		std::optional<size_t> listAt, openStatusAt;
		for (size_t i = 0; i < opened.size(); i++) {
			if (opened[i].name == "SM_LOOT_ITEMLIST" && !list) {
				list = decoders::decodeLootItemList(opened[i].data);
				listAt = i;
			} else if (opened[i].name == "SM_LOOT_STATUS" && !openStatusAt) {
				const decoders::LootStatus status = decoders::decodeLootStatus(opened[i].data);
				if (status.targetObjectId == kill.corpseId && status.status == decoders::LOOT_STATUS_OPEN_DROP_LIST)
					openStatusAt = i;
			}
		}
		if (!list)
			throw std::runtime_error("Y2: CM_START_LOOT(" + std::to_string(kill.corpseId) + ", 0) was answered by no SM_LOOT_ITEMLIST: " + join(namesOf(opened)));
		record.opened = true;
		record.entries = list->items;
		EXPECT_EQ(list->targetObjectId, kill.corpseId) << "Y2: SM_LOOT_ITEMLIST names another object";
		// Y2: exactly the oracle's entry count, indexes exactly {1..n}
		const int32_t expected = drops.entriesMin;
		std::vector<std::string> listed;
		std::set<int32_t> indexes;
		for (const decoders::LootItem& entry : list->items) {
			listed.push_back(std::to_string(entry.index) + ":" + std::to_string(entry.itemId) + "x" + std::to_string(entry.count));
			indexes.insert(entry.index);
		}
		EXPECT_EQ(static_cast<int32_t>(list->items.size()), expected)
		  << "Y2: the corpse of npc " << kill.templateId << " lists " << list->items.size() << " entries, and the oracle's rules make " << expected
		  << " at rate " << DROP_RATE << ": " << join(listed);
		std::set<int32_t> oneToN;
		for (int32_t i = 1; i <= static_cast<int32_t>(list->items.size()); i++)
			oneToN.insert(i);
		EXPECT_EQ(indexes, oneToN) << "Y2: the indexes are not 1..n (registerDrop's running index, DropRegistrationService): " << join(listed);
		// Y2: a bijection between the entries and the applicable rules by candidate membership, every count inside its rule's range
		const std::vector<int32_t> ruleOfEntry = matchEntriesToRules(list->items, drops.rules);
		std::set<int32_t> matchedRules;
		for (size_t i = 0; i < list->items.size(); i++) {
			const decoders::LootItem& entry = list->items[i];
			if (ruleOfEntry[i] < 0) {
				ADD_FAILURE() << "Y2: entry " << listed[i] << " belongs to no applicable rule of npc " << kill.templateId << " (or its rule is taken)";
				continue;
			}
			matchedRules.insert(ruleOfEntry[i]);
			const DropRule& rule = drops.rules[static_cast<size_t>(ruleOfEntry[i])];
			const DropCandidate* candidate = rule.candidate(entry.itemId);
			EXPECT_GE(entry.count, candidate->minCount) << "Y2: entry " << listed[i] << " of rule '" << rule.name << "'";
			EXPECT_LE(entry.count, candidate->maxCount) << "Y2: entry " << listed[i] << " of rule '" << rule.name << "'";
		}
		EXPECT_EQ(matchedRules.size(), drops.rules.size()) << "Y2: " << drops.rules.size() - matchedRules.size() << " applicable rule(s) of npc "
		                                                   << kill.templateId << " have no entry: " << join(listed);
		// Y2: then SM_LOOT_STATUS(OPEN_DROP_LIST) and SM_EMOTION(START_LOOT) naming the corpse (DropService.requestDropList)
		EXPECT_TRUE(openStatusAt && *openStatusAt > *listAt) << "Y2: no SM_LOOT_STATUS(OPEN_DROP_LIST) after the list: " << join(namesOf(opened));
		ASSERT_TRUE(emoted) << "Y2: no SM_EMOTION(START_LOOT) of the Warrior";
		EXPECT_EQ(decoders::decodeEmotion(a.game->recorded()[*emoted].data).targetObjectId, kill.corpseId) << "Y2: START_LOOT names another object";
		std::cout << "L2: npc " << kill.templateId << " (corpse " << kill.corpseId << ") listed " << join(listed) << std::endl;

		// ---- L3: loot it empty, in list order (Y3, Y4) ----
		std::vector<decoders::LootItem> remaining = list->items;
		const std::vector<decoders::LootItem> order = list->items;
		for (size_t n = 0; n < order.size(); n++) {
			const decoders::LootItem entry = order[n];
			const bool last = n + 1 == order.size();
			model.sync();
			const DropCandidate* candidate = drops.candidate(entry.itemId);
			const bool kinah = entry.itemId == KINAH_ITEM;
			const std::vector<ModelItem> stacks = model.byItemId(entry.itemId);
			const bool stackable = candidate != nullptr && candidate->maxStackCount > 1;
			const bool mergeExpected = !kinah && stackable && !stacks.empty() && stacks.front().count + entry.count <= candidate->maxStackCount;
			const int64_t kinahBefore = model.kinah();
			const std::optional<ModelItem> mergeTarget = mergeExpected ? std::optional<ModelItem>(stacks.front()) : std::nullopt;
			const size_t lootFrom = a.game->recorded().size();
			a.game->send(GameSession::CM_LOOT_ITEM, GameSession::buildCM_LOOT_ITEM(kill.corpseId, entry.index));
			// the loot ends with the next SM_LOOT_ITEMLIST, or for the last entry with the corpse's SM_DELETE (resendDropList)
			const std::optional<size_t> endAt = readUntil(
			  *a.game,
			  [&](const Packet& packet) {
				  if (last)
					  return packet.name == "SM_DELETE" && decoders::decodeDeleteObjectId(packet.data) == kill.corpseId;
				  return packet.name == "SM_LOOT_ITEMLIST";
			  },
			  5s);
			collectFor(*a.game, 300ms);
			const std::vector<Packet> window = slice(*a.game, lootFrom);
			const std::string label = "entry " + std::to_string(entry.index) + " (" + std::to_string(entry.itemId) + " x" + std::to_string(entry.count) + ")";
			model.sync();
			if (!endAt) {
				ADD_FAILURE() << "Y3: looting " << label << " of corpse " << kill.corpseId << " was followed by no "
				              << (last ? "SM_DELETE of the corpse within 5 s (resendDropList)" : "SM_LOOT_ITEMLIST") << ": " << join(namesOf(window));
				break;
			}
			const size_t end = *endAt - lootFrom;
			std::vector<Packet> before(window.begin(), window.begin() + static_cast<std::ptrdiff_t>(end));
			if (kinah) {
				// Y4: SM_INVENTORY_UPDATE_ITEM of the kinah item with K + k, INC_KINAH_COLLECT, and no SM_INVENTORY_ADD_ITEM (Storage.increaseKinah)
				EXPECT_EQ(ofName(before, "SM_INVENTORY_ADD_ITEM").size(), 0u) << "Y4: the kinah was added as an item: " << join(namesOf(before));
				// (EXPECT, not ASSERT, inside the loop: a wrong packet fails its row and the corpse is still looted empty, so the later cases run)
				const std::vector<Packet> updates = ofName(before, "SM_INVENTORY_UPDATE_ITEM");
				EXPECT_EQ(updates.size(), 1u) << "Y4: looting the kinah sent " << updates.size() << " SM_INVENTORY_UPDATE_ITEM: " << join(namesOf(before));
				if (!updates.empty()) {
					const decoders::InventoryUpdateItem update = decoders::decodeInventoryUpdateItem(updates[0].data);
					const std::optional<ModelItem> kinahItem = model.byObjectId(update.item.objectId);
					EXPECT_TRUE(kinahItem && kinahItem->itemId == KINAH_ITEM) << "Y4: the update names object " << update.item.objectId << ", not the kinah item";
					EXPECT_TRUE(update.item.general) << "Y4: the kinah update carries no count";
					const int64_t kinahAfter = update.item.general ? update.item.general->count : -1;
					EXPECT_EQ(kinahAfter, kinahBefore + entry.count) << "Y4: the kinah went from " << kinahBefore << " by " << entry.count << " to " << kinahAfter;
					EXPECT_EQ(update.updateTypeMask, std::optional<uint16_t>(decoders::ITEM_UPDATE_INC_KINAH_COLLECT))
					  << "Y4: ItemService.addItem's kinah arm (Storage.increaseKinah(count) -> INC_KINAH_COLLECT)";
					std::cout << "Y4: kinah " << kinahBefore << " + " << entry.count << " = " << kinahAfter << std::endl;
				}
				EXPECT_TRUE(candidate != nullptr && entry.count >= candidate->minCount && entry.count <= candidate->maxCount)
				  << "Y4: the kinah entry's count " << entry.count << " is outside the oracle's range";
			} else if (mergeExpected) {
				// Y3, the merge arm: SM_INVENTORY_UPDATE_ITEM alone with old + looted, no SM_CUBE_UPDATE (ItemPacketService.java:191-204)
				const std::vector<Packet> updates = ofName(before, "SM_INVENTORY_UPDATE_ITEM");
				EXPECT_EQ(ofName(before, "SM_INVENTORY_ADD_ITEM").size(), 0u)
				  << "Y3: " << label << " should merge into stack " << mergeTarget->objectId << " (" << mergeTarget->count
				  << ") and was added as a new stack: " << join(namesOf(before));
				EXPECT_EQ(ofName(before, "SM_CUBE_UPDATE").size(), 0u) << "Y3: a merge sends no SM_CUBE_UPDATE: " << join(namesOf(before));
				EXPECT_EQ(updates.size(), 1u) << "Y3: the merge of " << label << ": " << join(namesOf(before));
				if (!updates.empty()) {
					const decoders::InventoryUpdateItem update = decoders::decodeInventoryUpdateItem(updates[0].data);
					EXPECT_EQ(update.item.objectId, mergeTarget->objectId);
					EXPECT_TRUE(update.item.general && update.item.general->count == mergeTarget->count + entry.count)
					  << "Y3: the merged count of " << label << " is not " << mergeTarget->count << " + " << entry.count;
					EXPECT_EQ(update.updateTypeMask, std::optional<uint16_t>(decoders::ITEM_UPDATE_INC_ITEM_COLLECT));
				}
				if (!ofName(before, "SM_INVENTORY_ADD_ITEM").empty())
					lootedItemIds.insert(entry.itemId); // the stack the merge should not have made may go at L3b
			} else {
				// Y3, the new-stack arm: SM_INVENTORY_ADD_ITEM then SM_CUBE_UPDATE (ItemPacketService.java:214-228)
				EXPECT_FALSE(certainMerges.contains(entry.itemId)) << "Y3: the oracle calls " << entry.itemId << " a certain merge and the model has no stack";
				const std::vector<std::string> names = namesOf(before);
				const auto add = std::ranges::find(names, "SM_INVENTORY_ADD_ITEM");
				EXPECT_NE(add, names.end()) << "Y3: " << label << " sent no SM_INVENTORY_ADD_ITEM: " << join(names);
				if (add != names.end()) {
					EXPECT_NE(std::find(add, names.end(), "SM_CUBE_UPDATE"), names.end()) << "Y3: no SM_CUBE_UPDATE after the add: " << join(names);
					EXPECT_EQ(ofName(before, "SM_INVENTORY_UPDATE_ITEM").size(), 0u) << "Y3: a new stack is no update: " << join(names);
					const decoders::InventoryAddItem added = decoders::decodeInventoryAddItem(ofName(before, "SM_INVENTORY_ADD_ITEM")[0].data);
					EXPECT_EQ(added.items.size(), 1u);
					EXPECT_TRUE(!added.items.empty() && added.items[0].templateId == entry.itemId && added.items[0].general &&
					            added.items[0].general->count == entry.count)
					  << "Y3: the added item of " << label << " is not that item with that count";
				}
				lootedItemIds.insert(entry.itemId);
			}
			if (certainMerges.contains(entry.itemId))
				EXPECT_TRUE(mergeExpected) << "Y3: " << entry.itemId << " is a certain merge (m5b3-drops cube.deterministicMerges) and the model has no stack for it";
			std::erase_if(remaining, [&](const decoders::LootItem& item) { return item.index == entry.index; });
			if (!last) {
				// Y3: then an SM_LOOT_ITEMLIST one entry shorter, without the looted index
				const decoders::LootItemList next = decoders::decodeLootItemList(window[end].data);
				EXPECT_EQ(next.items.size(), remaining.size()) << "Y3: after looting " << label << " the list has " << next.items.size() << " entries";
				EXPECT_TRUE(std::ranges::none_of(next.items, [&](const decoders::LootItem& item) { return item.index == entry.index; }))
				  << "Y3: the looted entry is still listed";
			} else {
				// Y3: after the last, SM_LOOT_STATUS(CLOSE_DROP_LIST), SM_EMOTION(END_LOOT) and SM_DELETE(corpse) within 1 s (resendDropList)
				std::optional<size_t> closeAt, endLootAt;
				for (size_t i = 0; i < before.size(); i++) {
					if (before[i].name == "SM_LOOT_STATUS") {
						const decoders::LootStatus status = decoders::decodeLootStatus(before[i].data);
						if (status.targetObjectId == kill.corpseId && status.status == decoders::LOOT_STATUS_CLOSE_DROP_LIST && !closeAt)
							closeAt = i;
					} else if (before[i].name == "SM_EMOTION") {
						const decoders::Emotion emotion = decoders::decodeEmotion(before[i].data);
						if (emotion.emotionType == EMOTION_END_LOOT && emotion.senderObjectId == a.warriorId && !endLootAt)
							endLootAt = i;
					}
				}
				EXPECT_TRUE(closeAt) << "Y3: no SM_LOOT_STATUS(CLOSE_DROP_LIST) after the last entry: " << join(namesOf(before));
				EXPECT_TRUE(endLootAt && (!closeAt || *endLootAt > *closeAt)) << "Y3: no SM_EMOTION(END_LOOT) after the close: " << join(namesOf(before));
				const int64_t deleteAfter = millisBetween(a.game->recorded()[lootFrom].receivedAt, window[end].receivedAt);
				EXPECT_LE(deleteAfter, 1000) << "Y3: the corpse's SM_DELETE came " << deleteAfter << " ms after the last loot, not at once";
				EXPECT_TRUE(ofName(before, "SM_LOOT_ITEMLIST").empty()) << "Y3: an empty list was resent instead of closing";
				record.emptied = true;
			}
		}
		EXPECT_TRUE(model.decodeFailures.empty()) << join(model.decodeFailures, "\n  ");
	};

	/** CM_DELETE_ITEM of one cube stack with Y11's shape: SM_DELETE_ITEM(obj, DISCARD) and SM_CUBE_UPDATE (Storage.delete) */
	const auto deleteStack = [&](const ModelItem& stack, std::string_view why) {
		const size_t from = a.game->recorded().size();
		a.game->send(GameSession::CM_DELETE_ITEM, GameSession::buildCM_DELETE_ITEM(stack.objectId));
		const std::optional<size_t> deleted = readUntil(
		  *a.game, [&](const Packet& packet) { return packet.name == "SM_DELETE_ITEM" && decoders::decodeDeleteItem(packet.data).objectId == stack.objectId; },
		  5s);
		collectFor(*a.game, 300ms);
		const std::vector<Packet> window = slice(*a.game, from);
		model.sync();
		EXPECT_TRUE(deleted) << "Y11 (" << why << "): CM_DELETE_ITEM(" << stack.objectId << ") sent no SM_DELETE_ITEM: " << join(namesOf(window));
		if (!deleted)
			return;
		const decoders::DeleteItem item = decoders::decodeDeleteItem(a.game->recorded()[*deleted].data);
		EXPECT_EQ(item.deleteTypeMask, decoders::ITEM_DELETE_DISCARD) << "Y11 (" << why << "): CM_DELETE_ITEM deletes with ItemDeleteType.DISCARD";
		bool cubeAfter = false;
		for (size_t i = *deleted + 1; i < a.game->recorded().size(); i++)
			cubeAfter = cubeAfter || a.game->recorded()[i].name == "SM_CUBE_UPDATE";
		EXPECT_TRUE(cubeAfter) << "Y11 (" << why << "): no SM_CUBE_UPDATE after the delete (ItemPacketService.sendItemDeletePacket)";
		EXPECT_FALSE(model.byObjectId(stack.objectId).has_value());
	};

	/** oracle.py m5b3-drops for `npcId` with the model's cube as --inventory: the cube budget of the next corpse */
	const auto cubeBudget = [&](int32_t npcId) {
		model.sync();
		std::vector<std::string> arguments{"m5b3-drops", "--npc", std::to_string(npcId), "--drop-rate", std::string(DROP_RATE)};
		for (const ModelItem& stack : model.cubeStacks()) {
			arguments.push_back("--inventory");
			arguments.push_back(std::to_string(stack.itemId) + ":" + std::to_string(stack.count));
		}
		return parseDrops(oracle->run(arguments));
	};

	/**
	 * L3b (rev 2): deletes looted stacks no later case uses until the cube has `needed` free slots. Never the starter items, the shard stack, the
	 * junk stacks or the sword - only item ids a loot of this run added.
	 */
	const auto makeRoom = [&](int32_t limit, int32_t needed, std::string_view why) {
		model.sync();
		const std::set<int32_t> keep{LIFE_POTION, MANA_POTION, EVENT_POTION, TRAINING_SWORD, GODSTONE, shardItem, lootJunk, kinahJunk, KINAH_ITEM};
		int32_t free = limit - static_cast<int32_t>(model.cubeStacks().size());
		int32_t deleted = 0;
		while (free < needed) {
			std::optional<ModelItem> victim;
			for (const ModelItem& stack : model.cubeStacks())
				if (!keep.contains(stack.itemId) && lootedItemIds.contains(stack.itemId) && !victim)
					victim = stack;
			if (!victim)
				throw std::runtime_error("L3b: " + std::to_string(free) + " free slots, " + std::to_string(needed) + " needed (" + std::string(why) +
				                         ") and no looted stack left to delete: " + model.describe());
			deleteStack(*victim, why);
			deletedObjects.insert(victim->objectId);
			deleted++;
			const int32_t now = limit - static_cast<int32_t>(model.cubeStacks().size());
			if (now <= free)
				throw std::runtime_error("L3b: deleting " + std::to_string(victim->objectId) + " freed no slot");
			free = now;
		}
		std::cout << "L3b (" << why << "): " << free << " free slots of " << limit << " after " << deleted << " delete(s), " << needed << " needed" << std::endl;
		return free;
	};

	// ---- L1-L3: the kill of 210663, its corpse opened and looted empty ----
	std::optional<KillRecord> firstKill;
	int32_t cubeLimit = 27;
	int32_t sparkieJunkObject = 0, kerubJunkObject = 0; // the junk stacks L3 and L4 created (L6c merges into the first, L8/L11 move it, L10 destroys the second)
	/**
	 * L3b's spare slot: one more than the oracle's worst case, so that a port which adds a stack the oracle merges (Y3's never-merge mutant)
	 * fails Y3 at the loot and not Y5 at the next unequip on a full cube
	 */
	constexpr int32_t SPARE_SLOTS = 1;
	runCase("L1-L3", "kill 210663, open the corpse and loot it empty (Y1, Y2, Y3)", [&] {
		const OracleDrops budget = cubeBudget(LOOT_NPC_ID);
		cubeLimit = budget.cubeLimit;
		ASSERT_GT(cubeLimit, 0);
		EXPECT_LE(budget.slotsUsed + budget.worstCaseNewSlots, cubeLimit) << "the first corpse must fit the fresh cube (§13 question 5)";
		firstKill = killAt(*lootMonster.nearestPlainSpot, LOOT_NPC_ID, std::nullopt, 15s);
		lootCorpse(*firstKill, lootDrops, budget.deterministicMergeItems);
		ASSERT_FALSE(loots.empty());
		EXPECT_TRUE(loots.back().emptied) << "L3: the corpse was not looted empty";
		const std::vector<ModelItem> junk = model.byItemId(lootJunk);
		ASSERT_EQ(junk.size(), 1u) << "L3 looted no sparkie junk: " << model.describe();
		sparkieJunkObject = junk.front().objectId;
	});

	// ---- L3b + L4: make room, then the kinah kill ----
	runCase("L4", "make room (L3b), kill 210133 and loot it empty, kinah included (Y2, Y3, Y4)", [&] {
		OracleDrops budget = cubeBudget(KINAH_NPC_ID);
		// the corpse's worst case plus the sword L5 unequips (Equipment.unEquipItem refuses on a full cube)
		makeRoom(cubeLimit, budget.worstCaseNewSlots + 1 + SPARE_SLOTS, "the 210133 corpse and L5's sword");
		budget = cubeBudget(KINAH_NPC_ID);
		EXPECT_TRUE(budget.deterministicMergeItems.contains(shardItem)) << "Y3: the shard of L3 is the certain merge of L4 (§10.3 Y3)";
		const KillRecord kill = killAt(*kinahMonster.nearestPlainSpot, KINAH_NPC_ID, std::nullopt, 15s);
		lootCorpse(kill, kinahDrops, budget.deterministicMergeItems);
		ASSERT_FALSE(loots.empty());
		EXPECT_TRUE(loots.back().emptied) << "L4: the corpse was not looted empty";
		EXPECT_TRUE(std::ranges::any_of(loots.back().entries, [](const decoders::LootItem& item) { return item.itemId == KINAH_ITEM; }))
		  << "Y4: the 210133 corpse listed no kinah";
		const std::vector<ModelItem> junk = model.byItemId(kinahJunk);
		ASSERT_EQ(junk.size(), 1u) << "L4 looted no kerub junk: " << model.describe();
		kerubJunkObject = junk.front().objectId;
	});

	// ---- the equip round trip (L5, and the unequip / equip of L6) ----
	struct EquipWindow {
		std::vector<Packet> packets;
		std::optional<decoders::InventoryUpdateItem> update;
		std::optional<decoders::StatsInfo> stats;
		std::optional<decoders::UpdatePlayerAppearance> appearance;
		std::optional<size_t> updateAt, statsAt, appearanceAt;
	};
	/** CM_EQUIP_ITEM(action, slot, sword) and everything until the Warrior's SM_UPDATE_PLAYER_APPEARANCE (CM_EQUIP_ITEM.java's last statement) */
	const auto equipAction = [&](uint8_t action, int64_t slot, int32_t objectId) {
		EquipWindow result;
		const size_t from = a.game->recorded().size();
		a.game->send(GameSession::CM_EQUIP_ITEM, GameSession::buildCM_EQUIP_ITEM(action, slot, objectId));
		readUntil(
		  *a.game,
		  [&](const Packet& packet) {
			  if (packet.name != "SM_UPDATE_PLAYER_APPEARANCE")
				  return false;
			  try {
				  return decoders::decodeUpdatePlayerAppearance(packet.data).playerObjectId == a.warriorId;
			  } catch (const DecodeError&) {
				  return true; // it ends the wait; the window below reports that it does not decode
			  }
		  },
		  5s);
		collectFor(*a.game, 500ms);
		model.sync();
		result.packets = slice(*a.game, from);
		for (size_t i = 0; i < result.packets.size(); i++) {
			const Packet& packet = result.packets[i];
			try {
				if (packet.name == "SM_INVENTORY_UPDATE_ITEM" && !result.update) {
					const decoders::InventoryUpdateItem update = decoders::decodeInventoryUpdateItem(packet.data);
					if (update.item.objectId == objectId) {
						result.update = update;
						result.updateAt = i;
					}
				} else if (packet.name == "SM_STATS_INFO") {
					result.stats = decoders::decodeStatsInfo(packet.data);
					result.statsAt = i;
				} else if (packet.name == "SM_UPDATE_PLAYER_APPEARANCE" && !result.appearance) {
					const decoders::UpdatePlayerAppearance appearance = decoders::decodeUpdatePlayerAppearance(packet.data);
					if (appearance.playerObjectId == a.warriorId) {
						result.appearance = appearance;
						result.appearanceAt = i;
					}
				}
			} catch (const DecodeError& error) {
				// a body that does not match its Java writeImpl fails the row that reads it, not the whole script
				ADD_FAILURE() << "Y5: " << packet.name << " of CM_EQUIP_ITEM(" << static_cast<int32_t>(action) << ") does not decode: " << error.what();
			}
		}
		return result;
	};
	const auto swordObject = [&]() -> int32_t {
		model.sync();
		if (std::optional<ModelItem> sword = model.equipped(TRAINING_SWORD))
			return sword->objectId;
		const std::vector<ModelItem> stacks = model.byItemId(TRAINING_SWORD);
		return stacks.empty() ? 0 : stacks.front().objectId;
	};
	/** Y5's shape of one half of the round trip; `equipped` is the state CM_EQUIP_ITEM must leave the sword in */
	const auto expectEquipShape = [&](const EquipWindow& window, int32_t sword, bool equipped, std::string_view what) {
		// non-fatal throughout: a wrong equip packet fails Y5 and the script goes on with the state the model shows
		const std::string names = join(namesOf(window.packets));
		EXPECT_TRUE(window.update) << "Y5 (" << what << "): no SM_INVENTORY_UPDATE_ITEM of the sword: " << names;
		if (window.update) {
			EXPECT_FALSE(window.update->updateTypeMask.has_value())
			  << "Y5 (" << what << "): EQUIP_UNEQUIP is not sendable, so the packet carries no update type (SM_INVENTORY_UPDATE_ITEM.java:58-59)";
			EXPECT_EQ(window.update->item.equippedSlotBlob, std::optional<int64_t>(equipped ? MAIN_HAND : 0))
			  << "Y5 (" << what << "): the EQUIP_UNEQUIP blob is the EQUIPPED_SLOT entry alone (:40-43), the sword's slot";
		}
		EXPECT_TRUE(window.stats) << "Y5 (" << what << "): no SM_STATS_INFO (Equipment.equip/unEquip -> updateStatsAndSpeedVisually): " << names;
		EXPECT_TRUE(window.appearance) << "Y5 (" << what << "): no SM_UPDATE_PLAYER_APPEARANCE of the Warrior: " << names;
		if (window.update && window.stats && window.appearance)
			EXPECT_TRUE(*window.updateAt < *window.statsAt && *window.statsAt < *window.appearanceAt)
			  << "Y5 (" << what << "): Java's order is the item update, the stats, then the appearance (Equipment.java, CM_EQUIP_ITEM.java): " << names;
		if (window.appearance) {
			EXPECT_EQ((window.appearance->equipment.mask & MAIN_HAND) != 0, equipped)
			  << "Y5 (" << what << "): the appearance's slot mask " << window.appearance->equipment.mask;
			EXPECT_EQ(std::ranges::any_of(window.appearance->equipment.items,
			                              [](const decoders::EquippedItem& item) { return item.skinTemplateId == TRAINING_SWORD; }),
			          equipped)
			  << "Y5 (" << what << "): the sword in the appearance";
		}
		const std::optional<ModelItem> item = model.byObjectId(sword);
		EXPECT_TRUE(item && (item->equippedSlot != 0) == equipped) << "Y5 (" << what << "): the model's sword";
	};

	runCase("L5", "unequip and re-equip the sword; the stats return exactly (Y5)", [&] {
		const int32_t sword = swordObject();
		ASSERT_NE(sword, 0) << "the model has no Training Sword: " << model.describe();
		const std::optional<decoders::StatsInfo> before = lastStats();
		ASSERT_TRUE(before);
		const EquipWindow off = equipAction(GameSession::UNEQUIP, 0, sword);
		expectEquipShape(off, sword, false, "unequip");
		ASSERT_TRUE(off.stats);
		// the sword's weapon_stats (item_templates.xml:376: parry 173, physical_accuracy 52, critical 50) have no stat functions of the item's own:
		// PlayerGameStats adds them to the BASE from Equipment.getMainHandWeapon (PlayerGameStats.java:141-147 and its getMainHandPCritical /
		// getMainHandPAccuracy), and the Warrior's sword mastery passive checks the weapon group - so each of these falls exactly when the sword
		// leaves the main hand (Equipment.unEquip), whatever ItemEquipmentListener does (its endEffect(item) ends nothing for this sword: the
		// equivalent mutant of 2026-09-24, m5b3-plan.md §17)
		EXPECT_LT(off.stats->mainHandPAttack, before->mainHandPAttack) << "Y5: the main hand attack did not fall with the sword off";
		EXPECT_LT(off.stats->parry, before->parry) << "Y5: the parry did not fall with the sword off (the weapon's parry is part of the base)";
		EXPECT_LT(off.stats->mainHandPAccuracy, before->mainHandPAccuracy) << "Y5: the main hand accuracy did not fall with the sword off";
		EXPECT_LT(off.stats->mainHandPCritical, before->mainHandPCritical) << "Y5: the main hand critical did not fall with the sword off";
		const EquipWindow on = equipAction(GameSession::EQUIP, MAIN_HAND, sword);
		expectEquipShape(on, sword, true, "re-equip");
		ASSERT_TRUE(on.stats);
		const std::vector<std::string> differences = statFieldDifferences(*before, *on.stats);
		EXPECT_TRUE(differences.empty()) << "Y5: the stats after the round trip differ from before it (before/after): " << join(differences);
		std::cout << "Y5: the main hand attack " << before->mainHandPAttack << " -> " << off.stats->mainHandPAttack << " -> " << on.stats->mainHandPAttack
		          << "; max HP " << before->maxHp << " -> " << off.stats->maxHp << " -> " << on.stats->maxHp << std::endl;
	});

	/** CM_QUIT back to the character list (the gate's re-entry is gameserver.character.reentry.time = 1 s later) */
	const auto quit = [&] {
		a.game->send(GameSession::CM_QUIT, GameSession::buildCM_QUIT(true));
		waitFor(*a.game, "SM_QUIT_RESPONSE", 30s);
	};

	// ---- L6: socket a godstone ----
	int32_t godstoneObject = 0;
	runCase("L6", "quit, seed 168000116, re-enter, unequip, CM_MANASTONE(4), equip (Y6)", [&] {
		// the seeded stone and the unequipped sword each take a slot, and the unequip needs a cube that is not full (L3b's rule, §8 risk 5)
		makeRoom(cubeLimit, 2 + SPARE_SLOTS, "L6's seeded godstone and sword");
		quit();
		godstoneObject = database.seedInventoryItem(schema, {a.warriorId, GODSTONE, 1, LOCATION_CUBE, 65535});
		std::this_thread::sleep_for(1500ms);
		enterWorld(false);
		levelReady(false);
		const std::optional<ModelItem> stone = model.byObjectId(godstoneObject);
		ASSERT_TRUE(stone && stone->itemId == GODSTONE) << "the seeded godstone row " << godstoneObject << " was not loaded: " << model.describe();
		const int32_t sword = swordObject();
		ASSERT_NE(sword, 0);
		const EquipWindow off = equipAction(GameSession::UNEQUIP, 0, sword);
		expectEquipShape(off, sword, false, "L6 unequip");

		const size_t from = a.game->recorded().size();
		const auto sentAt = std::chrono::steady_clock::now();
		a.game->send(GameSession::CM_MANASTONE,
		             GameSession::buildCM_MANASTONE(GameSession::MANASTONE_SOCKET_GODSTONE, 0, sword, godstoneObject, 0));
		collectFor(*a.game, 2500ms);
		const std::vector<Packet> window = slice(*a.game, from);
		model.sync();
		const EquipWindow on = equipAction(GameSession::EQUIP, MAIN_HAND, sword);
		expectEquipShape(on, sword, true, "L6 equip");
		deletedObjects.insert(godstoneObject);

		// Y6: SM_ITEM_USAGE_ANIMATION(time 2000) at once; >= 1,900 ms later the closing one, the stone's SM_DELETE_ITEM, the message and the sword's
		// update whose blob carries the godstone (ItemSocketService.socketGodstone, ItemSocketService.java:153-207)
		const std::string names = join(namesOf(window));
		std::optional<size_t> openAt, closeAt, deleteAt, messageAt, updateAt;
		std::optional<decoders::ItemUsageAnimation> open, close;
		std::optional<decoders::DeleteItem> deleted;
		std::optional<decoders::InventoryUpdateItem> update;
		for (size_t i = 0; i < window.size(); i++) {
			const Packet& packet = window[i];
			if (packet.name == "SM_ITEM_USAGE_ANIMATION") {
				const decoders::ItemUsageAnimation animation = decoders::decodeItemUsageAnimation(packet.data);
				if (animation.itemObjectId != godstoneObject)
					continue;
				if (animation.end == 0 && !open) {
					open = animation;
					openAt = i;
				} else if (animation.end == 1 && !close) {
					close = animation;
					closeAt = i;
				}
			} else if (packet.name == "SM_DELETE_ITEM" && !deleted) {
				const decoders::DeleteItem item = decoders::decodeDeleteItem(packet.data);
				if (item.objectId == godstoneObject) {
					deleted = item;
					deleteAt = i;
				}
			} else if (packet.name == "SM_SYSTEM_MESSAGE" && decoders::decodeSystemMessageId(packet.data) == STR_GIVE_ITEM_PROC_ENCHANTED_TARGET_ITEM) {
				messageAt = i;
			} else if (packet.name == "SM_INVENTORY_UPDATE_ITEM" && !update) {
				const decoders::InventoryUpdateItem item = decoders::decodeInventoryUpdateItem(packet.data);
				if (item.item.objectId == sword) {
					update = item;
					updateAt = i;
				}
			}
		}
		ASSERT_TRUE(open) << "Y6: no opening SM_ITEM_USAGE_ANIMATION of the stone: " << names;
		EXPECT_EQ(open->playerObjectId, a.warriorId);
		EXPECT_EQ(open->itemId, GODSTONE);
		EXPECT_EQ(open->time, 2000) << "Y6: the socketing's 2 s bar (ItemSocketService.java:191)";
		EXPECT_LE(millisBetween(sentAt, window[*openAt].receivedAt), 1000) << "Y6: the opening animation did not come at once";
		ASSERT_TRUE(close) << "Y6: no closing SM_ITEM_USAGE_ANIMATION (end 1) within 2.5 s: " << names;
		EXPECT_EQ(close->time, 0);
		const int64_t gap = millisBetween(window[*openAt].receivedAt, window[*closeAt].receivedAt);
		EXPECT_GE(gap, 1900) << "Y6: the task ran " << gap << " ms after the opening animation, and it is scheduled at 2000 ms";
		ASSERT_TRUE(deleted) << "Y6: the stone was not consumed (decreaseByObjectId): " << names;
		EXPECT_EQ(deleted->deleteTypeMask, decoders::ITEM_DELETE_USE) << "Y6: decreaseByObjectId's DEC_ITEM_USE deletes with ItemDeleteType.USE";
		EXPECT_TRUE(messageAt) << "Y6: no STR_GIVE_ITEM_PROC_ENCHANTED_TARGET_ITEM: " << names;
		ASSERT_TRUE(update) << "Y6: no SM_INVENTORY_UPDATE_ITEM of the sword (updateItemAfterInfoChange): " << names;
		ASSERT_TRUE(update->item.enchant) << "Y6: the sword's update has no ENCHANT_INFO entry";
		EXPECT_EQ(update->item.enchant->godStoneId, GODSTONE) << "Y6: the sword's blob does not carry the godstone (EnchantInfoBlobEntry.java:51)";
		EXPECT_TRUE(*closeAt < *deleteAt && *deleteAt < *updateAt && (!messageAt || (*deleteAt < *messageAt && *messageAt < *updateAt)))
		  << "Y6: the task's order is the closing animation, the delete, the message, the update: " << names;
		ASSERT_TRUE(on.appearance);
		EXPECT_TRUE(std::ranges::any_of(on.appearance->equipment.items, [](const decoders::EquippedItem& item) { return item.godStoneId == GODSTONE; }))
		  << "the re-equipped sword's appearance does not carry the godstone";
		std::cout << "Y6: the socketing took " << gap << " ms" << std::endl;
	});

	// ---- L3b + L6b + L6c: the proc, and the second corpse looted ----
	runCase("L6b-L6c", "make room, fight the respawned 210663 with the godstone until it dies, loot it empty (Y7, Y2, Y3)", [&] {
		ASSERT_TRUE(firstKill);
		OracleDrops budget = cubeBudget(LOOT_NPC_ID);
		// the corpse's worst case plus the stack L9's split creates
		makeRoom(cubeLimit, budget.worstCaseNewSlots + 1 + SPARE_SLOTS, "the second 210663 corpse and L9's split");
		budget = cubeBudget(LOOT_NPC_ID);
		EXPECT_TRUE(budget.deterministicMergeItems.contains(shardItem) && budget.deterministicMergeItems.contains(lootJunk))
		  << "Y3: the shard and the sparkie junk are the certain merges of L6c (§10.3 Y3)";
		const KillRecord kill = killAt(*lootMonster.nearestPlainSpot, LOOT_NPC_ID, firstKill->corpseId,
		                               std::chrono::seconds(lootMonster.respawnTime) + 60s);
		collectFor(*a.game, 1s); // the proc message of a killing proc follows the death (CreatureController.applyGodStoneEffect)

		// ---- Y7: one STR_SKILL_PROC_EFFECT_OCCURRED per evaluated hit, at least one proc that did damage ----
		const OracleItemInfo& godstone = itemInfo.at(GODSTONE);
		struct Swing {
			size_t index = 0;
			std::chrono::steady_clock::time_point at;
			int8_t status = 0;
		};
		std::vector<Swing> swings;
		std::vector<size_t> procStatuses, messages;
		std::optional<size_t> death;
		int32_t landed = 0, resisted = 0;
		std::vector<std::string> procValues;
		const std::vector<Packet>& packets = a.game->recorded();
		for (size_t i = kill.fightFrom; i < packets.size(); i++) {
			const Packet& packet = packets[i];
			try {
				if (packet.name == "SM_ATTACK") {
					const decoders::Attack attack = decoders::decodeAttack(packet.data);
					if (attack.attackerObjectId == a.warriorId && attack.targetObjectId == kill.corpseId)
						swings.push_back({i, packet.receivedAt, attack.results.front().attackStatusId});
				} else if (packet.name == "SM_ATTACK_STATUS") {
					const decoders::AttackStatusUpdate status = decoders::decodeAttackStatus(packet.data);
					if (status.creatureObjectId == kill.corpseId && status.skillId == godstone.godstoneSkillId) {
						procStatuses.push_back(i);
						procValues.push_back(std::to_string(status.value) + "/type " + std::to_string(status.type) + "/log " + std::to_string(status.logId));
						if (status.type == ATTACK_STATUS_TYPE_HP_OR_DAMAGE && status.logId == ATTACK_STATUS_LOG_PROCATKINSTANT && status.value < 0)
							landed++;
						else if (status.value == 0)
							resisted++;
					}
				} else if (packet.name == "SM_SYSTEM_MESSAGE") {
					if (decoders::decodeSystemMessageId(packet.data) == STR_SKILL_PROC_EFFECT_OCCURRED)
						messages.push_back(i);
				} else if (packet.name == "SM_EMOTION" && !death) {
					const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
					if (emotion.emotionType == decoders::EMOTION_DIE && emotion.senderObjectId == kill.corpseId)
						death = i;
				}
			} catch (const DecodeError& error) {
				ADD_FAILURE() << "Y7: " << packet.name << " at " << i << " does not decode: " << error.what();
			}
		}
		ASSERT_TRUE(death);
		// an evaluated hit (CreatureController.onAttack, CreatureController.java:250-256): an auto-attack whose first status is neither DODGE nor
		// RESIST, after which the monster still lives - a swing whose own damage killed is followed by the death with no proc status in between -
		// and that comes >= the evaluation cooldown after the previous evaluation (GodStone.tryActivate)
		int32_t evaluated = 0;
		std::optional<std::chrono::steady_clock::time_point> lastEvaluation;
		std::vector<std::string> swingLines;
		for (size_t s = 0; s < swings.size(); s++) {
			const Swing& swing = swings[s];
			const size_t next = s + 1 < swings.size() ? swings[s + 1].index : packets.size();
			bool killedByTheSwing = false;
			if (*death > swing.index && *death < next)
				killedByTheSwing = std::ranges::none_of(procStatuses, [&](size_t p) { return p > swing.index && p < *death; });
			bool counts = !isDodgeOrResist(swing.status) && !killedByTheSwing && *death > swing.index;
			if (counts && lastEvaluation && swing.at - *lastEvaluation < 750ms)
				counts = false;
			if (counts) {
				evaluated++;
				lastEvaluation = swing.at;
			}
			swingLines.push_back("status " + std::to_string(swing.status) + (counts ? " evaluated" : killedByTheSwing ? " killed" : " not evaluated"));
		}
		std::cout << "Y7: " << swings.size() << " swings (" << join(swingLines) << "), " << evaluated << " evaluated, " << messages.size()
		          << " proc messages, proc statuses " << join(procValues) << "; " << landed << " landed, " << resisted << " resisted" << std::endl;
		EXPECT_GE(evaluated, 1) << "Y7: no evaluated hit; the proc path was not exercised";
		EXPECT_EQ(static_cast<int32_t>(messages.size()), evaluated)
		  << "Y7: " << messages.size() << " STR_SKILL_PROC_EFFECT_OCCURRED for " << evaluated
		  << " evaluated hits - probability 1000 makes every evaluation a proc (GodStone.tryActivate, CreatureController.applyGodStoneEffect)";
		EXPECT_GE(landed, 1) << "Y7: no SM_ATTACK_STATUS of skill " << godstone.godstoneSkillId
		                     << " with the PROCATKINSTANT log and damage on the monster (ProcAtkInstantEffect.applyEffect): " << join(procValues);

		// ---- L6c: loot the second corpse empty ----
		lootCorpse(kill, lootDrops, budget.deterministicMergeItems);
		ASSERT_FALSE(loots.empty());
		EXPECT_TRUE(loots.back().emptied) << "L6c: the corpse was not looted empty";
	});

	// ---- L7: a potion ----
	runCase("L7", "quit, seed HP low, re-enter; the potion heals exactly, then its cooldown refuses (Y8)", [&] {
		const OracleItemInfo& potion = itemInfo.at(LIFE_POTION);
		quit();
		database.setLifeStatHp(schema, a.warriorId, POTION_CASE_HP);
		std::this_thread::sleep_for(1500ms);
		const std::vector<Packet> burst = enterWorld(false);
		const std::vector<Packet> statsPackets = ofName(burst, "SM_STATS_INFO");
		ASSERT_FALSE(statsPackets.empty());
		const decoders::StatsInfo stats = decoders::decodeStatsInfo(statsPackets.back().data);
		EXPECT_EQ(stats.currentHp, POTION_CASE_HP) << "the seeded HP was not restored";
		const std::optional<ModelItem> starter = model.byObjectId(lifePotionObject);
		ASSERT_TRUE(starter) << "the starter Minor Life Potion stack " << lifePotionObject << " is gone: " << model.describe();
		const ModelItem stack = *starter;
		const int64_t c = stack.count; // Y8 (rev 2): the count of L7's enter-world SM_INVENTORY_INFO
		levelReady(false);

		const size_t first = a.game->recorded().size();
		a.game->send(GameSession::CM_USE_ITEM, GameSession::buildCM_USE_ITEM(stack.objectId, 0));
		collectFor(*a.game, 1000ms);
		const size_t second = a.game->recorded().size();
		a.game->send(GameSession::CM_USE_ITEM, GameSession::buildCM_USE_ITEM(stack.objectId, 0));
		collectFor(*a.game, 1500ms);
		model.sync();
		const std::vector<Packet> use = slice(*a.game, first, second);
		const std::vector<Packet> refused = slice(*a.game, second);

		// the first use: the closing SM_ITEM_USAGE_ANIMATION (skill 9889 has no cast time, Skill.startCast/sendCastSpellEnd), the instant heal as
		// SM_ATTACK_STATUS(HP, skill 0, LOG.REGULAR) (AbstractHealEffect.applyEffect's ProcHealInstantEffect arm, CreatureLifeStats.increaseHp),
		// the stack's DEC_ITEM_USE (Skill.payCastCosts) and the heal-over-time's SM_ABNORMAL_STATE
		bool animated = false;
		for (const Packet& packet : ofName(use, "SM_ITEM_USAGE_ANIMATION")) {
			const decoders::ItemUsageAnimation animation = decoders::decodeItemUsageAnimation(packet.data);
			animated = animated || (animation.itemObjectId == stack.objectId && animation.itemId == LIFE_POTION && animation.end == 1);
		}
		EXPECT_TRUE(animated) << "Y8: no SM_ITEM_USAGE_ANIMATION of the potion: " << join(namesOf(use));
		std::optional<decoders::AttackStatusUpdate> heal;
		std::optional<size_t> healAt;
		for (size_t i = first; i < second; i++) {
			const Packet& packet = a.game->recorded()[i];
			if (packet.name != "SM_ATTACK_STATUS")
				continue;
			const decoders::AttackStatusUpdate status = decoders::decodeAttackStatus(packet.data);
			if (status.creatureObjectId == a.warriorId && status.skillId == 0 && status.logId == decoders::ATTACK_STATUS_LOG_REGULAR &&
			    status.type != decoders::ATTACK_STATUS_TYPE_NATURAL_HP && !heal) {
				heal = status;
				healAt = i;
			}
		}
		ASSERT_TRUE(heal) << "Y8: no SM_ATTACK_STATUS of the instant heal: " << join(namesOf(use));
		EXPECT_EQ(heal->type, ATTACK_STATUS_TYPE_HP_OR_DAMAGE) << "Y8: an item heal is TYPE.HP (7), a skill heal REGULAR (5) (AbstractHealEffect.java)";
		// the HP the heal found: the last absolute value before it plus the natural regeneration after that (the M5b-2 X8 reconstruction)
		int32_t hpFound = stats.currentHp;
		size_t baseIndex = 0;
		for (size_t i = 0; i < *healAt; i++)
			if (a.game->recorded()[i].name == "SM_STATUPDATE_HP") {
				hpFound = decoders::decodeStatUpdateHp(a.game->recorded()[i].data).currentHp;
				baseIndex = i;
			}
		for (size_t i = baseIndex + 1; i < *healAt; i++)
			if (a.game->recorded()[i].name == "SM_ATTACK_STATUS") {
				const decoders::AttackStatusUpdate status = decoders::decodeAttackStatus(a.game->recorded()[i].data);
				if (status.creatureObjectId == a.warriorId && status.type == decoders::ATTACK_STATUS_TYPE_NATURAL_HP)
					hpFound += status.value;
			}
		const int32_t instantHeal = potion.effects.front().second;
		// the review of stage 2 (m5b3-plan.md §18.4): L7 seeds 100 of the Warrior's 284 HP, so this row only ever checks the 37 arm of the min;
		// the maxHp - hp arm (calculateHealValue's cap, and CreatureLifeStats.increaseHp's clamp, which sends newHp - previousHp) is not reached -
		// the mutant RV2 (calculateHealValue without its cap) survived, equivalent on the wire as long as increaseHp clamps
		EXPECT_EQ(heal->value, std::min(instantHeal, stats.maxHp - hpFound))
		  << "Y8: the instant heal is min(" << instantHeal << ", maxHp - hp) (AbstractHealEffect.calculateHealValue), found " << hpFound << " of " << stats.maxHp;
		std::optional<decoders::InventoryUpdateItem> used;
		for (const Packet& packet : ofName(use, "SM_INVENTORY_UPDATE_ITEM")) {
			const decoders::InventoryUpdateItem update = decoders::decodeInventoryUpdateItem(packet.data);
			if (update.item.objectId == stack.objectId && !used)
				used = update;
		}
		ASSERT_TRUE(used) << "Y8: the potion was not consumed: " << join(namesOf(use));
		ASSERT_TRUE(used->item.general);
		EXPECT_EQ(used->item.general->count, c - 1) << "Y8: the stack was " << c << " at the enter world";
		EXPECT_EQ(used->updateTypeMask, std::optional<uint16_t>(decoders::ITEM_UPDATE_DEC_ITEM_USE));
		std::optional<decoders::AbnormalEntry> overTime;
		for (const Packet& packet : ofName(use, "SM_ABNORMAL_STATE"))
			for (const decoders::AbnormalEntry& entry : decoders::decodeAbnormalState(packet.data).effects)
				if (entry.skillId == potion.skillId && !overTime)
					overTime = entry;
		ASSERT_TRUE(overTime) << "Y8: no SM_ABNORMAL_STATE holds skill " << potion.skillId;
		const auto duration = potion.effectDuration2.find("HealEffect");
		ASSERT_NE(duration, potion.effectDuration2.end()) << "the oracle reports no HealEffect duration for the potion";
		EXPECT_LE(overTime->remainingTimeToDisplay, duration->second) << "Y8: the heal-over-time's lifetime is its getDuration2(), duration2 + 1000";
		EXPECT_GE(overTime->remainingTimeToDisplay, duration->second - 1000) << "Y8: the heal-over-time's remaining time right after the use";

		// the second use, 1 s later: refused by the item cooldown (PlayerRestrictions.canUseItem -> Player.hasCooldown), nothing consumed
		bool refusedMessage = false;
		for (const Packet& packet : ofName(refused, "SM_SYSTEM_MESSAGE"))
			refusedMessage = refusedMessage || decoders::decodeSystemMessageId(packet.data) == STR_ITEM_CANT_USE_UNTIL_DELAY_TIME;
		EXPECT_TRUE(refusedMessage) << "Y8: the second use was not refused with STR_ITEM_CANT_USE_UNTIL_DELAY_TIME: " << join(namesOf(refused));
		for (const Packet& packet : ofName(refused, "SM_INVENTORY_UPDATE_ITEM"))
			EXPECT_NE(decoders::decodeInventoryUpdateItem(packet.data).item.objectId, stack.objectId) << "Y8: the refused use consumed a potion";
		for (const Packet& packet : ofName(refused, "SM_ITEM_USAGE_ANIMATION"))
			EXPECT_NE(decoders::decodeItemUsageAnimation(packet.data).itemObjectId, stack.objectId) << "Y8: the refused use was animated";
		const std::optional<ModelItem> after = model.byObjectId(stack.objectId);
		ASSERT_TRUE(after);
		EXPECT_EQ(after->count, c - 1);
		std::cout << "Y8: the potion healed " << heal->value << " (found " << hpFound << " of " << stats.maxHp << "), the stack " << c << " -> "
		          << after->count << ", heal-over-time " << overTime->remainingTimeToDisplay << " ms" << std::endl;
	});

	// ---- L8: move ----
	int32_t warehouseJunk = 0;
	runCase("L8", "move: junk to the warehouse, the event potion refused, two moves inside the cube (Y9)", [&] {
		model.sync();
		const std::optional<ModelItem> junkStack = model.byObjectId(sparkieJunkObject);
		ASSERT_TRUE(junkStack && junkStack->location == LOCATION_CUBE) << "the sparkie junk stack of L3 (merged at L6c): " << model.describe();
		const std::vector<ModelItem> junk{*junkStack};
		warehouseJunk = junk.front().objectId;
		// (a) the junk to the regular warehouse: SM_DELETE_ITEM(MOVE) + SM_CUBE_UPDATE, SM_WAREHOUSE_ADD_ITEM + SM_CUBE_UPDATE (ItemMoveService.moveItem)
		size_t from = a.game->recorded().size();
		a.game->send(GameSession::CM_MOVE_ITEM, GameSession::buildCM_MOVE_ITEM(warehouseJunk, LOCATION_CUBE, LOCATION_WAREHOUSE, -1));
		readUntil(*a.game, [](const Packet& packet) { return packet.name == "SM_WAREHOUSE_ADD_ITEM"; }, 5s);
		collectFor(*a.game, 700ms);
		model.sync();
		std::vector<Packet> window = slice(*a.game, from);
		std::vector<std::string> shape;
		for (const Packet& packet : window) {
			if (packet.name == "SM_DELETE_ITEM") {
				const decoders::DeleteItem item = decoders::decodeDeleteItem(packet.data);
				shape.push_back("SM_DELETE_ITEM(" + std::string(item.objectId == warehouseJunk ? "junk" : std::to_string(item.objectId)) + ", " +
				                std::to_string(item.deleteTypeMask) + ")");
			} else if (packet.name == "SM_CUBE_UPDATE") {
				shape.push_back("SM_CUBE_UPDATE(" + std::to_string(decoders::decodeCubeUpdate(packet.data).actionValue) + ")");
			} else if (packet.name == "SM_WAREHOUSE_ADD_ITEM") {
				const decoders::WarehouseAddItem add = decoders::decodeWarehouseAddItem(packet.data);
				shape.push_back("SM_WAREHOUSE_ADD_ITEM(" + std::to_string(add.warehouseType) + ", " +
				                (add.items.size() == 1 && add.items[0].objectId == warehouseJunk ? std::string("junk") : std::string("?")) + ")");
			} else if (packet.name.find("ITEM") != std::string::npos) {
				shape.push_back(packet.name);
			}
		}
		const std::vector<std::string> expectedShape{"SM_DELETE_ITEM(junk, " + std::to_string(decoders::ITEM_DELETE_MOVE) + ")", "SM_CUBE_UPDATE(0)",
		                                             "SM_WAREHOUSE_ADD_ITEM(1, junk)", "SM_CUBE_UPDATE(1)"};
		EXPECT_EQ(shape, expectedShape) << "Y9: the junk's move to the warehouse: " << join(shape);
		const std::optional<ModelItem> moved = model.byObjectId(warehouseJunk);
		ASSERT_TRUE(moved);
		EXPECT_EQ(moved->location, LOCATION_WAREHOUSE);
		EXPECT_EQ(moved->count, junk.front().count);

		// (b) the event potion, not storable in a warehouse: STR_WAREHOUSE_CANT_DEPOSIT_ITEM and the unlock SM_INVENTORY_ADD_ITEM(ALL_SLOT), no delete
		const std::optional<ModelItem> eventStack = model.byObjectId(eventPotionObject);
		ASSERT_TRUE(eventStack);
		const std::vector<ModelItem> events{*eventStack};
		from = a.game->recorded().size();
		a.game->send(GameSession::CM_MOVE_ITEM, GameSession::buildCM_MOVE_ITEM(events.front().objectId, LOCATION_CUBE, LOCATION_WAREHOUSE, -1));
		collectFor(*a.game, 1500ms);
		model.sync();
		window = slice(*a.game, from);
		bool message = false;
		for (const Packet& packet : ofName(window, "SM_SYSTEM_MESSAGE"))
			message = message || decoders::decodeSystemMessageId(packet.data) == STR_WAREHOUSE_CANT_DEPOSIT_ITEM;
		EXPECT_TRUE(message) << "Y9: no STR_WAREHOUSE_CANT_DEPOSIT_ITEM (ItemRestrictionService.isItemRestrictedTo): " << join(namesOf(window));
		const std::vector<Packet> unlocks = ofName(window, "SM_INVENTORY_ADD_ITEM");
		EXPECT_EQ(unlocks.size(), 1u) << "Y9: the refused move's unlock packet (ItemPacketService.sendItemUnlockPacket): " << join(namesOf(window));
		if (!unlocks.empty()) {
			const decoders::InventoryAddItem unlock = decoders::decodeInventoryAddItem(unlocks[0].data);
			EXPECT_EQ(unlock.addTypeMask, decoders::ITEM_ADD_ALL_SLOT);
			EXPECT_TRUE(unlock.items.size() == 1 && unlock.items[0].objectId == events.front().objectId);
		}
		EXPECT_TRUE(ofName(window, "SM_DELETE_ITEM").empty()) << "Y9: the refused move deleted the event potion";
		EXPECT_TRUE(ofName(window, "SM_WAREHOUSE_ADD_ITEM").empty()) << "Y9: the event potion reached the warehouse";
		const std::optional<ModelItem> event = model.byObjectId(events.front().objectId);
		EXPECT_TRUE(event && event->location == LOCATION_CUBE && event->count == events.front().count);

		// (c) two moves inside the cube: moveInSameStorage sends nothing at all (ItemMoveService.java:37-41, 82-86); Y12 reads the slots
		const std::optional<ModelItem> lifeStack = model.byObjectId(lifePotionObject);
		const std::optional<ModelItem> manaStack = model.byObjectId(manaPotionObject);
		ASSERT_TRUE(lifeStack && manaStack) << model.describe();
		const std::vector<ModelItem> life{*lifeStack};
		const std::vector<ModelItem> mana{*manaStack};
		from = a.game->recorded().size();
		a.game->send(GameSession::CM_MOVE_ITEM, GameSession::buildCM_MOVE_ITEM(life.front().objectId, LOCATION_CUBE, LOCATION_CUBE, LIFE_POTION_SLOT));
		a.game->send(GameSession::CM_MOVE_ITEM, GameSession::buildCM_MOVE_ITEM(mana.front().objectId, LOCATION_CUBE, LOCATION_CUBE, MANA_POTION_SLOT));
		collectFor(*a.game, 1500ms);
		window = slice(*a.game, from);
		// "no packet at all" of the move: no item or storage packet and no message. The character's own HP ticks of L7's heal-over-time
		// (HealEffect, 37 every 2 s for 21 s) and its regeneration keep arriving meanwhile; they are no answer to the move
		const std::set<std::string_view> itemPackets{"SM_INVENTORY_ADD_ITEM", "SM_INVENTORY_UPDATE_ITEM", "SM_DELETE_ITEM", "SM_CUBE_UPDATE",
		                                             "SM_WAREHOUSE_ADD_ITEM", "SM_WAREHOUSE_UPDATE_ITEM", "SM_DELETE_WAREHOUSE_ITEM", "SM_SYSTEM_MESSAGE"};
		std::vector<std::string> unexpected;
		for (const Packet& packet : window)
			if (itemPackets.contains(packet.name))
				unexpected.push_back(packet.name);
		EXPECT_TRUE(unexpected.empty()) << "Y9: a move inside the cube sent " << join(unexpected);
		std::cout << "Y9: " << join(shape) << std::endl;
	});

	// ---- L9: split ----
	int32_t splitObject = 0;
	runCase("L9", "split the life potions (Y10)", [&] {
		model.sync();
		const std::optional<ModelItem> lifeStack = model.byObjectId(lifePotionObject);
		ASSERT_TRUE(lifeStack);
		const std::vector<ModelItem> life{*lifeStack};
		const int64_t before = life.front().count; // Y10: c', the count of the last packet about the stack (Y8's)
		const size_t from = a.game->recorded().size();
		a.game->send(GameSession::CM_SPLIT_ITEM,
		             GameSession::buildCM_SPLIT_ITEM(life.front().objectId, SPLIT_COUNT, LOCATION_CUBE, 0, LOCATION_CUBE, SPLIT_SLOT));
		readUntil(*a.game, [](const Packet& packet) { return packet.name == "SM_INVENTORY_ADD_ITEM"; }, 5s);
		collectFor(*a.game, 700ms);
		model.sync();
		const std::vector<Packet> window = slice(*a.game, from);
		std::vector<std::string> shape;
		for (const Packet& packet : window) {
			if (packet.name == "SM_INVENTORY_UPDATE_ITEM") {
				const decoders::InventoryUpdateItem update = decoders::decodeInventoryUpdateItem(packet.data);
				shape.push_back("SM_INVENTORY_UPDATE_ITEM(" + std::string(update.item.objectId == life.front().objectId ? "stack" : "?") + ", " +
				                std::to_string(update.item.general ? update.item.general->count : -1) + ", " +
				                std::to_string(update.updateTypeMask.value_or(0xFFFF)) + ")");
			} else if (packet.name == "SM_CUBE_UPDATE") {
				shape.push_back("SM_CUBE_UPDATE(" + std::to_string(decoders::decodeCubeUpdate(packet.data).actionValue) + ")");
			} else if (packet.name == "SM_INVENTORY_ADD_ITEM") {
				const decoders::InventoryAddItem add = decoders::decodeInventoryAddItem(packet.data);
				if (add.items.size() == 1 && add.items[0].objectId != life.front().objectId && add.items[0].templateId == LIFE_POTION) {
					splitObject = add.items[0].objectId;
					shape.push_back("SM_INVENTORY_ADD_ITEM(new, " + std::to_string(add.items[0].general ? add.items[0].general->count : -1) + ")");
				} else {
					shape.push_back("SM_INVENTORY_ADD_ITEM(?)");
				}
			} else if (packet.name.find("ITEM") != std::string::npos) {
				shape.push_back(packet.name);
			}
		}
		// ItemSplitService.splitItem, the same-storage arm: the source decreased with DEC_ITEM_SPLIT, its cube update, the new stack and its
		// cube update (ItemSplitService.java:85-88, Storage.add)
		const std::vector<std::string> expectedShape{
		  "SM_INVENTORY_UPDATE_ITEM(stack, " + std::to_string(before - SPLIT_COUNT) + ", " + std::to_string(decoders::ITEM_UPDATE_DEC_ITEM_SPLIT) + ")",
		  "SM_CUBE_UPDATE(0)", "SM_INVENTORY_ADD_ITEM(new, " + std::to_string(SPLIT_COUNT) + ")", "SM_CUBE_UPDATE(0)"};
		EXPECT_EQ(shape, expectedShape) << "Y10: the split of " << SPLIT_COUNT << " from " << before << ": " << join(shape);
		std::cout << "Y10: " << join(shape) << std::endl;
	});

	// ---- L10: destroy ----
	runCase("L10", "destroy the kerub junk (Y11)", [&] {
		model.sync();
		const std::optional<ModelItem> junk = model.byObjectId(kerubJunkObject);
		ASSERT_TRUE(junk) << "the kerub junk stack of L4: " << model.describe();
		deleteStack(*junk, "L10");
		deletedObjects.insert(junk->objectId);
	});

	// ---- L11: swap ----
	runCase("L11", "swap the mana potions in the cube with the junk in the warehouse (Y16)", [&] {
		model.sync();
		const std::optional<ModelItem> manaStack = model.byObjectId(manaPotionObject);
		ASSERT_TRUE(manaStack && manaStack->location == LOCATION_CUBE);
		const std::vector<ModelItem> mana{*manaStack};
		const std::optional<ModelItem> junk = model.byObjectId(warehouseJunk);
		ASSERT_TRUE(junk && junk->location == LOCATION_WAREHOUSE);
		const size_t from = a.game->recorded().size();
		a.game->send(GameSession::CM_REPLACE_ITEM, GameSession::buildCM_REPLACE_ITEM(LOCATION_CUBE, mana.front().objectId, LOCATION_WAREHOUSE, warehouseJunk));
		readUntil(*a.game, [](const Packet& packet) { return packet.name == "SM_WAREHOUSE_ADD_ITEM"; }, 5s);
		collectFor(*a.game, 700ms);
		model.sync();
		const std::vector<Packet> window = slice(*a.game, from);
		std::vector<std::string> shape;
		std::optional<uint16_t> junkSlot, manaSlot;
		for (const Packet& packet : window) {
			const auto who = [&](int32_t objectId) {
				return objectId == warehouseJunk ? std::string("junk") : objectId == mana.front().objectId ? std::string("mana") : std::to_string(objectId);
			};
			if (packet.name == "SM_DELETE_ITEM") {
				const decoders::DeleteItem item = decoders::decodeDeleteItem(packet.data);
				shape.push_back("SM_DELETE_ITEM(" + who(item.objectId) + ", " + std::to_string(item.deleteTypeMask) + ")");
			} else if (packet.name == "SM_DELETE_WAREHOUSE_ITEM") {
				const decoders::DeleteWarehouseItem item = decoders::decodeDeleteWarehouseItem(packet.data);
				shape.push_back("SM_DELETE_WAREHOUSE_ITEM(" + std::to_string(item.warehouseType) + ", " + who(item.objectId) + ", " +
				                std::to_string(item.deleteTypeMask) + ")");
			} else if (packet.name == "SM_CUBE_UPDATE") {
				shape.push_back("SM_CUBE_UPDATE(" + std::to_string(decoders::decodeCubeUpdate(packet.data).actionValue) + ")");
			} else if (packet.name == "SM_INVENTORY_ADD_ITEM") {
				const decoders::InventoryAddItem add = decoders::decodeInventoryAddItem(packet.data);
				for (const decoders::InventoryItem& item : add.items) {
					shape.push_back("SM_INVENTORY_ADD_ITEM(" + who(item.objectId) + ")");
					if (item.objectId == warehouseJunk)
						junkSlot = item.equipmentSlot;
				}
			} else if (packet.name == "SM_WAREHOUSE_ADD_ITEM") {
				const decoders::WarehouseAddItem add = decoders::decodeWarehouseAddItem(packet.data);
				for (const decoders::InventoryItem& item : add.items) {
					shape.push_back("SM_WAREHOUSE_ADD_ITEM(" + std::to_string(add.warehouseType) + ", " + who(item.objectId) + ")");
					if (item.objectId == mana.front().objectId)
						manaSlot = item.equipmentSlot;
				}
			} else if (packet.name.find("ITEM") != std::string::npos) {
				shape.push_back(packet.name);
			}
		}
		// "correct UI update order is 1) delete items 2) add items" (ItemMoveService.switchItemsInStorages, ItemMoveService.java:115-125)
		const std::string move = std::to_string(decoders::ITEM_DELETE_MOVE);
		const std::vector<std::string> expectedShape{"SM_DELETE_ITEM(mana, " + move + ")",
		                                             "SM_CUBE_UPDATE(0)",
		                                             "SM_DELETE_WAREHOUSE_ITEM(1, junk, " + move + ")",
		                                             "SM_CUBE_UPDATE(1)",
		                                             "SM_INVENTORY_ADD_ITEM(junk)",
		                                             "SM_CUBE_UPDATE(0)",
		                                             "SM_WAREHOUSE_ADD_ITEM(1, mana)",
		                                             "SM_CUBE_UPDATE(1)"};
		EXPECT_EQ(shape, expectedShape) << "Y16: " << join(shape);
		// each added item carries the other's former slot: the mana potions were moved to MANA_POTION_SLOT at L8, the junk entered the warehouse
		// with slot -1 (moveItem's setEquipmentSlot(slot)), which the packets write as its low 16 bits
		EXPECT_EQ(junkSlot, std::optional<uint16_t>(static_cast<uint16_t>(MANA_POTION_SLOT))) << "Y16: the junk did not take the mana potions' slot";
		EXPECT_EQ(manaSlot, std::optional<uint16_t>(0xFFFF)) << "Y16: the mana potions did not take the junk's slot";
		std::cout << "Y16: " << join(shape) << std::endl;
	});

	// ---- L12: persistence ----
	runCase("L12", "quit, read inventory and item_stones, re-enter (Y12)", [&] {
		model.sync();
		const std::map<int32_t, ModelItem> expected = model.items;
		const int32_t sword = swordObject();
		quit();
		const std::vector<std::vector<std::optional<std::string>>> rows = database.queryRows(
		  schema, "SELECT item_unique_id, item_id, item_count, item_location, slot, is_equipped FROM inventory WHERE item_owner = " + std::to_string(a.warriorId),
		  6);
		std::map<int32_t, std::vector<std::optional<std::string>>> byObject;
		for (const auto& row : rows)
			byObject[std::stoi(row[0].value_or("0"))] = row;
		std::vector<std::string> problems;
		for (const auto& [objectId, item] : expected) {
			const auto found = byObject.find(objectId);
			if (found == byObject.end()) {
				problems.push_back("no row for " + std::to_string(objectId) + " (" + std::to_string(item.itemId) + " x" + std::to_string(item.count) + ")");
				continue;
			}
			const auto& row = found->second;
			const auto check = [&](std::string_view column, const std::optional<std::string>& value, const std::string& wanted) {
				if (value.value_or("NULL") != wanted)
					problems.push_back(std::to_string(objectId) + " (" + std::to_string(item.itemId) + ") " + std::string(column) + " " + value.value_or("NULL") +
					                   ", the client was told " + wanted);
			};
			check("item_id", row[1], std::to_string(item.itemId));
			check("item_count", row[2], std::to_string(item.count));
			check("item_location", row[3], std::to_string(item.location));
			check("is_equipped", row[5], item.equippedSlot != 0 ? "1" : "0");
			if (item.equippedSlot != 0)
				check("slot", row[4], std::to_string(item.equippedSlot));
		}
		for (const auto& [objectId, row] : byObject)
			if (!expected.contains(objectId))
				problems.push_back("row " + std::to_string(objectId) + " (" + row[1].value_or("NULL") + " x" + row[2].value_or("NULL") + " at " +
				                   row[3].value_or("NULL") + "), which the client was told is gone" +
				                   (deletedObjects.contains(objectId) ? " (deleted in this run)" : ""));
		// the slots the script set: the same-storage move of the life potions (L8), the split (L9) and the swap's exchange (L11)
		const auto slotOf = [&](int32_t objectId) -> std::string {
			const auto found = byObject.find(objectId);
			return found == byObject.end() ? std::string("no row") : found->second[4].value_or("NULL");
		};
		EXPECT_EQ(slotOf(lifePotionObject), std::to_string(LIFE_POTION_SLOT)) << "Y12: the slot of the life potions' cube move (L8)";
		EXPECT_EQ(slotOf(splitObject), std::to_string(SPLIT_SLOT)) << "Y12: the slot of the split stack (L9)";
		EXPECT_EQ(slotOf(warehouseJunk), std::to_string(MANA_POTION_SLOT)) << "Y12: the junk took the mana potions' slot (L11)";
		EXPECT_EQ(slotOf(manaPotionObject), "-1") << "Y12: the mana potions took the junk's warehouse slot (L11)";
		const std::optional<ModelItem> manaInWarehouse = model.byObjectId(manaPotionObject);
		EXPECT_TRUE(manaInWarehouse && manaInWarehouse->location == LOCATION_WAREHOUSE) << "the model has the mana potions in the warehouse";
		EXPECT_TRUE(problems.empty()) << "Y12: `inventory` against the last packets the client got:\n  " << join(problems, "\n  ");
		// the socketed godstone: one item_stones row of category GODSTONE on the sword (ItemStoneListDAO)
		const std::vector<std::vector<std::optional<std::string>>> stones =
		  database.queryRows(schema, "SELECT item_id, slot, category FROM item_stones WHERE item_unique_id = " + std::to_string(sword), 3);
		ASSERT_EQ(stones.size(), 1u) << "Y12: the sword has " << stones.size() << " item_stones rows";
		EXPECT_EQ(stones[0][0].value_or(""), std::to_string(GODSTONE));
		EXPECT_EQ(stones[0][2].value_or(""), std::to_string(ITEM_STONE_CATEGORY_GODSTONE));

		// the re-entry lists them all again, and the sword's blob still carries the godstone
		std::this_thread::sleep_for(1500ms);
		enterWorld(false);
		levelReady(false);
		std::vector<std::string> reloaded;
		for (const auto& [objectId, item] : expected) {
			const std::optional<ModelItem> now = model.byObjectId(objectId);
			if (!now || now->itemId != item.itemId || now->count != item.count || now->location != item.location ||
			    (now->equippedSlot != 0) != (item.equippedSlot != 0))
				reloaded.push_back(std::to_string(objectId) + " (" + std::to_string(item.itemId) + " x" + std::to_string(item.count) + " at " +
				                   std::to_string(item.location) + ")");
		}
		EXPECT_TRUE(reloaded.empty()) << "Y12: after the re-entry SM_INVENTORY_INFO/SM_WAREHOUSE_INFO do not list " << join(reloaded);
		EXPECT_EQ(model.items.size(), expected.size()) << "Y12: the re-entry lists " << model.items.size() << " items, " << expected.size() << " before the quit";
		const std::optional<ModelItem> reloadedSword = model.byObjectId(sword);
		ASSERT_TRUE(reloadedSword);
		EXPECT_EQ(reloadedSword->godStoneId, GODSTONE) << "Y12: the sword's SM_INVENTORY_INFO blob lost the godstone over the relog";
		std::cout << "Y12: " << rows.size() << " inventory rows, " << stones.size() << " item_stones row" << std::endl;
	});

	// ---- the camp fire (geo only, §10.5, Y15) ----
	if (variant.geodata)
		runCase("L14", "beside the camp fire's mesh no tick; on it a tick every 5 s; off it none (Y15)", [&] {
			const OracleCampFire fire = parseCampFire(oracle->run({"m5b3-material", "--map", std::to_string(ELYOS_START_MAP), "--near",
			                                                       std::to_string(creation.x) + "," + std::to_string(creation.y) + "," + std::to_string(creation.z),
			                                                       "--limit", "3", "--stand"}));
			EXPECT_EQ(fire.touchedZones, std::vector<std::string>{fire.zoneName}) << "the oracle's point touches more than the fire";
			EXPECT_EQ(fire.missInside, std::vector<std::string>{fire.zoneName}) << "the oracle's untouched point is not inside the fire's zone alone";
			EXPECT_TRUE(fire.missTouched.empty()) << "the oracle's untouched point touches " << join(fire.missTouched);
			std::cout << "Y15: the fire " << fire.zoneName << " (material " << fire.materialId << ", skill " << fire.skillId << " every " << fire.frequency
			          << " s), " << fire.distanceFromSpawn << " m from the spawn; standing at (" << fire.x << ", " << fire.y << ", " << fire.z << "), clearance "
			          << fire.clearance << " m, inside " << join(fire.insideZones) << "; step off at (" << fire.offX << ", " << fire.offY << ", " << fire.offZ
			          << "); untouched at (" << fire.missX << ", " << fire.missY << ", " << fire.missZ << "), " << fire.missHeight << " m above the floor, margin "
			          << fire.missMargin << " m; nearby " << join(fire.nearbyZones) << std::endl;
			walkTo(fire.offX, fire.offY, fire.offZ);
			const size_t before = a.game->recorded().size();
			collectFor(*a.game, 3s);
			// the negative case of the TOUCH ray (m5b3-plan.md §18.2, the review's mutant RV1): the oracle's untouched point is inside the fire's
			// SEMISPHERE (above its center, the client reporting a z above the floor) and no other zone, and its ray - from z + 1.8 down to the floor
			// - passes 0.25 m or more beside the fire's mesh. Entering the zone creates the fire's actor and runs its check at once
			// (MaterialZoneHandler.onEnterZone -> actor.moved()); a server that asks the ray never touches it, one that does not starts the task, whose
			// first run uses the skill at once. 7 s there, then back to the step-off point, which leaves the zone (onLeaveZone), before the fire.
			const size_t missFrom = a.game->recorded().size();
			a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(fire.offX, fire.offY, fire.offZ, 0, static_cast<int8_t>(0xE0), fire.missX, fire.missY, fire.missZ));
			a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(fire.missX, fire.missY, fire.missZ, 0, 0));
			collectFor(*a.game, 7s);
			a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(fire.missX, fire.missY, fire.missZ, 0, static_cast<int8_t>(0xE0), fire.offX, fire.offY, fire.offZ));
			a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(fire.offX, fire.offY, fire.offZ, 0, 0));
			collectFor(*a.game, 2s);
			const size_t on = a.game->recorded().size();
			a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(fire.offX, fire.offY, fire.offZ, 0, static_cast<int8_t>(0xE0), fire.x, fire.y, fire.z));
			a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(fire.x, fire.y, fire.z, 0, 0));
			atX = fire.x;
			atY = fire.y;
			atZ = fire.z;
			collectFor(*a.game, 12s);
			const size_t off = a.game->recorded().size();
			const auto offSentAt = std::chrono::steady_clock::now();
			a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(fire.x, fire.y, fire.z, 0, static_cast<int8_t>(0xE0), fire.offX, fire.offY, fire.offZ));
			a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(fire.offX, fire.offY, fire.offZ, 0, 0));
			atX = fire.offX;
			atY = fire.offY;
			atZ = fire.offZ;
			collectFor(*a.game, 7s);
			struct Tick {
				size_t index;
				std::chrono::steady_clock::time_point at;
				int32_t value;
			};
			std::vector<Tick> ticks;
			std::vector<std::string> other;
			for (size_t i = before; i < a.game->recorded().size(); i++) {
				const Packet& packet = a.game->recorded()[i];
				if (packet.name != "SM_ATTACK_STATUS")
					continue;
				const decoders::AttackStatusUpdate status = decoders::decodeAttackStatus(packet.data);
				if (status.creatureObjectId != a.warriorId || status.skillId != fire.skillId)
					continue;
				if (status.type == ATTACK_STATUS_TYPE_HP_OR_DAMAGE && status.logId == ATTACK_STATUS_LOG_PROCATKINSTANT && status.value < 0)
					ticks.push_back({i, packet.receivedAt, status.value});
				else
					other.push_back("type " + std::to_string(status.type) + " log " + std::to_string(status.logId) + " value " + std::to_string(status.value));
			}
			std::vector<std::string> lines;
			for (const Tick& tick : ticks)
				lines.push_back(std::to_string(-tick.value) + " at " + std::to_string(millisBetween(a.game->recorded()[on].receivedAt, tick.at)) + " ms");
			std::cout << "Y15: " << ticks.size() << " ticks (" << join(lines) << ")" << std::endl;
			EXPECT_TRUE(other.empty()) << "Y15: statuses of skill " << fire.skillId << " that are no damage: " << join(other);
			std::vector<Tick> standing, before2, untouched, after;
			for (const Tick& tick : ticks) {
				if (tick.index < missFrom)
					before2.push_back(tick);
				else if (tick.index < on)
					untouched.push_back(tick);
				else if (tick.index < off || tick.at < offSentAt + 500ms)
					standing.push_back(tick);
				else
					after.push_back(tick);
			}
			EXPECT_TRUE(before2.empty()) << "Y15: the step-off point, inside no zone, burned";
			EXPECT_TRUE(untouched.empty()) << "Y15: " << untouched.size() << " tick(s) at the untouched point, inside the fire's zone alone with every "
			                               << "TOUCH ray missing its mesh: the actor acted without the ray hitting (ZoneCollisionMaterialActor.onMoved: "
			                               << "isTouched = collisionResults.size() > 0; AbstractCollisionObserver.java:47-64)";
			EXPECT_GE(standing.size(), 2u) << "Y15: " << standing.size() << " damage ticks of skill " << fire.skillId << " in 12 s on the fire "
			                               << "(ZoneCollisionMaterialActor's TOUCH check, MaterialSkillTask)";
			for (size_t i = 1; i < standing.size(); i++) {
				const int64_t gap = millisBetween(standing[i - 1].at, standing[i].at);
				EXPECT_NEAR(static_cast<double>(gap), fire.frequency * 1000.0, 1000.0)
				  << "Y15: ticks " << i - 1 << " and " << i << " are " << gap << " ms apart; MaterialSkillTask runs every second and uses the skill every "
				  << fire.frequency << "th (AbstractMaterialSkillActor.java, `secondsElapsed++ % frequency`)";
			}
			// two paths stop the task and either suffices: the step-off move's TOUCH check untouches (ZoneCollisionMaterialActor.onMoved ->
			// abort, and MaterialSkillTask returns on !isTouched) and the zone's onLeaveZone aborts - one mutant of either survives this row, a
			// port that breaks both does not (the mutation runs of 2026-09-24, m5b3-plan.md §17)
			EXPECT_TRUE(after.empty()) << "Y15: " << after.size() << " tick(s) after stepping off (ZoneCollisionMaterialActor.onMoved's untouch and "
			                           << "MaterialZoneHandler.onLeaveZone both abort the task)";
		});

	// ---- L13: the reports and the shutdown (Y1, Y13, Y14) ----
	std::optional<int32_t> gameServerExit;
	int64_t connectionsAtShutdown = 0;
	if (a.game && !a.game->client.socket.isClosed())
		connectionsAtShutdown = 1;
	gameServerExit = servers.stopGameServer(); // with the Warrior online, as the M5a gate's case 7 does
	if (a.game)
		a.game->waitClosed(60s);
	const std::optional<int32_t> loginServerExit = servers.stopLoginServer();

	cases.run("L13", "reports: the Q8 bar and the allow-list, the drop classes, one LOOT_ENABLE per kill (Y1, Y13, Y14)", [&] {
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
		  << "Y13: the game server wrote no check output in " << servers.checkOutputDir();

		// ---- Y13: the M5a Q8 bar ----
		EXPECT_TRUE(servers.readReportLines("unported_trace.txt").empty())
		  << "Y13: AION_UNPORTED sites were reached on the loot and item path:\n" << join(servers.readReportLines("unported_trace.txt"), "\n");
		const std::vector<AllowlistEntry> allowlist = readAllowlist();
		ASSERT_FALSE(allowlist.empty()) << "Y13: tests/scenario/m5b3_partial_allowlist.txt is empty or missing";
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
			EXPECT_TRUE(allowed) << "Y13: the AION_PARTIAL site " << hit.site << " is not in tests/scenario/m5b3_partial_allowlist.txt (" << hit.line << ")";
			EXPECT_EQ(hit.site.find("DropRegistrationService"), std::string::npos) << "Y13: a DropRegistrationService partial was hit: " << hit.line;
		}
		for (const AllowlistEntry& entry : allowlist) {
			if (entry.section == AllowlistSection::HitAtLeastOnce)
				EXPECT_GT(hitsByEntry[entry.site], 0) << "Y13: the section A row " << entry.site << " was never hit";
			else if (entry.section == AllowlistSection::HitNever)
				EXPECT_EQ(hitsByEntry[entry.site], 0) << "Y13: the section B row " << entry.site << " was hit " << hitsByEntry[entry.site] << " times";
		}
		std::cout << "Y13: AION_PARTIAL hits by allow-list row (hits, section, site):\n";
		for (const AllowlistEntry& entry : allowlist)
			std::cout << "  " << hitsByEntry[entry.site] << "\t" << sectionName(entry.section) << "\t" << entry.site << "\n";
		std::cout << std::flush;

		const std::vector<std::string> census = servers.readReportLines("census.txt");
		EXPECT_TRUE(census.empty()) << "Y13: the final census reports leaks:\n" << join(census, "\n");
		EXPECT_TRUE(servers.readReportLines("lockdep.txt").empty()) << "Y13: the lock order validator reported:\n"
		                                                            << join(servers.readReportLines("lockdep.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("watchdog.txt").empty()) << "Y13: the watchdog dumped:\n" << join(servers.readReportLines("watchdog.txt"), "\n");
		const std::map<std::string, std::vector<std::string>> summary = servers.readSummary();
		const auto value = [&](std::string_view key) -> std::string {
			const auto found = summary.find(std::string(key));
			return found == summary.end() || found->second.empty() ? std::string() : found->second[0];
		};
		EXPECT_EQ(value("started"), "true");
		EXPECT_EQ(value("exitCode"), "0");
		EXPECT_EQ(value("knownListNotifyFailures"), "0");
		EXPECT_EQ(value("liveCountsEnabled"), "true") << "Y14: a release build counts nothing: build it checked";
		EXPECT_EQ(value("zombieCuts"), "0");
		const auto notPorted = summary.find("notPortedClientPacket");
		if (notPorted != summary.end())
			ADD_FAILURE() << "Y13: the scripted path sent client packets that are not ported: " << join(notPorted->second);
		std::vector<std::string> errors;
		ASSERT_NE(servers.gameServer(), nullptr);
		for (const std::string& line : servers.gameServer()->findLogLines(" ERROR "))
			errors.push_back("game server: " + line);
		if (servers.loginServer() != nullptr)
			for (const std::string& line : servers.loginServer()->findLogLines(" ERROR "))
				errors.push_back("login server: " + line);
		EXPECT_TRUE(errors.empty()) << "Y13: ERROR lines in the server logs (NpcController::onDie and CreatureController::useSkill swallow into ERROR "
		                               "lines):\n"
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
			EXPECT_TRUE(fileErrors.empty()) << "Y13: " << errorLog << " is not empty:\n" << join(fileErrors, "\n");
		} else {
			ADD_FAILURE() << "Y13: the game server wrote no " << errorLog;
		}
		EXPECT_TRUE(servers.gameServer()->findLogLines("did not leave world cleanly", 5).empty());
		EXPECT_TRUE(servers.gameServer()->findLogLines("stale pin", 5).empty());
		EXPECT_TRUE(servers.gameServer()->findLogLines("objects removed from the world are still alive", 5).empty())
		  << "Y13: " << join(servers.gameServer()->findLogLines("objects removed from the world are still alive", 5), "\n");

		// ---- Y14: the drop classes and what the world still holds ----
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
		const std::optional<LiveCount> dropNpc = liveCount("model::gameobjects::DropNpc");
		ASSERT_TRUE(dropNpc) << "Y14: m5a_summary.txt has no liveCount row for DropNpc (CheckOutput G-06)";
		EXPECT_EQ(dropNpc->created, static_cast<int64_t>(kills.size())) << "Y14: " << dropNpc->line << " against " << kills.size() << " kills";
		EXPECT_EQ(kills.size(), 3u) << "Y14: two 210663 and one 210133 (§10.3)";
		EXPECT_EQ(dropNpc->live, 0) << "Y14: " << dropNpc->line << " - every corpse was looted empty and deleted (unregisterDrop)";
		EXPECT_EQ(value("dropNpcsHeld"), "0") << "Y14: DropRegistrationService still held a DropNpc at the stop";
		EXPECT_EQ(value("dropItemsHeld"), "0") << "Y14: DropRegistrationService still held drop items at the stop";
		int64_t listed = 0;
		for (const LootRecord& loot : loots)
			listed += static_cast<int64_t>(loot.entries.size());
		for (const std::string_view name : {"model::drop::DropItem", "RuntimeDropItem"}) {
			const std::optional<LiveCount> dropItem = liveCount(name);
			if (!dropItem) {
				ADD_FAILURE() << "Y14: m5a_summary.txt has no liveCount row for " << name << " (CheckOutput G-06)";
				continue;
			}
			EXPECT_EQ(dropItem->live, 0) << "Y14: " << dropItem->line << " - a drop kept past its corpse";
			// the run-time drops are RuntimeDropItem (DropItem.cpp); the static-data DropItem row counts the custom drops of NpcDrop, none here
			// (CheckOutput.cpp RUNTIME_DROP_ITEM_CLASS)
			if (name == "RuntimeDropItem")
				EXPECT_EQ(dropItem->created, listed) << "Y14: " << dropItem->line << " against the " << listed << " entries the three corpses listed";
		}
		// the skill classes against their G-07 relations (M5b-2's X13): the gate's procs, potion and fire ended and are not retained
		const std::optional<LiveCount> effect = liveCount("skillengine::model::Effect");
		const std::optional<int64_t> effectsHeld = held("effectsHeld");
		ASSERT_TRUE(effect && effectsHeld) << "Y14: m5a_summary.txt lacks the Effect row or effectsHeld";
		EXPECT_EQ(effect->live, *effectsHeld) << "Y14: " << effect->line << " against " << *effectsHeld << " Effects the creatures of the world hold";
		const std::optional<LiveCount> skill = liveCount("skillengine::model::Skill");
		const std::optional<int64_t> skillsHeld = held("skillsHeld");
		ASSERT_TRUE(skill && skillsHeld);
		EXPECT_EQ(skill->live, *skillsHeld) << "Y14: " << skill->line << " against " << *skillsHeld << " Skills held";
		const std::set<std::string> perConnection = {"Account", "AccountTime", "ConnectionAliveChecker"};
		const std::set<std::string> perCharacter = {"PlayerAccountData", "PlayerCommonData", "PlayerAppearance"};
		std::vector<std::string> itemRows;
		for (const auto& [name, count] : readLiveCounts(servers, "live_counts.txt")) {
			if (perConnection.contains(name) || name.ends_with("Storage"))
				EXPECT_LE(count.live, connectionsAtShutdown) << "Y14: live instances left: " << count.line;
			if (perCharacter.contains(name))
				EXPECT_LE(count.live, connectionsAtShutdown) << "Y14: live instances left: " << count.line;
			if (name == "Player" || name == "AttackResult")
				EXPECT_EQ(count.live, 0) << "Y14: live instances left: " << count.line;
			if (name == "Item" || name == "ItemStone" || name == "GodStone")
				itemRows.push_back(count.line);
		}
		std::cout << "Y14: " << dropNpc->line << "; " << effect->line << " / effectsHeld " << *effectsHeld << "; " << join(itemRows, "; ") << std::endl;

		// ---- Y1: per kill exactly one SM_LOOT_STATUS(LOOT_ENABLE) naming the corpse, lootEffectId 1003 iff a listed godstone has one ----
		std::map<int32_t, std::vector<int32_t>> enables;
		std::vector<std::string> stray;
		if (a.game)
			for (const Packet& packet : a.game->recorded())
				if (packet.name == "SM_LOOT_STATUS") {
					const decoders::LootStatus status = decoders::decodeLootStatus(packet.data);
					if (status.status == decoders::LOOT_STATUS_LOOT_ENABLE)
						enables[status.targetObjectId].push_back(status.lootEffectId);
				}
		for (const KillRecord& kill : kills) {
			const std::vector<int32_t>& ids = enables[kill.corpseId];
			EXPECT_EQ(ids.size(), 1u) << "Y1: the corpse " << kill.corpseId << " of npc " << kill.templateId << " got " << ids.size()
			                          << " SM_LOOT_STATUS(LOOT_ENABLE) (registerDrop's last statement but one, once per kill)";
			const auto loot = std::ranges::find_if(loots, [&](const LootRecord& record) { return record.kill.corpseId == kill.corpseId; });
			if (ids.empty() || loot == loots.end() || !loot->opened)
				continue;
			const OracleDrops& drops = kill.templateId == KINAH_NPC_ID ? kinahDrops : lootDrops;
			int32_t expectedEffect = 0;
			for (const decoders::LootItem& entry : loot->entries)
				if (const DropCandidate* candidate = drops.candidate(entry.itemId); candidate != nullptr && candidate->lootEffectId != 0)
					expectedEffect = candidate->lootEffectId;
			EXPECT_EQ(ids.front(), expectedEffect) << "Y1: the corpse " << kill.corpseId << "'s loot effect (SM_LOOT_STATUS.getLootEffect over DropItem.getLootEffectId, "
			                                        << "DropItem.java:204-210)";
			std::cout << "Y1: corpse " << kill.corpseId << " (npc " << kill.templateId << "): LOOT_ENABLE lootEffectId " << ids.front() << ", expected "
			          << expectedEffect << std::endl;
		}
		for (const auto& [target, ids] : enables)
			if (std::ranges::none_of(kills, [&](const KillRecord& kill) { return kill.corpseId == target; }))
				stray.push_back(std::to_string(target));
		EXPECT_TRUE(stray.empty()) << "Y1: SM_LOOT_STATUS(LOOT_ENABLE) for objects the Warrior did not kill: " << join(stray);
	});

	finishRun(servers, outputDir, variant.testName);
}

} // namespace

// ---- the gates ---------------------------------------------------------------------------------------------------------------------------

/** `gs.scenario.m5b3` (G-03): the loot and item cases of §10.2 with `gameserver.geodata.enable=false` */
TEST(M5b3Scenario, Run) {
	runM5b3Gate({false, "gs.scenario.m5b3", "m5b3", "m5b3", "m5b3a"});
}

/**
 * `gs.scenario.m5b3_geo` (G-04, §10.5): the same script with `gameserver.geodata.enable=true`, its own output directory, schema pair and CTest
 * entry, under the same RESOURCE_LOCK, plus case L14 - the camp fire, which only a server with the geo meshes has: a material zone is created
 * per placed geometry with the MATERIAL intention (GeoWorldLoader.createZone, ZoneService.createMaterialZoneTemplate), and a creature inside it
 * gets a ZoneCollisionMaterialActor whose TOUCH check casts a vertical ray against that geometry.
 *
 * **Where it stands (m5b3-plan.md §13 question 3, measured by `oracle.py m5b3-material --stand`).** The nearest unconditional fire of Poeta
 * (material 60) overlaps a material-61 firepot (SUNNY or NIGHT, 0.18 m away), and a creature has ONE ZONE_MATERIAL_ACTION task, started by the
 * actor that is touched first on the thread pool (AbstractMaterialSkillActor.act) - so on a point that touches both, whether the ticks follow
 * the weather and the game time is a race. The oracle emulates the TOUCH ray over a 2 cm grid of the fire's geometry and picks the point that
 * touches the fire and not the firepot, with the largest clearance from every point that does not; standing there, the firepot's actor exists
 * but is never touched, and the ticks are the unconditional fire's. The gate walks to the oracle's step-off point (inside no zone), stands 7 s
 * on the oracle's untouched point - inside the fire's zone alone, where every TOUCH ray misses the meshes, so no tick may come (m5b3-plan.md
 * §18.2: the stand point alone could not tell a port that ignores the ray) -, goes back to the step-off point, steps onto the fire, stands
 * 12 s and steps off.
 */
TEST(M5b3ScenarioGeo, Run) {
	runM5b3Gate({true, "gs.scenario.m5b3_geo", "m5b3_geo", "m5b3geo", "m5b3g"});
}

} // namespace aion::gameserver::scenario

