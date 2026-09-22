// The M5a scenario gate (m5a-plan.md S-11, F-06, §5): one login server and one game server as child processes on their own test schemas, a
// fake client that logs in, creates an Elyos Warrior and an Asmodian Mage, enters the world, sees the NPCs the spawn oracle predicts, moves
// into a new region, quits, logs in again and finally stays online while the server shuts down.
//
// Every assertion is independent of the C++ server code: server packets are read with the decoders of tests/scenario/decoders (written from
// the Java writeImpl methods, D9), the expected values come from tools/oracle/oracle.py (F-05) or from direct database queries, and the packet
// order comes from the notation of §5.8 through PacketSequence with the async-allowed set of §5.9.
//
// The whole run is one GoogleTest case, because it owns one pair of server processes; CTest registers it as gs.scenario.m5a
// (tests/scenario/ScenarioTests.cmake) and the discovered case is disabled so that it cannot run twice. It is skipped without
// AION_TEST_GS_DATABASE_URL / AION_TEST_LS_DATABASE_URL or without a Python interpreter for the oracle.
//
// This file holds a SECOND such gate since stage 3 wave B: M5aScenarioGeo.Run, registered as gs.scenario.m5a_geo, which walks the same path
// through a server started with -Dgameserver.geodata.enable=true (§5.1 "Geodata"). It runs only the cases geo can change, shares everything
// above it in this file, and has an output directory, a schema pair and a CTest entry of its own while sharing the RESOURCE_LOCK, so the two
// never run at the same time. Its own header comment says what is geo-specific about it and what 4.8 turns out not to have.

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
#include "decoders/PacketDecoders.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {
namespace {

using namespace std::chrono_literals;
using decoders::DecodeError;
using Packet = GameSession::Packet;

/** the quiet period that ends a burst of server packets (§5.4 "collect until 1 s passes with no packet") */
constexpr std::chrono::milliseconds QUIET = 1000ms;
/** the upper bound of one burst */
constexpr std::chrono::milliseconds BURST_LIMIT = 90s;

/** SM_CREATE_CHARACTER response codes (SM_CREATE_CHARACTER.java) */
constexpr int32_t RESPONSE_OK = 0;
constexpr int32_t RESPONSE_NAME_ALREADY_USED = 10;
constexpr int32_t RESPONSE_OTHER_RACE = 12;
constexpr int32_t RESPONSE_OPEN_CREATION_WINDOW = 22;

/** PlayerClass ids (PlayerClass.java) */
constexpr int32_t CLASS_WARRIOR = 0;
constexpr int32_t CLASS_MAGE = 6;

/** Race and Gender ids (Race.java, Gender.java); PlayerCommonData.java:482-484 builds the template id as 100000 + race * 2 + gender */
constexpr int32_t RACE_ELYOS = 0;
constexpr int32_t RACE_ASMODIAN = 1;
constexpr int32_t GENDER_MALE = 0;
constexpr int32_t GENDER_FEMALE = 1;

/** the spawn maps of the two starting areas */
constexpr int32_t ELYOS_START_MAP = 210010000;
constexpr int32_t ASMODIAN_START_MAP = 220010000;

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

/** GameTime.getHour(): the in-game hour of the minute counter SM_GAME_TIME carries */
int32_t gameHourOf(int32_t gameTimeMinutes) {
	return (gameTimeMinutes % 1440) / 60;
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

/**
 * V1 / V5: an SM_NPC_INFO is accepted when the oracle has a spot of that npc id within 95 m plus the 5 m slack of the visibility radius (10 m
 * for a walker, which has moved since the spawn), or when the id is a flag npc of the map.
 */
bool visibilityAcceptsNpc(const OracleSpawns& spawns, int32_t npcId) {
	for (const OracleSpot& spot : spawns.spots)
		if (spot.npcId == npcId && spot.distance <= (spot.walker ? 105.0 : 100.0))
			return true;
	for (const OracleSpot& spot : spawns.flagNpcs)
		if (spot.npcId == npcId)
			return true;
	return false;
}

/** V3 / V5: the spots whose object stands at a known position within 90 m */
std::vector<const OracleSpot*> deterministicSpotsWithin90m(const OracleSpawns& spawns) {
	std::vector<const OracleSpot*> spots;
	for (const OracleSpot& spot : spawns.spots)
		if (spot.spawned && spot.deterministic && !spot.pool && !spot.walker && !spot.gatherable && spot.distance <= 90.0)
			spots.push_back(&spot);
	return spots;
}

/** V4: the same for the gather spots, which V3 excludes (the completeness half of V4) */
std::vector<const OracleSpot*> deterministicGatherSpotsWithin90m(const OracleSpawns& spawns) {
	std::vector<const OracleSpot*> spots;
	for (const OracleSpot& spot : spawns.spots)
		if (spot.spawned && spot.deterministic && !spot.pool && spot.gatherable && spot.distance <= 90.0)
			spots.push_back(&spot);
	return spots;
}

/** the 0.01 m tolerance V2 / V3 / V4 compare a decoded position with */
bool onSpot(float x, float y, float z, const OracleSpot& spot) {
	return std::abs(x - spot.x) <= 0.01 && std::abs(y - spot.y) <= 0.01 && std::abs(z - spot.z) <= 0.01;
}

std::string positionOf(const OracleSpot& spot) {
	return "(" + std::to_string(spot.x) + ", " + std::to_string(spot.y) + ", " + std::to_string(spot.z) + ")";
}

/**
 * The appearance a decoded packet carries against the appearance CM_CREATE_CHARACTER sent. scenarioAppearance() gives every field a distinct
 * value, so this pins the whole block: a swapped pair of bytes in the port's writeImpl changes two named fields instead of passing as "some
 * 51 bytes of appearance".
 */
void expectAppearance(const decoders::Appearance& got, const CharacterAppearance& sent, std::string_view label) {
	const auto check = [&](std::string_view field, int64_t expected, int64_t actual) {
		EXPECT_EQ(actual, expected) << label << ": SM_PLAYER_INFO appearance." << field;
	};
	check("voice", sent.voice, got.voice);
	check("skinRGB", sent.skinRGB, got.skinRGB);
	check("hairRGB", sent.hairRGB, got.hairRGB);
	check("eyeRGB", sent.eyeRGB, got.eyeRGB);
	check("lipRGB", sent.lipRGB, got.lipRGB);
	check("face", sent.face, got.face);
	check("hair", sent.hair, got.hair);
	check("deco", sent.deco, got.deco);
	check("tattoo", sent.tattoo, got.tattoo);
	check("faceContour", sent.faceContour, got.faceContour);
	check("expression", sent.expression, got.expression);
	check("jawLine", sent.jawLine, got.jawLine);
	check("forehead", sent.forehead, got.forehead);
	check("eyeHeight", sent.eyeHeight, got.eyeHeight);
	check("eyeSpace", sent.eyeSpace, got.eyeSpace);
	check("eyeWidth", sent.eyeWidth, got.eyeWidth);
	check("eyeSize", sent.eyeSize, got.eyeSize);
	check("eyeShape", sent.eyeShape, got.eyeShape);
	check("eyeAngle", sent.eyeAngle, got.eyeAngle);
	check("browHeight", sent.browHeight, got.browHeight);
	check("browAngle", sent.browAngle, got.browAngle);
	check("browShape", sent.browShape, got.browShape);
	check("nose", sent.nose, got.nose);
	check("noseBridge", sent.noseBridge, got.noseBridge);
	check("noseWidth", sent.noseWidth, got.noseWidth);
	check("noseTip", sent.noseTip, got.noseTip);
	check("cheek", sent.cheek, got.cheek);
	check("lipHeight", sent.lipHeight, got.lipHeight);
	check("mouthSize", sent.mouthSize, got.mouthSize);
	check("lipSize", sent.lipSize, got.lipSize);
	check("smile", sent.smile, got.smile);
	check("lipShape", sent.lipShape, got.lipShape);
	check("jawHeight", sent.jawHeight, got.jawHeight);
	check("chinJut", sent.chinJut, got.chinJut);
	check("earShape", sent.earShape, got.earShape);
	check("headSize", sent.headSize, got.headSize);
	check("neck", sent.neck, got.neck);
	check("neckLength", sent.neckLength, got.neckLength);
	check("shoulderSize", sent.shoulderSize, got.shoulderSize);
	check("torso", sent.torso, got.torso);
	check("chest", sent.chest, got.chest);
	check("waist", sent.waist, got.waist);
	check("hips", sent.hips, got.hips);
	check("armThickness", sent.armThickness, got.armThickness);
	check("handSize", sent.handSize, got.handSize);
	check("legThickness", sent.legThickness, got.legThickness);
	check("footSize", sent.footSize, got.footSize);
	check("facialRate", sent.facialRate, got.facialRate);
	check("armLength", sent.armLength, got.armLength);
	check("legLength", sent.legLength, got.legLength);
	check("shoulders", sent.shoulders, got.shoulders);
	check("faceShape", sent.faceShape, got.faceShape);
	EXPECT_FLOAT_EQ(got.height, sent.height) << label << ": SM_PLAYER_INFO appearance.height";
}

// ---- V9: the exact equipment SM_PLAYER_INFO has to announce -----------------------------------------------------------------------------

/** ItemSlot.java:12-31, the single-bit constants the expectation below needs by name */
constexpr int64_t SLOT_MAIN_HAND = 1LL << 0;
constexpr int64_t SLOT_SUB_HAND = 1LL << 1;
constexpr int64_t SLOT_MAIN_OFF_HAND = 1LL << 17;
constexpr int64_t SLOT_SUB_OFF_HAND = 1LL << 18;
/** ItemSlot.MAIN_OR_SUB and ItemSlot.MAIN_OFF_OR_SUB_OFF (ItemSlot.java:34-35), the two masks isTwoHandedWeapon tests */
constexpr int64_t SLOT_MAIN_OR_SUB = SLOT_MAIN_HAND | SLOT_SUB_HAND;
constexpr int64_t SLOT_MAIN_OFF_OR_SUB_OFF = SLOT_MAIN_OFF_HAND | SLOT_SUB_OFF_HAND;

/**
 * ItemSlot.VISIBLE (ItemSlot.java:41-56), spelled out as the same OR the Java enum constant is built from rather than as the literal 589055,
 * so that a reader can check it against the enum. Rings are deliberately absent: the comment there says they were designed to be visible but
 * have no skins.
 */
constexpr int64_t SLOT_VISIBLE =
  // MAIN_HAND, SUB_HAND, HELMET, TORSO, GLOVES, BOOTS, EARRINGS_LEFT, EARRINGS_RIGHT
  SLOT_MAIN_HAND | SLOT_SUB_HAND | (1LL << 2) | (1LL << 3) | (1LL << 4) | (1LL << 5) | (1LL << 6) | (1LL << 7)
  // NECKLACE, SHOULDER, PANTS, POWER_SHARD_RIGHT, POWER_SHARD_LEFT, WINGS, PLUME (RING_LEFT/RIGHT, bits 8 and 9, are NOT in it)
  | (1LL << 10) | (1LL << 11) | (1LL << 12) | (1LL << 13) | (1LL << 14) | (1LL << 15) | (1LL << 19);
static_assert(SLOT_VISIBLE == 589055, "ItemSlot.VISIBLE");

/** ItemSlot.isVisible (ItemSlot.java:109-111): the whole slot mask has to be inside VISIBLE, not merely overlap it */
bool slotIsVisible(int64_t slot) {
	return (SLOT_VISIBLE & slot) == slot;
}

/** ItemSlot.isTwoHandedWeapon (ItemSlot.java:113-115) */
bool slotIsTwoHandedWeapon(int64_t slot) {
	return (slot & SLOT_MAIN_OR_SUB) == SLOT_MAIN_OR_SUB || (slot & SLOT_MAIN_OFF_OR_SUB_OFF) == SLOT_MAIN_OFF_OR_SUB_OFF;
}

/** what writeEquippedItems has to put on the wire for a freshly created character */
struct ExpectedEquipment {
	/** the item template ids in the order the entries follow the mask */
	std::vector<int32_t> itemIds;
	/** the slot mask written before the entries */
	int32_t mask = 0;
	/** the slot of each entry, for the failure message */
	std::vector<int64_t> slots;
};

/**
 * V9: the exact equipment block of SM_PLAYER_INFO, derived from the creation oracle - not from the C++ server.
 *
 * SM_PLAYER_INFO.java:94 writes `writeEquippedItems(player.getEquipment().getEquippedForAppearance())`. Equipment.java:364-373 returns every
 * item of the equipment map whose slot passes ItemSlot.isVisible (a two-handed weapon, which sits in the map twice, is added once), and the
 * map is a `SortedMap<Long, Item>` keyed by the slot bit (Equipment.java:48, onLoadHandler at :445-447), so the list is in ascending slot
 * order. AbstractPlayerInfoPacket.java:148-164 then writes `mask |= item.getEquipmentSlot()` over that list - clearing the SUB_HAND bit again
 * for a two-handed weapon - followed by one entry per item, with no count of its own.
 *
 * The oracle's rows are exactly the `inventory` rows the gate already checked in case 2: PlayerService.java:219-222 equips every armour or
 * weapon of the class's `player_data` block with `ItemSlot.getSlotFor(...)`, i.e. ONE non-combo bit, which is what the `slot` column stores and
 * what the equipment map is later keyed by. Ascending slot therefore is ascending map key here.
 *
 * Why equality and not containment: a server that announced only one of the three visible items, or that ORed one slot bit into the mask
 * instead of three, satisfies "not empty", "mask != 0" and a per-entry subset check - the very shape V4's comment above condemns.
 */
ExpectedEquipment expectedEquipmentOf(const OracleCreation& creation) {
	std::vector<std::pair<int64_t, int32_t>> visible; // slot -> item id
	for (const OracleItem& item : creation.items)
		if (item.equipped && slotIsVisible(item.slot))
			visible.emplace_back(item.slot, item.itemId);
	std::sort(visible.begin(), visible.end());

	ExpectedEquipment expected;
	for (const auto& [slot, itemId] : visible) {
		expected.itemIds.push_back(itemId);
		expected.slots.push_back(slot);
		expected.mask = static_cast<int32_t>(expected.mask | slot);
		if (slotIsTwoHandedWeapon(slot))
			expected.mask = static_cast<int32_t>(expected.mask & ~SLOT_SUB_HAND);
	}
	return expected;
}

/**
 * V9, the SM_PLAYER_INFO half of §5.4: decodes §5.8 #33 and pins the fields a real 4.8 client needs to render the character. SM_PLAYER_INFO is
 * its own writeImpl (SM_PLAYER_INFO.java:36-228), not the writePlayerInfo block that SM_CHARACTER_LIST and SM_CREATE_CHARACTER share, so
 * nothing else in the gate covers its bytes. decodePlayerInfo ends in expectFullyConsumed, i.e. calling it at all checks the whole framing.
 *
 * @return the decoded packet, so that the caller can cross-check it against the SM_STATS_INFO of the same character (std::nullopt if the burst
 *         carried none, which is a failure of its own)
 */
std::optional<decoders::PlayerInfo> expectPlayerInfo(const std::vector<Packet>& burst, int32_t playerId, const std::string& name,
	const OracleCreation& creation, int32_t classId, int32_t raceId, int32_t genderId, const CharacterAppearance& sentAppearance,
	std::string_view label) {
	const Packet* packet = firstOfName(burst, "SM_PLAYER_INFO");
	if (packet == nullptr) {
		ADD_FAILURE() << label << ": no SM_PLAYER_INFO after CM_LEVEL_READY; got: " << join(namesOf(burst));
		return std::nullopt;
	}
	const decoders::PlayerInfo info = decoders::decodePlayerInfo(packet->data);
	EXPECT_EQ(info.objectId, playerId) << label << ": SM_PLAYER_INFO object id";
	EXPECT_EQ(info.name, name) << label << ": SM_PLAYER_INFO name";
	EXPECT_EQ(static_cast<int32_t>(info.classId), classId) << label << ": SM_PLAYER_INFO class id";
	EXPECT_EQ(static_cast<int32_t>(info.raceId), raceId) << label << ": SM_PLAYER_INFO race id";
	EXPECT_EQ(static_cast<int32_t>(info.genderId), genderId) << label << ": SM_PLAYER_INFO gender id";
	EXPECT_EQ(static_cast<int32_t>(info.level), 1) << label << ": SM_PLAYER_INFO level";
	EXPECT_EQ(static_cast<int32_t>(info.hpPercentage), 100) << label << ": SM_PLAYER_INFO HP%";
	EXPECT_NEAR(info.x, creation.x, 0.01) << label << ": SM_PLAYER_INFO x";
	EXPECT_NEAR(info.y, creation.y, 0.01) << label << ": SM_PLAYER_INFO y";
	EXPECT_NEAR(info.z, creation.z, 0.01) << label << ": SM_PLAYER_INFO z";
	// the second position block of the packet (SM_PLAYER_INFO.java:193-195 writes x/y/z again after the movement vector)
	EXPECT_FLOAT_EQ(info.moveX, info.x) << label << ": the two x of SM_PLAYER_INFO differ";
	EXPECT_FLOAT_EQ(info.moveY, info.y) << label << ": the two y of SM_PLAYER_INFO differ";
	EXPECT_FLOAT_EQ(info.moveZ, info.z) << label << ": the two z of SM_PLAYER_INFO differ";
	EXPECT_FALSE(info.legionMember) << label << ": a fresh character is in no legion";

	// PlayerCommonData.java:482-484 is the template id (100000 + race * 2 + gender), and a player who is not transformed answers the same value
	// through TransformModel.getModelId() (TransformModel.java:98-107: no active transform and no event model, so the object template's id)
	const int32_t templateId = 100000 + raceId * 2 + genderId;
	EXPECT_EQ(info.templateId, templateId) << label << ": SM_PLAYER_INFO template id";
	EXPECT_EQ(info.transformModelId, templateId) << label << ": SM_PLAYER_INFO transform model id of an untransformed character";
	EXPECT_EQ(info.robotId, 0) << label << ": SM_PLAYER_INFO robot id";
	EXPECT_EQ(static_cast<int32_t>(info.enemyFlag), 0x26) << label << ": SM_PLAYER_INFO enemy flag of the own character (SM_PLAYER_INFO.java:56)";
	// SM_PLAYER_INFO.java:73 writes pcd.getTitleId() as a short, and a character without a title carries -1, not 0
	// (PlayerCommonData.java:50 `private int titleId = -1`, aion_gs.sql:930 `title_id int NOT NULL DEFAULT '-1'`), so the wire value is 0xFFFF
	EXPECT_EQ(static_cast<int32_t>(info.titleId), 0xFFFF) << label << ": the title of a character that has none is -1";
	EXPECT_EQ(static_cast<int32_t>(info.dp), 0) << label << ": a fresh character has no DP";
	EXPECT_EQ(static_cast<int32_t>(info.visualState), 0) << label << ": SM_PLAYER_INFO visual state";
	EXPECT_EQ(info.targetObjectId, 0) << label << ": a character that entered the world has no target";
	EXPECT_EQ(info.currentTeamId, 0) << label << ": a fresh character is in no group";
	EXPECT_EQ(info.houseAddressId, 0) << label << ": a fresh character owns no house";
	// SM_PLAYER_INFO.java:216-219: membership 0 (a normal account) writes 1, everything else 3 + membership
	EXPECT_EQ(info.membership, 1) << label << ": SM_PLAYER_INFO membership of a normal account";
	EXPECT_TRUE(info.note.empty()) << label << ": SM_PLAYER_INFO note";
	EXPECT_TRUE(info.storeMessage.empty()) << label << ": a character without a store sends an empty store message";
	EXPECT_GT(info.movementSpeed, 0.0f) << label << ": SM_PLAYER_INFO movement speed";
	EXPECT_GT(info.attackSpeedBase, 0) << label << ": SM_PLAYER_INFO base attack speed";
	EXPECT_GT(info.attackSpeedCurrent, 0) << label << ": SM_PLAYER_INFO current attack speed";
	expectAppearance(info.appearance, sentAppearance, label);

	// The equipment block, against the exact expectation expectedEquipmentOf derives from the creation oracle (count, ids, order and the full
	// slot mask), not against a subset of it: SM_PLAYER_INFO is what a real client renders the character's gear from, and announcing two of the
	// three starter pieces - or ORing one slot bit instead of three - leaves the character half naked while "not empty" and "mask != 0" pass.
	const ExpectedEquipment expectedEquipment = expectedEquipmentOf(creation);
	// the same non-vacuity guard V3 and V4 carry: both M5a starter sets equip a weapon (MAIN_HAND), a torso and a pair of pants, so an empty
	// expectation means the oracle or the `player_data` block changed - never that there is nothing to check
	EXPECT_FALSE(expectedEquipment.itemIds.empty())
	  << label << ": the creation oracle predicts no visible equipped item, so the equipment half of V9 would assert nothing";

	std::vector<int32_t> announced;
	for (const decoders::EquippedItem& item : info.equipment.items)
		announced.push_back(item.skinTemplateId);
	// writeEquippedItems writes item.getItemSkinTemplate().getTemplateId() per entry, and a starter item has no skin (Item.java:240-244 falls
	// back to the item template), so each entry is the item's own template id
	std::vector<std::string> expectedWithSlots;
	for (size_t i = 0; i < expectedEquipment.itemIds.size(); i++)
		expectedWithSlots.push_back(std::to_string(expectedEquipment.itemIds[i]) + "@slot " + std::to_string(expectedEquipment.slots[i]));
	EXPECT_EQ(announced, expectedEquipment.itemIds)
	  << label << ": SM_PLAYER_INFO announced " << announced.size() << " equipped items where the creation oracle predicts "
	  << expectedEquipment.itemIds.size() << " (" << join(expectedWithSlots) << "), in ascending slot order (Equipment's SortedMap)";
	EXPECT_EQ(info.equipment.mask, expectedEquipment.mask)
	  << label << ": SM_PLAYER_INFO equipment slot mask; expected the OR of " << join(expectedWithSlots);
	// a starter item is unenchanted, undyed and carries no god stone, so every remaining field of an entry is 0 as well
	for (size_t i = 0; i < info.equipment.items.size(); i++) {
		const decoders::EquippedItem& item = info.equipment.items[i];
		const std::string which = std::string(label) + ": SM_PLAYER_INFO equipped item " + std::to_string(i + 1) + " (" +
		                          std::to_string(item.skinTemplateId) + ")";
		EXPECT_EQ(item.godStoneId, 0) << which << " god stone";
		EXPECT_FALSE(item.color.dyed()) << which << " dye";
		EXPECT_EQ(static_cast<int32_t>(item.enchantParam), 0) << which << " enchant parameter";
	}
	return info;
}

/**
 * V1 to V4 of §5.5 for one level-ready burst; V5 is the same set for the Mage on the Asmodian map, which is why this is a function and not
 * inline code of case 4.
 *
 * V2 checks EVERY decoded npc, not only the ones whose position already matches: level and HP% do not depend on where an npc stands, so they
 * are compared for all of them, and the position is compared against every spot of the id.
 *
 * Which npcs may stand off their spot is exactly §5.5 V2's rule, and not "walkers only" as this function first read it: an npc fails here only
 * when the oracle knows its id **solely** as fixed spots ("the id has no pool, walker or randomWalk spot at all"). An id with a walker spot
 * moves with its group's formation, an id with a randomWalk spot wanders by definition, and an id with a pool spot is one of several candidates
 * of that pool - all three are covered by V1 (id plus oracle distance) instead, and each is counted into its own bucket below. Reading the rule
 * as "walker" alone made V2 fail a randomWalk-only or pool-only id that legitimately stood somewhere else, which is a gate that fails on correct
 * behaviour; the check that matters - an id the oracle pins to fixed coordinates standing anywhere else - is unchanged.
 *
 * Each npc is counted into exactly one bucket and the buckets are printed, so a run can be read afterwards: an assertion that silently covers
 * nothing shows up as a bucket of zero.
 */
void checkVisibility(const std::vector<Packet>& burst, const OracleSpawns& spawns, std::string_view label) {
	std::vector<decoders::NpcInfo> npcs;
	for (const Packet& packet : ofName(burst, "SM_NPC_INFO"))
		npcs.push_back(decoders::decodeNpcInfo(packet.data)); // a body that does not decode exactly throws (V2 "decodes exactly")
	EXPECT_FALSE(npcs.empty()) << label << " V1: not a single SM_NPC_INFO arrived";

	// V1: every npc id is a spot within 95 m (+5 m slack, +10 m for walkers) or a flag npc of the map
	for (const decoders::NpcInfo& npc : npcs)
		EXPECT_TRUE(visibilityAcceptsNpc(spawns, npc.templateId))
		  << label << " V1: SM_NPC_INFO for npc " << npc.templateId << " at (" << npc.x << ", " << npc.y << ", " << npc.z
		  << ") has no spot within the visibility radius";

	// V2: an npc stands on a spot of its id; heading, level and HP% match
	size_t onFixedSpot = 0, onMovingSpot = 0, offSpotMoving = 0, flagOnly = 0, unknownToTheOracle = 0;
	for (const decoders::NpcInfo& npc : npcs) {
		const OracleSpot* match = nullptr;
		bool matchIsFixed = false;
		bool isFlagNpc = false;
		std::optional<int32_t> templateLevel;
		std::vector<std::string> spotsOfId;
		for (const OracleSpot& spot : spawns.spots) {
			if (spot.npcId != npc.templateId)
				continue;
			// every spot of an id carries the npc_template level of that id (oracle m5a/spawns.py:254), so any spot that has one answers it;
			// a level of 0 is a level like any other and is compared (OracleSpot::level)
			if (spot.level)
				templateLevel = spot.level;
			spotsOfId.push_back(positionOf(spot));
			// a fixed spot wins over a pool or walker spot at the same coordinates: only its heading is worth comparing
			const bool fixed = !spot.pool && !spot.walker && !spot.randomWalk;
			if (onSpot(npc.x, npc.y, npc.z, spot) && (match == nullptr || (fixed && !matchIsFixed))) {
				match = &spot;
				matchIsFixed = fixed;
			}
		}
		for (const OracleSpot& spot : spawns.flagNpcs)
			if (spot.npcId == npc.templateId) {
				isFlagNpc = true;
				if (!templateLevel && spot.level)
					templateLevel = spot.level;
			}

		// level and HP% are template properties: they are checked for every npc, matched or not, so that they no longer depend on V3 having
		// matched the position first
		if (templateLevel)
			EXPECT_EQ(static_cast<int32_t>(npc.level), *templateLevel) << label << " V2: level of npc " << npc.templateId;
		else
			EXPECT_GT(static_cast<int32_t>(npc.level), 0) << label << " V2: level of npc " << npc.templateId << ", which the oracle has no level for";
		EXPECT_EQ(npc.hpPercentage, 100) << label << " V2: HP% of npc " << npc.templateId;

		if (match != nullptr) {
			// a walker's heading comes from its formation, not from the spot, so only a spot the oracle calls fixed pins it
			if (matchIsFixed)
				EXPECT_EQ(npc.heading, match->heading) << label << " V2: heading of npc " << npc.templateId << " at " << positionOf(*match);
			matchIsFixed ? onFixedSpot++ : onMovingSpot++;
		} else if (spotsOfId.empty() && isFlagNpc) {
			flagOnly++; // a flag npc has no spawn spot of its own
		} else if (spotsOfId.empty()) {
			unknownToTheOracle++; // V1 already failed for this one
		} else if (isPinnedToFixedSpots(spawns.spots, npc.templateId)) {
			ADD_FAILURE() << label << " V2: SM_NPC_INFO for npc " << npc.templateId << " at (" << npc.x << ", " << npc.y << ", " << npc.z
			              << "), which is none of its " << spotsOfId.size() << " oracle spots " << join(spotsOfId)
			              << " - and the oracle knows this id only as fixed spots, so it cannot legitimately stand anywhere else";
		} else {
			offSpotMoving++; // a pool member, a walker group member or a randomWalk npc: V1 covered it by id and oracle distance (§5.5 V2)
		}
	}
	std::cout << label << ": " << npcs.size() << " SM_NPC_INFO - " << onFixedSpot << " on a fixed spot, " << onMovingSpot
	          << " on a pool, walker or randomWalk spot, " << offSpotMoving << " off-spot npcs of an id with a pool, walker or randomWalk spot, "
	          << flagOnly << " flag npcs, " << unknownToTheOracle << " unknown to the oracle" << std::endl;

	// V3: every deterministic spot within 90 m is among the npcs, matched by id and position
	const std::vector<const OracleSpot*> expected = deterministicSpotsWithin90m(spawns);
	// without this the whole of V3 is satisfied by an empty expectation, exactly as V4 was
	EXPECT_FALSE(expected.empty()) << label << " V3: the spawn oracle predicts no deterministic spawn spot within 90 m of the spawn point, so "
	                                           "V3 would assert nothing";
	EXPECT_GE(npcs.size(), expected.size()) << label << " V3: fewer SM_NPC_INFO than deterministic spots within 90 m";
	for (const OracleSpot* spot : expected) {
		bool found = false;
		for (const decoders::NpcInfo& npc : npcs)
			if (npc.templateId == spot->npcId && onSpot(npc.x, npc.y, npc.z, *spot))
				found = true;
		EXPECT_TRUE(found) << label << " V3: no SM_NPC_INFO for the deterministic spot of npc " << spot->npcId << " at " << positionOf(*spot) << ", "
		                   << spot->distance << " m away";
	}

	// V4: the gatherable ids are a subset of the gather spots within 100 m, AND every deterministic gather spot within 90 m is among them.
	// The completeness half mirrors V3 and is what makes V4 fail on a world without gatherables: a subset assertion alone is satisfied by
	// zero SM_GATHERABLE_INFO, and levelReadyPattern's `(SM_NPC_INFO | SM_GATHERABLE_INFO)+` is satisfied by the npcs on their own, so the
	// 18,432 skipped gatherable spawns this wave started with would have passed the gate unnoticed.
	std::set<int32_t> gatherSpots;
	for (const OracleSpot& spot : spawns.spots)
		if (spot.gatherable && spot.distance <= 100.0)
			gatherSpots.insert(spot.npcId);
	std::vector<decoders::GatherableInfo> gatherables;
	for (const Packet& packet : ofName(burst, "SM_GATHERABLE_INFO"))
		gatherables.push_back(decoders::decodeGatherableInfo(packet.data));
	for (const decoders::GatherableInfo& gatherable : gatherables)
		EXPECT_TRUE(gatherSpots.contains(gatherable.templateId))
		  << label << " V4: SM_GATHERABLE_INFO for " << gatherable.templateId << ", which is no gather spot within 100 m";
	const std::vector<const OracleSpot*> expectedGatherables = deterministicGatherSpotsWithin90m(spawns);
	// The completeness half only asserts something if the oracle predicts something: on a start point without gather nodes the loop below runs
	// zero times and "no gather node anywhere" stays invisible. Both M5a start points have four deterministic gather spots within 90 m (npc
	// 400601 on 210010000, 400651 on 220010000), so an empty expectation means the oracle, the spawn data or the radius changed - never that
	// there is nothing to check.
	EXPECT_FALSE(expectedGatherables.empty())
	  << label << " V4: the spawn oracle predicts no deterministic gather spot within 90 m of the spawn point, so the completeness half of V4 "
	              "would assert nothing";
	std::cout << label << ": " << gatherables.size() << " SM_GATHERABLE_INFO against " << expectedGatherables.size()
	          << " deterministic gather spots within 90 m" << std::endl;
	for (const OracleSpot* spot : expectedGatherables) {
		bool found = false;
		for (const decoders::GatherableInfo& gatherable : gatherables)
			if (gatherable.templateId == spot->npcId && onSpot(gatherable.x, gatherable.y, gatherable.z, *spot))
				found = true;
		EXPECT_TRUE(found) << label << " V4: no SM_GATHERABLE_INFO for the deterministic gather spot of " << spot->npcId << " at "
		                   << positionOf(*spot) << ", " << spot->distance << " m away";
	}
}

// ---- the case-by-case report of §5.7 Q8 --------------------------------------------------------------------------------------------------

struct CaseResult {
	std::string id;
	std::string title;
	bool ran = false;
	bool failed = false;
	/** the exception that ended the case, if it was ended by one */
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
	/** @return true if the case ran without a failure (a later case that depends on it can then go on) */
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

	/** the report of case 8, printed whether the gate passes or fails */
	std::string report() const {
		std::ostringstream text;
		text << "M5a scenario gate, case by case:\n";
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

// ---- packet stream helpers --------------------------------------------------------------------------------------------------------------

/**
 * Reads the next server packet and fails unless it has that name. Packets of the async-allowed set are skipped (they are recorded and the
 * cases check them separately).
 */
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

/** Fails if any packet arrives within the window (§5.2 step 7 "CM_GAMEGUARD -> nothing within 500 ms") */
void expectSilence(GameSession& session, std::chrono::milliseconds window) {
	std::optional<Packet> packet = session.next(window);
	EXPECT_FALSE(packet.has_value()) << "expected no packet, got " << (packet ? packet->name : std::string());
}

/** Matches the recorded names against the §5.8 notation with the async-allowed set of §5.9 */
void expectSequence(const std::vector<Packet>& packets, std::string_view pattern, const AsyncAllowed& async) {
	const PacketSequence sequence = PacketSequence::parse(pattern);
	const std::vector<std::string> names = namesOf(packets);
	const PacketSequence::Result result = sequence.match(names, async.predicate(packets));
	EXPECT_TRUE(result.matched) << result.message << "\n  expected: " << sequence.toString() << "\n  got (" << names.size() << "): " << join(names);
}

// ---- the scenario client ----------------------------------------------------------------------------------------------------------------

/** One account with its login server session and its game connection */
struct ScenarioClient {
	std::string account;
	std::string password = "m5aPassword1";
	std::unique_ptr<FakeLoginClient> login;
	std::unique_ptr<GameSession> game;
	FakeLoginClient::SessionKey key;
	int32_t playerId = 0;
	std::string characterName;
};

/** §5.2: the login server conversation and the game server login up to SM_CHARACTER_LIST */
decoders::CharacterList logIn(ScenarioServers& servers, ScenarioClient& client, const AsyncAllowed& async, bool full) {
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
	if (full) {
		client.game->send(GameSession::CM_PING, GameSession::buildCM_PING());
		expectNext(*client.game, "SM_PONG", async);
		client.game->send(GameSession::CM_GAMEGUARD, GameSession::buildCM_GAMEGUARD(std::vector<uint8_t>{1, 2, 3, 4}));
		expectSilence(*client.game, 500ms);
		client.game->send(GameSession::CM_SECURITY_TOKEN, GameSession::buildCM_SECURITY_TOKEN());
		expectNext(*client.game, "SM_SECURITY_TOKEN", async);
	}
	return decoders::decodeCharacterList(characters.data);
}

/** The appearance the scenario sends; every field the `player_appearance` row stores is distinct, so a swapped column shows up. */
CharacterAppearance scenarioAppearance() {
	CharacterAppearance appearance;
	appearance.voice = 2;
	appearance.skinRGB = 0x00E1C3B4;
	appearance.hairRGB = 0x00202021;
	appearance.eyeRGB = 0x00503011;
	appearance.lipRGB = 0x00A06062;
	appearance.face = 3;
	appearance.hair = 5;
	appearance.deco = 7;
	appearance.tattoo = 9;
	appearance.faceContour = 11;
	appearance.expression = 13;
	appearance.jawLine = 15;
	appearance.forehead = 17;
	appearance.eyeHeight = 19;
	appearance.eyeSpace = 21;
	appearance.eyeWidth = 23;
	appearance.eyeSize = 25;
	appearance.eyeShape = 27;
	appearance.eyeAngle = 29;
	appearance.browHeight = 31;
	appearance.browAngle = 33;
	appearance.browShape = 35;
	appearance.nose = 37;
	appearance.noseBridge = 39;
	appearance.noseWidth = 41;
	appearance.noseTip = 43;
	appearance.cheek = 45;
	appearance.lipHeight = 47;
	appearance.mouthSize = 49;
	appearance.lipSize = 51;
	appearance.smile = 53;
	appearance.lipShape = 55;
	appearance.jawHeight = 57;
	appearance.chinJut = 59;
	appearance.earShape = 61;
	appearance.headSize = 63;
	appearance.neck = 65;
	appearance.neckLength = 67;
	appearance.shoulderSize = 69;
	appearance.torso = 71;
	appearance.chest = 73;
	appearance.waist = 75;
	appearance.hips = 77;
	appearance.armThickness = 79;
	appearance.handSize = 81;
	appearance.legThickness = 83;
	appearance.footSize = 85;
	appearance.facialRate = 87;
	appearance.armLength = 89;
	appearance.legLength = 91;
	appearance.shoulders = 93;
	appearance.faceShape = 95;
	appearance.height = 1.0f;
	return appearance;
}

/** the expected `player_appearance` row of scenarioAppearance(), in the column order the gate queries */
std::vector<std::pair<std::string, int64_t>> expectedAppearanceRow(const CharacterAppearance& appearance) {
	return {
	  {"face", appearance.face},
	  {"hair", appearance.hair},
	  {"deco", appearance.deco},
	  {"tattoo", appearance.tattoo},
	  {"face_contour", appearance.faceContour},
	  {"expression", appearance.expression},
	  {"jaw_line", appearance.jawLine},
	  {"skin_rgb", appearance.skinRGB},
	  {"hair_rgb", appearance.hairRGB},
	  {"lip_rgb", appearance.lipRGB},
	  {"eye_rgb", appearance.eyeRGB},
	  {"face_shape", appearance.faceShape},
	  {"forehead", appearance.forehead},
	  {"eye_height", appearance.eyeHeight},
	  {"eye_space", appearance.eyeSpace},
	  {"eye_width", appearance.eyeWidth},
	  {"eye_size", appearance.eyeSize},
	  {"eye_shape", appearance.eyeShape},
	  {"eye_angle", appearance.eyeAngle},
	  {"brow_height", appearance.browHeight},
	  {"brow_angle", appearance.browAngle},
	  {"brow_shape", appearance.browShape},
	  {"nose", appearance.nose},
	  {"nose_bridge", appearance.noseBridge},
	  {"nose_width", appearance.noseWidth},
	  {"nose_tip", appearance.noseTip},
	  {"cheek", appearance.cheek},
	  {"lip_height", appearance.lipHeight},
	  {"mouth_size", appearance.mouthSize},
	  {"lip_size", appearance.lipSize},
	  {"smile", appearance.smile},
	  {"lip_shape", appearance.lipShape},
	  {"jaw_height", appearance.jawHeight},
	  {"chin_jut", appearance.chinJut},
	  {"ear_shape", appearance.earShape},
	  {"head_size", appearance.headSize},
	  {"neck", appearance.neck},
	  {"neck_length", appearance.neckLength},
	  {"shoulders", appearance.shoulders},
	  {"shoulder_size", appearance.shoulderSize},
	  {"torso", appearance.torso},
	  {"chest", appearance.chest},
	  {"waist", appearance.waist},
	  {"hips", appearance.hips},
	  {"arm_thickness", appearance.armThickness},
	  {"arm_length", appearance.armLength},
	  {"hand_size", appearance.handSize},
	  {"leg_thickness", appearance.legThickness},
	  {"leg_length", appearance.legLength},
	  {"foot_size", appearance.footSize},
	  {"facial_rate", appearance.facialRate},
	  {"voice", appearance.voice},
	};
}

// ---- the §5.8 sequences -----------------------------------------------------------------------------------------------------------------

/**
 * The CM_ENTER_WORLD part of §5.8 (#0 to #32). `inventoryPackets` is §5.8 #13: PlayerEnterWorldService.sendItemInfos splits the kinah item
 * plus every equipped and inventory item into parts of ten and appends one empty packet.
 */
std::string enterWorldPattern(bool firstEnter, int32_t inventoryPackets) {
	std::string pattern;
	if (firstEnter)
		pattern += "SM_STATS_INFO, SM_ACTION_ANIMATION, SM_NEARBY_QUESTS, ";
	pattern += "SM_HOUSE_SCRIPTS, SM_UNK_3_5_1, SM_ENTER_WORLD_CHECK, ";
	pattern += "SM_SKILL_LIST+, [SM_SKILL_COOLDOWN], [SM_ITEM_COOLDOWN], ";
	// SM_TITLE_INFO twice: PlayerEnterWorldService.java:239 sends SM_TITLE_INFO(pcd.getTitleId()), and lines 240-242 then run
	// `if (pcd.getBonusTitleId() != 0) player.getTitleList().setBonusTitle(...)`, whose first statement (TitleList.java:88) is
	// SM_TITLE_INFO(6, bonusTitleId). A character's bonus title is -1, not 0 (PlayerCommonData.java:51, aion_gs.sql:931
	// `bonus_title_id int NOT NULL DEFAULT '-1'`), so the second packet is sent on every enter world, first enter and relogin alike.
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

/** The CM_LEVEL_READY part of §5.8 (#33 to #44) */
std::string levelReadyPattern() {
	return "SM_PLAYER_INFO, SM_PLAYER_STATE, SM_ACCOUNT_PROPERTIES, SM_MOTION, "
	       "SM_WINDSTREAM_ANNOUNCE*, "
	       "(SM_NPC_INFO | SM_GATHERABLE_INFO)+, "
	       "SM_RIFT_ANNOUNCE, "
	       "SM_NEARBY_QUESTS, [SM_QUEST_REPEAT], [SM_WEATHER], "
	       "SM_ABNORMAL_STATE, SM_CUBE_UPDATE";
}

// ---- the check output reports (§5.7 Q8) -------------------------------------------------------------------------------------------------

/**
 * §5.7 Q8: the classes whose live instance count must be 0 after the shutdown. Everything here belongs to a character that logged out or to a
 * per-session task; nothing keeps one alive once its Player is gone, so a leftover is a real leak of the free-threaded design (D7).
 */
const std::set<std::string>& strictlyZeroLiveClasses() {
	static const std::set<std::string> classes = {
	  "Player", "AbyssRank", // the character and what hangs off it
	  "GatheringTask", "AbstractInteractionTask", "GatheringTask_ActionObserver", // the wave's periodic-task subsystem
	};
	// Item is NOT here (m5a-plan.md §10.1): a connection that is still registered at the shutdown keeps its Account, and with it the account
	// warehouse Storage and every Item in it, exactly as Java does. It is bounded below by what this scenario puts into an account warehouse.
	return classes;
}

/**
 * §5.7 Q8: the classes that Java's own shutdown path keeps alive once per client that is still connected when the stop file is written, so
 * their live count is bounded by that number of connections instead of being 0.
 *
 * AionConnection.java:239-243 returns from onDisconnect right after safeLogout() when GameServer.isShuttingDownSoon(), i.e. BEFORE
 * LoginServer.getInstance().onDisconnect(this) - and LoginServer.java:119 is the only place that removes the connection from
 * loggedInAccounts. The still-registered AionConnection therefore holds its Account, and with it AccountTime, the PlayerAccountData with its
 * PlayerCommonData and PlayerAppearance, and the account warehouse (one ItemStorage), until the process exits. §5.7 Q7 deliberately arranges
 * exactly that by shutting the server down with account B in the world, and the C++ port reproduces the early return (item S-12), so a 0 here
 * would demand a port less faithful than Java.
 *
 * The bound is per connection, not a blanket exemption: account A quits normally in case 6, so if ITS account-level objects leaked the count
 * would be two where one connection is open, and the check still fails. Everything that belongs to a character - Player, Item and every
 * per-character storage of the 104 an enter world creates - stays at the strict 0 above.
 */
const std::set<std::string>& perConnectionLiveClasses() {
	static const std::set<std::string> classes = {
	  "Account", "AccountTime", "PlayerAccountData", "PlayerCommonData", "PlayerAppearance", "ConnectionAliveChecker",
	};
	return classes;
}

/** one row of live_counts.txt / live_counts_baseline.txt: "<live>\t<created>\t<qualified class name>" */
struct LiveCount {
	int64_t live = 0;
	int64_t created = 0;
	std::string line;
};

/**
 * The rows of a live-count report with the unqualified class name (the last "::" component) of each. A vector, not a map: two qualified names
 * can share their last component, and a map would silently drop one of the two rows.
 */
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

std::vector<std::string> readAllowlist() {
	std::vector<std::string> sites;
	std::ifstream in(AION_SCENARIO_PARTIAL_ALLOWLIST, std::ios::binary);
	std::string line;
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (line.empty() || line.starts_with('#'))
			continue;
		sites.push_back(line);
	}
	return sites;
}

/**
 * An allow-list entry matches a site of partial_trace.txt. An entry WITH a line number ("<file>:<line>") must match the whole site: a prefix
 * match would let "InstanceService.cpp:133" also cover :1330 to :1339 and "BaseService.cpp:18" cover :180 to :189, so a future partial at one
 * of those lines would be allow-listed by accident. An entry without a line number stays an explicit whole-file wildcard.
 */
bool allowlistEntryMatches(const std::string& entry, const std::string& site) {
	if (entry.find(':') != std::string::npos)
		return entry == site;
	return site.starts_with(entry) && (site.size() == entry.size() || site[entry.size()] == ':');
}

/**
 * The end of a run: the two test schemas are dropped, and a failed run says where its evidence is.
 *
 * Before stage 3 a failed run kept its schemas unconditionally "for the post mortem", which meant that every failing run of every build tree
 * left a pair behind: the next run of the same gate recreates them (the name is a hash of the output directory) and createSchemas() sweeps
 * what is an hour old, but neither happens if nobody runs the gate again - and since this file has two gates a failing tree now leaves four.
 * What a post mortem reads first is the run's own reports and the two server logs, which are kept and named below whatever happens; the
 * database rows are one environment variable and one repeat run away (AION_SCENARIO_KEEP_SCHEMAS, ScenarioServers::keepSchemasOnFailure).
 */
void finishRun(ScenarioServers& servers, const std::filesystem::path& outputDir, std::string_view testName) {
	// The destructor's report is the net, not the gate: it runs after this function, so a stop problem it found - a killed login server, a game
	// server that had to be terminated - would arrive after `failed` was computed, and the run would take the passed branch, drop its schemas and
	// print nothing. The gate takes responsibility for them here, while the verdict still counts.
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
		// never mask the failure this run is about: the schemas are recreated by the next run of the same gate and swept after an hour
		std::cout << "the scenario schemas could not be dropped (" << exception.what() << ")" << std::endl;
	}
}

/** "<hits>\t<site>\t<function>\t<reason>" of partial_trace.txt -> the site column */
std::string siteOf(const std::string& traceLine) {
	const size_t first = traceLine.find('\t');
	if (first == std::string::npos)
		return traceLine;
	const size_t second = traceLine.find('\t', first + 1);
	return traceLine.substr(first + 1, second == std::string::npos ? std::string::npos : second - first - 1);
}

} // namespace

// ---- the gate ---------------------------------------------------------------------------------------------------------------------------

TEST(M5aScenario, Run) {
	// A skipped gate is NOT a passed gate. Without the database URLs or a Python interpreter the gate skips itself for developer convenience,
	// ScenarioTests.cmake turns that into CTest's "***Skipped", and CTest counts a skip as passed - so a full ctest run without the four
	// AION_TEST_* variables reports 100% green while the milestone was never executed. AION_SCENARIO_REQUIRE=1 (the milestone and CI
	// invocation) turns every skip reason into a failure instead, the same shape as GS_SMOKE_REQUIRE_STARTED for the startup smoke test.
	const char* requireEnvironment = std::getenv("AION_SCENARIO_REQUIRE");
	const bool required = requireEnvironment != nullptr && *requireEnvironment != '\0' && std::string_view(requireEnvironment) != "0";
	const auto unavailable = [&](std::string_view reason) {
		if (required)
			ADD_FAILURE() << "gs.scenario.m5a was not configured and AION_SCENARIO_REQUIRE is set: " << reason;
		else
			GTEST_SKIP() << "gs.scenario.m5a: skipped (" << reason << ")";
	};

	std::optional<ScenarioEnvironment> environment = ScenarioEnvironment::fromEnvironment();
	if (!environment) {
		unavailable("set AION_TEST_GS_DATABASE_URL and AION_TEST_LS_DATABASE_URL");
		return;
	}
	const std::filesystem::path outputDir = std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "m5a";
	std::optional<Oracle> oracle = Oracle::fromEnvironment(outputDir / "oracle");
	if (!oracle) {
		unavailable("no Python interpreter for tools/oracle: set AION_TEST_PYTHON");
		return;
	}

	CaseLog cases;
	// the reports of case 8 are printed whether the gate passes or fails
	struct ReportPrinter {
		const CaseLog& cases;
		~ReportPrinter() { std::cout << cases.report() << std::flush; }
	} printer{cases};

	// ---- §5.1 processes and databases ----
	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.startupTimeout = 10min;
	config.stopTimeout = 3min;
	ScenarioServers servers(config, *environment);
	const std::string schema = servers.gameSchema();
	const ScenarioDatabase& database = servers.gameDatabase();

	// case 0 is not one of the plan's cases; it is here so that a startup that does not reach "Game server started" ends with the reason
	// (the unported body, the last startup step) instead of a bare timeout, which is what the fixup lanes need as a work item
	bool ok = true;
	// a case whose predecessor failed is reported as "not run" instead of adding a second, misleading failure
	const auto runCase = [&](std::string_view id, std::string_view title, const std::function<void()>& body) {
		if (!ok)
			cases.skip(id, title, "an earlier case failed");
		else
			ok = cases.run(id, title, body);
	};

	ok = cases.run("case 0", "the servers start (§5.1)", [&] {
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

	// the two accounts of D5; the schema suffix keeps parallel build trees apart. The character names are letters only and in the retail
	// format (first letter upper case, the rest lower case): NameConfig's gameserver.name.character_pattern is [a-zA-Z]{2,16} and
	// Util.convertName normalises the case, so a digit or an underscore would be rejected with RESPONSE_INVALID_NAME.
	ScenarioClient a;
	ScenarioClient b;
	a.account = "m5aa" + servers.gameSchema().substr(servers.gameSchema().size() - 8);
	b.account = "m5ab" + servers.gameSchema().substr(servers.gameSchema().size() - 8);
	a.characterName = "Scenariowarrior";
	b.characterName = "Scenariomage";
	const CharacterAppearance appearance = scenarioAppearance();

	OracleCreation elyos;
	OracleCreation asmodian;
	runCase("case 0b", "the creation oracle answers (F-05)", [&] {
		elyos = oracle->creation("ELYOS", "WARRIOR");
		asmodian = oracle->creation("ASMODIANS", "MAGE");
		ASSERT_FALSE(elyos.items.empty());
		ASSERT_FALSE(elyos.skills.empty());
		EXPECT_GT(elyos.baseMaxHp, 0);
		EXPECT_GT(asmodian.baseMaxMp, 0);
	});
	AsyncAllowed async = AsyncAllowed::m5aDefault();

	// ---- case 1: login (§5.2) ----
	runCase("case 1", "login (account A)", [&] {
		const decoders::CharacterList list = logIn(servers, a, async, true);
		EXPECT_EQ(list.playOk2, a.key.playOk2);
		EXPECT_EQ(list.characterCount, 0) << "a fresh account must have no character";
		EXPECT_TRUE(list.characters.empty());
	});

	// ---- case 2: create (§5.3) ----
	runCase("case 2", "create an Elyos Warrior", [&] {
		NewCharacter warrior;
		warrior.name = a.characterName;
		warrior.asmodian = false;
		warrior.playerClassId = CLASS_WARRIOR;
		warrior.appearance = appearance;

		a.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(a.key.accountId, a.account, warrior, 1));
		EXPECT_EQ(decoders::decodeCreateCharacter(expectNext(*a.game, "SM_CREATE_CHARACTER", async).data).responseCode, RESPONSE_OPEN_CREATION_WINDOW);

		a.game->send(GameSession::CM_CHECK_NICKNAME, GameSession::buildCM_CHECK_NICKNAME(a.characterName));
		const Packet nickname = expectNext(*a.game, "SM_NICKNAME_CHECK_RESPONSE", async);
		ASSERT_EQ(nickname.data.size(), 1u) << "SM_NICKNAME_CHECK_RESPONSE is one byte (SM_NICKNAME_CHECK_RESPONSE.java)";
		EXPECT_EQ(nickname.data[0], 0);

		a.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(a.key.accountId, a.account, warrior, 0));
		const decoders::CreateCharacter created = decoders::decodeCreateCharacter(expectNext(*a.game, "SM_CREATE_CHARACTER", async).data);
		ASSERT_EQ(created.responseCode, RESPONSE_OK);
		ASSERT_TRUE(created.player) << "SM_CREATE_CHARACTER(0) carries the player info block";
		a.playerId = created.player->playerId;
		EXPECT_EQ(created.player->name, a.characterName);
		EXPECT_EQ(created.player->raceId, 0) << "PlayerInfo race: ELYOS is 0";
		EXPECT_EQ(created.player->classId, CLASS_WARRIOR);
		EXPECT_EQ(created.player->level, 1);
		EXPECT_EQ(created.player->mapId, elyos.mapId);
		EXPECT_EQ(elyos.mapId, ELYOS_START_MAP);
		EXPECT_NEAR(created.player->x, elyos.x, 0.01);
		EXPECT_NEAR(created.player->y, elyos.y, 0.01);
		EXPECT_NEAR(created.player->z, elyos.z, 0.01);
		EXPECT_EQ(created.player->heading, elyos.heading);

		// the same name again: RESPONSE_NAME_ALREADY_USED
		a.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(a.key.accountId, a.account, warrior, 0));
		EXPECT_EQ(decoders::decodeCreateCharacter(expectNext(*a.game, "SM_CREATE_CHARACTER", async).data).responseCode, RESPONSE_NAME_ALREADY_USED);

		// an Asmodian on the same account: RESPONSE_OTHER_RACE (D5, character.creation.mode 0)
		NewCharacter otherRace;
		otherRace.name = "Scenarioother";
		otherRace.asmodian = true;
		otherRace.playerClassId = CLASS_MAGE;
		otherRace.appearance = appearance;
		a.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(a.key.accountId, a.account, otherRace, 0));
		EXPECT_EQ(decoders::decodeCreateCharacter(expectNext(*a.game, "SM_CREATE_CHARACTER", async).data).responseCode, RESPONSE_OTHER_RACE);

		// ---- the database rows (§5.3 step 5) ----
		const std::string player = std::to_string(a.playerId);
		const auto rows = database.queryRows(
		  schema, "SELECT name, race, player_class, exp, old_level, world_id, x, y, z, heading, online FROM players WHERE id = " + player, 11);
		ASSERT_EQ(rows.size(), 1u) << "no players row for the created character";
		const auto& row = rows[0];
		EXPECT_EQ(row[0].value_or(""), a.characterName);
		EXPECT_EQ(row[1].value_or(""), "ELYOS");
		EXPECT_EQ(row[2].value_or(""), "WARRIOR");
		EXPECT_EQ(row[3].value_or(""), "0") << "level 1 is exp 0";
		EXPECT_EQ(row[4].value_or(""), "0") << "old_level is 0 until the first enter world";
		EXPECT_EQ(std::stoi(row[5].value_or("0")), elyos.mapId);
		EXPECT_NEAR(std::stod(row[6].value_or("0")), elyos.x, 0.01);
		EXPECT_NEAR(std::stod(row[7].value_or("0")), elyos.y, 0.01);
		EXPECT_NEAR(std::stod(row[8].value_or("0")), elyos.z, 0.01);
		EXPECT_EQ(std::stoi(row[9].value_or("-1")), elyos.heading);
		EXPECT_EQ(row[10].value_or(""), "0") << "the character is not online yet";

		for (const auto& [column, expected] : expectedAppearanceRow(appearance)) {
			const std::optional<int64_t> stored = database.queryLong(schema, "SELECT `" + column + "` FROM player_appearance WHERE player_id = " + player);
			EXPECT_EQ(stored, expected) << "player_appearance." << column;
		}

		for (const OracleItem& item : elyos.items) {
			const auto itemRows = database.queryRows(
			  schema, "SELECT item_count, is_equipped, slot FROM inventory WHERE item_owner = " + player + " AND item_id = " + std::to_string(item.itemId),
			  3);
			ASSERT_EQ(itemRows.size(), 1u) << "inventory row of item " << item.itemId;
			EXPECT_EQ(std::stoll(itemRows[0][0].value_or("0")), item.count) << "count of item " << item.itemId;
			EXPECT_EQ(itemRows[0][1].value_or("") == "1", item.equipped) << "is_equipped of item " << item.itemId;
			if (item.equipped)
				EXPECT_EQ(std::stoll(itemRows[0][2].value_or("0")), item.slot) << "equipment slot of item " << item.itemId;
		}
		EXPECT_EQ(database.queryLong(schema, "SELECT COUNT(*) FROM inventory WHERE item_owner = " + player), static_cast<int64_t>(elyos.items.size()));

		std::vector<std::string> skills;
		for (const auto& skillRow :
		     database.queryRows(schema, "SELECT skill_id, skill_level FROM player_skills WHERE player_id = " + player + " ORDER BY skill_id", 2))
			skills.push_back(skillRow[0].value_or("?") + "/" + skillRow[1].value_or("?"));
		std::vector<std::string> expectedSkills;
		for (const OracleSkill& skill : elyos.skills)
			expectedSkills.push_back(std::to_string(skill.skillId) + "/" + std::to_string(skill.level));
		std::sort(expectedSkills.begin(), expectedSkills.end());
		std::sort(skills.begin(), skills.end());
		EXPECT_EQ(skills, expectedSkills) << "the autolearn skill set of a level 1 Warrior";
	});

	// ---- case 2b: account B, Asmodian Mage (§5.3 step 6) ----
	runCase("case 2b", "login B and create an Asmodian Mage", [&] {
		const decoders::CharacterList list = logIn(servers, b, async, false);
		EXPECT_EQ(list.characterCount, 0);

		NewCharacter mage;
		mage.name = b.characterName;
		mage.asmodian = true;
		mage.playerClassId = CLASS_MAGE;
		mage.appearance = appearance;
		mage.female = true;
		b.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(b.key.accountId, b.account, mage, 0));
		const decoders::CreateCharacter created = decoders::decodeCreateCharacter(expectNext(*b.game, "SM_CREATE_CHARACTER", async).data);
		ASSERT_EQ(created.responseCode, RESPONSE_OK);
		ASSERT_TRUE(created.player);
		b.playerId = created.player->playerId;
		EXPECT_EQ(created.player->raceId, 1) << "PlayerInfo race: ASMODIANS is 1";
		EXPECT_EQ(created.player->classId, CLASS_MAGE);
		EXPECT_EQ(created.player->mapId, asmodian.mapId);
		EXPECT_EQ(asmodian.mapId, ASMODIAN_START_MAP);

		const std::string player = std::to_string(b.playerId);
		const auto rows =
		  database.queryRows(schema, "SELECT race, player_class, world_id, x, y, z, heading, online FROM players WHERE id = " + player, 8);
		ASSERT_EQ(rows.size(), 1u);
		EXPECT_EQ(rows[0][0].value_or(""), "ASMODIANS");
		EXPECT_EQ(rows[0][1].value_or(""), "MAGE");
		EXPECT_EQ(std::stoi(rows[0][2].value_or("0")), asmodian.mapId);
		EXPECT_NEAR(std::stod(rows[0][3].value_or("0")), asmodian.x, 0.01);
		EXPECT_NEAR(std::stod(rows[0][4].value_or("0")), asmodian.y, 0.01);
		EXPECT_NEAR(std::stod(rows[0][5].value_or("0")), asmodian.z, 0.01);
		EXPECT_EQ(std::stoi(rows[0][6].value_or("-1")), asmodian.heading);
		EXPECT_EQ(rows[0][7].value_or(""), "0");
		EXPECT_EQ(database.queryLong(schema, "SELECT COUNT(*) FROM inventory WHERE item_owner = " + player), static_cast<int64_t>(asmodian.items.size()));
		EXPECT_EQ(database.queryLong(schema, "SELECT COUNT(*) FROM player_skills WHERE player_id = " + player),
		          static_cast<int64_t>(asmodian.skills.size()));
	});

	// ---- case 3: enter world (§5.4) ----
	int32_t gameHour = 0;
	std::vector<Packet> enterBurst;
	/** the last SM_STATS_INFO of the enter-world burst, cross-checked against the SM_PLAYER_INFO of case 4 (V9) */
	std::optional<decoders::StatsInfo> enterStats;
	runCase("case 3", "enter world (A, first enter)", [&] {
		async.selfPlayerState(a.playerId);
		a.game->send(GameSession::CM_MAY_LOGIN_INTO_GAME, GameSession::buildCM_MAY_LOGIN_INTO_GAME());
		expectNext(*a.game, "SM_MAY_LOGIN_INTO_GAME", async);
		const size_t before = a.game->recorded().size();
		a.game->send(GameSession::CM_ENTER_WORLD, GameSession::buildCM_ENTER_WORLD(a.playerId));
		enterBurst = a.game->collectUntilQuiet(QUIET, BURST_LIMIT);
		ASSERT_FALSE(enterBurst.empty()) << "no packet after CM_ENTER_WORLD (recorded before: " << before << ")";

		// §5.8 #13: PlayerEnterWorldService.sendItemInfos splits the kinah item plus every equipped and inventory item into parts of 10 and
		// appends one empty packet; the oracle's item list is exactly those rows, kinah included
		const int32_t inventoryPackets = static_cast<int32_t>((elyos.items.size() + 9) / 10) + 1;
		expectSequence(enterBurst, enterWorldPattern(true, inventoryPackets), async);

		const Packet* check = firstOfName(enterBurst, "SM_ENTER_WORLD_CHECK");
		ASSERT_NE(check, nullptr);
		ASSERT_EQ(check->data.size(), 3u) << "SM_ENTER_WORLD_CHECK is three bytes";
		EXPECT_EQ(check->data[0], 0) << "SM_ENTER_WORLD_CHECK must not report an error (CONNECTION_ERROR is 1)";

		// V6: SM_PLAYER_SPAWN world and position equal the spawn point
		const Packet* spawn = firstOfName(enterBurst, "SM_PLAYER_SPAWN");
		ASSERT_NE(spawn, nullptr);
		const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(spawn->data);
		EXPECT_EQ(spawned.worldId, elyos.mapId);
		EXPECT_NEAR(spawned.x, elyos.x, 0.01);
		EXPECT_NEAR(spawned.y, elyos.y, 0.01);
		EXPECT_NEAR(spawned.z, elyos.z, 0.01);
		EXPECT_EQ(spawned.heading, elyos.heading);

		// V7: SM_INVENTORY_INFO item ids, counts and equip slots equal the inventory rows
		// the entries are counted, not keyed by template id alone: a map would collapse a duplicated item into one entry and still satisfy the
		// size assertion below
		std::vector<decoders::InventoryItem> sentItems;
		for (const Packet& packet : ofName(enterBurst, "SM_INVENTORY_INFO"))
			for (decoders::InventoryItem& item : decoders::decodeInventoryInfo(packet.data).items)
				sentItems.push_back(item);
		std::map<int32_t, decoders::InventoryItem> sent;
		std::map<int32_t, size_t> sentCount;
		for (const decoders::InventoryItem& item : sentItems) {
			sent[item.templateId] = item;
			sentCount[item.templateId]++;
		}
		for (const OracleItem& item : elyos.items) {
			const auto found = sent.find(item.itemId);
			ASSERT_NE(found, sent.end()) << "SM_INVENTORY_INFO has no item " << item.itemId;
			EXPECT_EQ(sentCount[item.itemId], 1u) << "SM_INVENTORY_INFO announces item " << item.itemId << " more than once";
			ASSERT_TRUE(found->second.general) << "item " << item.itemId << " has no general info blob entry";
			EXPECT_EQ(found->second.general->count, item.count) << "count of item " << item.itemId;
			// SM_INVENTORY_INFO.java:56 writes (item.getEquipmentSlot() & 0xFFFF): the ItemSlot mask for an equipped item, and for an item that
			// was never equipped or moved the untouched ItemStorage.FIRST_AVAILABLE_SLOT, which is 65535 (ItemStorage.java:16, Item.java:45)
			const uint16_t expectedSlot = item.equipped ? static_cast<uint16_t>(item.slot & 0xFFFF) : uint16_t{0xFFFF};
			EXPECT_EQ(found->second.equipmentSlot, expectedSlot)
			  << "equipment slot of item " << item.itemId << (item.equipped ? " (equipped)" : " (not equipped)");

			// The second, independent statement about the same fact: ItemInfoBlob.getFullBlob adds the EQUIPPED_SLOT entry (0x06) for every item
			// whose ItemGroup has valid equipment slots (ItemInfoBlob.java:67-69), and EquippedSlotBlobEntry.java:20 writes
			// `isEquipped() ? getEquipmentSlot() : 0` into it - a long, while the short above is the low half of the same value written 60 lines
			// later in SM_INVENTORY_INFO.java. An item the server wrongly believes to be equipped therefore has to lie twice to pass, and the
			// not-equipped expectation is asserted instead of being computed and dropped. Measured on the starter inventory: the three equipped
			// items carry the entry and are compared; the ten other rows are kinah, potions and food, whose ItemGroup has no valid equipment
			// slot, so they carry no 0x06 entry at all and the `else if` below is the branch that would catch a wrongly equipped consumable.
			if (item.equipped) {
				if (!found->second.equippedSlotBlob)
					ADD_FAILURE() << "the equipped item " << item.itemId << " has no EQUIPPED_SLOT blob entry (0x06)";
				else
					EXPECT_EQ(*found->second.equippedSlotBlob, item.slot) << "EQUIPPED_SLOT blob entry of the equipped item " << item.itemId;
			} else if (found->second.equippedSlotBlob) {
				EXPECT_EQ(*found->second.equippedSlotBlob, 0)
				  << "item " << item.itemId << " is not equipped, so its EQUIPPED_SLOT blob entry must be 0";
			}
		}
		EXPECT_EQ(sentItems.size(), elyos.items.size()) << "SM_INVENTORY_INFO announced " << sentItems.size() << " entries for "
		                                                << elyos.items.size() << " inventory rows";
		EXPECT_EQ(sent.size(), elyos.items.size());

		// V8: SM_SKILL_LIST ids and levels equal the autolearn set
		std::vector<std::string> sentSkills;
		for (const Packet& packet : ofName(enterBurst, "SM_SKILL_LIST"))
			for (const decoders::SkillEntry& entry : decoders::decodeSkillList(packet.data).skills)
				sentSkills.push_back(std::to_string(entry.skillId) + "/" + std::to_string(entry.skillLevel));
		std::vector<std::string> expectedSkills;
		for (const OracleSkill& skill : elyos.skills)
			expectedSkills.push_back(std::to_string(skill.skillId) + "/" + std::to_string(skill.level));
		std::sort(sentSkills.begin(), sentSkills.end());
		std::sort(expectedSkills.begin(), expectedSkills.end());
		EXPECT_EQ(sentSkills, expectedSkills);

		const Packet* time = firstOfName(enterBurst, "SM_GAME_TIME");
		ASSERT_NE(time, nullptr);
		ASSERT_EQ(time->data.size(), 4u) << "SM_GAME_TIME is one int (SM_GAME_TIME.java)";
		const int32_t minutes =
		  static_cast<int32_t>(time->data[0] | time->data[1] << 8 | time->data[2] << 16 | static_cast<uint32_t>(time->data[3]) << 24);
		gameHour = gameHourOf(minutes);

		// V9: base max HP/MP of SM_STATS_INFO equal the oracle's PlayerStatCalculator values, current max >= base.
		// SCOPE (m5a-plan.md §5.4 V9 and the M5b item O-13): passive skill effects are not applied at M5a (O-09, the allow-listed
		// AION_PARTIAL in SkillEngine::applyEffectDirectly), and the creation oracle answers base max HP and MP only, so no oracle exists for
		// attack, accuracy, evasion, crit, magic boost or the current maxima. Everything that IS checkable without that oracle is asserted
		// here: the identity fields, the two oracle values in EVERY SM_STATS_INFO of the burst (the first-enter one of §5.8 #0 as well as #26,
		// not only the last), the life stats of a character that has never fought, the exp of a level 1 character, the game time the packet
		// carries against the SM_GAME_TIME of the same burst, and "a class stats template was applied at all" for the six base attributes and
		// the base attack values, which a stat container that silently returned zeros would fail.
		const std::vector<Packet> stats = ofName(enterBurst, "SM_STATS_INFO");
		ASSERT_FALSE(stats.empty());
		for (size_t i = 0; i < stats.size(); i++) {
			const decoders::StatsInfo info = decoders::decodeStatsInfo(stats[i].data);
			const std::string which = "SM_STATS_INFO " + std::to_string(i + 1) + " of " + std::to_string(stats.size());
			EXPECT_EQ(info.objectId, a.playerId) << which;
			EXPECT_EQ(info.level, 1) << which;
			EXPECT_EQ(info.classId, CLASS_WARRIOR) << which;
			EXPECT_EQ(info.baseMaxHp, elyos.baseMaxHp) << which;
			EXPECT_EQ(info.baseMaxMp, elyos.baseMaxMp) << which;
			EXPECT_GE(info.maxHp, info.baseMaxHp) << which;
			EXPECT_GE(info.maxMp, info.baseMaxMp) << which;
		}
		const decoders::StatsInfo statsInfo = decoders::decodeStatsInfo(stats.back().data);
		enterStats = statsInfo;
		// a character that was created seconds ago has full HP and MP, no DP and no experience (Q3 proves the opposite case, where the harness
		// seeds player_life_stat.hp with half and the relogin has to show it)
		EXPECT_EQ(statsInfo.currentHp, statsInfo.maxHp) << "V9: a character that never fought enters the world with full HP";
		EXPECT_EQ(statsInfo.currentMp, statsInfo.maxMp) << "V9: a character that never fought enters the world with full MP";
		EXPECT_EQ(statsInfo.currentDp, 0) << "V9: a fresh character has no DP";
		EXPECT_EQ(statsInfo.expShown, 0) << "V9: level 1 is exp 0";
		EXPECT_EQ(statsInfo.expRecoverable, 0) << "V9: a character that never died has no recoverable exp";
		EXPECT_GT(statsInfo.expNeed, 0) << "V9: the exp needed for level 2";
		EXPECT_GT(statsInfo.inventoryLimit, 0) << "V9: the cube size";
		// SM_STATS_INFO.java:35 and SM_GAME_TIME both write GameTimeService.getGameTime().getTime(); the two packets are milliseconds apart in
		// the same burst, so they may differ by at most the game minutes of that burst
		EXPECT_NEAR(statsInfo.gameTime, minutes, 2) << "V9: the game time of SM_STATS_INFO against the SM_GAME_TIME of the same burst";
		// the class stats template of a level 1 Warrior: every base attribute and the base attack values are positive for every class
		EXPECT_GT(statsInfo.basePower, 0) << "V9: base power";
		EXPECT_GT(statsInfo.baseHealth, 0) << "V9: base health";
		EXPECT_GT(statsInfo.baseAccuracy, 0) << "V9: base accuracy";
		EXPECT_GT(statsInfo.baseAgility, 0) << "V9: base agility";
		EXPECT_GT(statsInfo.baseKnowledge, 0) << "V9: base knowledge";
		EXPECT_GT(statsInfo.baseWill, 0) << "V9: base will";
		EXPECT_GT(statsInfo.baseMainHandPAttack, 0) << "V9: base main hand attack";
		EXPECT_GT(statsInfo.baseAttackRange, 0.0f) << "V9: base attack range";
		EXPECT_GT(statsInfo.attackSpeed, 0) << "V9: attack speed";
		EXPECT_GT(statsInfo.castingSpeed, 0.0f) << "V9: casting speed";

		// V10: the quest lists are empty, the warehouses and macros decode as empty
		const Packet* questList = firstOfName(enterBurst, "SM_QUEST_LIST");
		ASSERT_NE(questList, nullptr);
		EXPECT_TRUE(decoders::decodeQuestList(questList->data).quests.empty());
		for (const Packet& packet : ofName(enterBurst, "SM_QUEST_COMPLETED_LIST"))
			EXPECT_TRUE(decoders::decodeQuestCompletedList(packet.data).quests.empty());
		const std::vector<Packet> warehouses = ofName(enterBurst, "SM_WAREHOUSE_INFO");
		EXPECT_EQ(warehouses.size(), 43u);
		for (const Packet& packet : warehouses)
			EXPECT_TRUE(decoders::decodeWarehouseInfo(packet.data).items.empty());
		for (const Packet& packet : ofName(enterBurst, "SM_MACRO_LIST"))
			EXPECT_TRUE(decoders::decodeMacroList(packet.data).macros.empty());
	});

	// ---- case 4: level ready and NPC visibility (§5.5) ----
	std::vector<Packet> levelReadyBurst;
	OracleSpawns spawns;
	runCase("case 4", "level ready and NPC visibility", [&] {
		a.game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
		levelReadyBurst = a.game->collectUntilQuiet(QUIET, BURST_LIMIT);
		ASSERT_FALSE(levelReadyBurst.empty()) << "no packet after CM_LEVEL_READY";
		expectSequence(levelReadyBurst, levelReadyPattern(), async);

		// V9 (the SM_PLAYER_INFO half): §5.8 #33 is the packet the client renders the own character and its equipment from, and it is its own
		// 230-line writeImpl (SM_PLAYER_INFO.java), not the writePlayerInfo block of SM_CHARACTER_LIST. decodePlayerInfo ends in
		// expectFullyConsumed, so decoding it here also proves its framing; without this call a shifted field in it would not fail the gate.
		const std::optional<decoders::PlayerInfo> playerInfo =
		  expectPlayerInfo(levelReadyBurst, a.playerId, a.characterName, elyos, CLASS_WARRIOR, RACE_ELYOS, GENDER_MALE, appearance, "V9");
		// the one derived stat two different writeImpl methods answer: SM_PLAYER_INFO.java:171-173 writes getAttackSpeed().getBase() and
		// .getCurrent(), SM_STATS_INFO.java:91 the same Stat2's current value. A stat container that recomputes per packet fails here.
		if (playerInfo && enterStats) {
			EXPECT_EQ(playerInfo->attackSpeedCurrent, enterStats->attackSpeed)
			  << "V9: the attack speed of SM_PLAYER_INFO and of SM_STATS_INFO differ";
			EXPECT_EQ(static_cast<int32_t>(playerInfo->level), static_cast<int32_t>(enterStats->level)) << "V9: the level of the two packets differs";
			EXPECT_EQ(playerInfo->objectId, enterStats->objectId) << "V9: the object id of the two packets differs";
			EXPECT_EQ(static_cast<int32_t>(playerInfo->classId), enterStats->classId) << "V9: the class id of the two packets differs";
			// SM_PLAYER_INFO.java:90 writes getHpPercentage(), which is 100 exactly when current HP equals max HP (SM_STATS_INFO.java:64-65)
			EXPECT_EQ(playerInfo->hpPercentage == 100, enterStats->currentHp == enterStats->maxHp)
			  << "V9: the HP% of SM_PLAYER_INFO does not match the current and max HP of SM_STATS_INFO";
		}

		spawns = oracle->spawns(elyos.mapId, elyos.x, elyos.y, elyos.z, gameHour);
		checkVisibility(levelReadyBurst, spawns, "V1-V4 (Warrior on 210010000)");
	});

	// ---- case 4b: the in-world client packets a real client sends by itself (item C-01) ----
	// Not part of §5: the scripted path sends 15 packet types, and Q8's notPortedClientPacket check can only report packets the script itself
	// sent, so a client packet nobody scripts is invisible to the gate until a real 4.8 client sends it. These two need no user action.
	// CM_SUBZONE_CHANGE matters most: it is the client-driven entry into Player::revalidateZones and therefore into the siege and vortex zone
	// handlers this wave ported. It runs as its own case and deliberately does NOT gate the cases after it - it is additive coverage, and a
	// failure here should not cost the run its §5.6 and §5.7 evidence (the assertions still fail the gate).
	if (ok)
		cases.run("case 4b", "CM_CUSTOM_SETTINGS and CM_SUBZONE_CHANGE (C-01)", [&] {
			// CM_CUSTOM_SETTINGS stores display/deny in PlayerSettings and then broadcasts SM_CUSTOM_SETTINGS(player) with toSelf, so the
			// sender gets it back
			a.game->send(GameSession::CM_CUSTOM_SETTINGS, GameSession::buildCM_CUSTOM_SETTINGS(0, 0));
			const std::vector<Packet> settings = a.game->collectUntilQuiet(QUIET, 30s);
			EXPECT_FALSE(ofName(settings, "SM_CUSTOM_SETTINGS").empty())
			  << "CM_CUSTOM_SETTINGS did not come back as SM_CUSTOM_SETTINGS; got: " << join(namesOf(settings));

			// CM_SUBZONE_CHANGE calls Player::revalidateZones and is silent for a non-staff account (the per-zone messages are behind
			// AdminConfig::ZONE_INFO), so the only assertion is that the zone handlers did not kill the connection
			a.game->send(GameSession::CM_SUBZONE_CHANGE, GameSession::buildCM_SUBZONE_CHANGE(1));
			const std::vector<Packet> subzone = a.game->collectUntilQuiet(QUIET, 30s);
			EXPECT_TRUE(ofName(subzone, "SM_SYSTEM_MESSAGE").empty())
			  << "CM_SUBZONE_CHANGE answered a non-staff account: " << join(namesOf(subzone));
			EXPECT_FALSE(a.game->client.socket.isClosed()) << "the connection died on CM_SUBZONE_CHANGE (Player::revalidateZones)";
		});

	// ---- case 5: region move (§5.6) ----
	OracleBorderTarget target;
	runCase("case 5", "region move", [&] {
		target = oracle->borderTarget(elyos.mapId, elyos.x, elyos.y, elyos.z, gameHour);
		// the temporary-spawn updates of an hour change stay out of the async set here (§5.9): in this window M1 and M2 expect every
		// SM_NPC_INFO and SM_DELETE to be part of the region move
		const double total = distance2d(target.startX, target.startY, target.targetX, target.targetY);
		const int32_t steps = std::max(1, static_cast<int32_t>(total / 5.0));
		std::vector<std::array<float, 3>> path;
		float x = target.startX, y = target.startY, z = target.startZ;
		for (int32_t step = 1; step <= steps; step++) {
			const float t = static_cast<float>(step) / static_cast<float>(steps);
			x = target.startX + (target.targetX - target.startX) * t;
			y = target.startY + (target.targetY - target.startY) * t;
			z = target.startZ + (target.targetZ - target.startZ) * t;
			path.push_back({x, y, z});
			a.game->send(GameSession::CM_MOVE,
			             GameSession::buildCM_MOVE(x, y, z, 0, static_cast<int8_t>(0xE0), target.targetX, target.targetY, target.targetZ));
			std::this_thread::sleep_for(120ms);
		}
		a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(target.targetX, target.targetY, target.targetZ, 0, 0));
		const std::vector<Packet> moveBurst = a.game->collectUntilQuiet(QUIET, BURST_LIMIT);

		// M0: SM_PLAYER_STATE of the own player (the end of the spawn protection)
		bool ownState = false;
		for (const Packet& packet : moveBurst)
			if (packet.name == "SM_PLAYER_STATE" && decoders::decodePlayerStateObjectId(packet.data) == a.playerId)
				ownState = true;
		EXPECT_TRUE(ownState) << "M0: no SM_PLAYER_STATE for the own player after the first CM_MOVE";

		// M1: new npcs are within 100 m of a path point and include the deterministic spots near the target
		std::set<int32_t> appeared;
		for (const Packet& packet : ofName(moveBurst, "SM_NPC_INFO")) {
			const decoders::NpcInfo npc = decoders::decodeNpcInfo(packet.data);
			appeared.insert(npc.templateId);
			bool nearPath = false;
			for (const auto& point : path)
				if (distance2d(npc.x, npc.y, point[0], point[1]) <= 100.0)
					nearPath = true;
			EXPECT_TRUE(nearPath) << "M1: SM_NPC_INFO for npc " << npc.templateId << " at (" << npc.x << ", " << npc.y
			                      << "), farther than 100 m from every path point";
		}
		for (const OracleSpot& spot : target.appear)
			EXPECT_TRUE(appeared.contains(spot.npcId)) << "M1: npc " << spot.npcId << " should have appeared on the way to the target";

		// M2: deleted objects were known before and include the start objects far from the target
		// Everything the server announced, NOT only the npcs: a region move leaves gatherables behind too, and the world holds 18,432 of them
		// since spawnGatherable was ported, so an SM_DELETE for a gatherable is expected and must not read as "never announced".
		std::set<int32_t> knownObjectIds;
		const std::array<const std::vector<Packet>*, 2> announcedIn = {&levelReadyBurst, &moveBurst};
		for (const std::vector<Packet>* burst : announcedIn) {
			for (const Packet& packet : ofName(*burst, "SM_NPC_INFO"))
				knownObjectIds.insert(decoders::decodeNpcInfo(packet.data).objectId);
			for (const Packet& packet : ofName(*burst, "SM_GATHERABLE_INFO"))
				knownObjectIds.insert(decoders::decodeGatherableInfo(packet.data).objectId);
		}
		std::set<int32_t> deletedObjectIds;
		for (const Packet& packet : ofName(moveBurst, "SM_DELETE")) {
			const int32_t objectId = decoders::decodeDeleteObjectId(packet.data);
			deletedObjectIds.insert(objectId);
			EXPECT_TRUE(knownObjectIds.contains(objectId)) << "M2: SM_DELETE for object " << objectId << ", which was never announced";
		}
		std::map<int32_t, std::set<int32_t>> objectIdsByNpcId;
		for (const Packet& packet : ofName(levelReadyBurst, "SM_NPC_INFO")) {
			const decoders::NpcInfo npc = decoders::decodeNpcInfo(packet.data);
			objectIdsByNpcId[npc.templateId].insert(npc.objectId);
		}
		for (const OracleSpot& spot : target.disappear) {
			const auto found = objectIdsByNpcId.find(spot.npcId);
			if (found == objectIdsByNpcId.end())
				continue; // it was never visible, so it cannot disappear
			bool deleted = false;
			for (int32_t objectId : found->second)
				if (deletedObjectIds.contains(objectId))
					deleted = true;
			EXPECT_TRUE(deleted) << "M2: npc " << spot.npcId << " should have been deleted on the way to the target";
		}

		// M3: no forced move and no SM_MOVE to self
		EXPECT_TRUE(ofName(moveBurst, "SM_FORCED_MOVE").empty()) << "M3: the server corrected the position";
		for (const Packet& packet : ofName(moveBurst, "SM_MOVE")) {
			ASSERT_GE(packet.data.size(), 4u);
			const int32_t objectId =
			  static_cast<int32_t>(packet.data[0] | packet.data[1] << 8 | packet.data[2] << 16 | static_cast<uint32_t>(packet.data[3]) << 24);
			EXPECT_NE(objectId, a.playerId) << "M3: the server sent SM_MOVE for the moving player itself";
		}
	});

	// ---- case 6: quit, persistence, relogin (§5.7 Q1-Q5) ----
	runCase("case 6", "quit, persistence and relogin", [&] {
		// Q1: CM_QUIT(1) keeps the connection and returns to the character list
		a.game->send(GameSession::CM_QUIT, GameSession::buildCM_QUIT(true));
		expectNext(*a.game, "SM_QUIT_RESPONSE", async, 30s);
		a.game->send(GameSession::CM_CHARACTER_LIST, GameSession::buildCM_CHARACTER_LIST(a.key.playOk2));
		expectNext(*a.game, "SM_ACCOUNT_PROPERTIES", async);
		const decoders::CharacterList list = decoders::decodeCharacterList(expectNext(*a.game, "SM_CHARACTER_LIST", async).data);
		EXPECT_EQ(list.characterCount, 1);
		ASSERT_EQ(list.characters.size(), 1u);

		// Q2: the stored position is the region move target and the character is offline again
		const std::string player = std::to_string(a.playerId);
		const auto rows = database.queryRows(schema, "SELECT online, world_id, x, y, z, old_level, last_online FROM players WHERE id = " + player, 7);
		ASSERT_EQ(rows.size(), 1u);
		EXPECT_EQ(rows[0][0].value_or(""), "0") << "Q2: the character is still online";
		EXPECT_EQ(std::stoi(rows[0][1].value_or("0")), elyos.mapId);
		EXPECT_NEAR(std::stod(rows[0][2].value_or("0")), target.targetX, 5.0) << "Q2: the stored x is not the move target";
		EXPECT_NEAR(std::stod(rows[0][3].value_or("0")), target.targetY, 5.0) << "Q2: the stored y is not the move target";
		EXPECT_EQ(rows[0][5].value_or(""), "1") << "Q2: old_level is 1 after the first enter world";
		EXPECT_TRUE(rows[0][6].has_value()) << "Q2: last_online is not set";
		EXPECT_EQ(database.queryLong(schema, "SELECT COUNT(*) FROM abyss_rank WHERE player_id = " + player), 1);
		EXPECT_EQ(database.queryLong(schema, "SELECT COUNT(*) FROM player_life_stats WHERE player_id = " + player), 1);
		EXPECT_EQ(database.queryLong(schema, "SELECT COUNT(*) FROM inventory WHERE item_owner = " + player), static_cast<int64_t>(elyos.items.size()));
		EXPECT_EQ(database.queryLong(schema, "SELECT COUNT(*) FROM player_skills WHERE player_id = " + player),
		          static_cast<int64_t>(elyos.skills.size()));

		// Q3: half HP is restored from the database and the restore task pins the player.
		// TODO(m5a-plan §5.7 Q3): the idian stone the plan seeds on the equipped weapon is still missing. It needs a row in `item_stones`
		// (category 3 = ItemStone.ItemStoneType.IDIANSTONE) whose item_id is a real idian stone template: ItemStone's constructor runs
		// Objects.requireNonNull(getItemTemplate()), so a guessed id would break the load instead of testing the ENCHANT_INFO blob.
		const std::optional<int64_t> maxHp = database.queryLong(schema, "SELECT hp FROM player_life_stats WHERE player_id = " + player);
		ASSERT_TRUE(maxHp);
		const int64_t halfHp = std::max<int64_t>(1, *maxHp / 2);
		database.execute(schema, "UPDATE player_life_stats SET hp = " + std::to_string(halfHp) + " WHERE player_id = " + player);
		std::this_thread::sleep_for(1500ms); // gameserver.character.reentry.time is 1 second in the scenario profile

		a.game->send(GameSession::CM_ENTER_WORLD, GameSession::buildCM_ENTER_WORLD(a.playerId));
		const std::vector<Packet> reenter = a.game->collectUntilQuiet(QUIET, BURST_LIMIT);
		const int32_t inventoryPackets = static_cast<int32_t>((elyos.items.size() + 9) / 10) + 1;
		expectSequence(reenter, enterWorldPattern(false, inventoryPackets), async);
		const Packet* spawn = firstOfName(reenter, "SM_PLAYER_SPAWN");
		ASSERT_NE(spawn, nullptr);
		const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(spawn->data);
		EXPECT_NEAR(spawned.x, target.targetX, 5.0) << "Q3: the character did not respawn at the move target";
		EXPECT_NEAR(spawned.y, target.targetY, 5.0);
		const std::vector<Packet> stats = ofName(reenter, "SM_STATS_INFO");
		ASSERT_FALSE(stats.empty());
		const decoders::StatsInfo statsInfo = decoders::decodeStatsInfo(stats.back().data);
		EXPECT_EQ(statsInfo.currentHp, static_cast<int32_t>(halfHp)) << "Q3: the stored half HP was not restored";
		a.game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
		a.game->collectUntilQuiet(QUIET, BURST_LIMIT);

		// Q4: CM_QUIT(0) ends the connection
		a.game->send(GameSession::CM_QUIT, GameSession::buildCM_QUIT(false));
		expectNext(*a.game, "SM_QUIT_RESPONSE", async, 30s);
		EXPECT_TRUE(a.game->waitClosed(30s)) << "Q4: the socket stayed open after CM_QUIT(0)";
		a.game.reset();
		a.login.reset();

		// Q5: a new login server and game server login, with the stored character in the list
		ScenarioClient again;
		again.account = a.account;
		again.characterName = a.characterName;
		const decoders::CharacterList stored = logIn(servers, again, async, false);
		ASSERT_EQ(stored.characters.size(), 1u);
		const decoders::PlayerInfoBlock& block = stored.characters[0];
		EXPECT_EQ(block.playerId, a.playerId);
		EXPECT_EQ(block.name, a.characterName);
		EXPECT_EQ(block.level, 1);
		EXPECT_EQ(block.mapId, elyos.mapId);
		EXPECT_NEAR(block.x, target.targetX, 5.0);
		EXPECT_NEAR(block.y, target.targetY, 5.0);
		EXPECT_EQ(block.appearance.face, appearance.face) << "Q5: the appearance of the stored character changed";
		EXPECT_EQ(block.appearance.hairRGB, appearance.hairRGB);
		EXPECT_FLOAT_EQ(block.appearance.height, appearance.height);
		std::set<int32_t> visible;
		for (const decoders::VisibleItem& item : block.visibleItems)
			if (item.itemId != 0)
				visible.insert(item.itemId);
		for (const OracleItem& item : elyos.equippedItems())
			EXPECT_TRUE(visible.contains(item.itemId)) << "Q5: the equipped item " << item.itemId << " is not visible in the character list";

		again.game->send(GameSession::CM_MAY_LOGIN_INTO_GAME, GameSession::buildCM_MAY_LOGIN_INTO_GAME());
		expectNext(*again.game, "SM_MAY_LOGIN_INTO_GAME", async);
		// The character logged out seconds ago in Q4, and PlayerEnterWorldService.java:152-156 answers SM_ENTER_WORLD_CHECK(REENTRY_TIME) and
		// returns while `now - lastOnline < CHARACTER_REENTRY_TIME * 1000`. The scenario profile sets that key to 1 second, and the login above
		// takes less than that, so Q5 waits it out exactly as Q3 does - otherwise the relogin is refused and no SM_PLAYER_SPAWN follows.
		std::this_thread::sleep_for(1500ms);
		again.game->send(GameSession::CM_ENTER_WORLD, GameSession::buildCM_ENTER_WORLD(a.playerId));
		const std::vector<Packet> burst = again.game->collectUntilQuiet(QUIET, BURST_LIMIT);
		const Packet* enterCheck = firstOfName(burst, "SM_ENTER_WORLD_CHECK");
		ASSERT_NE(enterCheck, nullptr) << "Q5: no SM_ENTER_WORLD_CHECK after the relogin; got: " << join(namesOf(burst));
		ASSERT_FALSE(enterCheck->data.empty());
		EXPECT_EQ(enterCheck->data[0], 0) << "Q5: the relogin was refused (SM_ENTER_WORLD_CHECK msg " << static_cast<int32_t>(enterCheck->data[0])
		                                  << "; 3 is REENTRY_TIME, 1 is CONNECTION_ERROR)";
		const Packet* reSpawn = firstOfName(burst, "SM_PLAYER_SPAWN");
		ASSERT_NE(reSpawn, nullptr) << "Q5: no SM_PLAYER_SPAWN after the relogin; got: " << join(namesOf(burst));
		const decoders::PlayerSpawn reSpawned = decoders::decodePlayerSpawn(reSpawn->data);
		EXPECT_NEAR(reSpawned.x, target.targetX, 5.0);
		EXPECT_NEAR(reSpawned.y, target.targetY, 5.0);
		again.game->send(GameSession::CM_QUIT, GameSession::buildCM_QUIT(false));
		expectNext(*again.game, "SM_QUIT_RESPONSE", async, 30s);
		EXPECT_TRUE(again.game->waitClosed(30s));
	});

	// ---- case 7: shutdown with a player online (§5.7 Q7) ----
	std::optional<int32_t> gameServerExit;
	/**
	 * §5.7 Q8: how many client connections were still open when the stop file was written. Java's shutdown path keeps one Account and its
	 * account-level objects per such connection (see perConnectionLiveClasses()), so this is the bound the live-count check uses.
	 */
	int64_t connectionsAtShutdown = 0;
	/**
	 * §5.7 Q8 / §10.1: how many Items the still-open connections keep alive through their account warehouse. This scenario never stores an item
	 * there (the two accounts are created empty and no case opens the warehouse), so a surviving warehouse holds nothing and the bound is 0.
	 * A case that does store one raises this number instead of the check demanding a 0 that Java does not deliver.
	 */
	const int64_t itemsInOpenAccountWarehouses = 0;
	const auto openSessions = [&]() -> int64_t {
		int64_t open = 0;
		for (const ScenarioClient* client : {&a, &b})
			if (client->game && !client->game->client.socket.isClosed())
				open++;
		return open;
	};
	float shutdownX = 0, shutdownY = 0;
	runCase("case 7", "shutdown with a player online", [&] {
		AsyncAllowed shutdownAsync = AsyncAllowed::m5aDefault();
		shutdownAsync.selfPlayerState(b.playerId).serverShutdownMessage();

		b.game->send(GameSession::CM_MAY_LOGIN_INTO_GAME, GameSession::buildCM_MAY_LOGIN_INTO_GAME());
		expectNext(*b.game, "SM_MAY_LOGIN_INTO_GAME", shutdownAsync);
		b.game->send(GameSession::CM_ENTER_WORLD, GameSession::buildCM_ENTER_WORLD(b.playerId));
		const std::vector<Packet> burst = b.game->collectUntilQuiet(QUIET, BURST_LIMIT);
		const int32_t inventoryPackets = static_cast<int32_t>((asmodian.items.size() + 9) / 10) + 1;
		expectSequence(burst, enterWorldPattern(true, inventoryPackets), shutdownAsync);
		b.game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
		const std::vector<Packet> ready = b.game->collectUntilQuiet(QUIET, BURST_LIMIT);
		expectSequence(ready, levelReadyPattern(), shutdownAsync);
		expectPlayerInfo(ready, b.playerId, b.characterName, asmodian, CLASS_MAGE, RACE_ASMODIAN, GENDER_FEMALE, appearance, "V5");

		// V5: the same visibility assertions for the Mage on the Asmodian start map - literally the same code as case 4, which is what §5.5 V5
		// asks for ("the same for the Mage"); before this the Mage's burst was only checked for V1 and V3
		const Packet* time = firstOfName(burst, "SM_GAME_TIME");
		ASSERT_NE(time, nullptr);
		const int32_t minutes =
		  static_cast<int32_t>(time->data[0] | time->data[1] << 8 | time->data[2] << 16 | static_cast<uint32_t>(time->data[3]) << 24);
		const OracleSpawns mageSpawns = oracle->spawns(asmodian.mapId, asmodian.x, asmodian.y, asmodian.z, gameHourOf(minutes));
		checkVisibility(ready, mageSpawns, "V5 (Mage on 220010000)");

		// three moves to P, then the shutdown with B online
		shutdownX = asmodian.x;
		shutdownY = asmodian.y;
		for (int32_t step = 1; step <= 3; step++) {
			shutdownX = asmodian.x + static_cast<float>(step) * 3.0f;
			b.game->send(GameSession::CM_MOVE,
			             GameSession::buildCM_MOVE(shutdownX, shutdownY, asmodian.z, 0, static_cast<int8_t>(0xE0), shutdownX, shutdownY, asmodian.z));
			std::this_thread::sleep_for(150ms);
		}
		b.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(shutdownX, shutdownY, asmodian.z, 0, 0));
		b.game->collectUntilQuiet(500ms, 10s);

		connectionsAtShutdown = openSessions();
		gameServerExit = servers.stopGameServer();
		bool shutdownMessage = false;
		for (const Packet& packet : b.game->collectUntilQuiet(5s, 120s))
			if (packet.name == "SM_SYSTEM_MESSAGE" && decoders::decodeSystemMessageId(packet.data) == AsyncAllowed::STR_SERVER_SHUTDOWN)
				shutdownMessage = true;
		EXPECT_TRUE(shutdownMessage) << "Q7: no STR_SERVER_SHUTDOWN system message";
		EXPECT_TRUE(b.game->waitClosed(60s)) << "Q7: the socket of the online player stayed open";
		ASSERT_TRUE(gameServerExit) << "Q7: the game server did not exit after the stop file was written";
		EXPECT_EQ(*gameServerExit, 0) << "Q7: the game server exited with " << *gameServerExit;

		const std::string player = std::to_string(b.playerId);
		const auto rows = database.queryRows(schema, "SELECT online, x, y, last_online FROM players WHERE id = " + player, 4);
		ASSERT_EQ(rows.size(), 1u);
		EXPECT_EQ(rows[0][0].value_or(""), "0") << "Q7: the shutdown did not mark the character offline";
		EXPECT_NEAR(std::stod(rows[0][1].value_or("0")), shutdownX, 5.0) << "Q7: the shutdown did not store the position";
		EXPECT_NEAR(std::stod(rows[0][2].value_or("0")), shutdownY, 5.0);
		EXPECT_TRUE(rows[0][3].has_value()) << "Q7: last_online is not set";
	});

	// ---- case 8: the reports (§5.7 Q8) ----
	if (!gameServerExit) {
		// an earlier case failed, so case 7 never wrote the stop file: whatever is still connected now is what the shutdown will hold on to
		connectionsAtShutdown = openSessions();
		gameServerExit = servers.stopGameServer();
	}
	const std::optional<int32_t> loginServerExit = servers.stopLoginServer();

	cases.run("case 8", "reports", [&] {
		ASSERT_TRUE(loginServerExit) << "the login server did not exit on CTRL_BREAK";
		// 98: Windows refused CTRL_BREAK because the test has no console, and the harness terminated the process (ScenarioServers.cpp). That is
		// not the same as an orderly shutdown, and Q8 then scans the log of a killed process, so it is only accepted when the log proves the
		// login server still closed its channels; otherwise the kill is reported.
		if (*loginServerExit == 98) {
			ASSERT_NE(servers.loginServer(), nullptr);
			const std::vector<std::string> closed = servers.loginServer()->findLogLines("ServerChannels closed.", 1);
			EXPECT_FALSE(closed.empty()) << "the login server was terminated (exit 98) without shutting down: CTRL_BREAK was refused and the log "
			                                "does not contain 'ServerChannels closed.'";
		} else {
			EXPECT_EQ(*loginServerExit, 0) << "the login server exited with " << *loginServerExit;
		}

		// --check-output writes the reports at shutdown; without them there is nothing to read (the server never got that far)
		ASSERT_TRUE(std::filesystem::is_regular_file(servers.checkOutputDir() / "m5a_summary.txt"))
		  << "Q8: the game server wrote no check output in " << servers.checkOutputDir();

		EXPECT_TRUE(servers.readReportLines("unported_trace.txt").empty()) << "Q8: AION_UNPORTED sites were reached:\n"
		                                                                   << join(servers.readReportLines("unported_trace.txt"), "\n");

		const std::vector<std::string> allowlist = readAllowlist();
		std::set<std::string> usedEntries;
		for (const std::string& line : servers.readReportLines("partial_trace.txt")) {
			const std::string site = siteOf(line);
			bool allowed = false;
			for (const std::string& entry : allowlist)
				if (allowlistEntryMatches(entry, site)) {
					allowed = true;
					usedEntries.insert(entry);
				}
			EXPECT_TRUE(allowed) << "Q8: the AION_PARTIAL site " << site << " is not in tests/scenario/m5a_partial_allowlist.txt";
		}
		// a row the run never hit is not a failure - several of them describe paths only a relogin or a non-default config reaches - but it is
		// printed, so the list cannot silently rot after the site it describes has been ported
		std::vector<std::string> unusedEntries;
		for (const std::string& entry : allowlist)
			if (!usedEntries.contains(entry))
				unusedEntries.push_back(entry);
		if (!unusedEntries.empty())
			std::cout << "Q8: m5a_partial_allowlist.txt rows this run did not hit (check they still point at an AION_PARTIAL):\n  "
			          << join(unusedEntries, "\n  ") << std::endl;

		EXPECT_TRUE(servers.readReportLines("census.txt").empty()) << "Q8: the final census reports leaks:\n"
		                                                           << join(servers.readReportLines("census.txt"), "\n");
		// live_counts.txt (runtime::writeLiveCounts): "<live>\t<created>\t<class>" per class, written after the runtime shut down
		const std::vector<std::pair<std::string, LiveCount>> finalCounts = readLiveCounts(servers, "live_counts.txt");
		for (const auto& [name, count] : finalCounts) {
			if (strictlyZeroLiveClasses().contains(name))
				EXPECT_EQ(count.live, 0) << "Q8: live instances left: " << count.line;
			else if (name == "Item")
				// The scenario never stores anything in an account warehouse, so the warehouses that survive with their connection hold nothing
				// and every Item of the run must be gone. The day a case puts an item there, this number changes with it - unlike a strict 0,
				// which would then fail on correct behaviour (m5a-plan.md §10.1).
				EXPECT_EQ(count.live, itemsInOpenAccountWarehouses)
				  << "Q8: live instances left: " << count.line << "\n  the scenario's account warehouses are empty, so no Item may survive";
			else if (perConnectionLiveClasses().contains(name) || name.ends_with("Storage"))
				EXPECT_LE(count.live, connectionsAtShutdown)
				  << "Q8: live instances left: " << count.line << "\n  Java keeps at most one of these per client that is still connected when the "
				  << "stop file is written (AionConnection.java:239-243 returns before LoginServer.onDisconnect), and " << connectionsAtShutdown
				  << " connection(s) were open; anything above that is a leak";
		}

		// Diagnostics, not an assertion: live_counts_baseline.txt is written right after "Game server started", so the difference to the final
		// scan is what the whole login/enter-world/logout/shutdown cycle left behind. The gate asserts only the classes above - LeakCensus
		// itself sees only objects that passed through World::removeObject, so census.txt covers VisibleObjects alone - and this table is what
		// makes the rest of the 148 tracked classes visible to whoever reads a run.
		try {
			std::map<std::string, int64_t> before;
			for (const auto& [name, count] : readLiveCounts(servers, "live_counts_baseline.txt"))
				before[name] += count.live;
			std::map<std::string, int64_t> after;
			for (const auto& [name, count] : finalCounts)
				after[name] += count.live;
			for (const auto& [name, live] : before)
				after.try_emplace(name, 0);
			std::vector<std::pair<int64_t, std::string>> grown;
			for (const auto& [name, live] : after) {
				const auto found = before.find(name);
				const int64_t delta = live - (found == before.end() ? 0 : found->second);
				if (delta != 0)
					grown.emplace_back(delta, name);
			}
			std::ranges::sort(grown, [](const auto& left, const auto& right) { return left.first > right.first; });
			std::cout << "Q8: live instance counts that changed between 'Game server started' and the final scan (diagnostics):\n";
			for (const auto& [delta, name] : grown)
				std::cout << "  " << (delta > 0 ? "+" : "") << delta << "\t" << name << "\n";
			std::cout << std::flush;
		} catch (const std::exception& exception) {
			std::cout << "Q8: no live_counts_baseline.txt to compare against (" << exception.what() << ")" << std::endl;
		}
		EXPECT_TRUE(servers.readReportLines("lockdep.txt").empty()) << "Q8: the lock order validator reported:\n"
		                                                            << join(servers.readReportLines("lockdep.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("watchdog.txt").empty()) << "Q8: the watchdog dumped:\n"
		                                                             << join(servers.readReportLines("watchdog.txt"), "\n");

		const std::map<std::string, std::vector<std::string>> summary = servers.readSummary();
		const auto started = summary.find("started");
		EXPECT_TRUE(started != summary.end() && !started->second.empty() && started->second[0] == "true") << "Q8: the game server never started";
		// §10.2: the live-instance counters are compiled out of a release build, where "liveLeaks 0" means "not measured". Without this row the
		// whole live-count half of Q8 could pass vacuously in a build the gate was never meant to run against.
		const auto liveCountsEnabled = summary.find("liveCountsEnabled");
		EXPECT_TRUE(liveCountsEnabled != summary.end() && !liveCountsEnabled->second.empty() && liveCountsEnabled->second[0] == "true")
		  << "Q8: the game server reports liveCountsEnabled false, so its live-count rows measured nothing: build it checked";
		// the game server's own view of its exit: case 7 asserts the process exit code, but case 8 runs even when case 7 did not
		const auto exitCode = summary.find("exitCode");
		EXPECT_TRUE(exitCode != summary.end() && !exitCode->second.empty() && exitCode->second[0] == "0")
		  << "Q8: the game server reported exit code " << (exitCode == summary.end() || exitCode->second.empty() ? "(none)" : exitCode->second[0]);
		// W-07: KnownList swallows and logs every exception out of notifySee/notifyNotSee/notifyNotKnow, and counts it
		const auto notifyFailures = summary.find("knownListNotifyFailures");
		EXPECT_TRUE(notifyFailures != summary.end() && !notifyFailures->second.empty() && notifyFailures->second[0] == "0")
		  << "Q8: KnownList swallowed notification exceptions: "
		  << (notifyFailures == summary.end() || notifyFailures->second.empty() ? "(no counter in m5a_summary.txt)" : notifyFailures->second[0]);
		const auto notPorted = summary.find("notPortedClientPacket");
		if (notPorted != summary.end())
			ADD_FAILURE() << "Q8: the scripted path sent client packets that are not ported: " << join(notPorted->second);

		// no ERROR line in either log (read line by line: a failing run's log can be hundreds of megabytes)
		std::vector<std::string> errors;
		ASSERT_NE(servers.gameServer(), nullptr);
		for (const std::string& line : servers.gameServer()->findLogLines(" ERROR "))
			errors.push_back("game server: " + line);
		if (servers.loginServer() != nullptr)
			for (const std::string& line : servers.loginServer()->findLogLines(" ERROR "))
				errors.push_back("login server: " + line);
		EXPECT_TRUE(errors.empty()) << "Q8: ERROR lines in the server logs:\n" << join(errors, "\n");

		// The console stream above is only one appender. logback.xml declares several loggers with additivity="false", which never reach the
		// console at all, so the game server's own error file is read as well (it exists because the gate passes --log-folder).
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
			EXPECT_TRUE(fileErrors.empty()) << "Q8: " << errorLog << " is not empty:\n" << join(fileErrors, "\n");
		} else {
			ADD_FAILURE() << "Q8: the game server wrote no " << errorLog << " (is --log-folder still passed?)";
		}

		const std::vector<std::string> unclean = servers.gameServer()->findLogLines("did not leave world cleanly", 5);
		EXPECT_TRUE(unclean.empty()) << "Q8: objects did not leave the world cleanly:\n" << join(unclean, "\n");
		// These two cannot fail in a run of this length, and the plan (§5.7) must not be read as proof of D7's periodic machinery: the zombie
		// breaker cuts after gameserver.runtime.zombie_break_minutes (default 30) and the stale-pin scan reports after LeakCensus::stalePinAfter
		// (10 minutes, checked every minute), while a gate run is one to three minutes - and CheckOutput::runFinalCensus switches the zombie
		// breaker off before the final scan on purpose. They are kept because they cost nothing and would catch a threshold regression; what
		// they do NOT prove is stated in §5.7 of the plan. census.txt is not in this category: runFinalCensus sets censusAfter to 0, so the
		// census above is a real check for everything that passed through World::removeObject.
		const std::vector<std::string> stalePins = servers.gameServer()->findLogLines("stale pin", 5);
		EXPECT_TRUE(stalePins.empty()) << "Q8: stale pins:\n" << join(stalePins, "\n");
		const auto zombieCuts = summary.find("zombieCuts");
		if (zombieCuts != summary.end() && !zombieCuts->second.empty())
			EXPECT_EQ(zombieCuts->second[0], "0") << "Q8: the zombie breaker cut references";
	});

	finishRun(servers, outputDir, "gs.scenario.m5a");
}

// ---- the geo gate (stage 3 wave B; m5a-plan.md §5.1 "Geodata", §11 first row) ------------------------------------------------------------

/**
 * `gs.scenario.m5a_geo`: the enter-world half of the geo debt. `gs.scenario.m5a` above runs with `gameserver.geodata.enable=false`, and
 * `gs.smoke.startup_geo` (wave A) runs with it on but never lets a client in, so until this test no automated run had ever walked a character
 * through a world that has its geo data. That is the configuration the user plays in, and the one the first real-client session found a bug in
 * (m5a-client-session.md F-1: 13 npc spawns died inside fortress shields, invisible to both existing tests).
 *
 * It runs only the cases geo changes - enter world, visibility, the client-driven zone revalidation, one region move, quit - because the geo data
 * costs a startup: measured over six runs of this checked RelWithDebInfo tree, the case that brings both servers up takes 6 to 9 s with the geo
 * data against 4 s without, and a Debug build pays the 144 s and 3.2 GB of m5a-client-session.md. Everything §5 proves that geo cannot touch
 * (creation, the DB rows, persistence, the relogin, the shutdown with a player online) stays in `gs.scenario.m5a` and is not repeated here: this
 * gate runs in 18 to 21 s where that one takes 32 s for twice the cases.
 *
 * **What is geo-specific here, and what the brief of this wave asked for that 4.8 does not have.** The three facts it named as the geo-specific
 * assertions - an npc spawn z corrected against the terrain, a gatherable that `canSee` includes or excludes, and a move whose z the server
 * corrects - do not exist in this server. The code says so, and the first geo run of this gate confirmed all three:
 *   - **No npc spawn z is geo-corrected.** `SpawnEngine` and `VisibleObjectSpawner` call the geo engine only for the postman, the functional npc
 *     and a summon (VisibleObjectSpawner.cpp:223, :241, :300 = Java VisibleObjectSpawner.java:172, :188, :238), none of which a scripted M5a path
 *     reaches, and the one place that would correct a walker's height is commented out in Java itself (WalkerGroup.java:273-278 =
 *     WalkerGroup.cpp:316-322). An npc stands at its spawn template's z with geo on and off alike. That is why V1-V4 run unchanged against the
 *     same oracle here: in a geo-built world they are the assertion that the terrain moved nothing, to the 1 cm of `onSpot`, and they are what a
 *     geo z-snap added to the spawn path fails. Measured in the first run: 21 of 27 npcs on a fixed spot, 4 deterministic gather spots matched.
 *   - **`canSee` never decides what a client is told about.** Its call sites in the port are the gather dialog (GatherableController.cpp:89), the
 *     attack path (PlayerController.cpp:500), the siege weapon (SiegeWeaponController.cpp:63) and npc movement (NpcMoveController.cpp:483); the
 *     knownlist that fills SM_NPC_INFO and SM_GATHERABLE_INFO does not consult it at all. The gather dialog is the one a gatherable would go
 *     through, and no ported M5a client packet reaches it (CM_TARGET_SELECT is on the unported list, m5a-client-session.md F-2).
 *   - **No player move is geo-corrected.** CM_MOVE.cpp and AntiHackService.cpp contain no geo call, in the port and in Java. The only geo call a
 *     moving creature makes is `TerrainZoneCollisionMaterialActor::moved` (getTerrainMaterialAt), and the player of this gate has no such actor:
 *     see GEO4 in case geo 6, where the run's own numbers pin why.
 * What geo *does* add on this path is the world it is walked through: the terrain, the material zones and the terrain-material observers of the
 * creatures, three classes a geo-off run never creates once (GEO1-GEO3). GEO4 then measures, from the live-count baseline the server writes
 * before the first client connects, which of them the client half of the run touched - none, on this map - so that the three rows cannot be
 * misread as evidence about the enter-world path. The value of the cases in between is the F-1 class of bug they would catch: an exception, an
 * ERROR, an AION_UNPORTED site, a lost spawn or a hang that only a geo-built world reaches, on the path a player actually walks. The gather
 * obstacle check and a real terrain-z correction need the M5b client packet set and are named in §5.1, not faked here.
 */
TEST(M5aScenarioGeo, Run) {
	// the same "a skipped gate is not a passed gate" rule as the gate above (§5.10): AION_SCENARIO_REQUIRE turns every skip reason into a failure
	const char* requireEnvironment = std::getenv("AION_SCENARIO_REQUIRE");
	const bool required = requireEnvironment != nullptr && *requireEnvironment != '\0' && std::string_view(requireEnvironment) != "0";
	const auto unavailable = [&](std::string_view reason) {
		if (required)
			ADD_FAILURE() << "gs.scenario.m5a_geo was not configured and AION_SCENARIO_REQUIRE is set: " << reason;
		else
			GTEST_SKIP() << "gs.scenario.m5a_geo: skipped (" << reason << ")";
	};

	std::optional<ScenarioEnvironment> environment = ScenarioEnvironment::fromEnvironment();
	if (!environment) {
		unavailable("set AION_TEST_GS_DATABASE_URL and AION_TEST_LS_DATABASE_URL");
		return;
	}
	// its own output directory, so its schema pair, its logs and its reports are separate from those of gs.scenario.m5a (the schema name is a
	// hash of this path, ScenarioServers.cpp)
	const std::filesystem::path outputDir = std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "m5a_geo";
	std::optional<Oracle> oracle = Oracle::fromEnvironment(outputDir / "oracle");
	if (!oracle) {
		unavailable("no Python interpreter for tools/oracle: set AION_TEST_PYTHON");
		return;
	}
	// data/geo is this test's whole subject: without it the server would start with the geo data off and every assertion below would describe
	// the configuration the other gate already covers. It is a missing prerequisite, exactly as RunStartupSmoke.cmake MODE geo treats it.
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
	std::cout << "gs.scenario.m5a_geo: " << geoFiles << " .geo files in " << geoDirectory << std::endl;

	CaseLog cases;
	struct ReportPrinter {
		const CaseLog& cases;
		~ReportPrinter() { std::cout << cases.report() << std::flush; }
	} printer{cases};

	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	// The one key that separates this gate from gs.scenario.m5a. It overrides the profile's false (ScenarioServers::gameServerArguments merges
	// the configured keys over m5aProfile()), so the profile itself stays what §5.1 describes.
	config.gameServerProperties["gameserver.geodata.enable"] = "true";
	// The reference geo startup takes 144 s and about 5 GB in a Debug build (m5a-client-session.md "Setup"); the headroom is for a loaded
	// machine, and RunStartupSmoke.cmake gives its geo mode the same order of magnitude (2100 s).
	config.startupTimeout = 25min;
	config.stopTimeout = 3min;
	ScenarioServers servers(config, *environment);
	const ScenarioDatabase& database = servers.gameDatabase();
	const std::string schema = servers.gameSchema();

	bool ok = true;
	const auto runCase = [&](std::string_view id, std::string_view title, const std::function<void()>& body) {
		if (!ok)
			cases.skip(id, title, "an earlier case failed");
		else
			ok = cases.run(id, title, body);
	};

	ok = cases.run("geo 0", "the servers start with the geo data enabled", [&] {
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

	// The non-vacuity guard of the whole gate: every geo assertion below is worthless if the server silently ran without the geo data, and a
	// typo in one -D is all it takes. GeoService logs "Geo data is disabled" when GEO_ENABLE is off (GeoService.cpp:94) and GeoWorldLoader logs
	// the entity and terrain counts when it loaded (GeoWorldLoader.cpp:211, :256), which is what gs.smoke.startup_geo reads as well.
	runCase("geo 0a", "the run really loaded the geo data", [&] {
		ASSERT_NE(servers.gameServer(), nullptr);
		EXPECT_TRUE(servers.gameServer()->findLogLines("Geo data is disabled", 1).empty())
		  << "the game server logged 'Geo data is disabled', so -Dgameserver.geodata.enable=true did not arrive and this gate would prove "
		     "nothing that gs.scenario.m5a does not prove already";
		const std::vector<std::string> entities = servers.gameServer()->findLogLines("Loaded ", 200);
		std::vector<std::string> geoLines;
		for (const std::string& line : entities)
			if (line.find(" entities on ") != std::string::npos || line.find("Loaded terrains for ") != std::string::npos ||
			    line.find(" meshes") != std::string::npos)
				geoLines.push_back(line);
		EXPECT_FALSE(geoLines.empty()) << "no geo load line ('Loaded N meshes', 'Loaded terrains for N maps', 'Loaded N entities on M maps') in "
		                                  "the game server log";
		for (const std::string& line : geoLines)
			std::cout << "geo: " << line << std::endl;
	});

	ScenarioClient a;
	a.account = "m5ag" + servers.gameSchema().substr(servers.gameSchema().size() - 8);
	a.characterName = "Geowarrior";
	const CharacterAppearance appearance = scenarioAppearance();
	AsyncAllowed async = AsyncAllowed::m5aDefault();
	OracleCreation elyos;

	runCase("geo 1", "login and create an Elyos Warrior", [&] {
		elyos = oracle->creation("ELYOS", "WARRIOR");
		ASSERT_FALSE(elyos.items.empty());
		const decoders::CharacterList list = logIn(servers, a, async, false);
		EXPECT_EQ(list.characterCount, 0) << "a fresh account must have no character";

		NewCharacter warrior;
		warrior.name = a.characterName;
		warrior.asmodian = false;
		warrior.playerClassId = CLASS_WARRIOR;
		warrior.appearance = appearance;
		a.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(a.key.accountId, a.account, warrior, 0));
		const decoders::CreateCharacter created = decoders::decodeCreateCharacter(expectNext(*a.game, "SM_CREATE_CHARACTER", async).data);
		ASSERT_EQ(created.responseCode, RESPONSE_OK);
		ASSERT_TRUE(created.player);
		a.playerId = created.player->playerId;
		EXPECT_EQ(created.player->mapId, ELYOS_START_MAP);
	});

	int32_t gameHour = 0;
	std::vector<Packet> enterBurst;
	runCase("geo 2", "enter world with geo on", [&] {
		async.selfPlayerState(a.playerId);
		a.game->send(GameSession::CM_MAY_LOGIN_INTO_GAME, GameSession::buildCM_MAY_LOGIN_INTO_GAME());
		expectNext(*a.game, "SM_MAY_LOGIN_INTO_GAME", async);
		a.game->send(GameSession::CM_ENTER_WORLD, GameSession::buildCM_ENTER_WORLD(a.playerId));
		enterBurst = a.game->collectUntilQuiet(QUIET, BURST_LIMIT);
		ASSERT_FALSE(enterBurst.empty()) << "no packet after CM_ENTER_WORLD";
		const int32_t inventoryPackets = static_cast<int32_t>((elyos.items.size() + 9) / 10) + 1;
		// the §5.8 order must be the same with geo on: an extra or missing packet here would be a geo-only wire difference
		expectSequence(enterBurst, enterWorldPattern(true, inventoryPackets), async);

		const Packet* check = firstOfName(enterBurst, "SM_ENTER_WORLD_CHECK");
		ASSERT_NE(check, nullptr);
		ASSERT_FALSE(check->data.empty());
		EXPECT_EQ(check->data[0], 0) << "the geo-enabled enter world was refused";

		// V6 with geo on. The player's spawn z is the `players` row's z, which creation wrote from the oracle, and nothing on the enter-world
		// path corrects it against the terrain - so this is the assertion that a geo-only z correction (or a geo-driven teleport) would break.
		const Packet* spawn = firstOfName(enterBurst, "SM_PLAYER_SPAWN");
		ASSERT_NE(spawn, nullptr);
		const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(spawn->data);
		EXPECT_EQ(spawned.worldId, elyos.mapId);
		EXPECT_NEAR(spawned.x, elyos.x, 0.01) << "geo moved the player's spawn x";
		EXPECT_NEAR(spawned.y, elyos.y, 0.01) << "geo moved the player's spawn y";
		EXPECT_NEAR(spawned.z, elyos.z, 0.01) << "geo moved the player's spawn z";
		EXPECT_EQ(spawned.heading, elyos.heading);
		// the same position in the database, so a wrong z cannot be a decoder artefact
		const auto rows = database.queryRows(schema, "SELECT x, y, z FROM players WHERE id = " + std::to_string(a.playerId), 3);
		ASSERT_EQ(rows.size(), 1u);
		EXPECT_NEAR(std::stod(rows[0][2].value_or("0")), elyos.z, 0.01) << "the stored z of the character changed under a geo-enabled server";

		const Packet* time = firstOfName(enterBurst, "SM_GAME_TIME");
		ASSERT_NE(time, nullptr);
		ASSERT_EQ(time->data.size(), 4u);
		gameHour = gameHourOf(
		  static_cast<int32_t>(time->data[0] | time->data[1] << 8 | time->data[2] << 16 | static_cast<uint32_t>(time->data[3]) << 24));
	});

	std::vector<Packet> levelReadyBurst;
	runCase("geo 3", "level ready: the world a geo-enabled server shows", [&] {
		a.game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
		levelReadyBurst = a.game->collectUntilQuiet(QUIET, BURST_LIMIT);
		ASSERT_FALSE(levelReadyBurst.empty()) << "no packet after CM_LEVEL_READY";
		expectSequence(levelReadyBurst, levelReadyPattern(), async);
		expectPlayerInfo(levelReadyBurst, a.playerId, a.characterName, elyos, CLASS_WARRIOR, RACE_ELYOS, GENDER_MALE, appearance, "geo V9");
		// V1-V4 against the same oracle the geo-off gate uses, and with geo on they say more than they do there. The oracle knows nothing about
		// geo, so this is the assertion that a geo-built world puts the same objects in the same places, to the 1 cm of onSpot(): every npc of
		// this burst spawned into a world with terrain, meshes and material zones and ran its own revalidateZones on the way in (the F-1 class
		// of bug), and a terrain-z snap anywhere on the spawn path - the thing §5.1 feared and 4.8 does not do - shows up here as an npc that
		// is on none of its oracle spots, in a run where the geo-off gate stays green.
		const OracleSpawns spawns = oracle->spawns(elyos.mapId, elyos.x, elyos.y, elyos.z, gameHour);
		checkVisibility(levelReadyBurst, spawns, "geo V1-V4 (Warrior on 210010000, geodata enabled)");
	});

	runCase("geo 4", "the client-driven zone revalidation and a region move", [&] {
		// CM_SUBZONE_CHANGE is the client's entry into Creature::revalidateZones, and with geo on the zone set MapRegion::revalidateZones walks
		// includes the material zones GeoWorldLoader built through ZoneService::createMaterialZoneTemplate (a sphere, cylinder or semisphere per
		// material mesh, ZoneService.cpp:280-294): the player-side zone machinery of §5.1, which no automated run had ever asked a position
		// against. A non-staff account gets no packet back (MaterialZoneHandler.cpp:65-67 answers only a staff player, and only with
		// GEO_MATERIALS_SHOWDETAILS), so what is asserted is that the geo-built zone set neither answered nor killed the connection.
		a.game->send(GameSession::CM_SUBZONE_CHANGE, GameSession::buildCM_SUBZONE_CHANGE(1));
		const std::vector<Packet> subzone = a.game->collectUntilQuiet(QUIET, 30s);
		EXPECT_TRUE(ofName(subzone, "SM_SYSTEM_MESSAGE").empty())
		  << "CM_SUBZONE_CHANGE answered a non-staff account on a geo-enabled server: " << join(namesOf(subzone));
		EXPECT_FALSE(a.game->client.socket.isClosed()) << "the connection died on CM_SUBZONE_CHANGE (Player::revalidateZones with geo on)";

		const OracleBorderTarget target = oracle->borderTarget(elyos.mapId, elyos.x, elyos.y, elyos.z, gameHour);
		const double total = distance2d(target.startX, target.startY, target.targetX, target.targetY);
		const int32_t steps = std::max(1, static_cast<int32_t>(total / 5.0));
		std::vector<std::array<float, 3>> path;
		for (int32_t step = 1; step <= steps; step++) {
			const float t = static_cast<float>(step) / static_cast<float>(steps);
			const float x = target.startX + (target.targetX - target.startX) * t;
			const float y = target.startY + (target.targetY - target.startY) * t;
			const float z = target.startZ + (target.targetZ - target.startZ) * t;
			path.push_back({x, y, z});
			a.game->send(GameSession::CM_MOVE,
			             GameSession::buildCM_MOVE(x, y, z, 0, static_cast<int8_t>(0xE0), target.targetX, target.targetY, target.targetZ));
			std::this_thread::sleep_for(120ms);
		}
		a.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(target.targetX, target.targetY, target.targetZ, 0, 0));
		const std::vector<Packet> moveBurst = a.game->collectUntilQuiet(QUIET, BURST_LIMIT);

		// What a walk on a geo-enabled server can say through packets is that it was not corrected, answered or punished - the same M1-M3 as
		// §5.6. There is no geo call on the CM_MOVE path itself (neither CM_MOVE.cpp nor AntiHackService.cpp has one, in the port or in Java);
		// the one a moving creature can make is TerrainZoneCollisionMaterialActor::moved, and GEO4 in case geo 6 measures that this player has
		// no such actor because 210010000 carries no terrain materials. So this case is about the zone machinery around the move - every step
		// crosses regions and revalidates zones in a world whose zone set includes 7901 material zones - and not about a corrected z.
		std::set<int32_t> appeared;
		for (const Packet& packet : ofName(moveBurst, "SM_NPC_INFO")) {
			const decoders::NpcInfo npc = decoders::decodeNpcInfo(packet.data);
			appeared.insert(npc.templateId);
			bool nearPath = false;
			for (const auto& point : path)
				if (distance2d(npc.x, npc.y, point[0], point[1]) <= 100.0)
					nearPath = true;
			EXPECT_TRUE(nearPath) << "geo M1: SM_NPC_INFO for npc " << npc.templateId << " at (" << npc.x << ", " << npc.y
			                      << "), farther than 100 m from every path point";
		}
		for (const OracleSpot& spot : target.appear)
			EXPECT_TRUE(appeared.contains(spot.npcId)) << "geo M1: npc " << spot.npcId << " should have appeared on the way to the target";
		EXPECT_TRUE(ofName(moveBurst, "SM_FORCED_MOVE").empty())
		  << "geo M3: the geo-enabled server corrected the walk with SM_FORCED_MOVE, which the geo-off gate never sees";
		for (const Packet& packet : ofName(moveBurst, "SM_MOVE")) {
			ASSERT_GE(packet.data.size(), 4u);
			const int32_t objectId =
			  static_cast<int32_t>(packet.data[0] | packet.data[1] << 8 | packet.data[2] << 16 | static_cast<uint32_t>(packet.data[3]) << 24);
			EXPECT_NE(objectId, a.playerId) << "geo M3: the server sent SM_MOVE for the moving player itself";
		}
		EXPECT_FALSE(a.game->client.socket.isClosed()) << "the connection died during the walk on a geo-enabled server";
		// the walk ended where the client said it did: a material zone that killed or teleported the character would show here
		const auto rows = database.queryRows(schema, "SELECT online FROM players WHERE id = " + std::to_string(a.playerId), 1);
		ASSERT_EQ(rows.size(), 1u);
		EXPECT_EQ(rows[0][0].value_or(""), "1") << "the character is no longer online after the walk";
	});

	runCase("geo 5", "quit", [&] {
		a.game->send(GameSession::CM_QUIT, GameSession::buildCM_QUIT(false));
		expectNext(*a.game, "SM_QUIT_RESPONSE", async, 30s);
		EXPECT_TRUE(a.game->waitClosed(30s)) << "the socket stayed open after CM_QUIT(0)";
		a.game.reset();
		a.login.reset();
		const auto rows = database.queryRows(schema, "SELECT online FROM players WHERE id = " + std::to_string(a.playerId), 1);
		ASSERT_EQ(rows.size(), 1u);
		EXPECT_EQ(rows[0][0].value_or(""), "0") << "the character stayed online after the quit";
	});

	const std::optional<int32_t> gameServerExit = servers.stopGameServer();
	const std::optional<int32_t> loginServerExit = servers.stopLoginServer();

	cases.run("geo 6", "the reports of a geo-enabled run", [&] {
		ASSERT_TRUE(gameServerExit) << "the game server did not exit after the stop file was written";
		EXPECT_EQ(*gameServerExit, 0) << "the geo-enabled game server exited with " << *gameServerExit;
		ASSERT_TRUE(loginServerExit) << "the login server did not exit on CTRL_BREAK";
		ASSERT_TRUE(std::filesystem::is_regular_file(servers.checkOutputDir() / "m5a_summary.txt"))
		  << "the game server wrote no check output in " << servers.checkOutputDir();

		// the same clean-run bar as Q8, with the geo data on: this is where the F-1 class of bug (an unported zone handler on a path only geo
		// reaches) turns into a failure instead of into a user's first walk
		EXPECT_TRUE(servers.readReportLines("unported_trace.txt").empty())
		  << "AION_UNPORTED sites were reached on the geo-enabled path:\n" << join(servers.readReportLines("unported_trace.txt"), "\n");
		const std::vector<std::string> allowlist = readAllowlist();
		for (const std::string& line : servers.readReportLines("partial_trace.txt")) {
			const std::string site = siteOf(line);
			bool allowed = false;
			for (const std::string& entry : allowlist)
				if (allowlistEntryMatches(entry, site))
					allowed = true;
			EXPECT_TRUE(allowed) << "the AION_PARTIAL site " << site << " is not in tests/scenario/m5a_partial_allowlist.txt";
		}
		EXPECT_TRUE(servers.readReportLines("census.txt").empty()) << "the final census of the geo-enabled run reports leaks:\n"
		                                                            << join(servers.readReportLines("census.txt"), "\n");
		std::vector<std::string> errors;
		ASSERT_NE(servers.gameServer(), nullptr);
		for (const std::string& line : servers.gameServer()->findLogLines(" ERROR "))
			errors.push_back("game server: " + line);
		if (servers.loginServer() != nullptr)
			for (const std::string& line : servers.loginServer()->findLogLines(" ERROR "))
				errors.push_back("login server: " + line);
		EXPECT_TRUE(errors.empty()) << "ERROR lines in the logs of the geo-enabled run:\n" << join(errors, "\n");
		const std::vector<std::string> unclean = servers.gameServer()->findLogLines("did not leave world cleanly", 5);
		EXPECT_TRUE(unclean.empty()) << "objects did not leave the world cleanly:\n" << join(unclean, "\n");

		const std::map<std::string, std::vector<std::string>> summary = servers.readSummary();
		// The two rows of Q8 that say what the geo-enabled path did to the server itself. notPortedClientPacket is the F-2 class (a client
		// packet whose C++ port is missing answers nothing and is only counted), and knownListNotifyFailures is W-07: KnownList swallows every
		// exception out of notifySee/notifyNotSee, so a geo-only throw while the 83k npcs and the player learn about each other would otherwise
		// leave no trace at all in a run whose packets all arrived.
		const auto notPorted = summary.find("notPortedClientPacket");
		if (notPorted != summary.end())
			ADD_FAILURE() << "the scripted geo path sent client packets that are not ported: " << join(notPorted->second);
		const auto notifyFailures = summary.find("knownListNotifyFailures");
		EXPECT_TRUE(notifyFailures != summary.end() && !notifyFailures->second.empty() && notifyFailures->second[0] == "0")
		  << "KnownList swallowed notification exceptions on the geo-enabled path: "
		  << (notifyFailures == summary.end() || notifyFailures->second.empty() ? "(no counter in m5a_summary.txt)" : notifyFailures->second[0]);
		const auto liveCountsEnabled = summary.find("liveCountsEnabled");
		// GEO1-GEO4 below are live-instance counts, and in a release build every one of them is 0 whatever the run did (§10.2)
		ASSERT_TRUE(liveCountsEnabled != summary.end() && !liveCountsEnabled->second.empty() && liveCountsEnabled->second[0] == "true")
		  << "the game server reports liveCountsEnabled false, so its live-count rows measured nothing: build it checked";

		const std::vector<std::pair<std::string, LiveCount>> counts = readLiveCounts(servers, "live_counts.txt");
		// live_counts_baseline.txt is written right after "Game server started" (CheckOutput.h), i.e. after the geo load and the 83k spawns and
		// BEFORE the first client connects. The difference between the two files is therefore exactly what the scripted client path created, and
		// it is what separates a world-construction fact from an enter-world fact below.
		const std::vector<std::pair<std::string, LiveCount>> baselineCounts = readLiveCounts(servers, "live_counts_baseline.txt");
		ASSERT_FALSE(baselineCounts.empty()) << "live_counts_baseline.txt is empty, so the client half of the run cannot be told from the startup";
		const auto sumOf = [](const std::vector<std::pair<std::string, LiveCount>>& rows, std::string_view className) {
			LiveCount total;
			for (const auto& [name, count] : rows)
				if (name == className) {
					total.live += count.live;
					total.created += count.created;
					total.line += (total.line.empty() ? "" : " | ") + count.line;
				}
			return total;
		};
		const auto countOf = [&](std::string_view className) { return sumOf(counts, className); };
		const auto baselineOf = [&](std::string_view className) { return sumOf(baselineCounts, className); };
		// Diagnostics first, so that a failing row below can be read against the whole geo half of the table.
		std::cout << "geo: the live-instance counts of the geo classes (live / created / class, and what the client half added):\n";
		for (const auto& [name, count] : counts)
			if (name.find("Geo") != std::string::npos || name.find("Material") != std::string::npos || name.find("Terrain") != std::string::npos ||
			    name.find("Zone") != std::string::npos || name.find("Shield") != std::string::npos || name == "Mesh" || name == "Geometry")
				std::cout << "  " << count.line << "\t(client half: +" << count.created - baselineOf(name).created << ")\n";
		std::cout << std::flush;

		// GEO1: the terrain of the geo data. Terrain objects exist only where GeoWorldLoader read a heightmap .png next to the .geo files
		// (GeoWorldLoader.cpp:181 `terrainByMap.emplace_back(map, models::Terrain::create())`); a run with gameserver.geodata.enable=false
		// never enters that code and its count is 0. Measured on this tree: 89 for 78 terrain images.
		const LiveCount terrain = countOf("Terrain");
		EXPECT_GT(terrain.created, 0) << "GEO1: no Terrain was created, so the run loaded no terrain and 'geo on' means nothing here";

		// GEO2: the terrain-material observer of a creature. CreatureController::onAfterSpawn creates one per Creature that has a move controller
		// and whose world has terrain materials (CreatureController.cpp:553-562) - GeoService::worldHasTerrainMaterials, which is false for every
		// world without geo data (GeoService.cpp:236-238, GeoMap::hasTerrainMaterials at GeoMap.cpp:113-116). Measured: 6598, all of them npcs of
		// the 16 maps that the ten material images of data/geo cover; see GEO4 for why the player is not among them.
		const LiveCount terrainActor = countOf("TerrainZoneCollisionMaterialActor");
		EXPECT_GT(terrainActor.created, 0)
		  << "GEO2: no TerrainZoneCollisionMaterialActor was created, so not one creature of this world ran the terrain-material path of "
		     "CreatureController::onAfterSpawn";

		// GEO3: the material zones themselves. ZoneService::createMaterialZoneTemplate builds a MaterialZoneHandler per geometry with a
		// material (ZoneService.cpp:239-256), and it is called only from GeoWorldLoader while it reads the geo data. These are the player-side
		// zone handlers of §5.1 that the geo-off gate cannot reach, because with geo off they do not exist at all. Measured: 7901.
		const LiveCount materialZones = countOf("MaterialZoneHandler");
		EXPECT_GT(materialZones.created, 0)
		  << "GEO3: no MaterialZoneHandler was created, so the geo data brought no material zone and the player-side zone handlers this gate "
		     "exists to walk through were not there";

		// GEO4, the client half: GEO1-GEO3 are world-construction facts - all three were already at their final value in the baseline, before this
		// gate's client connected - and this row is the measurement that says so, so that the three above cannot be read as evidence about the
		// enter-world path. What it pins:
		//   - the client half of the run - the player's enter world, its zone revalidation, its walk, and every npc that respawned while it was
		//     online - created NO TerrainZoneCollisionMaterialActor. It would have, on a map with terrain materials: 210010000 has a heightmap
		//     image (210010000.png, 16 bit) but no material image, and only ten of the 78 terrain images of the 4.8 tree are material images
		//     (*_materials.png, 8 bit, covering 16 maps; the nearest one is 220020000, Morheim). worldHasTerrainMaterials(210010000) is therefore
		//     false and CreatureController::onAfterSpawn takes its other arm for the player, exactly as it does for every npc of Poeta.
		//   - nothing entered a material zone: MaterialZoneHandler::onEnterZone creates a ZoneCollisionMaterialActor per creature that enters one
		//     (MaterialZoneHandler.cpp:61-63), and not one was created in the whole run - not by the 83k spawning npcs and not by the player's
		//     revalidateZones, its CM_SUBZONE_CHANGE or its walk.
		// So the geo machinery this gate walks a character through is built, alive and asserted, but on this map and this path it stays passive.
		// The day that changes - a geo-aware start map, a material zone on the walk, a port that gives the player an actor - this row fails and
		// says that the player-side geo path has become assertable for real, which is the M5b work §5.1 names.
		const int64_t clientTerrainActors = terrainActor.created - baselineOf("TerrainZoneCollisionMaterialActor").created;
		EXPECT_EQ(clientTerrainActors, 0)
		  << "GEO4: the client half of the run created " << clientTerrainActors
		  << " TerrainZoneCollisionMaterialActor. The character now runs the terrain-material path on its own map, which §5.1 records as "
		     "impossible on 210010000 (no *_materials.png): assert that path here instead of this row";
		const LiveCount zoneActors = countOf("ZoneCollisionMaterialActor");
		EXPECT_EQ(zoneActors.created, 0)
		  << "GEO4: " << zoneActors.created
		  << " ZoneCollisionMaterialActor were created, so something entered a material zone in this run (" << zoneActors.line
		  << "). That is the player-side zone handler firing, which §5.1 records as unreached on this path: assert what it did instead of this row";
		EXPECT_EQ(materialZones.created, baselineOf("MaterialZoneHandler").created)
		  << "GEO4: the client half of the run created material zones, which only GeoWorldLoader does (ZoneService.cpp:239-256)";

		// everything above is geo; the ordinary leak bar of Q8 applies here as well
		for (const auto& [name, count] : counts)
			if (strictlyZeroLiveClasses().contains(name))
				EXPECT_EQ(count.live, 0) << "live instances left after the geo-enabled run: " << count.line;
	});

	finishRun(servers, outputDir, "gs.scenario.m5a_geo");
}

} // namespace aion::gameserver::scenario
