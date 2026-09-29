// gs.scenario.ascension (lane P6-Q asc-hand, chunk Q06; the test lives in P5-SC's harness directory under a lease, chunks.cmake): the retail
// ascension route end to end on the real servers and the real static data, for an Elyos Warrior and an Asmodian Warrior, with the four
// hand-ported handlers (handlers/quest/ascension: 1006, 2008, 1007, 2009) registered because gameserver.simple.secondclass.enable is off.
//
// Each character is created through the client packets, then seeded while its account is disconnected (m5c0-client-session.md F-3) as a
// level-9 Warrior with a full level-9 bar (players.exp = 126,069, the first exp of level 10, which a starting class holds at level 9) beside
// the npc where the mission starts. The route, packet by packet as the client plays it:
//   E1 enter world: 1006 / 2008 is started (PlayerEnterWorldService.onLevelChange -> defaultOnLevelChangedEvent)
//   E2 the dialog steps with their pages and vars, the beam teleports (SM_TELEPORT_LOC with the handler's destination,
//      CM_TELEPORT_ANIMATION_DONE), the item use in Cliona Lake's zone (1006) and the three cards (2008), the movies (SM_PLAY_MOVIE,
//      CM_PLAY_MOVIE_END)
//   E3 the solo instance (Karamatis B / Ataxiar B): SM_PLAYER_SPAWN into it, CM_LEVEL_READY, SM_ASCENSION_MORPH(1)
//   E4 the flight (SM_EMOTION START_FLYTELEPORT, CM_MOVE_IN_AIR, CM_EMOTION LAND_FLYTELEPORT) and, 43 s after the talk, the four raiders /
//      guardian assassins, fought with CM_ATTACK until each dies, then Orissan / Hellion, then the movie and Pernos / Munin inside
//   E5 the class selection page and the Gladiator, the reward page, SELECTED_QUEST_NOREWARD: the beam out of the instance, 1006 / 2008
//      COMPLETE and 1007 / 2009 started; the reward's 73,200 exp are lost (SM_STATUPDATE_EXP: still the full level-9 bar), Java's order kept
//      (docs/deviations/Q06.md)
//   E6 1007 / 2009: the beam to Sanctum / Pandaemonium, the two npcs with their movies, the reward npc of a Warrior, COMPLETE with its 13,125
//      exp, which lift the new Daeva to level 10 (SM_STATUPDATE_EXP: 13,125 above level 10's first exp)
//   E7 a relog: the character list shows a level-10 Gladiator, and the quest rows are COMPLETE in the database
// Where the route cannot go on, the step fails with a message that names the step and the packets it read (no GTEST_SKIP).
// The run needs the two test database URLs (AION_TEST_GS_DATABASE_URL, AION_TEST_LS_DATABASE_URL); AION_SCENARIO_REQUIRE=1 (the CTest
// default) turns their absence into a failure, as the other gates do.

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "AsyncAllowed.h"
#include "FakeLoginClient.h"
#include "GameSession.h"
#include "InventoryModel.h"
#include "ScenarioDatabase.h"
#include "ScenarioServers.h"
#include "decoders/CombatDecoders.h"
#include "decoders/PacketDecoders.h"
#include "decoders/QuestDecoders.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {
namespace {

using namespace std::chrono_literals;
using Packet = GameSession::Packet;
using decoders::DecodeError;
using network::test::PacketReader;
using network::test::PacketWriter;

// ---- client opcodes the harness has no constant for (AionClientPacketFactory.java: packets[15], [43], [49], [81]) ----
constexpr int32_t CM_TELEPORT_ANIMATION_DONE = 15;
constexpr int32_t CM_EMOTION = 43;
constexpr int32_t CM_MOVE_IN_AIR = 49;
constexpr int32_t CM_PLAY_MOVIE_END = 81;
/** EmotionType.LAND_FLYTELEPORT (EmotionType.java:15), START_FLYTELEPORT (:14) */
constexpr uint8_t EMOTION_LAND_FLYTELEPORT = 7;
constexpr uint8_t EMOTION_START_FLYTELEPORT = 6;

// ---- DialogAction ids (DialogAction.java) ----
constexpr uint16_t SELECTED_QUEST_REWARD1 = 8;
constexpr uint16_t SELECTED_QUEST_NOREWARD = 23;
constexpr uint16_t QUEST_SELECT = 31;
constexpr uint16_t SELECT2_1 = 1353;
constexpr uint16_t SELECT3_1 = 1694;
constexpr uint16_t SELECT5_1 = 2376;
constexpr uint16_t SETPRO1 = 10000;
constexpr uint16_t setpro(int n) { return static_cast<uint16_t>(SETPRO1 + n - 1); }

// ---- QuestStatus.value() ----
constexpr uint8_t START = 3, REWARD = 4, COMPLETE = 5;

constexpr int32_t RESPONSE_OK = 0;
constexpr int32_t RESPONSE_OPEN_CREATION_WINDOW = 22;
constexpr int32_t GLADIATOR_CLASS_ID = 1; // PlayerClass.GLADIATOR id
constexpr int64_t FULL_LEVEL_9_BAR = 126069; // player_experience_table.xml: the first exp of level 10
constexpr int64_t LEVEL_9_START = 82982; // player_experience_table.xml: the first exp of level 9
constexpr int64_t CEREMONY_EXP = 13125; // quest_data.xml: 1007's and 2009's <rewards exp="13125">

constexpr std::chrono::milliseconds QUIET = 1000ms;

struct Spot {
	float x, y, z;
};

double distance2d(double x1, double y1, double x2, double y2) {
	return std::sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
}

std::string join(const std::vector<std::string>& values, std::string_view separator = ", ") {
	std::string out;
	for (const std::string& value : values) {
		if (!out.empty())
			out.append(separator);
		out.append(value);
	}
	return out;
}

std::vector<std::string> namesOf(const std::vector<Packet>& packets) {
	std::vector<std::string> names;
	for (const Packet& packet : packets)
		names.push_back(packet.name);
	return names;
}

/** Reads and records everything that arrives within a fixed window */
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

/** Reads until `accept` answers true (recording everything), or `timeout`. @return the packets read, the accepted one last; nullopt on timeout */
std::optional<std::vector<Packet>> readUntil(GameSession& session, const std::function<bool(const Packet&)>& accept, std::chrono::milliseconds timeout) {
	std::vector<Packet> read;
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
		read.push_back(*packet);
		if (accept(*packet))
			return read;
	}
}

Packet waitFor(GameSession& session, std::string_view name, std::chrono::milliseconds timeout = 15s) {
	std::optional<std::vector<Packet>> read = readUntil(session, [name](const Packet& packet) { return packet.name == name; }, timeout);
	if (!read)
		throw std::runtime_error("timeout waiting for " + std::string(name) + (session.client.socket.isClosed() ? " (the connection closed)" : ""));
	return read->back();
}

/** SM_DIALOG_WINDOW (SM_DIALOG_WINDOW.java:29-40): D target, H page, D quest, ... */
struct DialogWindow {
	int32_t target = 0;
	int32_t page = 0;
	int32_t questId = 0;
};

DialogWindow decodeDialogWindow(const Packet& packet) {
	PacketReader reader(packet.data);
	DialogWindow window;
	window.target = reader.D();
	window.page = static_cast<uint16_t>(reader.H());
	window.questId = reader.remaining() >= 4 ? reader.D() : 0;
	return window;
}

/** SM_PLAY_MOVIE (SM_PLAY_MOVIE.java:27-35): C type, D target, D quest, D movie, C 0, C canSkip ? 0 : 1 */
struct Movie {
	uint8_t type = 0;
	int32_t target = 0, questId = 0, movieId = 0;
	uint8_t cannotSkip = 0;
};

Movie decodeMovie(const Packet& packet) {
	PacketReader reader(packet.data);
	Movie movie;
	movie.type = static_cast<uint8_t>(reader.C());
	movie.target = reader.D();
	movie.questId = reader.D();
	movie.movieId = reader.D();
	reader.C();
	movie.cannotSkip = static_cast<uint8_t>(reader.C());
	return movie;
}

std::vector<decoders::QuestAction> questActionsOf(const std::vector<Packet>& packets, int32_t questId) {
	std::vector<decoders::QuestAction> actions;
	for (const Packet& packet : packets) {
		if (packet.name != "SM_QUEST_ACTION")
			continue;
		try {
			decoders::QuestAction action = decoders::decodeQuestAction(packet.data);
			if (action.questId == questId)
				actions.push_back(action);
		} catch (const DecodeError&) {
		}
	}
	return actions;
}

std::string describeQuest(const std::vector<Packet>& packets, int32_t questId) {
	std::vector<std::string> out;
	for (const decoders::QuestAction& action : questActionsOf(packets, questId))
		out.push_back("type " + std::to_string(action.actionType) + " status " + std::to_string(action.status) + " vars " +
		              std::to_string(action.questVarsAndFlags));
	return out.empty() ? "no SM_QUEST_ACTION of " + std::to_string(questId) : join(out, "; ");
}

/** The last SM_QUEST_ACTION of the quest in `packets` has this status and var(0)... (the vars int, as the quest's own setQuestVar wrote it) */
bool questIs(const std::vector<Packet>& packets, int32_t questId, uint8_t status, std::optional<int32_t> vars = std::nullopt) {
	std::vector<decoders::QuestAction> actions = questActionsOf(packets, questId);
	if (actions.empty())
		return false;
	const decoders::QuestAction& last = actions.back();
	return last.status == status && (!vars || last.questVarsAndFlags == *vars);
}

struct Client {
	std::string label;
	std::string account;
	std::string password = "ascPassword1";
	std::string name;
	bool asmodian = false;
	std::unique_ptr<FakeLoginClient> login;
	std::unique_ptr<GameSession> game;
	FakeLoginClient::SessionKey key;
	int32_t playerId = 0;
	InventoryModel model;
	AsyncAllowed async = AsyncAllowed::m5aDefault();
	float x = 0, y = 0, z = 0;
	int32_t worldId = 0;

	size_t mark() const { return game->recorded().size(); }
	std::vector<Packet> since(size_t from) const {
		const std::vector<Packet>& all = game->recorded();
		return std::vector<Packet>(all.begin() + static_cast<std::ptrdiff_t>(std::min(from, all.size())), all.end());
	}
};

decoders::CharacterList logIn(ScenarioServers& servers, Client& client) {
	client.login = std::make_unique<FakeLoginClient>(servers.loginClientPort());
	client.login->login(client.account, client.password);
	client.login->requestServerList();
	client.key = client.login->play(1);
	client.game = std::make_unique<GameSession>(servers.gameClientPort());
	client.model.follow(client.game.get());
	client.game->readKey();
	client.game->send(GameSession::CM_VERSION_CHECK, GameSession::buildCM_VERSION_CHECK());
	waitFor(*client.game, "SM_VERSION_CHECK");
	client.game->send(GameSession::CM_L2AUTH_LOGIN_CHECK,
	                  GameSession::buildCM_L2AUTH_LOGIN_CHECK(client.key.playOk2, client.key.playOk1, client.key.accountId, client.key.loginOk));
	client.game->send(GameSession::CM_MAC_ADDRESS, GameSession::buildCM_MAC_ADDRESS());
	waitFor(*client.game, "SM_L2AUTH_LOGIN_CHECK");
	client.game->send(GameSession::CM_TIME_CHECK, GameSession::buildCM_TIME_CHECK(1));
	waitFor(*client.game, "SM_TIME_CHECK");
	client.game->send(GameSession::CM_CHARACTER_LIST, GameSession::buildCM_CHARACTER_LIST(client.key.playOk2));
	return decoders::decodeCharacterList(waitFor(*client.game, "SM_CHARACTER_LIST").data);
}

/** CM_LEVEL_READY and what it sends (the enter world of a map: quest onEnterWorld, the npcs of the map) */
std::vector<Packet> levelReady(Client& client) {
	client.game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
	std::vector<Packet> burst = collectFor(*client.game, 2500ms);
	client.model.sync();
	return burst;
}

/** CM_ENTER_WORLD, CM_LEVEL_READY. @return both bursts */
std::vector<Packet> enterWorld(Client& client) {
	const size_t from = client.mark();
	client.game->send(GameSession::CM_ENTER_WORLD, GameSession::buildCM_ENTER_WORLD(client.playerId));
	const Packet spawn = waitFor(*client.game, "SM_PLAYER_SPAWN", 60s);
	const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(spawn.data);
	client.x = spawned.x;
	client.y = spawned.y;
	client.z = spawned.z;
	client.worldId = spawned.worldId;
	collectFor(*client.game, 1500ms);
	levelReady(client);
	return client.since(from);
}

void disconnect(Client& client) {
	if (!client.game)
		return;
	client.game->send(GameSession::CM_QUIT, GameSession::buildCM_QUIT(false));
	waitFor(*client.game, "SM_QUIT_RESPONSE", 30s);
	if (!client.game->waitClosed(30s))
		throw std::runtime_error(client.label + ": the socket stayed open after CM_QUIT(0)");
	client.model.follow(nullptr);
	client.game.reset();
	client.login.reset();
}

std::pair<decoders::CharacterList, std::vector<Packet>> relogIn(ScenarioServers& servers, Client& client) {
	decoders::CharacterList list = logIn(servers, client);
	client.game->send(GameSession::CM_MAY_LOGIN_INTO_GAME, GameSession::buildCM_MAY_LOGIN_INTO_GAME());
	waitFor(*client.game, "SM_MAY_LOGIN_INTO_GAME");
	std::this_thread::sleep_for(1500ms);
	std::vector<Packet> burst = enterWorld(client);
	return {std::move(list), std::move(burst)};
}

/** The walk of the M5b gates: 5 m CM_MOVE steps, then a stop */
void walkTo(Client& client, float toX, float toY, float toZ) {
	const double total = distance2d(client.x, client.y, toX, toY);
	const int32_t steps = std::max(1, static_cast<int32_t>(total / 5.0));
	const float fromX = client.x, fromY = client.y, fromZ = client.z;
	for (int32_t step = 1; step <= steps; step++) {
		const float t = static_cast<float>(step) / static_cast<float>(steps);
		client.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(fromX + (toX - fromX) * t, fromY + (toY - fromY) * t,
		                                                                    fromZ + (toZ - fromZ) * t, 0, static_cast<int8_t>(0xE0), toX, toY, toZ));
		collectFor(*client.game, 100ms);
	}
	client.game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(toX, toY, toZ, 0, 0));
	client.x = toX;
	client.y = toY;
	client.z = toZ;
	collectFor(*client.game, 800ms);
}

/** CM_MOVE_IN_AIR along a straight line (the flying client's position while it follows a flight path), 5 m a step */
void flyTo(Client& client, float toX, float toY, float toZ) {
	const double total = distance2d(client.x, client.y, toX, toY);
	const int32_t steps = std::max(1, static_cast<int32_t>(total / 5.0));
	const float fromX = client.x, fromY = client.y, fromZ = client.z;
	for (int32_t step = 1; step <= steps; step++) {
		const float t = static_cast<float>(step) / static_cast<float>(steps);
		PacketWriter body;
		body.D(client.worldId).F(fromX + (toX - fromX) * t).F(fromY + (toY - fromY) * t).F(fromZ + (toZ - fromZ) * t).C(0).D(step);
		client.game->send(CM_MOVE_IN_AIR, body.data);
		collectFor(*client.game, 100ms);
	}
	client.x = toX;
	client.y = toY;
	client.z = toZ;
}

/** The object id of the npc of `templateId` nearest to (x, y) among the SM_NPC_INFO of this session, within `radius` */
std::optional<int32_t> npcObject(const Client& client, int32_t templateId, float x, float y, double radius = 8.0) {
	std::optional<int32_t> found;
	double best = radius;
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

/** The object ids of every npc of `templateId` announced from `from` on (newest last, no duplicates) */
std::vector<int32_t> npcsAnnounced(const Client& client, int32_t templateId, size_t from) {
	std::vector<int32_t> ids;
	for (const Packet& packet : client.since(from)) {
		if (packet.name != "SM_NPC_INFO")
			continue;
		try {
			const decoders::NpcInfo npc = decoders::decodeNpcInfo(packet.data);
			if (npc.templateId == templateId && std::find(ids.begin(), ids.end(), npc.objectId) == ids.end())
				ids.push_back(npc.objectId);
		} catch (const DecodeError&) {
		}
	}
	return ids;
}

/** Where the SM_NPC_INFO of the npc object put it */
std::optional<Spot> npcSpot(const Client& client, int32_t objectId) {
	std::optional<Spot> spot;
	for (const Packet& packet : client.game->recorded())
		if (packet.name == "SM_NPC_INFO") {
			try {
				const decoders::NpcInfo npc = decoders::decodeNpcInfo(packet.data);
				if (npc.objectId == objectId)
					spot = Spot{npc.x, npc.y, npc.z};
			} catch (const DecodeError&) {
			}
		}
	return spot;
}

int32_t requireNpc(const Client& client, int32_t templateId, Spot at, std::string_view step) {
	std::optional<int32_t> id = npcObject(client, templateId, at.x, at.y);
	if (!id)
		throw std::runtime_error(client.label + " " + std::string(step) + ": the server never announced npc " + std::to_string(templateId) + " near (" +
		                         std::to_string(at.x) + ", " + std::to_string(at.y) + ")");
	return *id;
}

/** Waits until the quest's SM_QUEST_ACTION with this status and vars was recorded from `from` on (it may have arrived already) */
bool awaitQuest(Client& client, size_t from, int32_t questId, uint8_t status, int32_t vars, std::chrono::milliseconds timeout) {
	const auto matches = [&](const Packet& packet) { return packet.name == "SM_QUEST_ACTION" && questIs({packet}, questId, status, vars); };
	const std::vector<Packet> seen = client.since(from);
	if (std::any_of(seen.begin(), seen.end(), matches))
		return true;
	return readUntil(*client.game, matches, timeout).has_value();
}

/** CM_DIALOG_SELECT and its burst */
std::vector<Packet> talk(Client& client, int32_t npcObjectId, uint16_t action, int32_t questId) {
	return client.game->talk(npcObjectId, action, questId, QUIET, 15s).packets;
}

/** CM_SHOW_DIALOG (the talk start, USE_OBJECT) and its burst */
std::vector<Packet> showDialog(Client& client, int32_t npcObjectId) {
	client.game->send(GameSession::CM_SHOW_DIALOG, GameSession::buildCM_SHOW_DIALOG(npcObjectId));
	return collectFor(*client.game, QUIET + 500ms);
}

std::optional<DialogWindow> pageOf(const std::vector<Packet>& packets, int32_t questId) {
	std::optional<DialogWindow> found;
	for (const Packet& packet : packets)
		if (packet.name == "SM_DIALOG_WINDOW") {
			DialogWindow window = decodeDialogWindow(packet);
			if (window.questId == questId || window.page == 0)
				found = window;
		}
	return found;
}

void expectPage(const Client& client, const std::vector<Packet>& packets, int32_t questId, int32_t page, std::string_view step) {
	std::optional<DialogWindow> window = pageOf(packets, questId);
	EXPECT_TRUE(window && window->page == page) << client.label << " " << step << ": expected dialog page " << page << " of quest " << questId
	                                            << ", got " << (window ? std::to_string(window->page) : std::string("none")) << "; the burst was "
	                                            << join(namesOf(packets));
}

/** SM_PLAY_MOVIE in the burst: its id, and the CM_PLAY_MOVIE_END the client sends when the movie ends (M5d D-05: CM_MOVE waits for it) */
void watchMovie(Client& client, const std::vector<Packet>& packets, int32_t questId, int32_t movieId, std::string_view step) {
	for (const Packet& packet : packets)
		if (packet.name == "SM_PLAY_MOVIE") {
			const Movie movie = decodeMovie(packet);
			EXPECT_EQ(movie.movieId, movieId) << client.label << " " << step;
			EXPECT_EQ(movie.questId, questId) << client.label << " " << step;
			PacketWriter end;
			end.C(movie.type).D(movie.target).D(movie.questId).D(movie.movieId).C(0).C(0);
			client.game->send(CM_PLAY_MOVIE_END, end.data);
			collectFor(*client.game, 500ms);
			return;
		}
	ADD_FAILURE() << client.label << " " << step << ": no SM_PLAY_MOVIE " << movieId << " in " << join(namesOf(packets));
}

/** SM_TELEPORT_LOC (SM_TELEPORT_LOC.java:31-40): C animation, D map, D map or instance, F x, F y, F z, C heading */
struct TeleportLoc {
	uint8_t animation = 0;
	int32_t mapId = 0;
	float x = 0, y = 0, z = 0;
};

TeleportLoc decodeTeleportLoc(const Packet& packet) {
	PacketReader reader(packet.data);
	TeleportLoc loc;
	loc.animation = reader.C();
	loc.mapId = reader.D();
	reader.D();
	loc.x = std::bit_cast<float>(reader.D());
	loc.y = std::bit_cast<float>(reader.D());
	loc.z = std::bit_cast<float>(reader.D());
	return loc;
}

bool sameSpot(float x, float y, float z, Spot to) {
	return std::fabs(x - to.x) < 0.001f && std::fabs(y - to.y) < 0.001f && std::fabs(z - to.z) < 0.001f;
}

/**
 * A beam teleport (FADE_OUT_BEAM): SM_TELEPORT_LOC, whose destination must be the handler's (`worldId`, `to`), then CM_TELEPORT_ANIMATION_DONE;
 * into another map the server answers with SM_PLAYER_SPAWN at that destination and waits for CM_LEVEL_READY. @return everything read from the
 * SM_TELEPORT_LOC on
 */
std::vector<Packet> finishTeleport(Client& client, const std::vector<Packet>& burst, int32_t worldId, Spot to, std::string_view step) {
	const auto teleportLoc = std::find_if(burst.begin(), burst.end(), [](const Packet& packet) { return packet.name == "SM_TELEPORT_LOC"; });
	if (teleportLoc == burst.end())
		throw std::runtime_error(client.label + " " + std::string(step) + ": no SM_TELEPORT_LOC; the burst was " + join(namesOf(burst)));
	const TeleportLoc loc = decodeTeleportLoc(*teleportLoc);
	EXPECT_TRUE(loc.mapId == worldId && sameSpot(loc.x, loc.y, loc.z, to))
	  << client.label << " " << step << ": SM_TELEPORT_LOC beams to map " << loc.mapId << " (" << loc.x << ", " << loc.y << ", " << loc.z
	  << "), the handler's destination is map " << worldId << " (" << to.x << ", " << to.y << ", " << to.z << ")";
	const size_t from = client.mark();
	client.game->send(CM_TELEPORT_ANIMATION_DONE, {});
	std::vector<Packet> after = collectFor(*client.game, 2000ms);
	if (worldId != client.worldId) {
		const auto spawn = std::find_if(after.begin(), after.end(), [](const Packet& packet) { return packet.name == "SM_PLAYER_SPAWN"; });
		if (spawn == after.end())
			throw std::runtime_error(client.label + " " + std::string(step) + ": no SM_PLAYER_SPAWN into map " + std::to_string(worldId) + " after " +
			                         "CM_TELEPORT_ANIMATION_DONE: " + join(namesOf(after)));
		const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(spawn->data);
		EXPECT_TRUE(spawned.worldId == worldId && sameSpot(spawned.x, spawned.y, spawned.z, to))
		  << client.label << " " << step << ": SM_PLAYER_SPAWN into map " << spawned.worldId << " at (" << spawned.x << ", " << spawned.y << ", "
		  << spawned.z << ")";
		client.worldId = worldId;
		levelReady(client);
	}
	client.x = to.x;
	client.y = to.y;
	client.z = to.z;
	return client.since(from);
}

/** CM_TARGET_SELECT and CM_ATTACK at the attack speed until the npc dies (SM_EMOTION DIE of the npc) */
void killNpc(Client& client, int32_t npcObjectId, int32_t attackSpeed, std::string_view step, std::chrono::milliseconds timeout = 120s) {
	client.game->send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(npcObjectId));
	collectFor(*client.game, 200ms);
	bool died = false, playerDied = false;
	const GameSession::FightOutcome outcome = client.game->fightUntil(
	  npcObjectId, std::chrono::milliseconds(attackSpeed),
	  [&](const Packet& packet) {
		  try {
			  if (packet.name == "SM_DELETE") { // the handlers delete a raider in its onKillEvent (_1006Ascension.java:211)
				  died = decoders::decodeDeleteObjectId(packet.data) == npcObjectId;
				  return died;
			  }
			  if (packet.name != "SM_EMOTION")
				  return false;
			  const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
			  if (emotion.emotionType != decoders::EMOTION_DIE)
				  return false;
			  died = emotion.senderObjectId == npcObjectId;
			  playerDied = emotion.senderObjectId == client.playerId;
			  return died || playerDied;
		  } catch (const DecodeError&) {
			  return false;
		  }
	  },
	  timeout, 600);
	if (playerDied)
		throw std::runtime_error(client.label + " " + std::string(step) + ": the character died fighting npc " + std::to_string(npcObjectId));
	if (!died)
		throw std::runtime_error(client.label + " " + std::string(step) + ": npc " + std::to_string(npcObjectId) + " did not die after " +
		                         std::to_string(outcome.attacksSent) + " CM_ATTACK in " + std::to_string(outcome.elapsed.count()) + " ms" +
		                         (outcome.closed ? " (the connection closed)" : ""));
	std::cout << client.label << " " << step << ": npc " << npcObjectId << " died after " << outcome.attacksSent << " CM_ATTACK in "
	          << outcome.elapsed.count() << " ms (attack speed " << attackSpeed << " ms)" << std::endl;
	collectFor(*client.game, 700ms);
}

/** The last SM_STATUPDATE_EXP in the packets (PlayerCommonData.setExp sends one whenever the exp changes) */
std::optional<decoders::StatUpdateExp> lastExpUpdate(const std::vector<Packet>& packets) {
	std::optional<decoders::StatUpdateExp> last;
	for (const Packet& packet : packets)
		if (packet.name == "SM_STATUPDATE_EXP")
			last = decoders::decodeStatUpdateExp(packet.data);
	return last;
}

std::string describeExp(const std::optional<decoders::StatUpdateExp>& exp) {
	return exp ? std::to_string(exp->currentExp) + " of " + std::to_string(exp->maxExp) : std::string("no SM_STATUPDATE_EXP");
}

/** The route of one race */
struct Route {
	bool asmodian;
	int32_t mission, ceremony;
	int32_t homeMap;
	int32_t instanceMap;
	int32_t capitalMap;
	Spot seed; // where the character is seeded, beside the mission's first npc
	int32_t startNpc;
	Spot startNpcAt;
	int32_t flightNpc;
	int32_t flightId;
	Spot arena;
	int32_t raider, boss;
	int32_t endNpc; // spawned in the instance after the boss
	int32_t classPage; // the Warrior's class selection page
	int32_t gladiatorAction;
	int32_t firstVarAfterBoss; // 5 (1006) or 6 (2008)
	int32_t bossMovie;
	Spot rewardExit; // the beam out of the instance
	Spot capitalArrival;
	int32_t ceremonyNpc1, ceremonyNpc2, rewardNpc;
	Spot ceremonyNpc1At, ceremonyNpc2At, rewardNpcAt;
	int32_t movie1, movie2;
};

const Route ELYOS_ROUTE{false, 1006, 1007, 210010000, 310020000, 110010000, {243.5f, 1639.5f, 100.4f}, 790001, {241.094f, 1639.46f, 100.375f},
	205000, 1001, {226.7f, 251.5f, 205.5f}, 211042, 211043, 790001, 2375, setpro(5), 5, 151, {245.14868f, 1639.1372f, 100.35713f},
	{1313.0f, 1512.0f, 568.0f}, 203725, 203752, 203758, {1369.60f, 1512.01f, 569.067f}, {1390.76f, 1693.14f, 573.286f},
	{1427.68f, 1614.14f, 573.706f}, 92, 91};
const Route ASMODIAN_ROUTE{true, 2008, 2009, 220010000, 320020000, 120010000, {380.5f, 1895.5f, 328.9f}, 203550, {378.74f, 1895.46f, 328.838f},
	205020, 3001, {301.0f, 259.0f, 205.5f}, 205040, 205041, 203550, 3057, setpro(7), 6, 152, {386.03476f, 1893.9309f, 327.62283f},
	{1685.0f, 1400.0f, 195.0f}, 204182, 204075, 204080, {1614.00f, 1397.96f, 193.127f}, {1469.10f, 1466.08f, 177.816f},
	{1455.60f, 1451.54f, 177.317f}, 121, 122};

/** 2 m short of the npc, from where the client comes */
Spot beside(Spot npc, float fromX, float fromY) {
	const double d = distance2d(npc.x, npc.y, fromX, fromY);
	if (d < 2.5)
		return {fromX, fromY, npc.z};
	const float t = static_cast<float>(2.0 / d);
	return {npc.x + (fromX - npc.x) * t, npc.y + (fromY - npc.y) * t, npc.z};
}

void walkBeside(Client& client, Spot npc) {
	const Spot to = beside(npc, client.x, client.y);
	walkTo(client, to.x, to.y, to.z);
}

/** The 2008 cards: Urd, Verdandi and Skuld with their beams (_2008Ascension.java:165-219) */
struct CardStep {
	int32_t npc;
	Spot npcAt;
	int32_t page;
	int32_t setpro;
	int32_t card;
	Spot beam;
};

const CardStep CARD_STEPS[] = {
	{790003, {587.621f, 2412.88f, 278.557f}, 1352, 2, 182203009, {940.74475f, 2295.5305f, 265.65674f}},
	{790002, {938.0f, 2298.0f, 266.125f}, 1693, 3, 182203010, {1111.5637f, 1719.2745f, 270.114256f}},
	{203546, {1114.31f, 1718.32f, 271.179f}, 2034, 4, 182203011, {383.10248f, 1895.3093f, 327.625f}},
};

/** One race's whole route; every step throws with a message when it cannot go on */
void playRoute(ScenarioServers& servers, Client& client, const Route& route, int32_t attackSpeedFallback) {
	const std::string& who = client.label;
	const int32_t mission = route.mission;

	// ---- E1: the enter world starts the mission ----
	std::vector<Packet> entered = relogIn(servers, client).second;
	ASSERT_EQ(client.worldId, route.homeMap) << who;
	EXPECT_TRUE(questIs(entered, mission, START, 0)) << who << " E1: the mission is started at the enter world (onLevelChange): "
	                                                << describeQuest(entered, mission);
	int32_t attackSpeed = attackSpeedFallback;
	for (const Packet& packet : entered)
		if (packet.name == "SM_STATS_INFO") {
			try {
				const decoders::StatsInfo stats = decoders::decodeStatsInfo(packet.data);
				EXPECT_EQ(stats.level, 9) << who << " E1: a starting class with the full bar is level 9";
				if (stats.attackSpeed > 0)
					attackSpeed = stats.attackSpeed;
			} catch (const DecodeError&) {
			}
		}
	const int32_t startNpc = requireNpc(client, route.startNpc, route.startNpcAt, "E1");

	// ---- E2: the steps in the home map ----
	std::vector<Packet> burst = talk(client, startNpc, QUEST_SELECT, mission);
	expectPage(client, burst, mission, 1011, "E2 QUEST_SELECT");
	burst = talk(client, startNpc, SETPRO1, mission);
	EXPECT_TRUE(questIs(burst, mission, START, 1)) << who << " E2 SETPRO1: " << describeQuest(burst, mission);
	if (!route.asmodian) {
		// 1006: the bottle, Cliona Lake, the item use in LF1_ITEMUSEAREA_Q1006, Daminu and his movie, back to Pernos
		finishTeleport(client, burst, route.homeMap, {657.0f, 1071.0f, 99.375f}, "E2 SETPRO1 beam");
		client.model.sync();
		std::vector<ModelItem> bottles = client.model.byItemId(182200007);
		ASSERT_EQ(bottles.size(), 1u) << who << " E2: Pernos gave the bottle 182200007; the cube is " << client.model.describe();
		const size_t useFrom = client.mark();
		client.game->send(GameSession::CM_USE_ITEM, GameSession::buildCM_USE_ITEM(bottles[0].objectId));
		ASSERT_TRUE(awaitQuest(client, useFrom, mission, START, 2, 15s))
		  << who << " E2: the bottle used in Cliona Lake's zone did not set var 2 (useQuestItem, 3 s): " << join(namesOf(client.since(useFrom)));
		collectFor(*client.game, 500ms);
		client.model.sync();
		EXPECT_EQ(client.model.byItemId(182200008).size(), 1u) << who << " E2: the filled bottle 182200008";
		const Spot daminuAt{600.0f, 1542.0f, 116.375f};
		walkBeside(client, daminuAt);
		const int32_t daminu = requireNpc(client, 730008, daminuAt, "E2 Daminu");
		burst = talk(client, daminu, QUEST_SELECT, mission);
		expectPage(client, burst, mission, 1352, "E2 Daminu QUEST_SELECT");
		burst = talk(client, daminu, SELECT2_1, mission);
		watchMovie(client, burst, mission, 14, "E2 Daminu SELECT2_1");
		expectPage(client, burst, mission, 1353, "E2 Daminu SELECT2_1");
		burst = talk(client, daminu, setpro(2), mission);
		EXPECT_TRUE(questIs(burst, mission, START, 3)) << who << " E2 Daminu SETPRO2: " << describeQuest(burst, mission);
		finishTeleport(client, burst, route.homeMap, {246.0f, 1639.0f, 100.316f}, "E2 SETPRO2 beam");
		burst = talk(client, startNpc, QUEST_SELECT, mission);
		expectPage(client, burst, mission, 1693, "E2 Pernos var 3");
		burst = talk(client, startNpc, setpro(3), mission);
	} else {
		// 2008: Urd, Verdandi and Skuld give the three cards, Munin plays movie 57 and takes them
		finishTeleport(client, burst, route.homeMap, {585.5074f, 2416.0312f, 278.625f}, "E2 SETPRO1 beam");
		int32_t var = 1;
		for (const CardStep& step : CARD_STEPS) {
			walkBeside(client, step.npcAt);
			const int32_t npc = requireNpc(client, step.npc, step.npcAt, "E2 card");
			burst = talk(client, npc, QUEST_SELECT, mission);
			expectPage(client, burst, mission, step.page, "E2 card QUEST_SELECT");
			burst = talk(client, npc, setpro(step.setpro), mission);
			var++;
			EXPECT_TRUE(questIs(burst, mission, START, var)) << who << " E2 card SETPRO" << step.setpro << ": " << describeQuest(burst, mission);
			finishTeleport(client, burst, route.homeMap, step.beam, "E2 card beam");
			client.model.sync();
			EXPECT_EQ(client.model.byItemId(step.card).size(), 1u) << who << " E2: the card " << step.card;
		}
		walkBeside(client, route.startNpcAt);
		burst = talk(client, startNpc, QUEST_SELECT, mission);
		expectPage(client, burst, mission, 2375, "E2 Munin var 4");
		burst = talk(client, startNpc, SELECT5_1, mission);
		watchMovie(client, burst, mission, 57, "E2 Munin SELECT5_1");
		client.model.sync();
		for (const CardStep& step : CARD_STEPS)
			EXPECT_TRUE(client.model.byItemId(step.card).empty()) << who << " E2: Munin takes the card " << step.card;
		burst = talk(client, startNpc, setpro(5), mission);
	}

	// ---- E3: the instance ----
	std::optional<std::vector<Packet>> spawnRead = std::nullopt;
	if (!std::any_of(burst.begin(), burst.end(), [](const Packet& packet) { return packet.name == "SM_PLAYER_SPAWN"; }))
		spawnRead = readUntil(*client.game, [](const Packet& packet) { return packet.name == "SM_PLAYER_SPAWN"; }, 10s);
	const bool spawned = std::any_of(burst.begin(), burst.end(), [](const Packet& packet) { return packet.name == "SM_PLAYER_SPAWN"; }) ||
	                     spawnRead.has_value();
	ASSERT_TRUE(spawned) << who << " E3: no SM_PLAYER_SPAWN into the instance after the SETPRO; the burst was " << join(namesOf(burst));
	EXPECT_TRUE(questIs(burst, mission, START, 99)) << who << " E3: var 99: " << describeQuest(burst, mission);
	{
		std::vector<Packet> spawnPackets = burst;
		if (spawnRead)
			spawnPackets.insert(spawnPackets.end(), spawnRead->begin(), spawnRead->end());
		for (const Packet& packet : spawnPackets)
			if (packet.name == "SM_PLAYER_SPAWN") {
				const decoders::PlayerSpawn into = decoders::decodePlayerSpawn(packet.data);
				EXPECT_EQ(into.worldId, route.instanceMap) << who << " E3";
				client.x = into.x;
				client.y = into.y;
				client.z = into.z;
			}
	}
	client.worldId = route.instanceMap;
	const size_t instanceFrom = client.mark();
	std::vector<Packet> ready = levelReady(client);
	EXPECT_TRUE(std::any_of(ready.begin(), ready.end(), [](const Packet& packet) {
		if (packet.name != "SM_ASCENSION_MORPH")
			return false;
		return !packet.data.empty() && packet.data[0] == 1;
	})) << who << " E3: SM_ASCENSION_MORPH(1) at the enter world of the instance: " << join(namesOf(ready));
	std::vector<int32_t> flightNpcs = npcsAnnounced(client, route.flightNpc, instanceFrom);
	ASSERT_EQ(flightNpcs.size(), 1u) << who << " E3: the instance's " << route.flightNpc;
	// the talk range: the npc stands where its spawn file puts it; the client walks there first
	const std::optional<Spot> flightNpcAt = npcSpot(client, flightNpcs[0]);
	ASSERT_TRUE(flightNpcAt.has_value());
	walkBeside(client, *flightNpcAt);

	// ---- E4: the flight and the fight ----
	const auto talkedAt = std::chrono::steady_clock::now();
	const size_t fightFrom = client.mark();
	burst = talk(client, flightNpcs[0], QUEST_SELECT, mission);
	EXPECT_TRUE(questIs(burst, mission, START, 50)) << who << " E4: var 50: " << describeQuest(burst, mission);
	bool flyTeleport = false;
	for (const Packet& packet : burst)
		if (packet.name == "SM_EMOTION") {
			try {
				const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
				flyTeleport |= emotion.senderObjectId == client.playerId && emotion.emotionType == EMOTION_START_FLYTELEPORT;
			} catch (const DecodeError&) {
			}
		}
	EXPECT_TRUE(flyTeleport) << who << " E4: SM_EMOTION START_FLYTELEPORT " << route.flightId;
	flyTo(client, route.arena.x, route.arena.y, route.arena.z + 2.0f);
	client.game->send(CM_EMOTION, PacketWriter().C(EMOTION_LAND_FLYTELEPORT).data);
	client.z = route.arena.z;
	collectFor(*client.game, 500ms);
	ASSERT_TRUE(awaitQuest(client, fightFrom, mission, START, 51, 60s)) << who << " E4: var 51 did not come after the flight";
	const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - talkedAt);
	EXPECT_GE(waited.count(), 42000) << who << " E4: the raiders come 43 s after the talk";
	collectFor(*client.game, 1500ms);
	std::vector<int32_t> raiders = npcsAnnounced(client, route.raider, fightFrom);
	ASSERT_EQ(raiders.size(), 4u) << who << " E4: four " << route.raider;
	for (size_t i = 0; i < raiders.size(); i++) {
		const size_t before = client.mark();
		killNpc(client, raiders[i], attackSpeed, "E4 raider");
		std::vector<Packet> after = client.since(before);
		if (i < 3)
			EXPECT_TRUE(questIs(after, mission, START, 52 + static_cast<int32_t>(i))) << who << " E4 kill " << i + 1 << ": "
			                                                                         << describeQuest(after, mission);
		else
			EXPECT_TRUE(questIs(after, mission, START, route.firstVarAfterBoss - 1)) << who << " E4 the fourth kill: " << describeQuest(after, mission);
	}
	collectFor(*client.game, 1000ms);
	std::vector<int32_t> bosses = npcsAnnounced(client, route.boss, fightFrom);
	ASSERT_EQ(bosses.size(), 1u) << who << " E4: the boss " << route.boss;
	const size_t bossFrom = client.mark();
	// Orissan and Hellion have 1,461 HP (npc_templates.xml), eight times a raider's 177: a starting Warrior's auto-attack needs minutes
	killNpc(client, bosses[0], attackSpeed, "E4 boss", 480s);
	std::vector<Packet> afterBoss = client.since(bossFrom);
	watchMovie(client, afterBoss, mission, route.bossMovie, "E4 boss movie");
	afterBoss = client.since(bossFrom);
	EXPECT_TRUE(questIs(afterBoss, mission, START, route.firstVarAfterBoss)) << who << " E4 the boss: " << describeQuest(afterBoss, mission);
	std::vector<int32_t> endNpcs = npcsAnnounced(client, route.endNpc, bossFrom);
	ASSERT_EQ(endNpcs.size(), 1u) << who << " E4: " << route.endNpc << " is spawned in the instance after the boss";

	// ---- E5: the class and the reward ----
	const std::optional<Spot> endNpcAt = npcSpot(client, endNpcs[0]);
	ASSERT_TRUE(endNpcAt.has_value());
	walkBeside(client, *endNpcAt);
	burst = talk(client, endNpcs[0], QUEST_SELECT, mission);
	expectPage(client, burst, mission, route.asmodian ? 2716 : 2034, "E5 QUEST_SELECT");
	burst = talk(client, endNpcs[0], setpro(route.asmodian ? 6 : 4), mission);
	expectPage(client, burst, mission, route.classPage, "E5 the class page");
	burst = talk(client, endNpcs[0], static_cast<uint16_t>(route.gladiatorAction), mission);
	EXPECT_TRUE(questIs(burst, mission, REWARD, route.firstVarAfterBoss)) << who << " E5 the Gladiator: " << describeQuest(burst, mission);
	expectPage(client, burst, mission, 5, "E5 the reward page");
	burst = talk(client, endNpcs[0], SELECTED_QUEST_NOREWARD, mission);
	std::vector<Packet> out = finishTeleport(client, burst, route.homeMap, route.rewardExit, "E5 the beam out");
	std::vector<Packet> finished = burst;
	finished.insert(finished.end(), out.begin(), out.end());
	EXPECT_TRUE(questIs(finished, mission, COMPLETE)) << who << " E5: the mission is COMPLETE: " << describeQuest(finished, mission);
	EXPECT_TRUE(questIs(finished, route.ceremony, START, 0)) << who << " E5: the ceremony starts: " << describeQuest(finished, route.ceremony);
	// Java's order, kept (docs/deviations/Q06.md): QuestService.finishQuest adds the reward's 73,200 exp before 1006 / 2008's completion event
	// makes him a Daeva, so PlayerCommonData.setExp holds him at level 9 with the full bar (43,087 of 43,087 above level 9's first exp)
	const std::optional<decoders::StatUpdateExp> missionExp = lastExpUpdate(finished);
	EXPECT_TRUE(missionExp && missionExp->currentExp == FULL_LEVEL_9_BAR - LEVEL_9_START && missionExp->maxExp == FULL_LEVEL_9_BAR - LEVEL_9_START)
	  << who << " E5: the reward's exp leaves the full level-9 bar: " << describeExp(missionExp);

	// ---- E6: the ceremony ----
	walkBeside(client, route.startNpcAt);
	const int32_t homeNpc = requireNpc(client, route.startNpc, route.startNpcAt, "E6");
	burst = talk(client, homeNpc, QUEST_SELECT, route.ceremony);
	expectPage(client, burst, route.ceremony, 1011, "E6 QUEST_SELECT");
	burst = talk(client, homeNpc, SETPRO1, route.ceremony);
	EXPECT_TRUE(questIs(burst, route.ceremony, START, 1)) << who << " E6 SETPRO1: " << describeQuest(burst, route.ceremony);
	finishTeleport(client, burst, route.capitalMap, route.capitalArrival, "E6 the beam to the capital");
	walkBeside(client, route.ceremonyNpc1At);
	const int32_t npc1 = requireNpc(client, route.ceremonyNpc1, route.ceremonyNpc1At, "E6 npc 1");
	burst = talk(client, npc1, QUEST_SELECT, route.ceremony);
	expectPage(client, burst, route.ceremony, 1352, "E6 npc 1");
	burst = talk(client, npc1, SELECT2_1, route.ceremony);
	watchMovie(client, burst, route.ceremony, route.movie1, "E6 npc 1 movie");
	burst = talk(client, npc1, setpro(2), route.ceremony);
	EXPECT_TRUE(questIs(burst, route.ceremony, START, 2)) << who << " E6 SETPRO2: " << describeQuest(burst, route.ceremony);
	walkBeside(client, route.ceremonyNpc2At);
	const int32_t npc2 = requireNpc(client, route.ceremonyNpc2, route.ceremonyNpc2At, "E6 npc 2");
	burst = talk(client, npc2, QUEST_SELECT, route.ceremony);
	expectPage(client, burst, route.ceremony, 1693, "E6 npc 2");
	burst = talk(client, npc2, SELECT3_1, route.ceremony);
	watchMovie(client, burst, route.ceremony, route.movie2, "E6 npc 2 movie");
	burst = talk(client, npc2, setpro(3), route.ceremony);
	EXPECT_TRUE(questIs(burst, route.ceremony, REWARD, 10)) << who << " E6 SETPRO3: a Warrior's var 10: " << describeQuest(burst, route.ceremony);
	walkBeside(client, route.rewardNpcAt);
	const int32_t rewardNpc = requireNpc(client, route.rewardNpc, route.rewardNpcAt, "E6 reward npc");
	burst = showDialog(client, rewardNpc);
	expectPage(client, burst, route.ceremony, 2034, "E6 the reward npc (USE_OBJECT)");
	const size_t ceremonyEndFrom = client.mark();
	burst = talk(client, rewardNpc, SELECTED_QUEST_REWARD1, route.ceremony);
	EXPECT_TRUE(questIs(burst, route.ceremony, COMPLETE)) << who << " E6: the ceremony is COMPLETE: " << describeQuest(burst, route.ceremony);
	collectFor(*client.game, 1000ms);
	// the ceremony's 13,125 exp, added to the Daeva's full level-9 bar, make him level 10: the bar shows them above level 10's first exp
	const std::optional<decoders::StatUpdateExp> ceremonyExp = lastExpUpdate(client.since(ceremonyEndFrom));
	EXPECT_TRUE(ceremonyExp && ceremonyExp->currentExp == CEREMONY_EXP)
	  << who << " E6: the ceremony's exp lift him to level 10 (13,125 above level 10's first exp): " << describeExp(ceremonyExp);
}

void runAscensionGate() {
	const std::string testName = "gs.scenario.ascension";
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
	const std::filesystem::path outputDir = std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "ascension";
	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.schemaPrefix = "asc";
	// the retail route: the four handlers register only while the simple class change is off (_1006Ascension.java:49-50)
	config.gameServerProperties["gameserver.simple.secondclass.enable"] = "false";
	config.gameServerProperties["gameserver.npcshouts.enable"] = "false";
	config.startupTimeout = 10min;
	config.stopTimeout = 3min;
	ScenarioServers servers(config, *environment);
	const std::string schema = servers.gameSchema();
	const ScenarioDatabase& database = servers.gameDatabase();

	try {
		servers.createSchemas();
		servers.startLoginServer();
		servers.startGameServer();

		Client elyos, asmodian;
		elyos.label = "Elyos";
		asmodian.label = "Asmodian";
		const std::string suffix = schema.substr(schema.size() - 8);
		elyos.account = "asce" + suffix;
		asmodian.account = "asca" + suffix;
		elyos.name = "Ascendely";
		asmodian.name = "Ascendasmo";
		asmodian.asmodian = true;

		// ---- the two Warriors, created by the client, seeded while disconnected (F-3) ----
		for (Client* client : {&elyos, &asmodian}) {
			logIn(servers, *client);
			NewCharacter character;
			character.name = client->name;
			character.asmodian = client->asmodian;
			character.playerClassId = NewCharacter::WARRIOR;
			client->game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(client->key.accountId, client->account, character, 1));
			ASSERT_EQ(decoders::decodeCreateCharacter(waitFor(*client->game, "SM_CREATE_CHARACTER").data).responseCode, RESPONSE_OPEN_CREATION_WINDOW);
			client->game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(client->key.accountId, client->account, character, 0));
			const decoders::CreateCharacter created = decoders::decodeCreateCharacter(waitFor(*client->game, "SM_CREATE_CHARACTER").data);
			ASSERT_TRUE(created.responseCode == RESPONSE_OK && created.player) << client->label << ": SM_CREATE_CHARACTER " << created.responseCode;
			client->playerId = created.player->playerId;
			disconnect(*client);
			const Route& route = client->asmodian ? ASMODIAN_ROUTE : ELYOS_ROUTE;
			database.execute(schema, "UPDATE players SET exp = " + std::to_string(FULL_LEVEL_9_BAR) + ", world_id = " + std::to_string(route.homeMap) +
			                           ", x = " + std::to_string(route.seed.x) + ", y = " + std::to_string(route.seed.y) + ", z = " +
			                           std::to_string(route.seed.z) + ", heading = 0 WHERE id = " + std::to_string(client->playerId));
		}

		for (Client* client : {&elyos, &asmodian}) {
			const Route& route = client->asmodian ? ASMODIAN_ROUTE : ELYOS_ROUTE;
			playRoute(servers, *client, route, 1500);
			if (::testing::Test::HasFatalFailure())
				break;
			// ---- E7: a relog shows the level-10 Gladiator; the rows are COMPLETE ----
			disconnect(*client);
			const decoders::CharacterList list = relogIn(servers, *client).first;
			ASSERT_EQ(list.characters.size(), 1u);
			EXPECT_EQ(list.characters[0].classId, GLADIATOR_CLASS_ID) << client->label << " E7: the character is a Gladiator";
			EXPECT_EQ(list.characters[0].level, 10) << client->label << " E7: level 10 after the ascension and the ceremony";
			for (int32_t questId : {route.mission, route.ceremony}) {
				const std::optional<std::string> status = database.queryString(schema, "SELECT status FROM player_quests WHERE player_id = " +
				                                                                         std::to_string(client->playerId) + " AND quest_id = " +
				                                                                         std::to_string(questId));
				EXPECT_EQ(status, std::optional<std::string>("COMPLETE")) << client->label << " E7: quest " << questId;
			}
			disconnect(*client);
		}
	} catch (const std::exception& exception) {
		ADD_FAILURE() << testName << ": " << exception.what();
	}

	try {
		servers.stopGameServer();
		servers.stopLoginServer();
	} catch (const std::exception& exception) {
		ADD_FAILURE() << testName << ": stopping the servers: " << exception.what();
	}
	for (const std::string& problem : servers.stopProblemsReported())
		ADD_FAILURE() << testName << ": " << problem;
	const bool failed = ::testing::Test::HasFailure();
	if (failed)
		std::cout << testName << " failed.\nlogs: " << (outputDir / "game_server.log") << ", " << (outputDir / "login_server.log") << std::endl;
	if (!(failed && ScenarioServers::keepSchemasOnFailure())) {
		try {
			servers.dropSchemas();
		} catch (const std::exception& exception) {
			std::cout << "the scenario schemas could not be dropped (" << exception.what() << ")" << std::endl;
		}
	}
}

} // namespace

TEST(AscensionScenario, Run) {
	runAscensionGate();
}

} // namespace aion::gameserver::scenario
