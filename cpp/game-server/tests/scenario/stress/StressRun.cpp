#include "StressRun.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "FakeLoginClient.h"
#include "GameSession.h"
#include "decoders/PacketDecoders.h"

#include "aion/commons/database/Connection.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario::stress {

namespace {

using namespace std::chrono_literals;
using Packet = GameSession::Packet;

/** PlayerClass.WARRIOR */
constexpr int32_t CLASS_WARRIOR = 0;
/** SM_CREATE_CHARACTER response codes (SM_CREATE_CHARACTER.java) */
constexpr int32_t RESPONSE_OK = 0;
constexpr int32_t RESPONSE_OPEN_CREATION_WINDOW = 22;
/** the last packet of the CM_ENTER_WORLD burst and of the CM_LEVEL_READY burst (m5a-plan.md §5.8 #32 and #44) */
constexpr std::string_view ENTER_WORLD_LAST = "SM_HOUSE_OWNER_INFO";
constexpr std::string_view LEVEL_READY_LAST = "SM_CUBE_UPDATE";

int32_t readPositiveInt(const std::map<std::string, std::string, std::less<>>& values, std::string_view key, int32_t fallback) {
	const auto found = values.find(key);
	if (found == values.end() || found->second.empty())
		return fallback;
	try {
		const int32_t value = std::stoi(found->second);
		if (value <= 0)
			throw std::out_of_range("not positive");
		return value;
	} catch (const std::exception&) {
		throw std::runtime_error(std::string(key) + " must be a positive number, not '" + found->second + "'");
	}
}

int32_t readNonNegativeInt(const std::map<std::string, std::string, std::less<>>& values, std::string_view key, int32_t fallback) {
	const auto found = values.find(key);
	if (found == values.end() || found->second.empty())
		return fallback;
	try {
		const int32_t value = std::stoi(found->second);
		if (value < 0)
			throw std::out_of_range("negative");
		return value;
	} catch (const std::exception&) {
		throw std::runtime_error(std::string(key) + " must be a number >= 0, not '" + found->second + "'");
	}
}

/**
 * Reads server packets until one of them is `name` and returns everything read, `name` included. Unlike collectUntilQuiet this does not end the
 * burst on a gap: with twenty clients on one server a second without a packet says nothing, and a truncated burst would look like a missing
 * packet. @throws std::runtime_error on timeout or a closed connection
 */
std::vector<Packet> collectUntil(GameSession& session, std::string_view name, std::chrono::milliseconds timeout) {
	std::vector<Packet> collected;
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	const auto whatWasRead = [&collected] {
		std::string names;
		for (size_t i = collected.size() > 8 ? collected.size() - 8 : 0; i < collected.size(); i++)
			names += (names.empty() ? "" : ", ") + collected[i].name;
		return std::to_string(collected.size()) + " packets read, last: " + (names.empty() ? "(none)" : names);
	};
	for (;;) {
		const auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
			throw std::runtime_error("timeout waiting for " + std::string(name) + " (" + whatWasRead() + ")");
		std::optional<Packet> packet = session.next(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
		if (!packet)
			throw std::runtime_error(std::string("no ") + std::string(name) + " (" +
			                         (session.client.socket.isClosed() ? "connection closed" : "timeout") + ", " + whatWasRead() + ")");
		const bool last = packet->name == name;
		collected.push_back(std::move(*packet));
		if (last)
			return collected;
	}
}

const Packet* firstOfName(const std::vector<Packet>& packets, std::string_view name) {
	for (const Packet& packet : packets)
		if (packet.name == name)
			return &packet;
	return nullptr;
}

const Packet* lastOfName(const std::vector<Packet>& packets, std::string_view name) {
	const Packet* found = nullptr;
	for (const Packet& packet : packets)
		if (packet.name == name)
			found = &packet;
	return found;
}

} // namespace

// ---- options ------------------------------------------------------------------------------------------------------------------------------

StressOptions StressOptions::fromValues(const std::map<std::string, std::string, std::less<>>& values) {
	StressOptions parsed;
	parsed.clients = readPositiveInt(values, "AION_STRESS_CLIENTS", parsed.clients);
	const auto minutes = values.find("AION_STRESS_MINUTES");
	if (minutes != values.end() && !minutes->second.empty())
		parsed.duration = std::chrono::seconds(readPositiveInt(values, "AION_STRESS_MINUTES", 1) * 60);
	// AION_STRESS_SECONDS wins, so that a mutation run can ask for 90 seconds without rewriting the registration
	const auto seconds = values.find("AION_STRESS_SECONDS");
	if (seconds != values.end() && !seconds->second.empty())
		parsed.duration = std::chrono::seconds(readPositiveInt(values, "AION_STRESS_SECONDS", 1));
	parsed.roundsPerConnection = readPositiveInt(values, "AION_STRESS_ROUNDS_PER_CONNECTION", parsed.roundsPerConnection);
	parsed.faultEveryNthRound = readNonNegativeInt(values, "AION_STRESS_FAULT_EVERY", parsed.faultEveryNthRound);
	parsed.moveSteps = readNonNegativeInt(values, "AION_STRESS_MOVE_STEPS", parsed.moveSteps);
	return parsed;
}

StressOptions StressOptions::fromEnvironment() {
	std::map<std::string, std::string, std::less<>> values;
	for (const char* key : {"AION_STRESS_CLIENTS", "AION_STRESS_MINUTES", "AION_STRESS_SECONDS", "AION_STRESS_ROUNDS_PER_CONNECTION",
	         "AION_STRESS_FAULT_EVERY", "AION_STRESS_MOVE_STEPS"}) {
		if (const char* value = std::getenv(key))
			values.emplace(key, value);
	}
	return fromValues(values);
}

std::string StressOptions::describe() const {
	std::ostringstream text;
	text << clients << " clients for " << duration.count() << " s, " << roundsPerConnection << " enter-world rounds per connection, "
	     << moveSteps << " moves per round, DAO fault injection ";
	if (faultEveryNthRound > 0)
		text << "on every " << faultEveryNthRound << ". round";
	else
		text << "off";
	return text.str();
}

// ---- what the log is scanned for ----------------------------------------------------------------------------------------------------------

std::vector<std::string> reusedObjectIdWarnings() {
	return {
		// IDFactory.cpp:249 - an id released twice, i.e. one that is free while an object still uses it
		"because it wasn't taken",
		// World.cpp:148 - the object being removed is not the one the id maps to
		"from world but ID already belongs to",
		// World.cpp:121 (DuplicateAionObjectException) - storeObject found the id taken
		"Duplicate object:",
		// PlayerEnterWorldService.cpp:277 - a character id that is already in the world
		"Duplicate character obj ID",
	};
}

std::vector<std::string> injectedDaoErrorMessages() {
	return {
		// PlayerLifeStatsDAO.cpp:68 (the only one whose message carries the exception text, so it carries FAULT_MARKER itself)
		"Could not update PlayerLifeStat data for player",
		// PlayerDAO.cpp:106, reached through PlayerService::storePlayer. NOT the bare prefix "Error saving ": that substring also carries nine
		// unrelated production errors (BonusPackDAO, FactionPackDAO, GuideDAO, PlayerDAO::storeNewPlayer - a FAILING CHARACTER CREATION, which
		// this BEFORE UPDATE trigger cannot even cause - PlayerPasskeyDAO, RewardServiceDAO, VeteranRewardDAO and AionConnection), so allowing it
		// would let a real defect pass as one of the injection's own.
		"Error saving Player [",
		// PlayerDAO.cpp:467
		"Error storing old_level:",
		// DB.cpp:72 for PlayerDAO::storeLastOnlineTime (PlayerDAO.cpp:282) and PlayerDAO::onlinePlayer (PlayerDAO.cpp:308)
		"Failed to execute IU query UPDATE players set last_online",
		"Failed to execute IU query UPDATE players SET online=?",
	};
}

bool isInjectedDaoError(std::string_view logLine) {
	for (const std::string& message : injectedDaoErrorMessages())
		if (logLine.find(message) != std::string_view::npos)
			return true;
	return false;
}

// ---- one client ---------------------------------------------------------------------------------------------------------------------------

/**
 * One stress client: its own login server session, its own game connection and its own database connection (for the HP seeding and the fault
 * injection of its character). Runs on its own thread and never lets an exception escape: a round that fails is recorded in its ClientStats and
 * the client goes on with the next connection, so one client's failure does not end the run - the test fails on the recorded lines afterwards.
 */
class StressRun::Client {
public:
	Client(StressRun& run, ClientStats& stats) : run(run), stats(stats) {}

	void operator()(std::chrono::steady_clock::time_point deadline) {
		try {
			database = run.servers.gameDatabase().open(run.servers.gameSchema());
		} catch (const std::exception& exception) {
			stats.failures.push_back(std::string("could not open a database connection: ") + exception.what());
			return;
		}
		while (std::chrono::steady_clock::now() < deadline) {
			try {
				connectionCycle(deadline);
			} catch (const std::exception& exception) {
				stats.failures.push_back(std::string("connection cycle ") + std::to_string(stats.connections) + ": " + exception.what());
			} catch (...) {
				stats.failures.push_back("connection cycle " + std::to_string(stats.connections) + ": unknown exception");
			}
			// whatever happened, the character must not stay armed and must not stay marked online in the database
			try {
				if (armed)
					disarmFault();
			} catch (const std::exception& exception) {
				stats.failures.push_back(std::string("could not disarm the fault injection: ") + exception.what());
			}
			game.reset();
			login.reset();
			std::this_thread::sleep_for(run.options_.cyclePause);
		}
	}

private:
	void connectionCycle(std::chrono::steady_clock::time_point deadline) {
		login = std::make_unique<FakeLoginClient>(run.servers.loginClientPort());
		login->login(stats.account, PASSWORD);
		const FakeLoginClient::ServerList list = login->requestServerList();
		bool online = false;
		for (const FakeLoginClient::GameServerEntry& entry : list.servers)
			if (entry.id == 1)
				online = entry.online;
		if (!online)
			throw std::runtime_error("game server 1 is not listed as online");
		const FakeLoginClient::SessionKey key = login->play(1);

		game = std::make_unique<GameSession>(run.servers.gameClientPort());
		game->readKey();
		game->send(GameSession::CM_VERSION_CHECK, GameSession::buildCM_VERSION_CHECK());
		game->expect("SM_VERSION_CHECK", run.options_.packetTimeout);
		game->send(GameSession::CM_L2AUTH_LOGIN_CHECK, GameSession::buildCM_L2AUTH_LOGIN_CHECK(key.playOk2, key.playOk1, key.accountId, key.loginOk));
		game->send(GameSession::CM_MAC_ADDRESS, GameSession::buildCM_MAC_ADDRESS());
		game->expect("SM_L2AUTH_LOGIN_CHECK", run.options_.packetTimeout);
		game->send(GameSession::CM_TIME_CHECK, GameSession::buildCM_TIME_CHECK(1));
		game->expect("SM_TIME_CHECK", run.options_.packetTimeout);
		game->send(GameSession::CM_CHARACTER_LIST, GameSession::buildCM_CHARACTER_LIST(key.playOk2));
		const Packet characters = game->expect("SM_CHARACTER_LIST", run.options_.packetTimeout);
		const decoders::CharacterList characterList = decoders::decodeCharacterList(characters.data);
		if (characterList.characters.empty())
			createCharacter(key.accountId);
		else
			stats.playerId = characterList.characters[0].playerId;
		stats.connections++;

		for (int32_t round = 0; round < run.options_.roundsPerConnection; round++) {
			if (std::chrono::steady_clock::now() >= deadline)
				break;
			playOneRound();
		}

		// CM_QUIT(0) with no character in the world: the server answers SM_QUIT_RESPONSE and closes the socket (CM_QUIT.cpp:61)
		game->send(GameSession::CM_QUIT, GameSession::buildCM_QUIT(false));
		game->expect("SM_QUIT_RESPONSE", run.options_.packetTimeout);
		if (!game->waitClosed(run.options_.packetTimeout))
			throw std::runtime_error("the socket stayed open after CM_QUIT(0)");
	}

	void createCharacter(int32_t accountId) {
		NewCharacter character;
		character.name = stats.characterName;
		character.playerClassId = CLASS_WARRIOR;
		game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(accountId, stats.account, character, 1));
		const decoders::CreateCharacter window = decoders::decodeCreateCharacter(game->expect("SM_CREATE_CHARACTER", run.options_.packetTimeout).data);
		if (window.responseCode != RESPONSE_OPEN_CREATION_WINDOW)
			throw std::runtime_error("CM_CREATE_CHARACTER(1) answered " + std::to_string(window.responseCode));
		game->send(GameSession::CM_CREATE_CHARACTER, GameSession::buildCM_CREATE_CHARACTER(accountId, stats.account, character, 0));
		const decoders::CreateCharacter created = decoders::decodeCreateCharacter(game->expect("SM_CREATE_CHARACTER", run.options_.packetTimeout).data);
		if (created.responseCode != RESPONSE_OK || !created.player)
			throw std::runtime_error("CM_CREATE_CHARACTER answered " + std::to_string(created.responseCode));
		stats.playerId = created.player->playerId;
	}

	/** one enter world, a few moves and one CM_QUIT(1) back to the character list */
	void playOneRound() {
		// §5.7 Q3's way of getting a character below full HP without a damage packet: halve the stored value before the login loads it. The
		// first round of a character's life finds no row (PlayerLifeStatsDAO inserts it during enter world) and then starts at full HP.
		execute("UPDATE player_life_stats SET hp = GREATEST(1, hp DIV 2) WHERE player_id = " + std::to_string(stats.playerId));
		// Every enter world waits out gameserver.character.reentry.time, not only a second one on the same connection: the previous round's
		// CM_QUIT ran seconds ago whether the client reconnected in between or not, and PlayerEnterWorldService.cpp:152-156 answers
		// SM_ENTER_WORLD_CHECK(REENTRY_TIME) and sends nothing else while `now - lastOnline` is below it.
		std::this_thread::sleep_for(run.options_.reentryPause);

		game->send(GameSession::CM_MAY_LOGIN_INTO_GAME, GameSession::buildCM_MAY_LOGIN_INTO_GAME());
		game->expect("SM_MAY_LOGIN_INTO_GAME", run.options_.packetTimeout);
		game->send(GameSession::CM_ENTER_WORLD, GameSession::buildCM_ENTER_WORLD(stats.playerId));
		// SM_ENTER_WORLD_CHECK comes first and is read on its own: a refused enter (REENTRY_TIME, CONNECTION_ERROR) is answered with that packet
		// and NOTHING else, so waiting for the last packet of the burst would turn a one-byte refusal into a packet timeout minutes later
		std::vector<Packet> burst = collectUntil(*game, "SM_ENTER_WORLD_CHECK", run.options_.packetTimeout);
		const Packet* check = firstOfName(burst, "SM_ENTER_WORLD_CHECK");
		if (check == nullptr || check->data.empty())
			throw std::runtime_error("no SM_ENTER_WORLD_CHECK after CM_ENTER_WORLD");
		if (check->data[0] != 0)
			throw std::runtime_error("SM_ENTER_WORLD_CHECK refused the enter with msg " + std::to_string(static_cast<int32_t>(check->data[0])) +
			                         " (1 CONNECTION_ERROR, 3 REENTRY_TIME)");
		for (Packet& packet : collectUntil(*game, ENTER_WORLD_LAST, run.options_.packetTimeout))
			burst.push_back(std::move(packet));

		const Packet* spawn = firstOfName(burst, "SM_PLAYER_SPAWN");
		if (spawn == nullptr)
			throw std::runtime_error("no SM_PLAYER_SPAWN after CM_ENTER_WORLD");
		const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(spawn->data);
		if (stats.enters == 0) {
			stats.homeX = spawned.x;
			stats.homeY = spawned.y;
			stats.homeZ = spawned.z;
		}
		stats.maxDrift = std::max(stats.maxDrift, std::abs(spawned.x - stats.homeX));
		stats.enters++;

		const Packet* statsInfo = lastOfName(burst, "SM_STATS_INFO");
		if (statsInfo == nullptr)
			throw std::runtime_error("no SM_STATS_INFO after CM_ENTER_WORLD");
		const decoders::StatsInfo info = decoders::decodeStatsInfo(statsInfo->data);
		if (info.currentHp < info.maxHp)
			stats.entersBelowMaxHp++;

		game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
		collectUntil(*game, LEVEL_READY_LAST, run.options_.packetTimeout);

		// The walk starts at the character's FIRST spawn point and ends there again, instead of starting wherever the last round left it. A
		// logout stores the position and the next enter world resumes there, so a walk that starts where the last one ended adds up over
		// hundreds of rounds: the first thirty minute run carried every character 5.4 km east - out of the npc-populated part of Poeta after
		// the first minute and, after 22 minutes, past the edge of the region grid, which the server reported 40 times as "New MapRegion for
		// Player ... doesn't exist at coordinates" (Java's own warning for a position outside the grid, not a defect). A character standing in
		// empty terrain sees nothing, and the knownlist churn this run exists to produce goes with it. Alternating the direction per round was
		// not enough - the measured drift then still grew by one round's walk every two rounds - so the anchor is absolute: every round walks
		// out from home and the closing stop move puts the character back on it, which is the position the logout then stores.
		const float y = stats.homeY;
		for (int32_t step = 1; step <= run.options_.moveSteps; step++) {
			const float x = stats.homeX + static_cast<float>(step) * run.options_.moveStep;
			game->send(GameSession::CM_MOVE,
				GameSession::buildCM_MOVE(x, y, stats.homeZ, 0, static_cast<int8_t>(0xE0), x, y, stats.homeZ));
			stats.moves++;
			std::this_thread::sleep_for(run.options_.moveInterval);
		}
		if (run.options_.moveSteps > 0)
			game->send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(stats.homeX, y, stats.homeZ, 0, 0));

		const bool withFault = run.options_.faultEveryNthRound > 0 && (stats.enters % run.options_.faultEveryNthRound) == 0;
		if (withFault)
			armFault();
		// CM_QUIT(1) runs PlayerLeaveWorldService::leaveWorld synchronously and only then sends SM_QUIT_RESPONSE (CM_QUIT.cpp:56-59), so every
		// DAO call of the logout has happened by the time this returns and the injection can be disarmed right afterwards
		game->send(GameSession::CM_QUIT, GameSession::buildCM_QUIT(true));
		game->expect("SM_QUIT_RESPONSE", run.options_.packetTimeout);
		stats.logouts++;
		if (withFault) {
			disarmFault();
			stats.faultLogouts++;
		}
	}

	void armFault() {
		execute("INSERT IGNORE INTO " + std::string(FAULT_TABLE) + " (player_id) VALUES (" + std::to_string(stats.playerId) + ")");
		armed = true;
	}

	void disarmFault() {
		execute("DELETE FROM " + std::string(FAULT_TABLE) + " WHERE player_id = " + std::to_string(stats.playerId));
		armed = false;
		// PlayerDAO::onlinePlayer(player, false) was one of the statements the injection made fail, so `players.online` is still 1 and
		// PlayerEnterWorldService.cpp:252 would answer the next CM_ENTER_WORLD with REENTRY_TIME. The harness repairs the column it broke; the
		// in-memory cleanup this run measures is untouched by it.
		execute("UPDATE players SET online = 0 WHERE id = " + std::to_string(stats.playerId));
	}

	void execute(const std::string& sql) {
		database->executeSimple(sql);
	}

	static constexpr std::string_view PASSWORD = "m5aPassword1";

	StressRun& run;
	ClientStats& stats;
	std::unique_ptr<commons::database::Connection> database;
	std::unique_ptr<FakeLoginClient> login;
	std::unique_ptr<GameSession> game;
	bool armed = false;
};

// ---- the run ------------------------------------------------------------------------------------------------------------------------------

StressRun::StressRun(ScenarioServers& serversValue, StressOptions optionsValue) : servers(serversValue), options_(std::move(optionsValue)) {
	for (int32_t i = 0; i < options_.clients; i++) {
		ClientStats stats;
		stats.index = i;
		stats.account = accountOf(i);
		stats.characterName = characterNameOf(i);
		clientStats.push_back(std::move(stats));
	}
}

StressRun::~StressRun() = default;

std::string StressRun::accountOf(int32_t index) const {
	const std::string& schema = servers.gameSchema();
	const std::string suffix = schema.size() >= 8 ? schema.substr(schema.size() - 8) : schema;
	return "st" + suffix + characterNameOf(index).substr(6);
}

std::string StressRun::characterNameOf(int32_t index) {
	// letters only: NameConfig's gameserver.name.character_pattern is [a-zA-Z]{2,16}, and Util::convertName normalises the case
	const char first = static_cast<char>('a' + (index / 26) % 26);
	const char second = static_cast<char>('a' + index % 26);
	return std::string("Stress") + first + second;
}

void StressRun::installFaultInjection() {
	const ScenarioDatabase& database = servers.gameDatabase();
	const std::string schema = servers.gameSchema();
	const std::string table(FAULT_TABLE);
	database.execute(schema, "CREATE TABLE IF NOT EXISTS " + table + " (player_id INT NOT NULL PRIMARY KEY) ENGINE=InnoDB");
	// One trigger per table the logout writes. The row in the helper table is what arms them, so the triggers themselves are installed once and
	// are inert for every character that is not armed. SIGNAL SQLSTATE '45000' is a plain SQLException on the client side, which is exactly what
	// a DAO sees when a statement fails.
	for (const std::string& target : {std::string("players"), std::string("player_life_stats")}) {
		const std::string column = target == "players" ? "id" : "player_id";
		database.execute(schema, "DROP TRIGGER IF EXISTS aion_stress_fault_" + target);
		database.execute(schema, "CREATE TRIGGER aion_stress_fault_" + target + " BEFORE UPDATE ON " + target + " FOR EACH ROW BEGIN IF EXISTS "
		                             "(SELECT 1 FROM " + table + " f WHERE f.player_id = NEW." + column + ") THEN SIGNAL SQLSTATE '45000' SET "
		                             "MESSAGE_TEXT = '" + std::string(FAULT_MARKER) + "'; END IF; END");
	}
}

std::chrono::milliseconds StressRun::run() {
	const auto started = std::chrono::steady_clock::now();
	const auto deadline = started + options_.duration;
	std::vector<std::thread> threads;
	threads.reserve(static_cast<size_t>(options_.clients));
	for (int32_t i = 0; i < options_.clients; i++)
		threads.emplace_back([this, i, deadline] { Client(*this, clientStats[static_cast<size_t>(i)])(deadline); });
	for (std::thread& thread : threads)
		thread.join();
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
}

StressTotals StressRun::totals() const {
	StressTotals sum;
	for (const ClientStats& client : clientStats) {
		sum.connections += client.connections;
		sum.enters += client.enters;
		sum.logouts += client.logouts;
		sum.faultLogouts += client.faultLogouts;
		sum.entersBelowMaxHp += client.entersBelowMaxHp;
		sum.moves += client.moves;
		sum.failures += static_cast<int32_t>(client.failures.size());
	}
	return sum;
}

std::string StressRun::report() const {
	std::ostringstream text;
	text << "stress clients (connections / enters / logouts / fault logouts / enters below max HP / moves):\n";
	for (const ClientStats& client : clientStats) {
		text << "  " << client.characterName << " (" << client.account << ", id " << client.playerId << "): " << client.connections << " / "
		     << client.enters << " / " << client.logouts << " / " << client.faultLogouts << " / " << client.entersBelowMaxHp << " / "
		     << client.moves << "\n";
		for (const std::string& failure : client.failures)
			text << "      FAILED: " << failure << "\n";
	}
	const StressTotals sum = totals();
	text << "  total: " << sum.connections << " / " << sum.enters << " / " << sum.logouts << " / " << sum.faultLogouts << " / "
	     << sum.entersBelowMaxHp << " / " << sum.moves << ", " << sum.failures << " failure(s)\n";
	return text.str();
}

} // namespace aion::gameserver::scenario::stress
