// The M5f scenario gate (m5f-plan.md G-03, G-04, §10): one login server and one game server as child processes on their own test schemas,
// three accounts, and travel and the first instance:
//   E1 (account A), a fresh Elyos Warrior of level 1, is not seeded at all (D4): the Akarios hotspot takes it from the spawn to Akarios (C2),
//      a second hotspot is refused by the cooldown (C3), Daines refuses a non-Daeva his map (C4) and a crafted CM_TELEPORT_SELECT cannot pay
//      Verteron (C5), Kustanon flies it to Melponeh (C6) while E2 watches, it binds at the Melponeh obelisk (C7), Aero flies it back (C8),
//      *Return* takes it to the bind point (C9) and the bind point survives a relog (C10);
//   E2 (account B), an Elyos Templar Daeva of level 16 seeded at Melponeh, flies to Akarios (C11), takes Daines' route to Verteron while E1
//      watches (C12), quits during the jump to Sanctum (C12b), jumps to Sanctum (C13), casts *Return* there without a bind point (C14), is
//      seeded beside Haramel's entrance (C15), enters Haramel (C16), presses the leave button (C17), takes the exit portal (C18), re-enters
//      (C19), relogs inside (C20) and relogs after the instance is destroyed (C21);
//   S1 (account C), an Asmodian Templar Daeva of level 16 seeded beside Osmar, makes two map changes in a row: Altgard, then Morheim (C22);
// then the reports the server writes at shutdown (C23, the M5a Q8 bar and the instance rows).
//
// Every expectation is independent of the C++ server code, as in the earlier gates: server packets are read with the decoders of
// tests/scenario/decoders (TravelDecoders.h for the travel and instance packets, PacketDecoders.h, CombatDecoders.h, EconomyDecoders.h,
// ProgressionDecoders.h, SkillDecoders.h, QuestDecoders.h - all written from the Java writeImpl methods, m5a-plan.md D9), and every npc, loc,
// price, point, cooltime and spawn comes from `tools/oracle/oracle.py` - m5f-travel (G-01), m5a-spawns and m5a-creation - or from the Java
// method an assertion is about, cited at the line.
//
// It holds TWO gates: M5fScenario.Run (gs.scenario.m5f, geo off) and M5fScenarioGeo.Run (gs.scenario.m5f_geo, geo on), the same script through
// one shared body; the comment above TEST(M5fScenarioGeo, Run) says what the geo run adds (§10.5).
//
// **This file deliberately does not share the other gates' helpers** (M5b3ScenarioTest.cpp gives the reason: each gate owns one pair of
// server processes and its helpers live in an anonymous namespace). What is duplicated is scaffolding - the case log, the burst collector, the
// login conversation, the report readers - never an assertion. The prologue's checks are the one thing shared (PrologueSupport.h).
//
// **§10.4, the mutation proof:** the mutants live in the server's sources for the length of one run and are restored byte for byte
// (docs/deviations/P5-SC.md, "M5f gate"); nothing in this file reads a mutation switch.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
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
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

#include "AsyncAllowed.h"
#include "ChildProcess.h"
#include "FakeLoginClient.h"
#include "GameSession.h"
#include "InventoryModel.h"
#include "Oracle.h"
#include "PrologueSupport.h"
#include "ScenarioDatabase.h"
#include "ScenarioServers.h"
#include "decoders/CombatDecoders.h"
#include "decoders/EconomyDecoders.h"
#include "decoders/PacketDecoders.h"
#include "decoders/ProgressionDecoders.h"
#include "decoders/QuestDecoders.h"
#include "decoders/SkillDecoders.h"
#include "decoders/TravelDecoders.h"

namespace aion::gameserver::scenario {
namespace {

using namespace std::chrono_literals;
using decoders::DecodeError;
using json = nlohmann::json;
using Packet = GameSession::Packet;
using Clock = std::chrono::steady_clock;

constexpr std::chrono::milliseconds QUIET = 1000ms;
/** How long collectBurst waits for the FIRST packet of an answer (the M5b/M5c measurement: a loaded server answers a re-entry late) */
constexpr std::chrono::milliseconds FIRST_REPLY_WAIT = 5000ms;
constexpr std::chrono::milliseconds BURST_LIMIT = 90s;

constexpr int32_t RESPONSE_OK = 0;
constexpr int32_t RESPONSE_OPEN_CREATION_WINDOW = 22;
constexpr uint8_t ENTER_WORLD_OK = 0;
constexpr int32_t KINAH_ITEM = 182400001;

// ---- the maps, npcs and ids of §10.1 "Targets" (every number of each comes from the oracle in C0) ------------------------------------------
constexpr int32_t POETA = 210010000;
constexpr int32_t ISHALGEN = 220010000;
constexpr int32_t VERTERON = 210030000;
constexpr int32_t ALTGARD = 220030000;
constexpr int32_t MORHEIM = 220020000;
constexpr int32_t SANCTUM = 110010000;
constexpr int32_t HARAMEL = 300200000;
constexpr int32_t KUSTANON = 203070;
constexpr int32_t AERO = 203083;
constexpr int32_t DAINES = 203194;
constexpr int32_t URAKRON = 203091;
constexpr int32_t OSMAR = 203679;
constexpr int32_t UKIN = 203581;
constexpr int32_t MELPONEH_OBELISK = 700014;
constexpr int32_t HARAMEL_ENTRANCE = 730318;
constexpr int32_t HARAMEL_EXIT = 730320;
constexpr int32_t HOTSPOT_AKARIOS = 13;
constexpr int32_t HOTSPOT_MELPONEH = 15;
/** the teleporter locations of §10.2 */
constexpr int32_t LOC_SANCTUM = 2;
constexpr int32_t LOC_VERTERON = 4;
constexpr int32_t LOC_ALTGARD = 9;
constexpr int32_t LOC_MORHEIM = 10;
constexpr int32_t LOC_AKARIOS = 12;
constexpr int32_t LOC_MELPONEH = 13;
/** flypath_template.xml:7-8, flypaths 5 (Akarios -> Melponeh) and 6 (Melponeh -> Akarios): the client flies them, the gate reports them */
struct FlyPath {
	float sx, sy, sz, ex, ey, ez;
};
constexpr FlyPath FLYPATH_5{806.63f, 1242.11f, 119.0f, 426.17f, 1742.29f, 119.85f};
constexpr FlyPath FLYPATH_6{427.0f, 1741.83f, 119.82f, 806.78f, 1242.13f, 118.69f};
/** skill 243 Return (skill_templates.xml:3046-3058): autolearned at level 1 (skill_tree.xml:303), a 6,000 ms cast */
constexpr uint16_t SKILL_RETURN = 243;
constexpr int32_t RETURN_CAST_MS = 6000;

// ---- the ids of the Java packets the gate reads -------------------------------------------------------------------------------------------
/** SM_SYSTEM_MESSAGE ids (SM_SYSTEM_MESSAGE.java) */
constexpr int32_t STR_FLYING_TIME_NOT_READY = 1300961;                 // :14493-14494
constexpr int32_t STR_MSG_NOT_ENOUGH_KINA = 901285;                    // :28852-28853
constexpr int32_t STR_DEATH_REGISTER_RESURRECT_POINT = 1300670;        // :12499-12500
constexpr int32_t STR_ALREADY_REGISTER_THIS_RESURRECT_POINT = 1300688; // :12625-12626
constexpr int32_t STR_MSG_LEAVE_INSTANCE = 1400044;                    // :17941-17942
constexpr int32_t STR_MSG_LEAVE_INSTANCE_FORCE = 1400046;              // :17955-17956
constexpr int32_t STR_MSG_INSTANCE_DUNGEON_OPENED_FOR_SELF = 1400640;  // :22080-22081
/** SM_QUESTION_WINDOW.STR_ASK_REGISTER_RESURRECT_POINT (SM_QUESTION_WINDOW.java:128) */
constexpr int32_t STR_ASK_REGISTER_RESURRECT_POINT = 160012;
/** ActionAnimation.BIND_KISK (ActionAnimation.java:14) */
constexpr uint16_t ACTION_ANIMATION_BIND_KISK = 2;
/** DialogPage.NO_RIGHT (DialogPage.java:43) */
constexpr uint16_t DIALOG_PAGE_NO_RIGHT = 27;
/** DialogAction.AIRLINE_SERVICE (DialogAction.java) */
constexpr uint16_t DIALOG_AIRLINE_SERVICE = 44;
/** EmotionType START_QUESTLOOT (42) and END_QUESTLOOT (43), EmotionType.java:50-51 */
constexpr uint8_t EMOTION_START_QUESTLOOT = 42;
constexpr uint8_t EMOTION_END_QUESTLOOT = 43;
/** CreatureState.ACTIVE (1) and FLYING (2), CreatureState.java */
constexpr uint16_t STATE_ACTIVE = 1;
constexpr uint16_t STATE_FLYING = 2;
/** BindPointTeleportService.COOLDOWN_IN_SECONDS (BindPointTeleportService.java:24) */
constexpr int32_t HOTSPOT_COOLDOWN = 600;
/** VisibleObject.getVisibleDistance (VisibleObject.java:247-249) and the float test of PositionUtil.isInRange (:257-262) */
constexpr float VISIBILITY = 95.0f;

/** D4: the gate's Daevas are Templars (W-14), seeded with m5c C19's recipe; 20,000 kinah each (§10.1 "Characters") */
constexpr std::string_view DAEVA_CLASS = "TEMPLAR";
constexpr int64_t DAEVA_KINAH = 20000;
/** the Daeva quests (1006 / 2008) and the prologue quests the seeds already finished (1000 / 2000: an Elyos without 1000 plays its movie at
 *  every enter world, _1000Prologue.java:26-36, and the movie drops every CM_MOVE until it ends - quests are not this gate's subject) */
constexpr int32_t Q_ASCENSION_ELYOS = 1006;
constexpr int32_t Q_ASCENSION_ASMODIAN = 2008;
constexpr int32_t Q_PROLOGUE_ELYOS = 1000;
constexpr int32_t Q_PROLOGUE_ASMODIAN = 2000;

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

/** PositionUtil.isInRange's float test (PositionUtil.java:257-262) */
bool inRangeFloat(float x1, float y1, float z1, float x2, float y2, float z2, float range) {
	const float dx = x1 - x2, dy = y1 - y2, dz = z1 - z2;
	return dx * dx + dy * dy + dz * dz < range * range;
}

std::vector<std::string> namesOf(const std::vector<Packet>& packets) {
	std::vector<std::string> names;
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

std::vector<Packet> slice(const GameSession& session, size_t from, size_t to = SIZE_MAX) {
	const std::vector<Packet>& packets = session.recorded();
	to = std::min(to, packets.size());
	if (from >= to)
		return {};
	return std::vector<Packet>(packets.begin() + static_cast<std::ptrdiff_t>(from), packets.begin() + static_cast<std::ptrdiff_t>(to));
}

std::optional<uint8_t> enterWorldCheck(const std::vector<Packet>& burst) {
	const Packet* check = firstOfName(burst, "SM_ENTER_WORLD_CHECK");
	if (check == nullptr || check->data.empty())
		return std::nullopt;
	return check->data[0];
}

/** the system messages of a packet list with that id */
std::vector<decoders::SystemMessage> messagesOf(const std::vector<Packet>& packets, int32_t messageId) {
	std::vector<decoders::SystemMessage> found;
	for (const Packet& packet : ofName(packets, "SM_SYSTEM_MESSAGE")) {
		const decoders::SystemMessage message = decoders::decodeSystemMessage(packet.data);
		if (message.messageId == messageId)
			found.push_back(message);
	}
	return found;
}

/** the SM_DELETE of one object in a packet list */
std::vector<decoders::Delete> deletesOf(const std::vector<Packet>& packets, int32_t objectId) {
	std::vector<decoders::Delete> found;
	for (const Packet& packet : ofName(packets, "SM_DELETE")) {
		const decoders::Delete deleted = decoders::decodeDelete(packet.data);
		if (deleted.objectId == objectId)
			found.push_back(deleted);
	}
	return found;
}

/** the npc object ids SM_NPC_INFO announced in a packet list, with their template ids */
std::map<int32_t, int32_t> npcsAnnounced(const std::vector<Packet>& packets) {
	std::map<int32_t, int32_t> npcs;
	for (const Packet& packet : ofName(packets, "SM_NPC_INFO")) {
		try {
			const decoders::NpcInfo npc = decoders::decodeNpcInfo(packet.data);
			npcs[npc.objectId] = npc.templateId;
		} catch (const DecodeError&) {
		}
	}
	return npcs;
}

std::string fmt(float x, float y, float z) {
	std::ostringstream text;
	text.setf(std::ios::fixed);
	text.precision(3);
	text << "(" << x << ", " << y << ", " << z << ")";
	return text.str();
}

// ---- the oracle answers this gate reads (oracle.py m5f-travel, G-01) ------------------------------------------------------------------------

struct OSpot {
	int32_t map = 0;
	float x = 0, y = 0, z = 0;
	int32_t heading = 0;
};

struct OLoc {
	int32_t locId = 0;
	std::string type;
	int32_t teleportId = 0;
	int64_t price = 0;
	int64_t servicePrice = 0;
	int32_t requiredQuest = 0;
	int32_t map = 0;
	float x = 0, y = 0, z = 0;
	int32_t heading = 0;
};

struct ONpc {
	int32_t npc = 0;
	std::vector<OSpot> spots;
	bool daevaOnly = false;
	int32_t teleportId = 0;
	std::vector<OLoc> locations;

	const OLoc& loc(int32_t locId) const {
		for (const OLoc& location : locations)
			if (location.locId == locId)
				return location;
		throw std::runtime_error("the oracle has no location " + std::to_string(locId) + " of npc " + std::to_string(npc));
	}
	const OSpot& spot() const {
		if (spots.empty())
			throw std::runtime_error("the oracle has no spawn spot of npc " + std::to_string(npc));
		return spots.front();
	}
};

struct OHotspot {
	int32_t id = 0, map = 0;
	float x = 0, y = 0, z = 0;
	int32_t heading = 0;
	int64_t basePrice = 0, price = 0;
	double distance = 0;
};

struct OObelisk {
	int32_t npc = 0;
	std::vector<OSpot> spots;
	int64_t price = 0;
};

struct OPortalPath {
	int32_t locId = 0, map = 0;
	bool instance = false;
	float x = 0, y = 0, z = 0;
	int32_t heading = 0;
};

struct OPortal {
	int32_t npc = 0;
	std::vector<OSpot> spots;
	int32_t talkDelayMs = 0;
	std::vector<OPortalPath> paths;
	int32_t cooltimeId = 0, maxCount = 0;
	std::optional<int64_t> reuseTimeMs;
	std::optional<OSpot> exit;
};

struct OInstanceSpot {
	int32_t npcId = 0;
	float x = 0, y = 0, z = 0;
	int32_t heading = 0;
	bool fixed = false, spawned = false, temporary = false;
	double distance = 0;
};

float jf(const json& value) {
	return value.is_null() ? 0.0f : value.get<float>();
}

/** j[key] as T, `fallback` when the key is absent or null (the oracle writes null for what Java does not set, e.g. a hotspot's heading) */
template <typename T>
T jv(const json& j, const char* key, T fallback) {
	const auto found = j.find(key);
	return found == j.end() || found->is_null() ? fallback : found->get<T>();
}

OSpot parseOSpot(const json& j) {
	OSpot spot;
	spot.map = jv(j, "map", 0);
	spot.x = jf(j.at("x"));
	spot.y = jf(j.at("y"));
	spot.z = jf(j.at("z"));
	spot.heading = jv(j, "heading", 0);
	return spot;
}

std::vector<OSpot> parseOSpots(const json& j) {
	std::vector<OSpot> spots;
	if (j.contains("spots"))
		for (const json& spot : j.at("spots"))
			spots.push_back(parseOSpot(spot));
	return spots;
}

ONpc parseONpc(const json& j) {
	ONpc npc;
	npc.npc = j.at("npc").get<int32_t>();
	npc.spots = parseOSpots(j);
	npc.daevaOnly = jv(j, "daevaOnly", false);
	if (j.contains("teleporter") && !j.at("teleporter").is_null())
		npc.teleportId = jv(j.at("teleporter"), "teleportId", 0);
	if (j.contains("locations"))
		for (const json& l : j.at("locations")) {
			OLoc loc;
			loc.locId = l.at("locId").get<int32_t>();
			loc.type = jv(l, "type", std::string());
			loc.teleportId = jv(l, "teleportId", 0);
			loc.price = jv(l, "price", int64_t{0});
			loc.servicePrice = jv(l, "servicePrice", int64_t{0});
			loc.requiredQuest = jv(l, "requiredQuest", 0);
			loc.map = jv(l, "map", 0);
			loc.x = jf(l.at("x"));
			loc.y = jf(l.at("y"));
			loc.z = jf(l.at("z"));
			loc.heading = jv(l, "heading", 0);
			npc.locations.push_back(loc);
		}
	return npc;
}

OHotspot parseOHotspot(const json& j) {
	OHotspot hotspot;
	hotspot.id = j.at("hotspot").get<int32_t>();
	hotspot.map = j.at("map").get<int32_t>();
	hotspot.x = jf(j.at("x"));
	hotspot.y = jf(j.at("y"));
	hotspot.z = jf(j.at("z"));
	hotspot.heading = jv(j, "heading", 0);
	hotspot.basePrice = j.at("basePrice").get<int64_t>();
	hotspot.price = j.at("price").get<int64_t>();
	hotspot.distance = j.at("distance").get<double>();
	return hotspot;
}

OObelisk parseOObelisk(const json& j) {
	OObelisk obelisk;
	obelisk.npc = j.at("npc").get<int32_t>();
	obelisk.spots = parseOSpots(j);
	obelisk.price = j.at("bindPoint").at("price").get<int64_t>();
	return obelisk;
}

OPortal parseOPortal(const json& j) {
	OPortal portal;
	portal.npc = j.at("npc").get<int32_t>();
	portal.spots = parseOSpots(j);
	portal.talkDelayMs = jv(j, "talkDelayMs", 0);
	for (const json& p : j.at("paths")) {
		// the path Portal2Data.getPortalUsePath picks for the race comes first; the others only for the record
		if (!jv(p, "selected", false))
			continue;
		OPortalPath path;
		path.locId = jv(p, "locId", 0);
		path.map = p.at("map").get<int32_t>();
		path.instance = jv(p, "instance", false);
		path.x = jf(p.at("x"));
		path.y = jf(p.at("y"));
		path.z = jf(p.at("z"));
		path.heading = jv(p, "heading", 0);
		portal.paths.push_back(path);
	}
	if (j.contains("cooltime") && !j.at("cooltime").is_null()) {
		portal.cooltimeId = jv(j.at("cooltime"), "id", 0);
		portal.maxCount = jv(j.at("cooltime"), "maxCount", 0);
	}
	if (j.contains("reuseTimeMs") && !j.at("reuseTimeMs").is_null())
		portal.reuseTimeMs = j.at("reuseTimeMs").get<int64_t>();
	if (j.contains("exit") && !j.at("exit").is_null())
		portal.exit = parseOSpot(j.at("exit"));
	return portal;
}

std::vector<OInstanceSpot> parseOInstanceSpots(const json& j) {
	std::vector<OInstanceSpot> spots;
	for (const json& s : j.at("spots")) {
		OInstanceSpot spot;
		spot.npcId = s.at("npcId").get<int32_t>();
		spot.x = jf(s.at("x"));
		spot.y = jf(s.at("y"));
		spot.z = jf(s.at("z"));
		spot.heading = jv(s, "heading", 0);
		spot.fixed = jv(s, "fixed", false);
		spot.spawned = jv(s, "spawned", true);
		spot.temporary = jv(s, "temporary", false);
		spot.distance = jv(s, "distance", 0.0);
		spots.push_back(spot);
	}
	return spots;
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

/** Runs the cases in order; @return whether the NEXT case can run (false after an exception or a fatal failure) */
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
		const auto started = Clock::now();
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
		result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started);
		result.failed = failedAssertions() > failedBefore;
		results.push_back(result);
		std::cout << id << " " << (result.failed ? "FAILED" : "passed") << " in " << result.duration.count() << " ms" << std::endl;
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

	std::function<bool(int32_t)> predicate() {
		return [this](int32_t objectId) {
			scan();
			return ids.contains(objectId);
		};
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
				}
			}
		}
	}

	const GameSession* session = nullptr;
	size_t scanned = 0;
	std::set<int32_t> ids;
};

/** A burst ends `quiet` after the last packet the async-allowed set does not explain */
std::vector<Packet> collectBurst(GameSession& session, const AsyncAllowed& async, std::chrono::milliseconds quiet = QUIET,
	std::chrono::milliseconds limit = BURST_LIMIT) {
	std::vector<Packet> collected;
	const auto start = Clock::now();
	const auto deadline = start + limit;
	auto lastAwaited = start;
	for (;;) {
		const auto now = Clock::now();
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
			lastAwaited = Clock::now();
		collected.push_back(std::move(*packet));
	}
	return collected;
}

/** Reads and records everything that arrives within a FIXED window */
std::vector<Packet> collectFor(GameSession& session, std::chrono::milliseconds window) {
	std::vector<Packet> collected;
	const auto deadline = Clock::now() + window;
	for (;;) {
		const auto now = Clock::now();
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

/** Reads until a packet with that name (and, if given, matching `accept`) arrives and records everything on the way. @throws on timeout */
Packet waitFor(GameSession& session, std::string_view name, std::chrono::milliseconds timeout = 15s,
	const std::function<bool(const Packet&)>& accept = {}) {
	const auto deadline = Clock::now() + timeout;
	for (;;) {
		const auto now = Clock::now();
		if (now >= deadline)
			break;
		std::optional<Packet> packet = session.next(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
		if (!packet) {
			if (session.client.socket.isClosed())
				break;
			continue;
		}
		if (packet->name == name) {
			if (!accept)
				return *packet;
			try {
				if (accept(*packet))
					return *packet;
			} catch (const DecodeError&) {
			}
		}
	}
	throw std::runtime_error("timeout waiting for " + std::string(name) + (session.client.socket.isClosed() ? " (the connection closed)" : ""));
}

/** Reads until a packet with that name arrives, skipping the async-allowed set. @throws on timeout, close or another packet */
Packet expectNext(GameSession& session, std::string_view name, const AsyncAllowed& async, std::chrono::milliseconds timeout = 15s) {
	const auto deadline = Clock::now() + timeout;
	for (;;) {
		const auto now = Clock::now();
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

// ---- the clients ----------------------------------------------------------------------------------------------------------------------

struct ScenarioClient {
	std::string label;
	std::string account;
	std::string password = "m5fPassword1";
	std::string name;
	bool asmodian = false;
	std::unique_ptr<FakeLoginClient> login;
	std::unique_ptr<GameSession> game;
	FakeLoginClient::SessionKey key;
	int32_t playerId = 0;
	InventoryModel model;
	AnnouncedNpcs npcs;
	AsyncAllowed async = AsyncAllowed::m5aDefault();
	float x = 0, y = 0, z = 0;
	int32_t worldId = 0;
	std::vector<Packet> lastEnterWorld, lastLevelReady;

	size_t mark() const { return game ? game->recorded().size() : 0; }
	std::vector<Packet> since(size_t from) const { return game ? slice(*game, from) : std::vector<Packet>{}; }
	int64_t kinah() {
		model.sync();
		return model.kinah();
	}
	/** reads what has arrived meanwhile (a client that is not the one acting) */
	void drain(std::chrono::milliseconds window = 300ms) {
		if (game)
			collectFor(*game, window);
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
		                         (check ? std::to_string(*check) : std::string("missing")) + "): " + join(namesOf(burst)));
	// the LAST SM_PLAYER_SPAWN is where the character is: a login into a destroyed instance spawns twice (§2.5 step 10, T18)
	const std::vector<Packet> spawns = ofName(burst, "SM_PLAYER_SPAWN");
	if (spawns.empty())
		throw std::runtime_error(client.label + ": no SM_PLAYER_SPAWN after CM_ENTER_WORLD: " + join(namesOf(burst)));
	const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(spawns.back().data);
	client.x = spawned.x;
	client.y = spawned.y;
	client.z = spawned.z;
	client.worldId = spawned.worldId;
	client.model.sync();
	client.lastEnterWorld = burst;
	return burst;
}

/**
 * CM_LEVEL_READY and its burst. A movie a quest plays here is ended as a real client ends it (CM_PLAY_MOVIE_END echoes its fields), since
 * SM_PLAY_MOVIE drops every CM_MOVE until then (CM_MOVE.cpp's WATCHING_CUTSCENE): quests are not this gate's subject.
 */
std::vector<Packet> levelReady(ScenarioClient& client, bool endMovies = true) {
	client.game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
	std::vector<Packet> burst = collectBurst(*client.game, client.async);
	if (burst.empty())
		throw std::runtime_error(client.label + ": no packet after CM_LEVEL_READY");
	client.lastLevelReady = burst;
	if (endMovies)
		for (const Packet& packet : ofName(burst, "SM_PLAY_MOVIE")) {
			const decoders::PlayMovie movie = decoders::decodePlayMovie(packet.data);
			std::cout << client.label << ": a quest plays movie " << movie.movieId << " (quest " << movie.questId << ") at the level ready; ended"
			          << std::endl;
			client.game->send(GameSession::CM_PLAY_MOVIE_END,
				GameSession::buildCM_PLAY_MOVIE_END(movie.cutsceneMovie ? 1 : 0, movie.objectId, movie.questId, movie.movieId));
			collectFor(*client.game, 1s);
		}
	client.model.sync();
	return burst;
}

/** CM_QUIT(0): the character leaves the world (if it is in one) and the connection ends - the only state in which a `players` seed survives */
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

/** a new login and the stored character into the world (M5a's Q5). @return the enter-world burst */
std::vector<Packet> relogIn(ScenarioServers& servers, ScenarioClient& client) {
	logIn(servers, client);
	client.game->send(GameSession::CM_MAY_LOGIN_INTO_GAME, GameSession::buildCM_MAY_LOGIN_INTO_GAME());
	expectNext(*client.game, "SM_MAY_LOGIN_INTO_GAME", client.async);
	std::this_thread::sleep_for(1500ms);
	std::vector<Packet> burst = enterWorld(client);
	levelReady(client);
	return burst;
}

/** the object id of the npc of `templateId` nearest to (x, y) within 5 m, from every SM_NPC_INFO this session recorded */
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
		}
	}
	return found;
}

int32_t requireNpc(const ScenarioClient& client, int32_t templateId, const OSpot& spot) {
	const std::optional<int32_t> object = npcObject(client, templateId, spot.x, spot.y);
	if (!object)
		throw std::runtime_error(client.label + ": no SM_NPC_INFO of npc " + std::to_string(templateId) + " near its spot " +
		                         fmt(spot.x, spot.y, spot.z));
	return *object;
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

// ---- the reports -----------------------------------------------------------------------------------------------------------------------

enum class AllowlistSection { HitAtLeastOnce, HitNever, NotPinned };

struct AllowlistEntry {
	std::string site;
	AllowlistSection section = AllowlistSection::NotPinned;
};

/** Reads tests/scenario/m5f_partial_allowlist.txt with its three sections ("# --- SECTION A/B/C" marker lines, as the M5c list) */
std::vector<AllowlistEntry> readAllowlist() {
	std::vector<AllowlistEntry> entries;
	std::ifstream in(AION_SCENARIO_M5F_PARTIAL_ALLOWLIST, std::ios::binary);
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

std::map<std::string, LiveCount> readLiveCounts(const ScenarioServers& servers, std::string_view fileName) {
	std::map<std::string, LiveCount> counts;
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
		counts[colons == std::string::npos ? qualified : qualified.substr(colons + 2)] = count;
	}
	return counts;
}

/** true if a Wednesday 09:00 local time lies in [from, to]: the hard-coded LegionDominion cron (m5c-plan.md G-07) */
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

// ---- the arrival's npcs (m5a-plan.md §5.5 V1 and V3, for one level-ready burst) -----------------------------------------------------------

/**
 * V1: every SM_NPC_INFO of the arrival's level-ready burst is an npc id the oracle has a spot of within 100 m (105 m for a walker) or a flag
 * npc of the map; V3: every deterministic spawned spot within 90 m of the arrival is announced, by id and position to 1 cm. The npcs a quest
 * spawns on arrival are not in the static spawns and are named, not failed, when they are of an id the oracle does not know (W-13).
 */
void checkArrivalNpcs(const std::vector<Packet>& burst, const OracleSpawns& spawns, std::string_view label) {
	std::vector<decoders::NpcInfo> npcs;
	for (const Packet& packet : ofName(burst, "SM_NPC_INFO"))
		npcs.push_back(decoders::decodeNpcInfo(packet.data));
	EXPECT_FALSE(npcs.empty()) << label << ": not a single SM_NPC_INFO arrived";
	std::vector<std::string> unknown;
	for (const decoders::NpcInfo& npc : npcs) {
		bool accepted = false;
		for (const OracleSpot& spot : spawns.spots)
			if (spot.npcId == npc.templateId && spot.distance <= (spot.walker ? 105.0 : 100.0))
				accepted = true;
		for (const OracleSpot& spot : spawns.flagNpcs)
			if (spot.npcId == npc.templateId)
				accepted = true;
		if (!accepted)
			unknown.push_back(std::to_string(npc.templateId) + " at " + fmt(npc.x, npc.y, npc.z));
	}
	EXPECT_TRUE(unknown.empty()) << label << " V1: SM_NPC_INFO of npcs the oracle has no spot of within the visibility radius: " << join(unknown);
	size_t expected = 0;
	for (const OracleSpot& spot : spawns.spots) {
		if (!(spot.spawned && spot.deterministic && !spot.pool && !spot.walker && !spot.gatherable && spot.distance <= 90.0))
			continue;
		expected++;
		bool found = false;
		for (const decoders::NpcInfo& npc : npcs)
			if (npc.templateId == spot.npcId && std::abs(npc.x - spot.x) <= 0.01 && std::abs(npc.y - spot.y) <= 0.01 &&
			    std::abs(npc.z - spot.z) <= 0.01)
				found = true;
		EXPECT_TRUE(found) << label << " V3: no SM_NPC_INFO for the deterministic spot of npc " << spot.npcId << " at " << fmt(spot.x, spot.y, spot.z)
		                   << ", " << spot.distance << " m away";
	}
	EXPECT_GT(expected, 0u) << label << " V3: the oracle predicts no deterministic spot within 90 m, so V3 would assert nothing";
	std::cout << label << ": " << npcs.size() << " SM_NPC_INFO, " << expected << " deterministic spots within 90 m checked" << std::endl;
}

/** the same for an instance's npcs, from m5f-travel --instance-spawns (§10.3 T13: "the instance's npc set within 90 m") */
void checkInstanceNpcs(const std::vector<Packet>& burst, const std::vector<OInstanceSpot>& spots, std::string_view label) {
	std::vector<decoders::NpcInfo> npcs;
	for (const Packet& packet : ofName(burst, "SM_NPC_INFO"))
		npcs.push_back(decoders::decodeNpcInfo(packet.data));
	EXPECT_FALSE(npcs.empty()) << label << ": not a single SM_NPC_INFO arrived (spawnInstance placed nothing?)";
	std::vector<std::string> unknown;
	for (const decoders::NpcInfo& npc : npcs) {
		bool accepted = false;
		for (const OInstanceSpot& spot : spots)
			if (spot.npcId == npc.templateId && spot.distance <= 105.0)
				accepted = true;
		if (!accepted)
			unknown.push_back(std::to_string(npc.templateId) + " at " + fmt(npc.x, npc.y, npc.z));
	}
	EXPECT_TRUE(unknown.empty()) << label << ": SM_NPC_INFO of npcs the instance's spawns do not place within the visibility radius: "
	                             << join(unknown);
	size_t expected = 0;
	for (const OInstanceSpot& spot : spots) {
		// a gatherable (ids 400001..499998) is announced with SM_GATHERABLE_INFO, not SM_NPC_INFO (VisibleObjectSpawner)
		if (!spot.fixed || !spot.spawned || spot.temporary || spot.distance > 90.0 || (spot.npcId >= 400001 && spot.npcId <= 499998))
			continue;
		expected++;
		bool found = false;
		for (const decoders::NpcInfo& npc : npcs)
			if (npc.templateId == spot.npcId && std::abs(npc.x - spot.x) <= 0.01 && std::abs(npc.y - spot.y) <= 0.01 &&
			    std::abs(npc.z - spot.z) <= 0.01)
				found = true;
		EXPECT_TRUE(found) << label << ": no SM_NPC_INFO for the instance spot of npc " << spot.npcId << " at " << fmt(spot.x, spot.y, spot.z);
	}
	EXPECT_GT(expected, 0u) << label << ": the oracle places no fixed instance spot within 90 m, so the row would assert nothing";
	std::cout << label << ": " << npcs.size() << " SM_NPC_INFO, " << expected << " fixed instance spots within 90 m checked" << std::endl;
}

/** What separates gs.scenario.m5f from gs.scenario.m5f_geo */
struct GateVariant {
	bool geodata = false;
	std::string testName;
	std::string outputSubdir;
	std::string schemaPrefix;
	std::string accountPrefix;
};

void runM5fGate(const GateVariant& variant) {
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
	}

	CaseLog cases;
	struct ReportPrinter {
		const CaseLog& cases;
		const std::string& testName;
		~ReportPrinter() { std::cout << cases.report(testName) << std::flush; }
	} printer{cases, variant.testName};

	// ---- §10.1 processes, databases and profile (m5f.properties.example) ----
	// The M5a set comes from ScenarioServers::m5aProfile; then M5c's quieting keys and M5f's own: the solo instance dies 1 s after it empties
	// (D11), the fly-path validator off (explicit, D7), the simple class window off (the retail route's profile), geodata per variant.
	std::map<std::string, std::string> gateKeys;
	gateKeys["gameserver.geodata.enable"] = variant.geodata ? "true" : "false";
	gateKeys["gameserver.npcshouts.enable"] = "false";
	gateKeys["gameserver.rates.drop"] = "0";
	gateKeys["gameserver.instance.solo.destroy_delay_seconds"] = "1";
	gateKeys["gameserver.security.validation.flypath"] = "false";
	gateKeys["gameserver.simple.secondclass.enable"] = "false";

	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.schemaPrefix = variant.schemaPrefix;
	config.gameServerProperties = gateKeys;
	config.startupTimeout = variant.geodata ? 25min : 10min;
	config.stopTimeout = 3min;
	ScenarioServers servers(config, *environment);
	const std::string schema = servers.gameSchema();
	const ScenarioDatabase& database = servers.gameDatabase();

	std::filesystem::create_directories(outputDir);
	const std::filesystem::path profileFile = outputDir / "m5f_oracle_profile.properties";
	{
		std::map<std::string, std::string> keys = ScenarioServers::m5aProfile();
		for (const auto& [key, value] : gateKeys)
			keys[key] = value;
		std::ofstream out(profileFile, std::ios::binary | std::ios::trunc);
		for (const auto& [key, value] : keys)
			out << key << " = " << value << "\n";
	}

	bool ok = true;
	const auto runCase = [&](std::string_view id, std::string_view title, const std::function<void()>& body) {
		if (!ok)
			cases.skip(id, title, "an earlier case ended with an exception or a fatal failure");
		else
			ok = cases.run(id, title, body);
	};

	// ======================================================================================================================================
	// C0: the oracle answers (G-01)
	// ======================================================================================================================================
	std::map<int32_t, ONpc> npcs;
	OHotspot hotspotAkarios;
	OObelisk obelisk;
	OPortal entrance, exitPortal;
	int64_t level16Exp = 0;
	OracleCreation elyosWarrior;
	json census;
	const auto travel = [&](std::vector<std::string> arguments) {
		arguments.insert(arguments.begin(), "m5f-travel");
		arguments.push_back("--profile");
		arguments.push_back(profileFile.string());
		return json::parse(oracle->run(arguments));
	};
	runCase("C0", "the oracle answers (m5f-travel, m5a-creation) and the plan's premises hold", [&] {
		elyosWarrior = oracle->creation("ELYOS", "WARRIOR");
		std::vector<std::string> arguments;
		for (const int32_t npc : {KUSTANON, AERO, DAINES, URAKRON, OSMAR, UKIN}) {
			arguments.push_back("--npc");
			arguments.push_back(std::to_string(npc));
		}
		arguments.insert(arguments.end(), {"--hotspot", std::to_string(HOTSPOT_AKARIOS), "--from",
		                                   std::to_string(elyosWarrior.x) + "," + std::to_string(elyosWarrior.y) + "," + std::to_string(elyosWarrior.z),
		                                   "--obelisk", std::to_string(MELPONEH_OBELISK), "--exp-for-level", "16", "--census"});
		const json answer = travel(arguments);
		for (const json& npc : answer.at("npcs")) {
			const ONpc parsed = parseONpc(npc);
			npcs[parsed.npc] = parsed;
		}
		hotspotAkarios = parseOHotspot(answer.at("hotspots").at(0));
		obelisk = parseOObelisk(answer.at("obelisks").at(0));
		level16Exp = answer.at("exp").at("exp").get<int64_t>();
		census = jv(answer, "census", json());
		for (const int32_t npc : {KUSTANON, AERO, DAINES, URAKRON, OSMAR, UKIN})
			ASSERT_TRUE(npcs.contains(npc)) << "the oracle answered no npc " << npc;
		// the premises of §10: Daines refuses a non-Daeva (DialogService.java:187-197), Osmar too, and the others' routes exist
		EXPECT_TRUE(npcs[DAINES].daevaOnly);
		EXPECT_TRUE(npcs[OSMAR].daevaOnly);
		EXPECT_FALSE(npcs[KUSTANON].daevaOnly);
		EXPECT_EQ(npcs[KUSTANON].loc(LOC_MELPONEH).type, "FLIGHT");
		EXPECT_EQ(npcs[AERO].loc(LOC_AKARIOS).type, "FLIGHT");
		EXPECT_EQ(npcs[DAINES].loc(LOC_VERTERON).map, VERTERON);
		EXPECT_EQ(npcs[DAINES].loc(LOC_VERTERON).requiredQuest, 0);
		EXPECT_EQ(npcs[URAKRON].loc(LOC_SANCTUM).requiredQuest, 0) << "C13: Urakron's Sanctum route has no required quest";
		EXPECT_EQ(npcs[OSMAR].loc(LOC_ALTGARD).map, ALTGARD);
		EXPECT_EQ(npcs[UKIN].loc(LOC_MORHEIM).map, MORHEIM);
		// E1's kinah budget (§10.1): 1,000 at creation - the hotspot, the flight, the bind, the flight back - stays positive, and C5's crafted
		// select cannot pay Verteron
		int64_t creationKinah = 0;
		for (const OracleItem& item : elyosWarrior.items)
			if (item.kinah)
				creationKinah += item.count;
		EXPECT_LT(creationKinah - hotspotAkarios.price, npcs[DAINES].loc(LOC_VERTERON).servicePrice) << "C5 must be refused for the kinah";
		EXPECT_GT(creationKinah - hotspotAkarios.price - npcs[KUSTANON].loc(LOC_MELPONEH).servicePrice - obelisk.price -
		            npcs[AERO].loc(LOC_AKARIOS).servicePrice,
		          0);
		EXPECT_GT(level16Exp, 0);
		// W-14 / W-21 (G-01 --census): the Templar enters at 16 with every passive ported, and Haramel holds no monster skill of an unported class
		if (!census.is_null()) {
			if (census.contains("w14") && census.at("w14").contains("TEMPLAR"))
				EXPECT_TRUE(census.at("w14").at("TEMPLAR").value("unported", json::array()).empty()) << census.at("w14").at("TEMPLAR").dump();
			if (census.contains("haramel"))
				EXPECT_TRUE(jv(census.at("haramel"), "unportedEffectClasses", json::array()).empty()) << census.at("haramel").dump();
		}
		std::cout << "C0: hotspot 13 from the Elyos spawn " << hotspotAkarios.price << " kinah (distance " << hotspotAkarios.distance << "), Daines -> "
		          << "Verteron " << npcs[DAINES].loc(LOC_VERTERON).servicePrice << ", Kustanon -> Melponeh "
		          << npcs[KUSTANON].loc(LOC_MELPONEH).servicePrice << " (flight " << npcs[KUSTANON].loc(LOC_MELPONEH).teleportId
		          << "), obelisk " << obelisk.price << ", level 16 = " << level16Exp << " exp" << std::endl;
	});

	const std::string suffix = servers.gameSchema().substr(servers.gameSchema().size() - 8);
	ScenarioClient e1, e2, s1;
	e1.label = "E1";
	e2.label = "E2";
	s1.label = "S1";
	e1.account = variant.accountPrefix + "a" + suffix;
	e2.account = variant.accountPrefix + "b" + suffix;
	s1.account = variant.accountPrefix + "c" + suffix;
	e1.name = "Travelera";
	e2.name = "Travelerb";
	s1.name = "Travelerc";
	s1.asmodian = true;

	std::chrono::system_clock::time_point serverUpFrom = std::chrono::system_clock::now();
	runCase("S-0", "the servers start (§10.1)", [&] {
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

	// ---- the helpers of the cases --------------------------------------------------------------------------------------------------------

	const auto createCharacter = [&](ScenarioClient& client) {
		NewCharacter character;
		character.name = client.name;
		character.asmodian = client.asmodian;
		character.playerClassId = NewCharacter::WARRIOR;
		client.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(client.key.accountId, client.account, character, 1));
		EXPECT_EQ(decoders::decodeCreateCharacter(expectNext(*client.game, "SM_CREATE_CHARACTER", client.async).data).responseCode,
		          RESPONSE_OPEN_CREATION_WINDOW);
		client.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(client.key.accountId, client.account, character, 0));
		const decoders::CreateCharacter created = decoders::decodeCreateCharacter(expectNext(*client.game, "SM_CREATE_CHARACTER", client.async).data);
		if (created.responseCode != RESPONSE_OK || !created.player)
			throw std::runtime_error(client.label + ": creating the character answered response code " + std::to_string(created.responseCode));
		client.playerId = created.player->playerId;
	};
	const auto seedPosition = [&](ScenarioClient& client, int32_t worldId, float x, float y, float z) {
		database.execute(schema, "UPDATE players SET world_id = " + std::to_string(worldId) + ", x = " + std::to_string(x) + ", y = " +
		                           std::to_string(y) + ", z = " + std::to_string(z) + ", heading = 0 WHERE id = " + std::to_string(client.playerId));
	};
	/** D4 (m5c C19's recipe, A-05): a Templar Daeva of level 16 with its kinah; the prologue finished, the ascension complete */
	const auto seedDaeva = [&](ScenarioClient& client) {
		const std::string id = std::to_string(client.playerId);
		database.execute(schema, "UPDATE players SET player_class = '" + std::string(DAEVA_CLASS) + "', exp = " + std::to_string(level16Exp) +
		                           " WHERE id = " + id);
		for (const int32_t quest : client.asmodian ? std::vector<int32_t>{Q_PROLOGUE_ASMODIAN, Q_ASCENSION_ASMODIAN}
		                                           : std::vector<int32_t>{Q_PROLOGUE_ELYOS, Q_ASCENSION_ELYOS})
			database.execute(schema, "INSERT INTO player_quests (player_id, quest_id, status, complete_count) VALUES (" + id + ", " +
			                           std::to_string(quest) + ", 'COMPLETE', 1)");
		database.execute(schema, "UPDATE inventory SET item_count = " + std::to_string(DAEVA_KINAH) + " WHERE item_owner = " + id +
		                           " AND item_id = " + std::to_string(KINAH_ITEM));
	};
	const auto kinahInDatabase = [&](const ScenarioClient& client) {
		return database.queryLong(schema, "SELECT item_count FROM inventory WHERE item_owner = " + std::to_string(client.playerId) +
		                                    " AND item_id = " + std::to_string(KINAH_ITEM)).value_or(-1);
	};
	/** the SM_PLAYER_INFO of the client itself in a packet list (a flying player's has the two flight ints: PlayerInfoOptions) */
	const auto selfPlayerInfos = [&](const ScenarioClient& client, const std::vector<Packet>& packets) {
		std::vector<decoders::PlayerInfo> infos;
		for (const Packet& packet : ofName(packets, "SM_PLAYER_INFO")) {
			try {
				const decoders::PlayerInfo info = decoders::decodePlayerInfo(packet.data);
				if (info.objectId == client.playerId)
					infos.push_back(info);
			} catch (const DecodeError&) {
			}
		}
		return infos;
	};
	/** CM_SHOW_DIALOG on an npc; @return the SM_DIALOG_WINDOW */
	const auto talk = [&](ScenarioClient& client, int32_t npcObjectId) {
		client.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(npcObjectId));
		return decoders::decodeDialogWindow(waitFor(*client.game, "SM_DIALOG_WINDOW").data);
	};
	/** CM_DIALOG_SELECT(npc, AIRLINE_SERVICE) -> SM_TELEPORT_MAP (DialogService.java:187-197 -> TeleportService.showMap) */
	const auto openMap = [&](ScenarioClient& client, int32_t npcObjectId) {
		client.game->send(GameSession::CM_DIALOG_SELECT, GameSession::buildCM_DIALOG_SELECT(npcObjectId, DIALOG_AIRLINE_SERVICE));
		return decoders::decodeTeleportMap(waitFor(*client.game, "SM_TELEPORT_MAP").data);
	};
	/** the first SM_EMOTION of `type` sent by `objectId` that arrives within `timeout`; std::nullopt if none */
	const auto awaitEmotion = [&](ScenarioClient& client, int32_t objectId, uint8_t type, std::chrono::milliseconds timeout) {
		std::optional<std::pair<size_t, decoders::Emotion>> found;
		const auto deadline = Clock::now() + timeout;
		while (!found && Clock::now() < deadline) {
			std::optional<Packet> packet = client.game->next(250ms);
			if (!packet || packet->name != "SM_EMOTION")
				continue;
			try {
				const decoders::Emotion emotion = decoders::decodeEmotion(packet->data);
				if (emotion.emotionType == type && emotion.senderObjectId == objectId)
					found = std::make_pair(client.game->recorded().size() - 1, emotion);
			} catch (const DecodeError&) {
				// another creature's emotion the decoder does not know
			}
		}
		return found;
	};
	/**
	 * A flight transport after its SM_EMOTION(START_FLYTELEPORT): CM_MOVE_IN_AIR along the straight line of `path` in `steps` steps with rising
	 * distances, then CM_EMOTION(LAND_FLYTELEPORT). @return the index (0-based) of each step's packet in the session's recording
	 */
	const auto fly = [&](ScenarioClient& client, const FlyPath& path, int32_t steps, const std::function<void(int32_t, float, float, float)>& onStep) {
		for (int32_t step = 1; step <= steps; step++) {
			const float t = static_cast<float>(step) / static_cast<float>(steps);
			const float px = path.sx + (path.ex - path.sx) * t, py = path.sy + (path.ey - path.sy) * t, pz = path.sz + (path.ez - path.sz) * t;
			client.game->send(GameSession::CM_MOVE_IN_AIR, GameSession::buildCM_MOVE_IN_AIR(client.worldId, px, py, pz, 0, step * 100));
			collectFor(*client.game, 150ms);
			if (onStep)
				onStep(step, px, py, pz);
		}
		client.x = path.ex;
		client.y = path.ey;
		client.z = path.ez;
		const size_t landing = client.mark();
		client.game->send(GameSession::CM_EMOTION, GameSession::buildCM_EMOTION(GameSession::EMOTION_LAND_FLYTELEPORT));
		const auto land = awaitEmotion(client, client.playerId, GameSession::EMOTION_LAND_FLYTELEPORT, 10s);
		if (!land)
			throw std::runtime_error(client.label + ": no SM_EMOTION(LAND_FLYTELEPORT): " + join(namesOf(client.since(landing))));
		EXPECT_FALSE(land->second.state & STATE_FLYING) << client.label << ": onFlyTeleportEnd unset FLYING: state " << land->second.state;
		EXPECT_TRUE(land->second.state & STATE_ACTIVE) << client.label << ": onFlyTeleportEnd set ACTIVE: state " << land->second.state;
		collectFor(*client.game, 500ms);
	};
	/** a flight master's route: the map, the select, the take-off emotion and the price (§10.3 T5 / T7). @return the take-off's recording index */
	const auto takeOff = [&](ScenarioClient& client, int32_t masterTemplate, int32_t locId) {
		const ONpc& master = npcs.at(masterTemplate);
		const int32_t masterObject = requireNpc(client, masterTemplate, master.spot());
		const decoders::TeleportMap map = openMap(client, masterObject);
		EXPECT_EQ(map.targetObjectId, masterObject);
		EXPECT_EQ(map.teleportId, master.teleportId) << client.label << ": npc_teleporter.xml's teleportId of " << masterTemplate;
		const int64_t before = client.kinah();
		const size_t selected = client.mark();
		client.game->send(GameSession::CM_TELEPORT_SELECT, GameSession::buildCM_TELEPORT_SELECT(masterObject, locId));
		const auto start = awaitEmotion(client, client.playerId, GameSession::EMOTION_START_FLYTELEPORT, 15s);
		if (!start)
			throw std::runtime_error(client.label + ": no SM_EMOTION(START_FLYTELEPORT) after CM_TELEPORT_SELECT: " + join(namesOf(client.since(selected))));
		const OLoc& loc = master.loc(locId);
		EXPECT_EQ(start->second.emotion, loc.teleportId) << client.label << ": the location's teleportid (TeleportService.java:121-122), not the "
		                                                    "template's teleportId " << master.teleportId;
		EXPECT_TRUE(start->second.state & STATE_FLYING) << "setState(FLYING): state " << start->second.state;
		EXPECT_FALSE(start->second.state & STATE_ACTIVE) << "unsetState(ACTIVE): state " << start->second.state;
		collectFor(*client.game, 500ms);
		EXPECT_EQ(client.kinah(), before - loc.servicePrice) << client.label << ": getPriceForService(" << loc.price << ")";
		EXPECT_TRUE(ofName(client.since(selected), "SM_TELEPORT_LOC").empty()) << "a flight is no teleport";
		return start->first;
	};
	/** a teleporter's route up to SM_TELEPORT_LOC (§2.1 steps 1-6). @return the decoded SM_TELEPORT_LOC and its recording index */
	const auto selectRoute = [&](ScenarioClient& client, int32_t teleporterTemplate, int32_t locId) {
		const ONpc& teleporter = npcs.at(teleporterTemplate);
		const int32_t teleporterObject = requireNpc(client, teleporterTemplate, teleporter.spot());
		EXPECT_EQ(talk(client, teleporterObject).targetObjectId, teleporterObject);
		const decoders::TeleportMap map = openMap(client, teleporterObject);
		EXPECT_EQ(map.targetObjectId, teleporterObject);
		EXPECT_EQ(map.teleportId, teleporter.teleportId) << client.label << ": npc_teleporter.xml's teleportId of " << teleporterTemplate;
		client.game->send(GameSession::CM_TELEPORT_SELECT, GameSession::buildCM_TELEPORT_SELECT(teleporterObject, locId));
		const Packet locPacket = waitFor(*client.game, "SM_TELEPORT_LOC");
		const decoders::TeleportLoc loc = decoders::decodeTeleportLoc(locPacket.data);
		const OLoc& expected = teleporter.loc(locId);
		EXPECT_EQ(loc.animation, decoders::TELEPORT_ANIMATION_JUMP_IN) << "npc.hasStatic() is false for a teleporter (CM_TELEPORT_SELECT.java:68)";
		EXPECT_EQ(loc.mapId, expected.map);
		EXPECT_EQ(loc.mapOrInstanceId, expected.map) << "no instance map: writeD(mapId) twice";
		EXPECT_FLOAT_EQ(loc.x, expected.x);
		EXPECT_FLOAT_EQ(loc.y, expected.y);
		EXPECT_FLOAT_EQ(loc.z, expected.z);
		EXPECT_EQ(loc.heading, expected.heading);
		return std::make_pair(loc, client.game->recorded().size() - 1);
	};
	/** the second half of an animated teleport: CM_TELEPORT_ANIMATION_DONE -> SM_CHANNEL_INFO + SM_PLAYER_SPAWN; @return the spawn */
	const auto animationDone = [&](ScenarioClient& client) {
		const size_t done = client.mark();
		client.game->send(GameSession::CM_TELEPORT_ANIMATION_DONE, GameSession::buildCM_TELEPORT_ANIMATION_DONE());
		const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(waitFor(*client.game, "SM_PLAYER_SPAWN").data);
		const std::vector<Packet> arrival = client.since(done);
		const Packet* channel = firstOfName(arrival, "SM_CHANNEL_INFO");
		EXPECT_NE(channel, nullptr) << client.label << ": SM_CHANNEL_INFO before SM_PLAYER_SPAWN: " << join(namesOf(arrival));
		if (channel != nullptr) {
			const decoders::ChannelInfo info = decoders::decodeChannelInfo(channel->data);
			EXPECT_EQ(info.currentChannel, 1) << "D7: the player is not spawned yet, so SM_CHANNEL_INFO is (1, 1)";
			EXPECT_EQ(info.instanceCount, 1);
		}
		client.worldId = spawned.worldId;
		client.x = spawned.x;
		client.y = spawned.y;
		client.z = spawned.z;
		return std::make_pair(spawned, done);
	};
	/** E2 / S1 at a teleporter: the whole map-to-map route with its price, its arrival point and the arrival's npcs (T10, T11, T19) */
	const auto mapToMap = [&](ScenarioClient& client, int32_t teleporterTemplate, int32_t locId, std::string_view label) {
		const OLoc& expected = npcs.at(teleporterTemplate).loc(locId);
		const int64_t before = client.kinah();
		const auto [loc, locIndex] = selectRoute(client, teleporterTemplate, locId);
		collectFor(*client.game, 500ms);
		EXPECT_EQ(client.kinah(), before - expected.servicePrice) << label << ": checkKinahForTransportation, getPriceForService(" << expected.price << ")";
		const auto [spawned, done] = animationDone(client);
		EXPECT_EQ(spawned.worldId, expected.map);
		EXPECT_FLOAT_EQ(spawned.x, expected.x);
		EXPECT_FLOAT_EQ(spawned.y, expected.y);
		EXPECT_FLOAT_EQ(spawned.z, expected.z);
		EXPECT_EQ(spawned.heading, expected.heading);
		const std::vector<Packet> ready = levelReady(client);
		return std::make_tuple(locIndex, done, ready);
	};
	/** the use bar of a portal (ActionItemNpcAI.java:36-80): SM_USE_OBJECT(1) + START_QUESTLOOT, `talkDelayMs` later END_QUESTLOOT + (2) */
	const auto useBar = [&](ScenarioClient& client, int32_t portalObject, int32_t talkDelayMs, std::string_view label) {
		const size_t from = client.mark();
		client.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(portalObject));
		const Packet start = waitFor(*client.game, "SM_USE_OBJECT", 10s);
		const decoders::UseObject startBar = decoders::decodeUseObject(start.data);
		EXPECT_EQ(startBar.playerObjectId, client.playerId) << label;
		EXPECT_EQ(startBar.targetObjectId, portalObject) << label;
		EXPECT_EQ(startBar.time, talkDelayMs) << label << ": the npc's talk delay in ms";
		EXPECT_EQ(startBar.actionType, decoders::USE_OBJECT_START_BAR) << label;
		const Packet end = waitFor(*client.game, "SM_USE_OBJECT", std::chrono::milliseconds(talkDelayMs) + 5s);
		const decoders::UseObject endBar = decoders::decodeUseObject(end.data);
		EXPECT_EQ(endBar.actionType, decoders::USE_OBJECT_CANCEL_BAR) << label << ": the task's own SM_USE_OBJECT(..., cancelBarAnimation)";
		EXPECT_EQ(endBar.time, talkDelayMs) << label;
		const double barMs = std::chrono::duration<double, std::milli>(end.receivedAt - start.receivedAt).count();
		EXPECT_NEAR(barMs, talkDelayMs, 500.0) << label << ": the bar's task runs talkDelayInMs after the start";
		bool startEmotion = false, endEmotion = false;
		for (const Packet& packet : ofName(client.since(from), "SM_EMOTION")) {
			try {
				const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
				if (emotion.senderObjectId != client.playerId)
					continue;
				startEmotion |= emotion.emotionType == EMOTION_START_QUESTLOOT;
				endEmotion |= emotion.emotionType == EMOTION_END_QUESTLOOT;
			} catch (const DecodeError&) {
			}
		}
		EXPECT_TRUE(startEmotion) << label << ": SM_EMOTION(START_QUESTLOOT) with the bar";
		EXPECT_TRUE(endEmotion) << label << ": SM_EMOTION(END_QUESTLOOT) at its end";
		return from;
	};

	// ======================================================================================================================================
	// C1: the characters
	// ======================================================================================================================================

	decoders::BindPointInfo e1Bind;
	runCase("C1", "create E1, E2, S1; seed E2 and S1 offline; E2 enters at Melponeh, E1 at the Elyos spawn (prologue)", [&] {
		// E2 and S1: created, then seeded while their accounts are logged out (F-3)
		for (ScenarioClient* client : {&e2, &s1}) {
			const decoders::CharacterList list = logIn(servers, *client);
			EXPECT_EQ(list.characterCount, 0);
			createCharacter(*client);
			disconnect(*client);
			seedDaeva(*client);
		}
		// E2 at Melponeh beside Aero: flypath 6's start, 1.5 m off it so that E1's landing (flypath 5's end) does not stand inside E2
		seedPosition(e2, POETA, FLYPATH_6.sx + 1.5f, FLYPATH_6.sy, FLYPATH_6.sz);
		const OSpot& osmar = npcs.at(OSMAR).spot();
		seedPosition(s1, osmar.map, osmar.x + 1.5f, osmar.y, osmar.z);
		relogIn(servers, e2);
		EXPECT_EQ(e2.worldId, POETA);
		EXPECT_EQ(e2.kinah(), DAEVA_KINAH);
		// E1: a fresh character, its first enter world in its start map with the prologue (PrologueSupport.h)
		const decoders::CharacterList list = logIn(servers, e1);
		EXPECT_EQ(list.characterCount, 0);
		createCharacter(e1);
		enterWorld(e1);
		EXPECT_EQ(e1.worldId, POETA);
		EXPECT_FLOAT_EQ(e1.x, elyosWarrior.x);
		levelReady(e1, false);
		AsyncAllowed async = e1.async;
		async.temporarySpawnUpdates(e1.npcs.predicate());
		endPrologue(*e1.game, e1.lastLevelReady, decoders::ELYOS_PROLOGUE, 0, async, [&] { return collectBurst(*e1.game, async); }, "E1's prologue");
		e1.model.sync();
		EXPECT_EQ(hotspotAkarios.map, POETA);
	});

	// ======================================================================================================================================
	// C2, C3: the hotspot (§2.3; T1, T2)
	// ======================================================================================================================================

	std::chrono::system_clock::time_point hotspotCharged;
	runCase("C2", "T1: the Akarios hotspot - the cast, the charge 10 s later, the same-map move 1 s after it", [&] {
		const int64_t before = e1.kinah();
		const size_t from = e1.mark();
		e1.game->send(GameSession::CM_BIND_POINT_TELEPORT,
		              GameSession::buildCM_BIND_POINT_TELEPORT(GameSession::BIND_POINT_TELEPORT_CAST, HOTSPOT_AKARIOS, hotspotAkarios.price));
		const auto action = [&](uint8_t wanted) {
			return [&, wanted](const Packet& packet) {
				const decoders::BindPointTeleport teleport = decoders::decodeBindPointTeleport(packet.data);
				return teleport.action == wanted && teleport.playerId == e1.playerId;
			};
		};
		const Packet cast = waitFor(*e1.game, "SM_BIND_POINT_TELEPORT", 5s, action(1));
		const decoders::BindPointTeleport castInfo = decoders::decodeBindPointTeleport(cast.data);
		EXPECT_EQ(castInfo.locId, HOTSPOT_AKARIOS);
		// nothing is charged until the 10-s task runs (BindPointTeleportService.java:53-57)
		collectFor(*e1.game, 8s);
		EXPECT_EQ(e1.kinah(), before) << "T1: the hotspot is charged by its 10-s task, not at the cast";
		const Packet done = waitFor(*e1.game, "SM_BIND_POINT_TELEPORT", 5s, action(3));
		hotspotCharged = std::chrono::system_clock::now();
		const decoders::BindPointTeleport doneInfo = decoders::decodeBindPointTeleport(done.data);
		EXPECT_EQ(doneInfo.locId, HOTSPOT_AKARIOS);
		EXPECT_EQ(doneInfo.cooldown, HOTSPOT_COOLDOWN);
		const double charged = std::chrono::duration<double>(done.receivedAt - cast.receivedAt).count();
		EXPECT_NEAR(charged, 10.0, 1.0) << "T1: SM_BIND_POINT_TELEPORT(3) 10 +- 1 s after (1)";
		// the move: the inner 1-s task, a same-map teleportTo (spawnOnSameMap: SM_CHANNEL_INFO, SM_PLAYER_INFO, SM_STATS_INFO, SM_MOTION)
		const Packet channel = waitFor(*e1.game, "SM_CHANNEL_INFO", 5s);
		const double moved = std::chrono::duration<double>(channel.receivedAt - done.receivedAt).count();
		EXPECT_NEAR(moved, 1.0, 0.5) << "T1: the move 1 +- 0.5 s after the charge";
		collectFor(*e1.game, 1500ms);
		EXPECT_EQ(e1.kinah(), before - hotspotAkarios.price) << "T1: the oracle's float-distance price";
		const std::vector<Packet> after = e1.since(from);
		const decoders::ChannelInfo channelInfo = decoders::decodeChannelInfo(channel.data);
		EXPECT_EQ(channelInfo.currentChannel, 1);
		EXPECT_EQ(channelInfo.instanceCount, 1);
		const std::vector<decoders::PlayerInfo> infos = selfPlayerInfos(e1, after);
		ASSERT_FALSE(infos.empty()) << "T1: no SM_PLAYER_INFO of E1 after the move: " << join(namesOf(after));
		EXPECT_NEAR(infos.back().x, hotspotAkarios.x, 0.01);
		EXPECT_NEAR(infos.back().y, hotspotAkarios.y, 0.01);
		EXPECT_NEAR(infos.back().z, hotspotAkarios.z, 0.01);
		EXPECT_NE(firstOfName(after, "SM_STATS_INFO"), nullptr) << join(namesOf(after));
		EXPECT_NE(firstOfName(after, "SM_MOTION"), nullptr) << join(namesOf(after));
		EXPECT_EQ(firstOfName(after, "SM_PLAYER_SPAWN"), nullptr) << "T1: a same-map move sends no SM_PLAYER_SPAWN";
		EXPECT_TRUE(servers.gameServer()->findLogLines("prices don't match", 5).empty()) << "T1: the client sent the oracle's price";
		e1.x = hotspotAkarios.x;
		e1.y = hotspotAkarios.y;
		e1.z = hotspotAkarios.z;
	});

	runCase("C3", "T2: the hotspot cooldown refuses a second hotspot", [&] {
		const int64_t before = e1.kinah();
		const size_t from = e1.mark();
		e1.game->send(GameSession::CM_BIND_POINT_TELEPORT,
		              GameSession::buildCM_BIND_POINT_TELEPORT(GameSession::BIND_POINT_TELEPORT_CAST, HOTSPOT_MELPONEH, hotspotAkarios.basePrice * 2));
		collectFor(*e1.game, 2s);
		const std::vector<Packet> after = e1.since(from);
		EXPECT_EQ(messagesOf(after, STR_FLYING_TIME_NOT_READY).size(), 1u) << join(namesOf(after));
		EXPECT_TRUE(ofName(after, "SM_BIND_POINT_TELEPORT").empty());
		EXPECT_EQ(e1.kinah(), before);
	});

	// ======================================================================================================================================
	// C4, C5: Daines and a non-Daeva (T3, T4)
	// ======================================================================================================================================

	runCase("C4", "T3: Daines refuses E1, a non-Daeva, his map (NO_RIGHT)", [&] {
		const int32_t daines = requireNpc(e1, DAINES, npcs.at(DAINES).spot());
		EXPECT_EQ(talk(e1, daines).targetObjectId, daines);
		const size_t from = e1.mark();
		e1.game->send(GameSession::CM_DIALOG_SELECT, GameSession::buildCM_DIALOG_SELECT(daines, DIALOG_AIRLINE_SERVICE));
		const decoders::DialogWindow page = decoders::decodeDialogWindow(waitFor(*e1.game, "SM_DIALOG_WINDOW").data);
		EXPECT_EQ(page.targetObjectId, daines);
		EXPECT_EQ(page.dialogPageId, DIALOG_PAGE_NO_RIGHT);
		collectFor(*e1.game, 1s);
		EXPECT_TRUE(ofName(e1.since(from), "SM_TELEPORT_MAP").empty()) << "T3: no map for a non-Daeva";
	});

	runCase("C5", "T4: a crafted CM_TELEPORT_SELECT to Verteron that E1 cannot pay (D7: the packet path has no Daeva check)", [&] {
		const int32_t daines = requireNpc(e1, DAINES, npcs.at(DAINES).spot());
		const int64_t before = e1.kinah();
		const size_t from = e1.mark();
		e1.game->send(GameSession::CM_TELEPORT_SELECT, GameSession::buildCM_TELEPORT_SELECT(daines, LOC_VERTERON));
		collectFor(*e1.game, 2s);
		const std::vector<Packet> after = e1.since(from);
		const std::vector<decoders::SystemMessage> refused = messagesOf(after, STR_MSG_NOT_ENOUGH_KINA);
		ASSERT_EQ(refused.size(), 1u) << join(namesOf(after));
		ASSERT_EQ(refused[0].params.size(), 1u);
		EXPECT_EQ(refused[0].params[0], std::to_string(npcs.at(DAINES).loc(LOC_VERTERON).servicePrice)) << "T4: the price after taxes";
		EXPECT_TRUE(ofName(after, "SM_TELEPORT_LOC").empty());
		EXPECT_TRUE(selfPlayerInfos(e1, after).empty()) << "T4: E1 does not move";
		EXPECT_EQ(e1.kinah(), before);
	});

	// ======================================================================================================================================
	// C6: the flight, watched by E2 at Melponeh (T5)
	// ======================================================================================================================================

	runCase("C6", "T5: Kustanon flies E1 to Melponeh; E2 sees E1 once on the way; the known lists follow the flight; a move after the landing", [&] {
		e2.drain();
		const size_t e2From = e2.mark();
		const size_t e1From = e1.mark();
		const int32_t kustanonObject = requireNpc(e1, KUSTANON, npcs.at(KUSTANON).spot());
		takeOff(e1, KUSTANON, LOC_MELPONEH);
		// the step count: no step may land within 2 m of E2's 95 m, so that "the step that first put E1 within range" is one step (§2.2 step 5)
		const float e2x = e2.x, e2y = e2.y, e2z = e2.z;
		int32_t steps = 20;
		for (; steps < 40; steps++) {
			bool clean = true;
			for (int32_t step = 1; step <= steps && clean; step++) {
				const float t = static_cast<float>(step) / static_cast<float>(steps);
				const double d = std::sqrt(std::pow(FLYPATH_5.sx + (FLYPATH_5.ex - FLYPATH_5.sx) * t - e2x, 2) +
				                           std::pow(FLYPATH_5.sy + (FLYPATH_5.ey - FLYPATH_5.sy) * t - e2y, 2) +
				                           std::pow(FLYPATH_5.sz + (FLYPATH_5.ez - FLYPATH_5.sz) * t - e2z, 2));
				clean = std::abs(d - VISIBILITY) > 2.0;
			}
			if (clean)
				break;
		}
		std::optional<int32_t> firstInRange;
		const auto flight = [&](int32_t step, float px, float py, float pz) {
			if (!firstInRange && inRangeFloat(px, py, pz, e2x, e2y, e2z, VISIBILITY))
				firstInRange = step;
			e2.drain(50ms);
		};
		// E1 flies flypath 5 from where it stands (Akarios, after the hotspot) to its end beside the Melponeh obelisk
		const FlyPath path{e1.x, e1.y, e1.z, FLYPATH_5.ex, FLYPATH_5.ey, FLYPATH_5.ez};
		const size_t flying = e1.mark();
		e2.drain();
		const size_t e2Flying = e2.mark();
		fly(e1, path, steps, flight);
		const size_t e1Landed = e1.mark();
		e2.drain(500ms);
		const size_t e2Landed = e2.mark();
		ASSERT_TRUE(firstInRange) << "the flight never comes within 95 m of E2";
		// E2 receives SM_PLAYER_INFO(E1) exactly once during the flight, with the flight id and the distance of the step that brought E1 in
		std::vector<decoders::PlayerInfo> seen;
		for (const Packet& packet : ofName(slice(*e2.game, e2Flying, e2Landed), "SM_PLAYER_INFO")) {
			try {
				decoders::PlayerInfoOptions options;
				options.flightPath = true;
				const decoders::PlayerInfo info = decoders::decodePlayerInfo(packet.data, options);
				if (info.objectId == e1.playerId)
					seen.push_back(info);
			} catch (const DecodeError& error) {
				ADD_FAILURE() << "T5: an SM_PLAYER_INFO E2 received during the flight does not decode as a flying player's: " << error.what();
			}
		}
		ASSERT_EQ(seen.size(), 1u) << "T5: E2 sees the flying E1 exactly once (KnownList.findVisibleObjects, PlayerController.see)";
		EXPECT_EQ(seen[0].flightPathId, npcs.at(KUSTANON).loc(LOC_MELPONEH).teleportId) << "T5: the flight id in SM_PLAYER_INFO";
		EXPECT_EQ(seen[0].flightPathDistance, *firstInRange * 100) << "T5: the distance of the CM_MOVE_IN_AIR that first put E1 within 95 m";
		// E1's own known list followed the flight: Aero at Melponeh announced, Kustanon at Akarios deleted
		const std::vector<Packet> e1Flight = slice(*e1.game, flying, e1Landed);
		bool aeroAnnounced = false;
		for (const auto& [object, templateId] : npcsAnnounced(e1Flight))
			aeroAnnounced |= templateId == AERO;
		EXPECT_TRUE(aeroAnnounced) << "T5: the Melponeh npcs are announced during the flight (CM_MOVE_IN_AIR's updatePosition)";
		EXPECT_FALSE(deletesOf(e1Flight, kustanonObject).empty()) << "T5: Kustanon is forgotten during the flight";
		// after the landing a CM_MOVE_IN_AIR to Akarios moves nothing: E1 no longer flies (CM_MOVE_IN_AIR.java: the FLYING check)
		const size_t afterLanding = e1.mark();
		e2.drain();
		const size_t e2AfterLanding = e2.mark();
		e1.game->send(GameSession::CM_MOVE_IN_AIR, GameSession::buildCM_MOVE_IN_AIR(e1.worldId, FLYPATH_5.sx, FLYPATH_5.sy, FLYPATH_5.sz, 0, 99999));
		collectFor(*e1.game, 2s);
		e2.drain(500ms);
		EXPECT_TRUE(deletesOf(slice(*e2.game, e2AfterLanding), e1.playerId).empty()) << "T5: E2 keeps seeing E1 after the post-landing CM_MOVE_IN_AIR";
		const std::vector<Packet> e1After = e1.since(afterLanding);
		EXPECT_TRUE(deletesOf(e1After, e2.playerId).empty()) << "T5: E1 keeps seeing E2";
		std::vector<std::string> akarios;
		for (const auto& [object, templateId] : npcsAnnounced(e1After))
			if (templateId == KUSTANON || templateId == DAINES)
				akarios.push_back(std::to_string(templateId));
		EXPECT_TRUE(akarios.empty()) << "T5: the Akarios npcs are not announced again: " << join(akarios);
		// the CM_MOVE at the landing point reaches E2 as SM_MOVE(E1) (CM_MOVE.java:149): E1 is still in E2's known list
		e2.drain();
		const size_t e2Move = e2.mark();
		// 2 m towards the obelisk (ResurrectAI's accept wants the player within 5 m of it, C7)
		e1.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(e1.x - 2.0f, e1.y, e1.z, 0, static_cast<int8_t>(0xE0), e1.x - 2.0f, e1.y, e1.z));
		e1.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(e1.x - 2.0f, e1.y, e1.z, 0, 0));
		e1.x -= 2.0f;
		collectFor(*e1.game, 1s);
		e2.drain(1s);
		bool moveSeen = false;
		for (const Packet& packet : ofName(slice(*e2.game, e2Move), "SM_MOVE"))
			moveSeen |= decoders::decodeMoveObjectId(packet.data) == e1.playerId;
		EXPECT_TRUE(moveSeen) << "T5: E2 receives SM_MOVE(E1) at the landing point";
		std::cout << "C6: " << steps << " CM_MOVE_IN_AIR, the first within 95 m of E2 is step " << *firstInRange << std::endl;
		(void)e1From;
		(void)e2From;
	});

	// ======================================================================================================================================
	// C7: the obelisk (T6)
	// ======================================================================================================================================

	runCase("C7", "T6: bind at the Melponeh obelisk; the second attempt is refused", [&] {
		const int32_t obeliskObject = requireNpc(e1, MELPONEH_OBELISK, obelisk.spots.at(0));
		const int64_t before = e1.kinah();
		const size_t from = e1.mark();
		e1.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(obeliskObject));
		const decoders::QuestionWindow question = decoders::decodeQuestionWindow(waitFor(*e1.game, "SM_QUESTION_WINDOW").data);
		EXPECT_EQ(question.code, STR_ASK_REGISTER_RESURRECT_POINT);
		EXPECT_EQ(question.params[0], std::to_string(obelisk.price)) << "T6: the raw bind price (ResurrectAI.java:106)";
		e1.game->send(GameSession::CM_QUESTION_RESPONSE, GameSession::buildCM_QUESTION_RESPONSE(question.code, 1, question.senderId));
		const decoders::BindPointInfo bind = decoders::decodeBindPointInfo(waitFor(*e1.game, "SM_BIND_POINT_INFO").data);
		collectFor(*e1.game, 1500ms);
		const std::vector<Packet> after = e1.since(from);
		EXPECT_EQ(bind.type, 0) << "an obelisk";
		EXPECT_EQ(bind.mapId, POETA);
		EXPECT_NEAR(bind.x, e1.x, 0.01) << "T6: the PLAYER's position, not the obelisk's (ResurrectAI.java:91-92)";
		EXPECT_NEAR(bind.y, e1.y, 0.01);
		EXPECT_NEAR(bind.z, e1.z, 0.01);
		EXPECT_EQ(e1.kinah(), before - obelisk.price);
		bool animation = false;
		for (const Packet& packet : ofName(after, "SM_ACTION_ANIMATION")) {
			const decoders::ActionAnimation decoded = decoders::decodeActionAnimation(packet.data);
			animation |= decoded.objectId == e1.playerId && decoded.animation == ACTION_ANIMATION_BIND_KISK;
		}
		EXPECT_TRUE(animation) << "T6: SM_ACTION_ANIMATION(E1, BIND_KISK)";
		EXPECT_EQ(messagesOf(after, STR_DEATH_REGISTER_RESURRECT_POINT).size(), 1u);
		const auto rows = database.queryRows(schema, "SELECT map_id, x, y, z FROM player_bind_point WHERE player_id = " + std::to_string(e1.playerId), 4);
		ASSERT_EQ(rows.size(), 1u) << "T6 / D12: the first write of player_bind_point";
		EXPECT_EQ(rows[0][0].value_or(""), std::to_string(POETA));
		EXPECT_NEAR(std::stod(rows[0][1].value_or("0")), e1.x, 0.01);
		EXPECT_NEAR(std::stod(rows[0][2].value_or("0")), e1.y, 0.01);
		EXPECT_NEAR(std::stod(rows[0][3].value_or("0")), e1.z, 0.01);
		e1Bind = bind;
		// the second attempt: already bound within 20 m
		const size_t again = e1.mark();
		e1.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(obeliskObject));
		collectFor(*e1.game, 1500ms);
		const std::vector<Packet> second = e1.since(again);
		EXPECT_EQ(messagesOf(second, STR_ALREADY_REGISTER_THIS_RESURRECT_POINT).size(), 1u) << join(namesOf(second));
		EXPECT_TRUE(ofName(second, "SM_QUESTION_WINDOW").empty());
		EXPECT_EQ(e1.kinah(), before - obelisk.price) << "no second charge";
	});

	// ======================================================================================================================================
	// C8, C11: the flights back (T7)
	// ======================================================================================================================================

	runCase("C8", "T7: Aero flies E1 back to Akarios", [&] {
		takeOff(e1, AERO, LOC_AKARIOS);
		fly(e1, FlyPath{e1.x, e1.y, e1.z, FLYPATH_6.ex, FLYPATH_6.ey, FLYPATH_6.ez}, 20, {});
	});

	runCase("C11", "T7: Aero flies E2 to Akarios (a second character, the second flight master's flight id)", [&] {
		takeOff(e2, AERO, LOC_AKARIOS);
		// E2 lands 1.5 m beside E1, within reach of Daines
		fly(e2, FlyPath{e2.x, e2.y, e2.z, FLYPATH_6.ex + 1.5f, FLYPATH_6.ey, FLYPATH_6.ez}, 20, {});
		e1.drain();
	});

	// ======================================================================================================================================
	// C12: map to map, watched by E1 (T10)
	// ======================================================================================================================================

	runCase("C12", "T10: Daines takes E2 to Verteron; E1 at Akarios watches the jump; the arrival waits for CM_TELEPORT_ANIMATION_DONE", [&] {
		e1.drain();
		const size_t e1From = e1.mark();
		const size_t e2From = e2.mark();
		const OLoc& verteron = npcs.at(DAINES).loc(LOC_VERTERON);
		const int64_t before = e2.kinah();
		const auto [loc, locIndex] = selectRoute(e2, DAINES, LOC_VERTERON);
		// nothing else arrives until the animation is done: no spawn, no channel, no deletions for the teleporting player
		collectFor(*e2.game, 2s);
		EXPECT_EQ(e2.kinah(), before - verteron.servicePrice) << "T10: getPriceForService(" << verteron.price << ")";
		const std::vector<Packet> waiting = slice(*e2.game, locIndex + 1);
		EXPECT_TRUE(ofName(waiting, "SM_PLAYER_SPAWN").empty()) << "T10: the arrival waits for CM_TELEPORT_ANIMATION_DONE";
		EXPECT_TRUE(ofName(waiting, "SM_CHANNEL_INFO").empty()) << join(namesOf(waiting));
		std::cout << "C12: E2 between SM_TELEPORT_LOC and the animation's end: " << join(namesOf(waiting)) << std::endl;
		// E1 saw E2 vanish with the jump animation (TeleportAnimation.JUMP_IN -> ObjectDeleteAnimation 11)
		e1.drain(500ms);
		const std::vector<decoders::Delete> vanished = deletesOf(slice(*e1.game, e1From), e2.playerId);
		ASSERT_EQ(vanished.size(), 1u) << "T10: the observer E1 receives SM_DELETE(E2) once";
		EXPECT_EQ(vanished[0].animation, decoders::DELETE_ANIMATION_JUMP_IN);
		const auto [spawned, done] = animationDone(e2);
		EXPECT_EQ(spawned.worldId, VERTERON);
		EXPECT_FLOAT_EQ(spawned.x, verteron.x);
		EXPECT_FLOAT_EQ(spawned.y, verteron.y);
		EXPECT_FLOAT_EQ(spawned.z, verteron.z);
		// the teleporting player receives no SM_DELETE (World.despawn marks it unspawned before the known list is cleared)
		EXPECT_TRUE(ofName(slice(*e2.game, e2From, e2.mark()), "SM_DELETE").empty()) << "T10: a teleporting player gets no deletion packets";
		const std::vector<Packet> ready = levelReady(e2);
		checkArrivalNpcs(ready, oracle->spawns(VERTERON, verteron.x, verteron.y, verteron.z, 0, 120.0), "T10 Verteron");
	});

	// ======================================================================================================================================
	// C9, C10: Return on the map and the bind round trip (T8, T9)
	// ======================================================================================================================================

	runCase("C9", "T8: E1 casts Return at Akarios and lands at the bind point of T6, on the same map", [&] {
		GameSession::CastRequest request;
		request.spellId = SKILL_RETURN;
		request.level = 1;
		request.targetType = 0;
		request.targetObjectId = e1.playerId;
		const GameSession::CastOutcome outcome = e1.game->castAndWait(e1.playerId, request, 20s);
		ASSERT_TRUE(outcome.castSpell) << "T8: no SM_CASTSPELL(243)";
		const decoders::CastSpell cast = decoders::decodeCastSpell(e1.game->recorded()[*outcome.castSpell].data);
		EXPECT_NEAR(cast.castDuration, RETURN_CAST_MS, RETURN_CAST_MS / 10) << "T8: a 6,000 ms bar";
		const size_t from = outcome.firstPacket;
		const Packet channel = waitFor(*e1.game, "SM_CHANNEL_INFO", 10s);
		collectFor(*e1.game, 1500ms);
		const std::vector<Packet> after = e1.since(from);
		const std::vector<decoders::PlayerInfo> infos = selfPlayerInfos(e1, after);
		ASSERT_FALSE(infos.empty()) << join(namesOf(after));
		EXPECT_NEAR(infos.back().x, e1Bind.x, 0.01) << "T8: moveToBindLocation's bind arm";
		EXPECT_NEAR(infos.back().y, e1Bind.y, 0.01);
		EXPECT_NEAR(infos.back().z, e1Bind.z, 0.01);
		EXPECT_EQ(firstOfName(after, "SM_PLAYER_SPAWN"), nullptr) << "T8: same map";
		e1.x = e1Bind.x;
		e1.y = e1Bind.y;
		e1.z = e1Bind.z;
		(void)channel;
	});

	runCase("C10", "T9: the bind point and the hotspot cooldown survive a relog", [&] {
		disconnect(e1);
		const auto rows = database.queryRows(schema, "SELECT map_id FROM player_bind_point WHERE player_id = " + std::to_string(e1.playerId), 1);
		EXPECT_EQ(rows.size(), 1u) << "T9: one player_bind_point row";
		const std::vector<Packet> burst = relogIn(servers, e1);
		const Packet* bind = firstOfName(burst, "SM_BIND_POINT_INFO");
		ASSERT_NE(bind, nullptr) << join(namesOf(burst));
		const decoders::BindPointInfo info = decoders::decodeBindPointInfo(bind->data);
		EXPECT_EQ(info.mapId, e1Bind.mapId);
		EXPECT_FLOAT_EQ(info.x, e1Bind.x);
		EXPECT_FLOAT_EQ(info.y, e1Bind.y);
		EXPECT_FLOAT_EQ(info.z, e1Bind.z);
		const int64_t elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - hotspotCharged).count();
		std::optional<decoders::BindPointTeleport> cooldown;
		for (const Packet& packet : ofName(burst, "SM_BIND_POINT_TELEPORT")) {
			const decoders::BindPointTeleport decoded = decoders::decodeBindPointTeleport(packet.data);
			if (decoded.action == 3 && decoded.playerId == e1.playerId)
				cooldown = decoded;
		}
		if (elapsed < HOTSPOT_COOLDOWN - 2) {
			ASSERT_TRUE(cooldown) << "T9: BindPointTeleportService.onLogin at the enter world (PlayerEnterWorldService.java:283): " << join(namesOf(burst));
			EXPECT_EQ(cooldown->locId, HOTSPOT_AKARIOS);
			EXPECT_NEAR(cooldown->cooldown, static_cast<double>(HOTSPOT_COOLDOWN - elapsed), 2.0) << "T9: the time left, " << elapsed << " s after the charge";
		} else {
			EXPECT_FALSE(cooldown) << "T9: " << elapsed << " s after the charge the cooldown is over";
			std::cout << "C10: " << elapsed << " s since the hotspot's charge: T9 asserts the cooldown's absence" << std::endl;
		}
	});

	// ======================================================================================================================================
	// C12b, C13: the jump to Sanctum (T10b, T11)
	// ======================================================================================================================================

	runCase("C12b", "T10b: E2 quits during the jump to Sanctum; the relog finds it where it was, the kinah paid", [&] {
		const float x = e2.x, y = e2.y, z = e2.z;
		const int64_t before = e2.kinah();
		const OLoc& sanctum = npcs.at(URAKRON).loc(LOC_SANCTUM);
		selectRoute(e2, URAKRON, LOC_SANCTUM);
		collectFor(*e2.game, 500ms);
		EXPECT_EQ(e2.kinah(), before - sanctum.servicePrice);
		disconnect(e2);
		EXPECT_EQ(kinahInDatabase(e2), before - sanctum.servicePrice) << "T10b: the kinah is already paid";
		relogIn(servers, e2);
		EXPECT_EQ(e2.worldId, VERTERON) << "T10b: Java's SpawnTask never ran: the pre-teleport map";
		EXPECT_NEAR(e2.x, x, 0.01);
		EXPECT_NEAR(e2.y, y, 0.01);
		EXPECT_NEAR(e2.z, z, 0.01);
	});

	runCase("C13", "T11: Urakron takes E2 to Sanctum (the first automated capital arrival by teleport, W-16)", [&] {
		const OLoc& sanctum = npcs.at(URAKRON).loc(LOC_SANCTUM);
		const auto [locIndex, done, ready] = mapToMap(e2, URAKRON, LOC_SANCTUM, "T11");
		checkArrivalNpcs(ready, oracle->spawns(SANCTUM, sanctum.x, sanctum.y, sanctum.z, 0, 120.0), "T11 Sanctum");
	});

	// ======================================================================================================================================
	// C14: Return across maps without a bind point (T12)
	// ======================================================================================================================================

	runCase("C14", "T12: E2 casts Return in Sanctum without a bind point: the Elyos spawn in Poeta, with no animation", [&] {
		GameSession::CastRequest request;
		request.spellId = SKILL_RETURN;
		request.level = 1;
		request.targetType = 0;
		request.targetObjectId = e2.playerId;
		const GameSession::CastOutcome outcome = e2.game->castAndWait(e2.playerId, request, 20s);
		ASSERT_TRUE(outcome.castSpell) << "T12: no SM_CASTSPELL(243)";
		const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(waitFor(*e2.game, "SM_PLAYER_SPAWN", 15s).data);
		const std::vector<Packet> after = e2.since(outcome.firstPacket);
		EXPECT_TRUE(ofName(after, "SM_TELEPORT_LOC").empty()) << "T12: TeleportAnimation.NONE sends no SM_TELEPORT_LOC";
		EXPECT_NE(firstOfName(after, "SM_CHANNEL_INFO"), nullptr);
		EXPECT_EQ(spawned.worldId, elyosWarrior.mapId) << "T12: player_initial_data.xml:4";
		EXPECT_FLOAT_EQ(spawned.x, elyosWarrior.x);
		EXPECT_FLOAT_EQ(spawned.y, elyosWarrior.y);
		EXPECT_FLOAT_EQ(spawned.z, elyosWarrior.z);
		e2.worldId = spawned.worldId;
		e2.x = spawned.x;
		e2.y = spawned.y;
		e2.z = spawned.z;
		levelReady(e2);
	});

	// ======================================================================================================================================
	// C15-C21: Haramel (T13-T18)
	// ======================================================================================================================================

	int32_t instanceId = 0;
	Clock::time_point instanceCreated;
	int64_t portalCooldownCount = 0;
	runCase("C15", "E2 quits, is seeded beside Haramel's entrance in Verteron, and relogs", [&] {
		const json answer = travel({"--portal", std::to_string(HARAMEL_ENTRANCE), "--race", "ELYOS", "--now-ms",
		                            std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
		                              std::chrono::system_clock::now().time_since_epoch()).count()),
		                            "--portal", std::to_string(HARAMEL_EXIT), "--race", "ELYOS"});
		entrance = parseOPortal(answer.at("portals").at(0));
		exitPortal = parseOPortal(answer.at("portals").at(1));
		ASSERT_FALSE(entrance.spots.empty());
		ASSERT_FALSE(entrance.paths.empty());
		EXPECT_EQ(entrance.paths[0].map, HARAMEL);
		EXPECT_TRUE(entrance.paths[0].instance);
		EXPECT_GT(entrance.cooltimeId, 0);
		disconnect(e2);
		const OSpot& spot = entrance.spots[0];
		seedPosition(e2, spot.map, spot.x + 1.5f, spot.y, spot.z);
		relogIn(servers, e2);
		EXPECT_EQ(e2.worldId, VERTERON);
	});

	runCase("C16", "T13: E2 enters Haramel: the use bar, the beam, a new instance with its npcs, the entrance cooldown", [&] {
		const int32_t portal = requireNpc(e2, HARAMEL_ENTRANCE, entrance.spots[0]);
		const size_t from = useBar(e2, portal, entrance.talkDelayMs, "T13");
		const Packet locPacket = waitFor(*e2.game, "SM_TELEPORT_LOC", 10s);
		instanceCreated = locPacket.receivedAt;
		const decoders::TeleportLoc loc = decoders::decodeTeleportLoc(locPacket.data);
		const OPortalPath& path = entrance.paths[0];
		EXPECT_EQ(loc.animation, decoders::TELEPORT_ANIMATION_FADE_OUT_BEAM);
		EXPECT_EQ(loc.mapId, HARAMEL);
		EXPECT_GE(loc.mapOrInstanceId, 2) << "T13: a new instance, not the inaccessible default 1";
		EXPECT_FLOAT_EQ(loc.x, path.x);
		EXPECT_FLOAT_EQ(loc.y, path.y);
		EXPECT_FLOAT_EQ(loc.z, path.z);
		EXPECT_EQ(loc.heading, path.heading);
		instanceId = loc.mapOrInstanceId;
		const Packet infoPacket = waitFor(*e2.game, "SM_INSTANCE_INFO", 5s);
		const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		const decoders::InstanceInfo info = decoders::decodeInstanceInfo(infoPacket.data);
		EXPECT_EQ(info.updateType, 2) << "PortalCooldownList.addPortalCooldown's SM_INSTANCE_INFO(2, ...)";
		EXPECT_EQ(info.cooltimeId, entrance.cooltimeId);
		const std::optional<decoders::InstanceCooldownEntry> haramel = info.entry(entrance.cooltimeId);
		ASSERT_TRUE(haramel) << "T13: Haramel's cooltime row in SM_INSTANCE_INFO";
		EXPECT_EQ(haramel->maxCount, entrance.maxCount);
		EXPECT_EQ(haramel->entryOffset, -1);
		if (entrance.reuseTimeMs) {
			const auto remaining = static_cast<int32_t>((*entrance.reuseTimeMs - nowMs) / 1000);
			EXPECT_NEAR(haramel->reuseSeconds, remaining, 2) << "T13: the seconds until the oracle's reuse time";
		}
		const auto rows = database.queryRows(schema, "SELECT world_id, entry_count FROM portal_cooldowns WHERE player_id = " +
		                                               std::to_string(e2.playerId), 2);
		ASSERT_EQ(rows.size(), 1u) << "T13 / D12: the first write of portal_cooldowns";
		EXPECT_EQ(rows[0][0].value_or(""), std::to_string(HARAMEL));
		portalCooldownCount = std::stoll(rows[0][1].value_or("0"));
		EXPECT_EQ(portalCooldownCount, 1);
		EXPECT_FALSE(servers.gameServer()->findLogLines("Created new instance: " + std::to_string(HARAMEL) + " [" + std::to_string(instanceId) + "]", 5).empty())
		  << "T13: InstanceService's log line";
		const auto [spawned, done] = animationDone(e2);
		EXPECT_EQ(spawned.worldId, HARAMEL);
		EXPECT_FLOAT_EQ(spawned.x, path.x);
		EXPECT_FLOAT_EQ(spawned.y, path.y);
		EXPECT_FLOAT_EQ(spawned.z, path.z);
		// the message follows SM_PLAYER_SPAWN (TeleportService.java:530-531), so it is read once the level ready's burst is in
		const std::vector<Packet> ready = levelReady(e2);
		const std::vector<decoders::SystemMessage> opened = messagesOf(slice(*e2.game, done), STR_MSG_INSTANCE_DUNGEON_OPENED_FOR_SELF);
		ASSERT_EQ(opened.size(), 1u) << "T13: STR_MSG_INSTANCE_DUNGEON_OPENED_FOR_SELF (TeleportService.java:530-531)";
		ASSERT_EQ(opened[0].params.size(), 1u);
		EXPECT_EQ(opened[0].params[0], std::to_string(HARAMEL)) << "T13: the world id parameter";
		const Packet* count = firstOfName(ready, "SM_INSTANCE_COUNT_INFO");
		ASSERT_NE(count, nullptr) << join(namesOf(ready));
		const decoders::InstanceCountInfo countInfo = decoders::decodeInstanceCountInfo(count->data);
		EXPECT_EQ(countInfo.mapId, HARAMEL);
		EXPECT_EQ(countInfo.instanceId, instanceId);
		const json spawns = travel({"--instance-spawns", std::to_string(HARAMEL), "--difficulty", "0", "--near",
		                            std::to_string(path.x) + "," + std::to_string(path.y) + "," + std::to_string(path.z), "--radius", "120",
		                            "--race", "ELYOS"});
		checkInstanceNpcs(ready, parseOInstanceSpots(spawns.at("instanceSpawns").at(0)), "T13 Haramel");
		(void)from;
	});

	/**
	 * C16-C20 must not straddle a checker run while E2 is outside: the checker runs 60 s after the creation and every 60 s after (InstanceService
	 * .java:58) and with D11's 1-s delay destroys the instance if E2 is outside. So before the first exit the gate makes sure `needed` is left in
	 * the current 60-s window, waiting for the next run (with E2 inside, which keeps the instance) otherwise.
	 */
	const auto awaitCheckerWindow = [&](std::chrono::seconds needed) {
		const auto sinceCreation = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - instanceCreated);
		const auto intoWindow = sinceCreation % 60000ms;
		const auto left = 60000ms - intoWindow;
		if (left < needed) {
			std::cout << "the checker runs in " << left.count() << " ms: waiting for it, with E2 inside" << std::endl;
			collectFor(*e2.game, left + 3s);
		}
	};

	runCase("C17", "T14: CM_INSTANCE_LEAVE is a no-op for a GeneralInstanceHandler (D7)", [&] {
		const size_t from = e2.mark();
		e2.game->send(GameSession::CM_INSTANCE_LEAVE, GameSession::buildCM_INSTANCE_LEAVE());
		collectFor(*e2.game, 2s);
		const std::vector<Packet> after = e2.since(from);
		EXPECT_TRUE(ofName(after, "SM_TELEPORT_LOC").empty()) << join(namesOf(after));
		EXPECT_TRUE(ofName(after, "SM_PLAYER_SPAWN").empty());
		EXPECT_TRUE(ofName(after, "SM_CHANNEL_INFO").empty());
		EXPECT_TRUE(messagesOf(after, STR_MSG_LEAVE_INSTANCE).empty());
	});

	runCase("C18", "T15: the exit portal: the bar, the beam to Verteron, STR_MSG_LEAVE_INSTANCE(0)", [&] {
		ASSERT_FALSE(exitPortal.spots.empty());
		ASSERT_FALSE(exitPortal.paths.empty());
		awaitCheckerWindow(50s);
		const OSpot& spot = exitPortal.spots[0];
		walkTo(e2, spot.x - 1.5f, spot.y, spot.z);
		const int32_t portal = requireNpc(e2, HARAMEL_EXIT, spot);
		useBar(e2, portal, exitPortal.talkDelayMs, "T15");
		const decoders::TeleportLoc loc = decoders::decodeTeleportLoc(waitFor(*e2.game, "SM_TELEPORT_LOC", 10s).data);
		const OPortalPath& path = exitPortal.paths[0];
		EXPECT_EQ(loc.animation, decoders::TELEPORT_ANIMATION_FADE_OUT_BEAM);
		EXPECT_EQ(loc.mapId, VERTERON);
		EXPECT_EQ(loc.mapOrInstanceId, VERTERON);
		EXPECT_FLOAT_EQ(loc.x, path.x);
		EXPECT_FLOAT_EQ(loc.y, path.y);
		EXPECT_FLOAT_EQ(loc.z, path.z);
		EXPECT_EQ(loc.heading, path.heading);
		const auto [spawned, done] = animationDone(e2);
		EXPECT_EQ(spawned.worldId, VERTERON);
		const std::vector<decoders::SystemMessage> left = messagesOf(e2.since(done), STR_MSG_LEAVE_INSTANCE);
		ASSERT_EQ(left.size(), 1u) << "T15: onLeaveInstance's solo message";
		ASSERT_EQ(left[0].params.size(), 1u);
		EXPECT_EQ(left[0].params[0], "0") << "T15: getDestroyDelaySeconds / 60 = 1 / 60 (D11)";
		levelReady(e2);
	});

	runCase("C19", "T16: E2 re-enters the same instance: no new instance, no new cooldown", [&] {
		const OSpot& spot = entrance.spots[0];
		walkTo(e2, spot.x - 1.5f, spot.y, spot.z);
		const int32_t portal = requireNpc(e2, HARAMEL_ENTRANCE, spot);
		const size_t from = useBar(e2, portal, entrance.talkDelayMs, "T16");
		const decoders::TeleportLoc loc = decoders::decodeTeleportLoc(waitFor(*e2.game, "SM_TELEPORT_LOC", 10s).data);
		EXPECT_EQ(loc.mapId, HARAMEL);
		EXPECT_EQ(loc.mapOrInstanceId, instanceId) << "T16: the registered instance (PortalService.java:101-110)";
		collectFor(*e2.game, 1s);
		EXPECT_TRUE(ofName(e2.since(from), "SM_INSTANCE_INFO").empty()) << "T16: reenter adds no cooldown (PortalService.java:363-366)";
		EXPECT_EQ(database.queryLong(schema, "SELECT entry_count FROM portal_cooldowns WHERE player_id = " + std::to_string(e2.playerId) +
		                                       " AND world_id = " + std::to_string(HARAMEL)).value_or(-1),
		          portalCooldownCount);
		const auto [spawned, done] = animationDone(e2);
		EXPECT_EQ(spawned.worldId, HARAMEL);
		levelReady(e2);
	});

	runCase("C20", "T17: E2 relogs inside the live instance and lands in the same instance", [&] {
		disconnect(e2);
		const std::vector<Packet> burst = relogIn(servers, e2);
		EXPECT_EQ(e2.worldId, HARAMEL);
		const Packet* count = firstOfName(e2.lastLevelReady, "SM_INSTANCE_COUNT_INFO");
		ASSERT_NE(count, nullptr) << join(namesOf(e2.lastLevelReady));
		EXPECT_EQ(decoders::decodeInstanceCountInfo(count->data).instanceId, instanceId) << "T17: onPlayerLogin's registered arm";
		(void)burst;
	});

	runCase("C21", "T18: E2 quits inside; the checker destroys the empty instance; the relog lands at the Haramel exit", [&] {
		disconnect(e2);
		const auto quit = Clock::now();
		std::vector<std::string> destroyed;
		const std::string line = "Destroying " ;
		while (Clock::now() - quit < 80s) {
			destroyed = servers.gameServer()->findLogLines("Destroying", 5);
			if (!destroyed.empty())
				break;
			std::this_thread::sleep_for(500ms);
		}
		ASSERT_FALSE(destroyed.empty()) << "T18: no 'Destroying' line within 80 s of the quit";
		const auto destroyedAt = Clock::now();
		const double sinceCreation = std::chrono::duration<double>(destroyedAt - instanceCreated).count();
		const double phase = std::fmod(sinceCreation, 60.0);
		EXPECT_TRUE(phase < 5.0 || phase > 59.0) << "T18: the destruction is a checker run, 60 s * n after the creation (measured "
		                                         << sinceCreation << " s)";
		EXPECT_NE(destroyed[0].find(std::to_string(HARAMEL)), std::string::npos) << destroyed[0];
		const std::vector<Packet> burst = relogIn(servers, e2);
		const std::vector<Packet> spawns = ofName(burst, "SM_PLAYER_SPAWN");
		ASSERT_EQ(spawns.size(), 2u) << "T18 / D7: moveToExitPoint's spawn and the enter world's own: " << join(namesOf(burst));
		const decoders::PlayerSpawn first = decoders::decodePlayerSpawn(spawns[0].data);
		ASSERT_TRUE(entrance.exit) << "the oracle has no instance exit of Haramel";
		EXPECT_EQ(first.worldId, entrance.exit->map);
		EXPECT_FLOAT_EQ(first.x, entrance.exit->x);
		EXPECT_FLOAT_EQ(first.y, entrance.exit->y);
		EXPECT_FLOAT_EQ(first.z, entrance.exit->z);
		EXPECT_EQ(e2.worldId, VERTERON);
		EXPECT_FLOAT_EQ(e2.x, entrance.exit->x);
		const Packet* info = firstOfName(burst, "SM_INSTANCE_INFO");
		ASSERT_NE(info, nullptr) << join(namesOf(burst));
		const std::optional<decoders::InstanceCooldownEntry> haramel = decoders::decodeInstanceInfo(info->data).entry(entrance.cooltimeId);
		ASSERT_TRUE(haramel);
		EXPECT_EQ(haramel->entryOffset, -1) << "T18: the entry is still counted";
	});

	// ======================================================================================================================================
	// C22: two map changes in a row (T19; the geo run's G2)
	// ======================================================================================================================================

	runCase("C22", "T19: S1 takes Osmar to Altgard, then Ukin to Morheim", [&] {
		relogIn(servers, s1);
		EXPECT_EQ(s1.worldId, ISHALGEN);
		EXPECT_EQ(s1.kinah(), DAEVA_KINAH);
		mapToMap(s1, OSMAR, LOC_ALTGARD, "T19 Altgard");
		EXPECT_EQ(s1.worldId, ALTGARD);
		const OLoc& morheim = npcs.at(UKIN).loc(LOC_MORHEIM);
		const auto [locIndex, done, ready] = mapToMap(s1, UKIN, LOC_MORHEIM, "T19 Morheim");
		EXPECT_EQ(s1.worldId, MORHEIM);
		EXPECT_EQ(s1.kinah(), DAEVA_KINAH - npcs.at(OSMAR).loc(LOC_ALTGARD).servicePrice - morheim.servicePrice);
		checkArrivalNpcs(ready, oracle->spawns(MORHEIM, morheim.x, morheim.y, morheim.z, 0, 120.0), "T19 Morheim");
	});

	// ======================================================================================================================================
	// C23: reports and shutdown (T20; the geo run's G2)
	// ======================================================================================================================================

	cases.run("C23a", "the stop file with E1 and S1 online", [&] {
		disconnect(e2);
	});
	const std::optional<int32_t> gameServerExit = servers.stopGameServer();
	const std::chrono::system_clock::time_point serverUpTo = std::chrono::system_clock::now();
	const std::optional<int32_t> loginServerExit = servers.stopLoginServer();
	cases.run("C23", "T20: reports - no AION_UNPORTED, the allow-list, no ERROR, the census, the instance's live counts", [&] {
		ASSERT_TRUE(gameServerExit) << "the game server did not exit after the stop file was written";
		EXPECT_EQ(*gameServerExit, 0) << "the game server exited with " << *gameServerExit;
		ASSERT_TRUE(loginServerExit) << "the login server did not exit on CTRL_BREAK";
		if (*loginServerExit != 98)
			EXPECT_EQ(*loginServerExit, 0) << "the login server exited with " << *loginServerExit;
		ASSERT_TRUE(std::filesystem::is_regular_file(servers.checkOutputDir() / "m5a_summary.txt"))
		  << "the game server wrote no check output in " << servers.checkOutputDir();
		const std::vector<std::string> unported = servers.readReportLines("unported_trace.txt");
		const std::string cronNote = crossesWednesdayNine(serverUpFrom, serverUpTo)
		                               ? "\n  NOTE: the server was up at a Wednesday 09:00 local time (the LegionDominion cron, m5c-plan.md G-07): rerun the gate"
		                               : "";
		EXPECT_TRUE(unported.empty()) << "T20: AION_UNPORTED sites were reached on the travel path:\n" << join(unported, "\n") << cronNote;
		const std::vector<AllowlistEntry> allowlist = readAllowlist();
		ASSERT_FALSE(allowlist.empty()) << "tests/scenario/m5f_partial_allowlist.txt is empty or missing";
		std::map<std::string, int64_t> hitsByEntry;
		for (const PartialHit& hit : readPartialHits(servers)) {
			bool allowed = false;
			for (const AllowlistEntry& entry : allowlist)
				if (allowlistEntryMatches(entry.site, hit.site)) {
					allowed = true;
					hitsByEntry[entry.site] += hit.hits;
				}
			EXPECT_TRUE(allowed) << "T20: the AION_PARTIAL site " << hit.site << " is not in tests/scenario/m5f_partial_allowlist.txt (" << hit.line << ")";
		}
		for (const AllowlistEntry& entry : allowlist) {
			if (entry.section == AllowlistSection::HitAtLeastOnce)
				EXPECT_GT(hitsByEntry[entry.site], 0) << "T20: the section A row " << entry.site << " was never hit";
			else if (entry.section == AllowlistSection::HitNever)
				EXPECT_EQ(hitsByEntry[entry.site], 0) << "T20: the section B row " << entry.site << " was hit " << hitsByEntry[entry.site] << " times";
			std::cout << "T20: allow-list row " << entry.site << ": " << hitsByEntry[entry.site] << " hits" << std::endl;
		}
		EXPECT_TRUE(servers.readReportLines("census.txt").empty()) << "T20: the final census reports leaks:\n"
		                                                           << join(servers.readReportLines("census.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("lockdep.txt").empty()) << join(servers.readReportLines("lockdep.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("watchdog.txt").empty()) << join(servers.readReportLines("watchdog.txt"), "\n");
		const std::map<std::string, std::vector<std::string>> summary = servers.readSummary();
		const auto value = [&](std::string_view key) -> std::string {
			const auto found = summary.find(std::string(key));
			return found == summary.end() || found->second.empty() ? std::string() : found->second[0];
		};
		EXPECT_EQ(value("started"), "true");
		EXPECT_EQ(value("exitCode"), "0");
		EXPECT_EQ(value("liveLeaks"), "0") << join(summary.contains("liveLeak") ? summary.at("liveLeak") : std::vector<std::string>{});
		EXPECT_EQ(value("liveCountsEnabled"), "true") << "T20: a release build counts nothing: build it checked";
		const auto notPorted = summary.find("notPortedClientPacket");
		if (notPorted != summary.end())
			ADD_FAILURE() << "T20: the travel path sent client packets that are not ported: " << join(notPorted->second);
		std::vector<std::string> errors;
		ASSERT_NE(servers.gameServer(), nullptr);
		for (const std::string& line : servers.gameServer()->findLogLines(" ERROR "))
			errors.push_back("game server: " + line);
		if (servers.loginServer() != nullptr)
			for (const std::string& line : servers.loginServer()->findLogLines(" ERROR "))
				errors.push_back("login server: " + line);
		EXPECT_TRUE(errors.empty()) << "T20: ERROR lines in the server logs:\n" << join(errors, "\n") << cronNote;
		EXPECT_TRUE(servers.gameServer()->findLogLines("did not leave world cleanly", 5).empty());
		EXPECT_TRUE(servers.gameServer()->findLogLines("objects removed from the world are still alive", 5).empty())
		  << join(servers.gameServer()->findLogLines("objects removed from the world are still alive", 5), "\n");

		// the instance and its handler were reclaimed (risk 1): WorldMapInstance and GeneralInstanceHandler back at the baseline, no checker
		// task and no SpawnTask left, each created at least once
		const std::map<std::string, LiveCount> counts = readLiveCounts(servers, "live_counts.txt");
		const std::map<std::string, LiveCount> baseline = readLiveCounts(servers, "live_counts_baseline.txt");
		const auto row = [](const std::map<std::string, LiveCount>& rows, std::string_view name) -> std::optional<LiveCount> {
			const auto found = rows.find(std::string(name));
			return found == rows.end() ? std::nullopt : std::optional<LiveCount>(found->second);
		};
		// a WorldMapInstance is counted by its concrete class (WorldMap2DInstance / WorldMap3DInstance): the two rows together
		const auto sum = [&](const std::map<std::string, LiveCount>& rows, const std::vector<std::string_view>& names) {
			LiveCount total;
			for (const std::string_view name : names)
				if (const std::optional<LiveCount> one = row(rows, name)) {
					total.live += one->live;
					total.created += one->created;
					total.line += one->line + "; ";
				}
			return total;
		};
		for (const std::vector<std::string_view>& names :
		     {std::vector<std::string_view>{"WorldMap2DInstance", "WorldMap3DInstance"}, std::vector<std::string_view>{"GeneralInstanceHandler"}}) {
			const LiveCount now = sum(counts, names);
			const LiveCount base = sum(baseline, names);
			ASSERT_FALSE(now.line.empty() || base.line.empty()) << "T20: live_counts.txt or its baseline has no row of " << names.front();
			EXPECT_EQ(now.live, base.live) << "T20: " << now.line << " against the baseline " << base.line;
			EXPECT_GT(now.created, base.created) << "T20: Haramel's instance and handler were created: " << now.line;
		}
		for (const std::string_view name : {"EmptyInstanceCheckerTask", "SpawnTask"}) {
			const std::optional<LiveCount> now = row(counts, name);
			ASSERT_TRUE(now) << "T20: live_counts.txt has no " << name << " row";
			EXPECT_EQ(now->live, 0) << "T20: " << now->line;
			EXPECT_GT(now->created, 0) << "T20: " << now->line;
		}
		const std::optional<LiveCount> players = row(counts, "Player");
		ASSERT_TRUE(players) << "T20: live_counts.txt has no Player row";
		EXPECT_EQ(players->live, 0) << "T20: " << players->line;
		if (variant.geodata) {
			// G2 (§10.5): S1 stands on Morheim, the only field map with a terrain-material image: the player-side material path ran
			const std::optional<LiveCount> material = row(counts, "TerrainZoneCollisionMaterialActor");
			const std::optional<LiveCount> materialBase = row(baseline, "TerrainZoneCollisionMaterialActor");
			ASSERT_TRUE(material) << "G2: live_counts.txt has no TerrainZoneCollisionMaterialActor row";
			EXPECT_GE(material->created - (materialBase ? materialBase->created : 0), 1) << "G2: " << material->line;
		}
	});

	finishRun(servers, outputDir, variant.testName);
}

} // namespace

// ---- the gates ---------------------------------------------------------------------------------------------------------------------------

/** `gs.scenario.m5f` (G-03): the cases of §10.2 with `gameserver.geodata.enable=false` */
TEST(M5fScenario, Run) {
	runM5fGate({false, "gs.scenario.m5f", "m5f", "m5f", "m5f"});
}

/**
 * `gs.scenario.m5f_geo` (G-04, §10.5): the same script with `gameserver.geodata.enable=true`, its own output directory, schema pair and CTest
 * entry, in the same gate slot. 4.8 corrects no arrival z except teleportToNpc's (TeleportService.java:327-329), so G1 is the script's own
 * rows: every arrival (T1, T5's client-given landing, T8, T10-T13, T15, T18, T19) still equals the data xyz to 1 cm with the geo meshes
 * loaded - a GeoService.getZ snap added to sendLoc or SpawnTask fails it, which the geo-off run cannot see (getZ answers NaN there). G2: S1
 * stands on Morheim, the only field map with a terrain-material image, and the run's TerrainZoneCollisionMaterialActor count grows. The
 * deterministic halves of G2 (G2a-G2c) are tests/geo rows.
 */
TEST(M5fScenarioGeo, Run) {
	runM5fGate({true, "gs.scenario.m5f_geo", "m5f_geo", "m5fgeo", "m5fg"});
}

} // namespace aion::gameserver::scenario
