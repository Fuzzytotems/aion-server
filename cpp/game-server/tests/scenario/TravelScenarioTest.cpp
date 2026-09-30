// The travel-core scenario gate (m5f-plan.md §16, the early travel slice of §5 T-01/T-05/P-01; gs.scenario.travel). One login server and one
// game server as child processes on their own test schemas, two accounts one after the other:
//   A, an Elyos seeded as a level-10 Daeva (a Gladiator, quest 1006 COMPLETE, m5c-plan.md C19's recipe) beside Polyidus (203726) in Sanctum,
//      talks to him, opens his map (AIRLINE_SERVICE), selects Verteron (loc 4), pays, jumps, and arrives in Verteron (CM_TELEPORT_ANIMATION_DONE,
//      SM_PLAYER_SPAWN, CM_LEVEL_READY); then flies with Mirdiena (203120), the flight master 1.9 m from the arrival, to Pilgrims Respite
//      (loc 15, flight 7001): SM_EMOTION(START_FLYTELEPORT), CM_MOVE_IN_AIR along the way and CM_EMOTION(LAND_FLYTELEPORT) at the end;
//   B, the Asmodian mirror: a level-10 Daeva (quest 2008 COMPLETE) beside Doman (204191) in Pandaemonium, to Altgard (loc 9);
// then the reports the server writes at shutdown (the M5a Q8 bar).
//
// Every expectation is independent of the C++ server code, as in the earlier gates: server packets are read with the decoders of
// tests/scenario/decoders (written from the Java writeImpl methods, m5a-plan.md D9) and with the two small decoders below (SM_TELEPORT_MAP and
// SM_TELEPORT_LOC, from their Java writeImpl); the numbers are the data rows (npc_teleporter.xml, teleport_location.xml, the spawn files, cited
// at their line) put through the Java arithmetic of the method an assertion is about (PricesService.getPriceForService with the shipped
// prices.properties and sieges off: global prices 125, modifier 100, taxes 113, each product truncated - m5f-plan.md §2.9). There is no
// oracle for travel yet (m5f-plan.md G-01 is stage 1's gate-harness item); when it lands, the constants below become its answers.
//
// This file does not share M5cScenarioTest.cpp's helpers, for the reason that file gives: each gate owns its pair of server processes and its
// helpers live in an anonymous namespace. What is duplicated is scaffolding (the case log, the burst collector, the login conversation, the
// report readers), never an assertion.

#include <gtest/gtest.h>

#include <algorithm>
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
#include "ChildProcess.h"
#include "FakeLoginClient.h"
#include "GameSession.h"
#include "InventoryModel.h"
#include "ScenarioDatabase.h"
#include "ScenarioServers.h"
#include "decoders/CombatDecoders.h"
#include "decoders/EconomyDecoders.h"
#include "decoders/PacketDecoders.h"

#include "../support/NetworkTestSupport.h" // the little endian PacketWriter

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {
namespace {

using namespace std::chrono_literals;
using decoders::DecodeError;
using Packet = GameSession::Packet;

constexpr std::chrono::milliseconds QUIET = 1000ms;
/** How long collectBurst waits for the FIRST packet of an answer. The quiet period alone (QUIET) ended a burst before the server had
 * answered at all when a loaded machine delayed a re-entry by a little over a second (gs.scenario.m5b K7b and m5c C9, 2026-09-29: "no
 * packet after CM_ENTER_WORLD"). Nothing expects an empty burst, so the longer first wait changes no result, only the time an answer
 * that never comes costs; after the first packet the quiet rule is unchanged. */
constexpr std::chrono::milliseconds FIRST_REPLY_WAIT = 5000ms;
constexpr std::chrono::milliseconds BURST_LIMIT = 90s;

constexpr int32_t RESPONSE_OK = 0;
constexpr int32_t RESPONSE_OPEN_CREATION_WINDOW = 22;
constexpr uint8_t ENTER_WORLD_OK = 0;
constexpr int32_t KINAH_ITEM = 182400001;

/** AionClientPacketFactory packets[148], [15], [49], [43] (ClientPacketInfo.gen.inc:131, :31, :60, :54) */
constexpr int32_t CM_TELEPORT_SELECT = 148;
constexpr int32_t CM_TELEPORT_ANIMATION_DONE = 15;
constexpr int32_t CM_MOVE_IN_AIR = 49;
constexpr int32_t CM_EMOTION = 43;
/** DialogAction.AIRLINE_SERVICE (DialogAction.java) */
constexpr uint16_t DIALOG_AIRLINE_SERVICE = 44;
/** EmotionType START_FLYTELEPORT (6) and LAND_FLYTELEPORT (7), EmotionType.java */
constexpr uint8_t EMOTION_START_FLYTELEPORT = 6;
constexpr uint8_t EMOTION_LAND_FLYTELEPORT = 7;
/** TeleportAnimation.JUMP_IN (TeleportAnimation.java: 3); npc.hasStatic() is false for the three teleporters (CM_TELEPORT_SELECT.java:68) */
constexpr uint8_t ANIMATION_JUMP_IN = 3;
/** CreatureState.ACTIVE (1) and FLYING (2), CreatureState.java */
constexpr uint16_t STATE_ACTIVE = 1;
constexpr uint16_t STATE_FLYING = 2;

/** The Daeva seed of m5c-plan.md C19 (D4/A-05): exp 126,069 is player_experience_table.xml's level 10, which only a Daeva reaches (F-1) */
constexpr std::string_view DAEVA_CLASS = "GLADIATOR";
constexpr int64_t DAEVA_EXP = 126069;
constexpr int64_t SEED_KINAH = 5000;

/** Java PricesService.getPriceForService with prices 125 (sieges off: influence 0), modifier 100 and taxes 113 - three truncations */
int64_t priceForService(int64_t basePrice) {
	int64_t price = static_cast<int64_t>(static_cast<double>(basePrice * 125) / 100.0);
	price = static_cast<int64_t>(static_cast<double>(price * 100) / 100.0);
	return static_cast<int64_t>(static_cast<double>(price * 113) / 100.0);
}

/** One route of the gate: the capital's teleporter, the spot the character is seeded at, and the destination */
struct Route {
	std::string label;
	bool asmodian = false;
	int32_t questId = 0;
	int32_t capital = 0;
	int32_t teleporterNpc = 0;
	/** the npc's spot in the spawn file */
	float npcX = 0, npcY = 0;
	/** the seed: 1.5 m from the npc */
	float seedX = 0, seedY = 0, seedZ = 0;
	int32_t teleportId = 0;
	int32_t locId = 0;
	int64_t basePrice = 0;
	int32_t destination = 0;
	float x = 0, y = 0, z = 0;
	uint8_t heading = 0;
};

/**
 * Sanctum: Polyidus 203726 (spawns/Npcs/110010000_Sanctum.xml:1065-1067), teleportId 1, loc 4 Verteron at 500 (npc_teleporter.xml:3-13),
 * teleport_location.xml:5 (no heading: 0)
 */
Route elyosRoute() {
	return Route{"A", false, 1006, 110010000, 203726, 1332.6f, 1512.0f, 1331.1f, 1512.0f, 569.039f, 1, 4, 500, 210030000, 1640.76f, 1500.32f,
		119.70999f, 0};
}

/** Pandaemonium: Doman 204191 (spawns/Npcs/120010000_Pandaemonium.xml:301-303), teleportId 50, loc 9 Altgard at 500, teleport_location.xml:10 */
Route asmodianRoute() {
	return Route{"B", true, 2008, 120010000, 204191, 1682.45f, 1397.31f, 1683.95f, 1397.31f, 195.362f, 50, 9, 500, 220030000, 1752.5322f, 1806.6096f,
		254.66133f, 60};
}

/** Verteron's flight master beside the arrival: Mirdiena 203120 (spawns/Npcs/210030000_Verteron.xml, 1639.08/1501.16/119.965), teleportId 105;
 *  loc 15 Pilgrims Respite, teleportid 7001, price 400 (npc_teleporter.xml, teleportId 105) */
constexpr int32_t FLIGHT_MASTER = 203120;
constexpr float FLIGHT_MASTER_X = 1639.08f, FLIGHT_MASTER_Y = 1501.16f;
constexpr int32_t FLIGHT_MASTER_TELEPORT_ID = 105;
constexpr int32_t FLIGHT_LOC = 15;
constexpr int32_t FLIGHT_ID = 7001;
constexpr int64_t FLIGHT_BASE_PRICE = 400;
/** the points the client reports with CM_MOVE_IN_AIR on the way, and their flight distances */
struct AirPoint {
	float x, y, z;
	int8_t heading;
	int32_t distance;
};
constexpr AirPoint FLIGHT_POINTS[] = {{1655.0f, 1520.0f, 135.0f, int8_t{15}, 1500}, {1680.0f, 1555.0f, 160.0f, int8_t{15}, 5800},
	{1710.0f, 1595.0f, 185.0f, int8_t{15}, 10900}};

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

std::vector<Packet> slice(const GameSession& session, size_t from) {
	const std::vector<Packet>& packets = session.recorded();
	if (from >= packets.size())
		return {};
	return std::vector<Packet>(packets.begin() + static_cast<std::ptrdiff_t>(from), packets.end());
}

std::optional<uint8_t> enterWorldCheck(const std::vector<Packet>& burst) {
	const Packet* check = firstOfName(burst, "SM_ENTER_WORLD_CHECK");
	if (check == nullptr || check->data.empty())
		return std::nullopt;
	return check->data[0];
}

// ---- the two travel packets, decoded from their Java writeImpl --------------------------------------------------------------------------

/** SM_TELEPORT_MAP.writeImpl: writeD(targetObjId), writeH(teleportId) */
struct TeleportMap {
	int32_t targetObjectId = 0;
	uint16_t teleportId = 0;
};

TeleportMap decodeTeleportMap(std::span<const uint8_t> body) {
	decoders::BodyReader reader(body, "SM_TELEPORT_MAP");
	TeleportMap map;
	map.targetObjectId = reader.D();
	map.teleportId = reader.H();
	reader.expectFullyConsumed();
	return map;
}

/** SM_TELEPORT_LOC.writeImpl: writeC(portAnimation), writeD(mapId), writeD(isInstance ? instanceId : mapId), writeF x/y/z, writeC(heading) */
struct TeleportLoc {
	uint8_t animation = 0;
	int32_t mapId = 0;
	int32_t mapOrInstanceId = 0;
	float x = 0, y = 0, z = 0;
	uint8_t heading = 0;
};

TeleportLoc decodeTeleportLoc(std::span<const uint8_t> body) {
	decoders::BodyReader reader(body, "SM_TELEPORT_LOC");
	TeleportLoc loc;
	loc.animation = reader.C();
	loc.mapId = reader.D();
	loc.mapOrInstanceId = reader.D();
	loc.x = reader.F();
	loc.y = reader.F();
	loc.z = reader.F();
	loc.heading = reader.C();
	reader.expectFullyConsumed();
	return loc;
}

// ---- the client packets of travel, each the Java readImpl field order ------------------------------------------------------------------

/** CM_TELEPORT_SELECT.readImpl (CM_TELEPORT_SELECT.java:38-43): readD targetObjId, readD locId, readH */
std::vector<uint8_t> buildTeleportSelect(int32_t targetObjId, int32_t locId) {
	return network::test::PacketWriter().D(targetObjId).D(locId).H(0).data;
}

/** CM_MOVE_IN_AIR.readImpl (CM_MOVE_IN_AIR.java:34-42): readD worldId, readF x/y/z, readC heading, readD distance */
std::vector<uint8_t> buildMoveInAir(int32_t worldId, const AirPoint& point) {
	return network::test::PacketWriter().D(worldId).F(point.x).F(point.y).F(point.z).C(point.heading).D(point.distance).data;
}

/** CM_EMOTION.readImpl (CM_EMOTION.java:52-60): readUC emotionType, and LAND_FLYTELEPORT reads nothing more */
std::vector<uint8_t> buildEmotion(uint8_t emotionType) {
	return network::test::PacketWriter().C(emotionType).data;
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

/** Reads and records everything that arrives within a FIXED window */
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

/** Reads until a packet with that name arrives and records everything on the way. @throws std::runtime_error on timeout or close */
Packet waitFor(GameSession& session, std::string_view name, std::chrono::milliseconds timeout = 15s) {
	const auto deadline = std::chrono::steady_clock::now() + timeout;
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
		if (packet->name == name)
			return *packet;
	}
	throw std::runtime_error("timeout waiting for " + std::string(name) + (session.client.socket.isClosed() ? " (the connection closed)" : ""));
}

/** Reads until a packet with that name arrives, skipping the async-allowed set. @throws std::runtime_error on timeout, close or another packet */
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

// ---- the clients ----------------------------------------------------------------------------------------------------------------------

struct ScenarioClient {
	std::string label;
	std::string account;
	std::string password = "travelPassword1";
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

	size_t mark() const { return game ? game->recorded().size() : 0; }
	std::vector<Packet> since(size_t from) const { return game ? slice(*game, from) : std::vector<Packet>{}; }
	int64_t kinah() {
		model.sync();
		return model.kinah();
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
	const Packet* spawn = firstOfName(burst, "SM_PLAYER_SPAWN");
	if (spawn == nullptr)
		throw std::runtime_error(client.label + ": no SM_PLAYER_SPAWN after CM_ENTER_WORLD: " + join(namesOf(burst)));
	const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(spawn->data);
	client.x = spawned.x;
	client.y = spawned.y;
	client.z = spawned.z;
	client.worldId = spawned.worldId;
	client.model.sync();
	return burst;
}

std::vector<Packet> levelReady(ScenarioClient& client) {
	client.game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
	std::vector<Packet> burst = collectBurst(*client.game, client.async);
	if (burst.empty())
		throw std::runtime_error(client.label + ": no packet after CM_LEVEL_READY");
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

// ---- the reports -----------------------------------------------------------------------------------------------------------------------

enum class AllowlistSection { HitAtLeastOnce, HitNever, NotPinned };

struct AllowlistEntry {
	std::string site;
	AllowlistSection section = AllowlistSection::NotPinned;
};

/** Reads tests/scenario/travel_partial_allowlist.txt with its three sections ("# --- SECTION A/B/C" marker lines, as the M5c list) */
std::vector<AllowlistEntry> readAllowlist() {
	std::vector<AllowlistEntry> entries;
	std::ifstream in(AION_SCENARIO_TRAVEL_PARTIAL_ALLOWLIST, std::ios::binary);
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

// ---- the travel steps ----------------------------------------------------------------------------------------------------------------

/** CM_DIALOG_SELECT(npc, AIRLINE_SERVICE) -> SM_TELEPORT_MAP (DialogService.java:187-197 -> TeleportService.showMap) */
TeleportMap openMap(ScenarioClient& client, int32_t npcObjectId) {
	client.game->send(GameSession::CM_DIALOG_SELECT, GameSession::buildCM_DIALOG_SELECT(npcObjectId, DIALOG_AIRLINE_SERVICE));
	return decodeTeleportMap(waitFor(*client.game, "SM_TELEPORT_MAP").data);
}

/**
 * One seeded capital route: the Daeva in the capital talks to the teleporter, opens its map, selects the destination, pays, jumps, and arrives
 * (CM_TELEPORT_ANIMATION_DONE -> SM_PLAYER_SPAWN -> CM_LEVEL_READY). Every row is an EXPECT, so a broken row still shows the later ones.
 */
void travel(ScenarioServers& servers, const ScenarioDatabase& database, const std::string& schema, ScenarioClient& client, const Route& route) {
	// ---- the character: created, then seeded as a level-10 Daeva beside the teleporter while its account is logged out (F-3) ----
	const decoders::CharacterList list = logIn(servers, client);
	EXPECT_EQ(list.characterCount, 0) << client.label << ": a fresh account must have no character";
	NewCharacter character;
	character.name = client.name;
	character.asmodian = route.asmodian;
	character.playerClassId = NewCharacter::WARRIOR;
	client.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(client.key.accountId, client.account, character, 1));
	EXPECT_EQ(decoders::decodeCreateCharacter(expectNext(*client.game, "SM_CREATE_CHARACTER", client.async).data).responseCode,
	          RESPONSE_OPEN_CREATION_WINDOW);
	client.game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(client.key.accountId, client.account, character, 0));
	const decoders::CreateCharacter created = decoders::decodeCreateCharacter(expectNext(*client.game, "SM_CREATE_CHARACTER", client.async).data);
	if (created.responseCode != RESPONSE_OK || !created.player)
		throw std::runtime_error(client.label + ": creating the character answered response code " + std::to_string(created.responseCode));
	client.playerId = created.player->playerId;
	disconnect(client);
	const std::string id = std::to_string(client.playerId);
	database.execute(schema, "UPDATE players SET player_class = '" + std::string(DAEVA_CLASS) + "', exp = " + std::to_string(DAEVA_EXP) +
	                           ", world_id = " + std::to_string(route.capital) + ", x = " + std::to_string(route.seedX) + ", y = " +
	                           std::to_string(route.seedY) + ", z = " + std::to_string(route.seedZ) + ", heading = 0 WHERE id = " + id);
	database.execute(schema, "INSERT INTO player_quests (player_id, quest_id, status, complete_count) VALUES (" + id + ", " +
	                           std::to_string(route.questId) + ", 'COMPLETE', 1)");
	database.execute(schema, "UPDATE inventory SET item_count = " + std::to_string(SEED_KINAH) + " WHERE item_owner = " + id + " AND item_id = " +
	                           std::to_string(KINAH_ITEM));

	// ---- the enter world in the capital ----
	relogIn(servers, client);
	EXPECT_EQ(client.worldId, route.capital) << client.label << " enters the capital";
	EXPECT_EQ(client.kinah(), SEED_KINAH);
	const std::optional<int32_t> teleporter = npcObject(client, route.teleporterNpc, route.npcX, route.npcY);
	if (!teleporter)
		throw std::runtime_error(client.label + ": no SM_NPC_INFO of the teleporter " + std::to_string(route.teleporterNpc) + " near its spot");

	// ---- talk: CM_SHOW_DIALOG -> SM_DIALOG_WINDOW; AIRLINE_SERVICE -> SM_TELEPORT_MAP(npc, teleportId) ----
	client.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(*teleporter));
	EXPECT_EQ(decoders::decodeDialogWindow(waitFor(*client.game, "SM_DIALOG_WINDOW").data).targetObjectId, *teleporter);
	const TeleportMap map = openMap(client, *teleporter);
	EXPECT_EQ(map.targetObjectId, *teleporter);
	EXPECT_EQ(map.teleportId, route.teleportId) << "npc_teleporter.xml's teleportId of " << route.teleporterNpc;

	// ---- CM_TELEPORT_SELECT(npc, loc): the price, SM_TELEPORT_LOC(JUMP_IN, destination), and nothing moves yet ----
	const size_t selected = client.mark();
	client.game->send(CM_TELEPORT_SELECT, buildTeleportSelect(*teleporter, route.locId));
	const TeleportLoc loc = decodeTeleportLoc(waitFor(*client.game, "SM_TELEPORT_LOC").data);
	EXPECT_EQ(loc.animation, ANIMATION_JUMP_IN);
	EXPECT_EQ(loc.mapId, route.destination);
	EXPECT_EQ(loc.mapOrInstanceId, route.destination) << "no instance map: writeD(mapId) twice";
	EXPECT_FLOAT_EQ(loc.x, route.x);
	EXPECT_FLOAT_EQ(loc.y, route.y);
	EXPECT_FLOAT_EQ(loc.z, route.z);
	EXPECT_EQ(loc.heading, route.heading);
	collectFor(*client.game, 500ms);
	const int64_t price = priceForService(route.basePrice);
	EXPECT_EQ(price, 706) << "m5f-plan.md §2.9: 500 -> 706";
	EXPECT_EQ(client.kinah(), SEED_KINAH - price) << "checkKinahForTransportation: getPriceForService(500) taken with DEC_KINAH_FLY";
	EXPECT_TRUE(ofName(client.since(selected), "SM_PLAYER_SPAWN").empty()) << "the arrival waits for CM_TELEPORT_ANIMATION_DONE";

	// ---- CM_TELEPORT_ANIMATION_DONE -> SM_CHANNEL_INFO, SM_PLAYER_SPAWN(destination); CM_LEVEL_READY spawns him there ----
	const size_t done = client.mark();
	client.game->send(CM_TELEPORT_ANIMATION_DONE, {});
	const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(waitFor(*client.game, "SM_PLAYER_SPAWN").data);
	const std::vector<std::string> arrival = namesOf(client.since(done));
	EXPECT_NE(std::find(arrival.begin(), arrival.end(), "SM_CHANNEL_INFO"), arrival.end()) << join(arrival);
	EXPECT_EQ(spawned.worldId, route.destination);
	EXPECT_FLOAT_EQ(spawned.x, route.x);
	EXPECT_FLOAT_EQ(spawned.y, route.y);
	EXPECT_FLOAT_EQ(spawned.z, route.z);
	EXPECT_EQ(spawned.heading, route.heading);
	client.worldId = spawned.worldId;
	client.x = spawned.x;
	client.y = spawned.y;
	client.z = spawned.z;
	const size_t ready = client.mark();
	levelReady(client);
	// spawned on the new map: its npcs are announced (the destination's own teleporter stands 2 m from the arrival, §2.9)
	int32_t npcsAnnounced = 0;
	for (const Packet& packet : ofName(client.since(ready), "SM_NPC_INFO"))
		try {
			const decoders::NpcInfo npc = decoders::decodeNpcInfo(packet.data);
			if (distance2d(npc.x, npc.y, route.x, route.y) < 95.0)
				++npcsAnnounced;
		} catch (const DecodeError&) {
		}
	EXPECT_GT(npcsAnnounced, 0) << client.label << ": CM_LEVEL_READY spawned him among no npc of the arrival: " << join(namesOf(client.since(ready)));
}

// ---- the gate ---------------------------------------------------------------------------------------------------------------------------

void runTravelGate() {
	const std::string testName = "gs.scenario.travel";
	const char* requireEnvironment = std::getenv("AION_SCENARIO_REQUIRE");
	const bool required = requireEnvironment != nullptr && *requireEnvironment != '\0' && std::string_view(requireEnvironment) != "0";
	std::optional<ScenarioEnvironment> environment = ScenarioEnvironment::fromEnvironment();
	if (!environment) {
		if (required)
			ADD_FAILURE() << testName << " was not configured and AION_SCENARIO_REQUIRE is set: set AION_TEST_GS_DATABASE_URL and AION_TEST_LS_DATABASE_URL";
		else
			GTEST_SKIP() << testName << ": skipped (set AION_TEST_GS_DATABASE_URL and AION_TEST_LS_DATABASE_URL)";
		return;
	}
	const std::filesystem::path outputDir = std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "travel";

	CaseLog cases;
	struct ReportPrinter {
		const CaseLog& cases;
		const std::string& testName;
		~ReportPrinter() { std::cout << cases.report(testName) << std::flush; }
	} printer{cases, testName};

	// the M5a profile (ScenarioServers::m5aProfile, with G-07's far-future wall-clock schedules) and the M5c keys that quiet the run
	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.schemaPrefix = "travel";
	config.gameServerProperties["gameserver.npcshouts.enable"] = "false";
	config.gameServerProperties["gameserver.rates.drop"] = "0";
	config.startupTimeout = 10min;
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

	std::chrono::system_clock::time_point serverUpFrom = std::chrono::system_clock::now();
	ok = cases.run("S-0", "the servers start", [&] {
		servers.createSchemas();
		servers.startLoginServer();
		serverUpFrom = std::chrono::system_clock::now();
		servers.startGameServer();
	});

	ScenarioClient a, b;
	a.label = "A";
	b.label = "B";
	const std::string suffix = servers.gameSchema().substr(servers.gameSchema().size() - 8);
	a.account = "trva" + suffix;
	b.account = "trvb" + suffix;
	a.name = "Travellera";
	b.name = "Travellerb";
	a.asmodian = false;
	b.asmodian = true;

	// ---- T1: Sanctum -> Verteron ----
	runCase("T1", "an Elyos Daeva in Sanctum takes Polyidus' route to Verteron: map, price, jump, arrival", [&] {
		travel(servers, database, schema, a, elyosRoute());
	});

	// ---- T2: a flight inside Verteron ----
	runCase("T2", "the flight master beside the arrival flies him: SM_EMOTION(START_FLYTELEPORT), CM_MOVE_IN_AIR, the landing", [&] {
		const std::optional<int32_t> master = npcObject(a, FLIGHT_MASTER, FLIGHT_MASTER_X, FLIGHT_MASTER_Y);
		ASSERT_TRUE(master) << "no SM_NPC_INFO of the flight master " << FLIGHT_MASTER << " at the arrival";
		const TeleportMap map = openMap(a, *master);
		EXPECT_EQ(map.targetObjectId, *master);
		EXPECT_EQ(map.teleportId, FLIGHT_MASTER_TELEPORT_ID);

		const int64_t before = a.kinah();
		const size_t selected = a.mark();
		a.game->send(CM_TELEPORT_SELECT, buildTeleportSelect(*master, FLIGHT_LOC));
		const std::optional<size_t> takeOff = [&]() -> std::optional<size_t> {
			const auto deadline = std::chrono::steady_clock::now() + 15s;
			while (std::chrono::steady_clock::now() < deadline) {
				std::optional<Packet> packet = a.game->next(1s);
				if (!packet)
					continue;
				if (packet->name != "SM_EMOTION")
					continue;
				try {
					if (decoders::decodeEmotion(packet->data).emotionType == EMOTION_START_FLYTELEPORT)
						return a.game->recorded().size() - 1;
				} catch (const DecodeError&) {
					// another creature's emotion the decoder does not know: not the take-off
				}
			}
			return std::nullopt;
		}();
		ASSERT_TRUE(takeOff) << "no SM_EMOTION(START_FLYTELEPORT) after CM_TELEPORT_SELECT: " << join(namesOf(a.since(selected)));
		const decoders::Emotion start = decoders::decodeEmotion(a.game->recorded()[*takeOff].data);
		EXPECT_EQ(start.senderObjectId, a.playerId);
		EXPECT_EQ(start.emotion, FLIGHT_ID) << "the location's teleportid (TeleportService.java:121-122)";
		EXPECT_TRUE(start.state & STATE_FLYING) << "setState(FLYING) before the broadcast: state " << start.state;
		EXPECT_FALSE(start.state & STATE_ACTIVE) << "unsetState(ACTIVE): state " << start.state;
		collectFor(*a.game, 500ms);
		EXPECT_EQ(a.kinah(), before - priceForService(FLIGHT_BASE_PRICE)) << "400 -> 565";
		EXPECT_EQ(priceForService(FLIGHT_BASE_PRICE), 565);
		EXPECT_TRUE(ofName(a.since(selected), "SM_TELEPORT_LOC").empty()) << "a flight is no teleport";

		// the client flies the path and reports it; the server answers nothing (CM_MOVE_IN_AIR.java:44-59) but keeps the position
		const size_t flying = a.mark();
		for (const AirPoint& point : FLIGHT_POINTS) {
			a.game->send(CM_MOVE_IN_AIR, buildMoveInAir(a.worldId, point));
			collectFor(*a.game, 300ms);
		}
		// (the npcs around him keep walking: their SM_MOVE arrive, his own must not)
		for (const Packet& packet : ofName(a.since(flying), "SM_MOVE"))
			EXPECT_NE(decoders::decodeMoveObjectId(packet.data), a.playerId) << "CM_MOVE_IN_AIR broadcasts nothing, to himself neither";

		// the landing: CM_EMOTION(LAND_FLYTELEPORT) -> onFlyTeleportEnd (FLYING off, ACTIVE on), echoed as SM_EMOTION(LAND_FLYTELEPORT)
		const size_t landing = a.mark();
		a.game->send(CM_EMOTION, buildEmotion(EMOTION_LAND_FLYTELEPORT));
		std::optional<decoders::Emotion> land;
		const auto deadline = std::chrono::steady_clock::now() + 10s;
		while (!land && std::chrono::steady_clock::now() < deadline) {
			std::optional<Packet> packet = a.game->next(1s);
			if (packet && packet->name == "SM_EMOTION") {
				try {
					const decoders::Emotion emotion = decoders::decodeEmotion(packet->data);
					if (emotion.emotionType == EMOTION_LAND_FLYTELEPORT)
						land = emotion;
				} catch (const DecodeError&) {
					// another creature's emotion the decoder does not know: not the landing
				}
			}
		}
		ASSERT_TRUE(land) << "no SM_EMOTION(LAND_FLYTELEPORT): " << join(namesOf(a.since(landing)));
		EXPECT_EQ(land->senderObjectId, a.playerId);
		EXPECT_FALSE(land->state & STATE_FLYING) << "onFlyTeleportEnd unset FLYING: state " << land->state;
		EXPECT_TRUE(land->state & STATE_ACTIVE) << "onFlyTeleportEnd set ACTIVE: state " << land->state;
		collectFor(*a.game, 500ms);

		// the server kept the last CM_MOVE_IN_AIR position: the quit stores it (F-3: the position is saved at logout)
		disconnect(a);
		const auto row = database.queryRows(schema, "SELECT world_id, x, y, z FROM players WHERE id = " + std::to_string(a.playerId), 4);
		ASSERT_EQ(row.size(), 1u);
		EXPECT_EQ(row[0][0].value_or(""), "210030000");
		const AirPoint& last = FLIGHT_POINTS[std::size(FLIGHT_POINTS) - 1];
		EXPECT_NEAR(std::stod(row[0][1].value_or("0")), last.x, 0.01) << "players.x is CM_MOVE_IN_AIR's last x";
		EXPECT_NEAR(std::stod(row[0][2].value_or("0")), last.y, 0.01);
		EXPECT_NEAR(std::stod(row[0][3].value_or("0")), last.z, 0.01);
		EXPECT_EQ(database.queryLong(schema, "SELECT item_count FROM inventory WHERE item_owner = " + std::to_string(a.playerId) + " AND item_id = " +
		                                       std::to_string(KINAH_ITEM)).value_or(-1),
		          SEED_KINAH - priceForService(500) - priceForService(FLIGHT_BASE_PRICE))
		  << "the two payments are stored";
	});

	// ---- T3: Pandaemonium -> Altgard ----
	runCase("T3", "an Asmodian Daeva in Pandaemonium takes Doman's route to Altgard: map, price, jump, arrival", [&] {
		travel(servers, database, schema, b, asmodianRoute());
		disconnect(b);
		EXPECT_EQ(database.queryLong(schema, "SELECT world_id FROM players WHERE id = " + std::to_string(b.playerId)).value_or(-1), 220030000);
	});

	// ---- T4: reports and shutdown (the M5a Q8 bar) ----
	cases.run("T4a", "both are logged out", [&] {
		disconnect(a);
		disconnect(b);
	});
	const std::optional<int32_t> gameServerExit = servers.stopGameServer();
	const std::chrono::system_clock::time_point serverUpTo = std::chrono::system_clock::now();
	const std::optional<int32_t> loginServerExit = servers.stopLoginServer();
	cases.run("T4", "reports: no AION_UNPORTED, the allow-list, no ERROR, the census", [&] {
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
		EXPECT_TRUE(unported.empty()) << "AION_UNPORTED sites were reached on the travel path:\n" << join(unported, "\n") << cronNote;
		const std::vector<AllowlistEntry> allowlist = readAllowlist();
		ASSERT_FALSE(allowlist.empty()) << "tests/scenario/travel_partial_allowlist.txt is empty or missing";
		std::map<std::string, int64_t> hitsByEntry;
		for (const PartialHit& hit : readPartialHits(servers)) {
			bool allowed = false;
			for (const AllowlistEntry& entry : allowlist)
				if (allowlistEntryMatches(entry.site, hit.site)) {
					allowed = true;
					hitsByEntry[entry.site] += hit.hits;
				}
			EXPECT_TRUE(allowed) << "the AION_PARTIAL site " << hit.site << " is not in tests/scenario/travel_partial_allowlist.txt (" << hit.line << ")";
		}
		for (const AllowlistEntry& entry : allowlist) {
			if (entry.section == AllowlistSection::HitAtLeastOnce)
				EXPECT_GT(hitsByEntry[entry.site], 0) << "the section A row " << entry.site << " was never hit";
			else if (entry.section == AllowlistSection::HitNever)
				EXPECT_EQ(hitsByEntry[entry.site], 0) << "the section B row " << entry.site << " was hit " << hitsByEntry[entry.site] << " times";
		}
		EXPECT_TRUE(servers.readReportLines("census.txt").empty()) << "the final census reports leaks:\n"
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
		const auto notPorted = summary.find("notPortedClientPacket");
		if (notPorted != summary.end())
			ADD_FAILURE() << "the travel path sent client packets that are not ported: " << join(notPorted->second);
		std::vector<std::string> errors;
		ASSERT_NE(servers.gameServer(), nullptr);
		for (const std::string& line : servers.gameServer()->findLogLines(" ERROR "))
			errors.push_back("game server: " + line);
		EXPECT_TRUE(errors.empty()) << "ERROR lines in the game server log:\n" << join(errors, "\n") << cronNote;
		EXPECT_TRUE(servers.gameServer()->findLogLines("did not leave world cleanly", 5).empty());
		EXPECT_TRUE(servers.gameServer()->findLogLines("objects removed from the world are still alive", 5).empty());
	});

	finishRun(servers, outputDir, testName);
}

} // namespace

/** `gs.scenario.travel` (m5f-plan.md §16): the capital teleporters of both races to Verteron and Altgard, and one flight in Verteron */
TEST(TravelScenario, Run) {
	runTravelGate();
}

} // namespace aion::gameserver::scenario
