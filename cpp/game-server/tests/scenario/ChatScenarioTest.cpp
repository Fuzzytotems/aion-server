// The M5j stage-0 chat gate (m5j-plan.md §10.3 as refreshed by §17.9, chat-server-port.md "Proposed three-server gate"; gs.scenario.chat).
// Three processes: the login server and the game server of the scenario harness, and the chat server (aion_chat_server) as a ChildProcess in a
// scratch copy of chat-server/ with its own mycs.properties (ports probed at run time, a test password, the chat log to the database) and its
// OWN schema per run, aion_cs_test_chat_<hash> on the server of AION_TEST_CS_DATABASE_URL, created from chat-server/sql/aion_cs.sql (§17.9: never
// aion_cs_test itself and never its named lock). Three Elyos accounts: G (access level 9, the gag of Y5), A and B (players, the chat of Y4).
//   Y1  "Gameserver #1 is now online" in the chat server log and "Connected to chat server" in the game server's, before the first client;
//   Y2  SM_VERSION_CHECK announces ONE chat server at 127.0.0.1 and the chat client port, and the channel-chat level byte is
//       gameserver.chatserver.min_level (10);
//   Y3  CM_CHAT_AUTH -> SM_CHAT_INIT with a 48-byte token; a fake chat client (chat-server/tests/support/FakePeers.h) authenticates with it;
//   Y4  A and B join one channel; A speaks; B (and A) receive exactly Java's SM_CHANNEL_MESSAGE; the chatlog row exists;
//   Y5  G's `//gag A 1 test` reaches the chat server as CM_PLAYER_GAG (its log line; D9: the duration is sent where a point in time is read);
//   Y6  the chat server is stopped and restarted within 5 s: the game server logs "Reconnecting to chat server in 5s..." and is online again
//       5 s after the drop; a second drop with the chat server down for 12 s: the connect at 5 s fails and the next comes 10 s later, online at
//       15 s; a new client's SM_VERSION_CHECK announces it again; no reconnect is attempted at the game server's shutdown;
//   Y7  logout -> the chat server closes the player's chat connection ("Player[id=...] logged out"); all three processes exit 0; no unported
//       hits, no ERROR line, the census clean.
//
// Every expectation is independent of the C++ server code: the game packets through tests/scenario's decoders (or the small SM_VERSION_CHECK
// decoder below, from its Java writeImpl), the chat packets through the chat server tests' builders, which are Java's byte layouts.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
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
#include "ScenarioDatabase.h"
#include "ScenarioServers.h"
#include "decoders/PacketDecoders.h"
#include "decoders/TravelDecoders.h"

#include "support/FakePeers.h" // chat-server/tests (m5j-plan.md I-03)

#include "../support/NetworkTestSupport.h" // the little endian PacketWriter

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {
namespace {

using namespace std::chrono_literals;
using decoders::DecodeError;
using Packet = GameSession::Packet;
namespace chat = ::aion::chatserver::test;

constexpr std::chrono::milliseconds QUIET = 1000ms;
constexpr std::chrono::milliseconds FIRST_REPLY_WAIT = 5000ms;
constexpr std::chrono::milliseconds BURST_LIMIT = 90s;

constexpr int32_t RESPONSE_OK = 0;
constexpr int32_t RESPONSE_OPEN_CREATION_WINDOW = 22;
constexpr uint8_t ENTER_WORLD_OK = 0;
/** SM_VERSION_CHECK.INTERNAL_VERSION (SM_VERSION_CHECK.java:33) */
constexpr uint16_t INTERNAL_VERSION = 207;
/** AionClientPacketFactory packets[174] (ClientPacketInfo.gen.inc:152) */
constexpr int32_t CM_CHAT_AUTH = 174;
/** the chat server's game server password of this gate (mycs.properties and gameserver.network.chat.password) */
constexpr std::string_view CS_PASSWORD = "chat-gate-secret";
/** config/main/gameserver.properties:26, gameserver.chatserver.min_level */
constexpr uint8_t CHANNEL_CHAT_LEVEL = 10;
/** Y6's slack on top of the reconnect delays (a Debug server on a loaded machine) */
constexpr std::chrono::milliseconds RECONNECT_SLACK = 4000ms;

// ---- small helpers ----------------------------------------------------------------------------------------------------------------------

std::string environmentValue(const char* name) {
	const char* value = std::getenv(name);
	return value == nullptr ? std::string() : std::string(value);
}

/**
 * The chat server's database settings, the variables chat-server/tests/support/ChatServerTestDatabase.h reads: AION_TEST_CS_DATABASE_URL (its
 * MariaDB server; the gate's own schema is created there), AION_TEST_CS_DATABASE_USER (default root), AION_TEST_CS_DATABASE_PASSWORD. That
 * header's schema and its named lock are not used (§17.9)
 */
std::string csDatabaseUrl() {
	return environmentValue("AION_TEST_CS_DATABASE_URL");
}

std::string csDatabaseUser() {
	const std::string user = environmentValue("AION_TEST_CS_DATABASE_USER");
	return user.empty() ? std::string("root") : user;
}

std::string csDatabasePassword() {
	return environmentValue("AION_TEST_CS_DATABASE_PASSWORD");
}

std::string join(const std::vector<std::string>& values, std::string_view separator = ", ") {
	std::string text;
	for (size_t i = 0; i < values.size(); i++)
		text += (i > 0 ? std::string(separator) : std::string()) + values[i];
	return text;
}

std::vector<std::string> namesOf(const std::vector<Packet>& packets) {
	std::vector<std::string> names;
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

std::vector<Packet> slice(const GameSession& session, size_t from) {
	const std::vector<Packet>& packets = session.recorded();
	return from >= packets.size() ? std::vector<Packet>{} : std::vector<Packet>(packets.begin() + static_cast<std::ptrdiff_t>(from), packets.end());
}

std::string readFile(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	std::stringstream content;
	content << in.rdbuf();
	return content.str();
}

// ---- SM_VERSION_CHECK, from its Java writeImpl --------------------------------------------------------------------------------------------

/** What Y2 reads of SM_VERSION_CHECK.writeImpl (SM_VERSION_CHECK.java): the channel-chat level and the announced chat servers */
struct VersionCheck {
	uint8_t answer = 0;
	uint8_t channelChatLevel = 0;
	uint16_t chatServerCount = 0;
	std::vector<uint8_t> chatAddress;
	uint16_t chatPort = 0;
};

VersionCheck decodeVersionCheck(std::span<const uint8_t> body) {
	decoders::BodyReader r(body, "SM_VERSION_CHECK");
	VersionCheck v;
	v.answer = r.C(); // answerID
	if (v.answer != 0)
		return v;     // a refused version writes nothing more
	r.C();            // serverId
	r.D();            // GSServBuildDate
	r.D();            // DBServBuildDate
	r.D();            // 0
	r.D();            // NPCServBuildDate
	r.D();            // start time
	r.C();            // 0
	r.C();            // country code
	r.C();            // 0
	r.C();            // ServerFlag
	r.D();            // PacketGenTimeOnServ
	r.H();            // skillPacketDelay
	r.C();            // enableClientPet
	r.C();            // minSendMailLevel
	r.C();            // minReceiveWhisperLevel
	r.C();            // minReceiveMailLevel
	v.channelChatLevel = r.C(); // GSConfig.CHAT_SERVER_MIN_LEVEL
	r.C();            // Trial_ChannelChatLevel
	r.C();            // Trial_Channelchatwritelevel1
	r.C();            // Trial_Channelchatwritelevel2
	r.H();            // MatchingCoolTimeSEC
	r.C();            // CHARACTER_REENTRY_TIME
	r.D();            // SceneStatus
	r.C();            // fatigueKoreaUse
	r.D();            // time zone offset
	r.C();            // MaxHousingChargePerid
	r.D();            // spawn_version
	r.C();            // DisposableItemTrade
	r.D();            // EnableNeutralChat
	r.D();            // updateServerAddr
	r.H();            // updateServerPort
	r.H();            // updateServerVersionCheckType
	r.C();            // ReduceSellPriceforGold
	r.C();            // RestrictWareandChargebyRank
	r.D();            // TimeDstBias
	r.C();            // SetDefaultAnimLength
	r.C();            // stonespear siege
	r.D();            // master server
	r.C();            // unk
	r.C();            // Atreian Passport menu
	for (int i = 0; i < 6; i++)
		r.C();        // the six 4.8 flags
	r.D();            // ITEM_WRAP_LIMIT
	for (int i = 0; i < 11; i++)
		r.D();        // the eleven rate modifiers (1000)
	r.C();            // augment
	r.F();            // 3.0f
	v.chatServerCount = r.H();
	if (v.chatServerCount > 0) {
		r.C();        // spacer
		v.chatAddress = r.B(4);
		v.chatPort = r.H();
	}
	r.expectFullyConsumed();
	return v;
}

// ---- the case log ----------------------------------------------------------------------------------------------------------------------

int32_t failedAssertions(bool fatalOnly = false) {
	const ::testing::TestResult* result = ::testing::UnitTest::GetInstance()->current_test_info()->result();
	int32_t failed = 0;
	for (int i = 0; i < result->total_part_count(); i++)
		if (fatalOnly ? result->GetTestPartResult(i).fatally_failed() : result->GetTestPartResult(i).failed())
			failed++;
	return failed;
}

class CaseLog {
public:
	bool run(std::string_view id, std::string_view title, const std::function<void()>& body) {
		Entry entry{std::string(id), std::string(title), true};
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
				entry.error = exception.what();
				ADD_FAILURE() << id << " (" << title << ") ended with an exception: " << exception.what();
			}
		}
		entry.duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
		entry.failed = failedAssertions() > failedBefore;
		entries.push_back(entry);
		return !threw && failedAssertions(true) == fatalBefore;
	}

	void skip(std::string_view id, std::string_view title, std::string_view reason) {
		entries.push_back(Entry{std::string(id), std::string(title), false, false, "not run: " + std::string(reason)});
	}

	std::string report(std::string_view testName) const {
		std::ostringstream text;
		text << testName << ", case by case:\n";
		for (const Entry& e : entries) {
			text << "  " << e.id << " " << e.title << ": ";
			if (!e.ran)
				text << "NOT RUN (" << e.error << ")";
			else
				text << (e.failed ? "FAILED" : "passed") << " in " << e.duration.count() << " ms" << (e.error.empty() ? "" : " [" + e.error + "]");
			text << "\n";
		}
		return text.str();
	}

private:
	struct Entry {
		std::string id, title;
		bool ran = false, failed = false;
		std::string error;
		std::chrono::milliseconds duration{0};
	};
	std::vector<Entry> entries;
};

// ---- packet stream helpers -------------------------------------------------------------------------------------------------------------

std::vector<Packet> collectBurst(GameSession& session, const AsyncAllowed& async) {
	std::vector<Packet> collected;
	const auto start = std::chrono::steady_clock::now();
	const auto deadline = start + BURST_LIMIT;
	auto lastAwaited = start;
	for (;;) {
		const auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
			break;
		const auto window = collected.empty() ? std::max(QUIET, FIRST_REPLY_WAIT) : QUIET;
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
	std::string password = "chatGatePassword1";
	std::string name;
	int32_t accessLevel = 0;
	std::unique_ptr<FakeLoginClient> login;
	std::unique_ptr<GameSession> game;
	FakeLoginClient::SessionKey key;
	int32_t playerId = 0;
	AsyncAllowed async = AsyncAllowed::m5aDefault();
	std::optional<VersionCheck> versionCheck;
	std::unique_ptr<chat::FakeChatClient> chatClient;

	size_t mark() const { return game ? game->recorded().size() : 0; }
	std::vector<Packet> since(size_t from) const { return game ? slice(*game, from) : std::vector<Packet>{}; }
	void say(std::string_view text) { game->send(GameSession::CM_CHAT_MESSAGE_PUBLIC, GameSession::buildGmCommand(text)); }
};

/** the login server conversation and the game server login up to SM_CHARACTER_LIST; SM_VERSION_CHECK is decoded on the way */
decoders::CharacterList logIn(ScenarioServers& servers, ScenarioClient& client) {
	client.login = std::make_unique<FakeLoginClient>(servers.loginClientPort());
	client.login->login(client.account, client.password);
	client.login->requestServerList();
	client.key = client.login->play(1);
	client.game = std::make_unique<GameSession>(servers.gameClientPort());
	client.async = AsyncAllowed::m5aDefault();
	client.game->readKey();
	// SM_VERSION_CHECK.java:70-77: a client version other than INTERNAL_VERSION (207, :33) is answered with answerID 1 and nothing more, so the
	// harness's default 206 would never see the chat server announcement Y2 asserts; this gate's clients send 207
	client.game->send(GameSession::CM_VERSION_CHECK, GameSession::buildCM_VERSION_CHECK(INTERNAL_VERSION));
	client.versionCheck = decodeVersionCheck(expectNext(*client.game, "SM_VERSION_CHECK", client.async).data);
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

void disconnect(ScenarioClient& client) {
	if (!client.game)
		return;
	client.game->send(GameSession::CM_QUIT, GameSession::buildCM_QUIT(false));
	waitFor(*client.game, "SM_QUIT_RESPONSE", 30s);
	if (!client.game->waitClosed(30s))
		throw std::runtime_error(client.label + ": the socket stayed open after CM_QUIT(0)");
	client.game.reset();
	client.login.reset();
}

void createCharacter(ScenarioServers& servers, ScenarioClient& client) {
	const decoders::CharacterList list = logIn(servers, client);
	EXPECT_EQ(list.characterCount, 0) << client.label;
	NewCharacter character;
	character.name = client.name;
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
}

std::vector<Packet> enterGame(ScenarioServers& servers, ScenarioClient& client) {
	logIn(servers, client);
	client.game->send(GameSession::CM_MAY_LOGIN_INTO_GAME, GameSession::buildCM_MAY_LOGIN_INTO_GAME());
	expectNext(*client.game, "SM_MAY_LOGIN_INTO_GAME", client.async);
	std::this_thread::sleep_for(1500ms);
	const size_t from = client.mark();
	client.async = AsyncAllowed::m5aDefault();
	client.async.selfPlayerState(client.playerId);
	client.game->send(GameSession::CM_ENTER_WORLD, GameSession::buildCM_ENTER_WORLD(client.playerId));
	std::vector<Packet> burst = collectBurst(*client.game, client.async);
	const Packet* check = firstOfName(burst, "SM_ENTER_WORLD_CHECK");
	if (check == nullptr || check->data.empty() || check->data[0] != ENTER_WORLD_OK)
		throw std::runtime_error(client.label + ": the enter world was refused: " + join(namesOf(burst)));
	client.game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
	collectBurst(*client.game, client.async);
	collectFor(*client.game, 500ms);
	return client.since(from);
}

/**
 * CM_CHAT_AUTH (CM_CHAT_AUTH.java: readD objectId, readB(6) mac) -> ChatServer.sendPlayerLoginRequest -> the chat server's token ->
 * SM_CHAT_INIT (SM_CHAT_INIT.java: writeD(token.length), writeB(token)). @return the token
 */
std::vector<uint8_t> chatToken(ScenarioClient& client) {
	client.game->send(CM_CHAT_AUTH, network::test::PacketWriter().D(client.playerId).B(std::vector<uint8_t>(6, 0)).data);
	const Packet init = waitFor(*client.game, "SM_CHAT_INIT");
	decoders::BodyReader r(init.data, "SM_CHAT_INIT");
	const int32_t length = r.D();
	std::vector<uint8_t> token = r.B(static_cast<size_t>(length));
	r.expectFullyConsumed();
	return token;
}

// ---- the chat server process -----------------------------------------------------------------------------------------------------------

struct ChatServerProcess {
	std::filesystem::path dir;
	uint16_t clientPort = 0;
	uint16_t gsPort = 0;
	std::unique_ptr<ChildProcess> process;
	int32_t runs = 0;

	std::filesystem::path stopFile() const { return dir / "cs.stop"; }

	/** a scratch copy of chat-server/config with this gate's mycs.properties (the operator's own never reaches the server) */
	void prepare(const std::filesystem::path& outputDir, const std::string& databaseUrl, const std::string& user, const std::string& password) {
		dir = outputDir / "chat_server";
		std::filesystem::remove_all(dir);
		std::filesystem::create_directories(dir);
		std::filesystem::copy(std::filesystem::path(AION_CHATSERVER_JAVA_DIR) / "config", dir / "config", std::filesystem::copy_options::recursive);
		std::filesystem::remove(dir / "config" / "mycs.properties");
		std::ofstream mycs(dir / "config" / "mycs.properties", std::ios::binary);
		mycs << "chatserver.network.client.socket_address = 127.0.0.1:" << clientPort << "\n"
		     << "chatserver.network.gameserver.socket_address = 127.0.0.1:" << gsPort << "\n"
		     << "chatserver.network.gameserver.password = " << CS_PASSWORD << "\n"
		     << "chatserver.log.chat = true\n"
		     << "chatserver.log.chat_to_db = true\n"
		     << "database.url = " << databaseUrl << "\n"
		     << "database.user = " << user << "\n"
		     << "database.password = " << password << "\n";
	}

	void start() {
		std::filesystem::remove(stopFile());
		ChildProcess::Options options;
		options.executable = AION_CHAT_SERVER_EXECUTABLE;
		options.arguments = {"--stop-file=" + stopFile().string()};
		options.workingDirectory = dir;
		options.logFile = dir / ("console_" + std::to_string(++runs) + ".log");
		process = std::make_unique<ChildProcess>(options);
		if (!process->waitForLog("Listening on 127.0.0.1:" + std::to_string(gsPort) + " for game servers", 60s))
			throw std::runtime_error("the chat server did not listen for game servers:\n" + process->readLogTail(4000));
	}

	/** the stop file; @return the exit code */
	std::optional<int32_t> stop() {
		if (!process)
			return std::nullopt;
		{
			std::ofstream stop(stopFile());
		}
		std::optional<int32_t> code = process->waitForExit(60s);
		return code;
	}
};

void finishRun(ScenarioServers& servers, const ScenarioDatabase* chatDatabase, const std::string& chatSchema, std::string_view testName) {
	for (const std::string& problem : servers.stopProblemsReported())
		ADD_FAILURE() << testName << ": " << problem;
	const bool failed = ::testing::Test::HasFailure();
	if (failed && ScenarioServers::keepSchemasOnFailure()) {
		std::cout << "the scenario schemas " << servers.gameSchema() << ", " << servers.loginSchema() << " and " << chatSchema
		          << " were kept for the post mortem (AION_SCENARIO_KEEP_SCHEMAS is set)" << std::endl;
		return;
	}
	try {
		servers.dropSchemas();
		if (chatDatabase != nullptr)
			chatDatabase->drop(chatSchema);
	} catch (const std::exception& exception) {
		std::cout << "the scenario schemas could not be dropped (" << exception.what() << ")" << std::endl;
	}
}

// ---- the gate ---------------------------------------------------------------------------------------------------------------------------

void runChatGate() {
	const std::string testName = "gs.scenario.chat";
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
	if (csDatabaseUrl().empty()) {
		unavailable("set AION_TEST_CS_DATABASE_URL (the chat server's schema is created on its MariaDB server)");
		return;
	}
	const std::filesystem::path outputDir = std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "chat";

	CaseLog cases;
	struct ReportPrinter {
		const CaseLog& cases;
		const std::string& testName;
		~ReportPrinter() { std::cout << cases.report(testName) << std::flush; }
	} printer{cases, testName};

	// the chat server: its ports, its own schema per run (§17.9) and its scratch directory
	const std::vector<uint16_t> ports = ScenarioServers::reservePorts(2);
	ChatServerProcess cs;
	cs.clientPort = ports[0];
	cs.gsPort = ports[1];

	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.schemaPrefix = "chat";
	config.gameServerProperties["gameserver.npcshouts.enable"] = "false";
	config.gameServerProperties["gameserver.rates.drop"] = "0";
	// §17.8's keys and the chat link of chat-server-port.md's gate step 1
	config.gameServerProperties["gameserver.chatserver.enable"] = "true";
	config.gameServerProperties["gameserver.chatserver.min_level"] = std::to_string(CHANNEL_CHAT_LEVEL);
	config.gameServerProperties["gameserver.network.chat.address"] = "127.0.0.1:" + std::to_string(cs.gsPort);
	config.gameServerProperties["gameserver.network.chat.password"] = std::string(CS_PASSWORD);
	config.gameServerProperties["gameserver.administration.login.execute_commands"] = "//invis, //invul, //enemy none, //see";
	config.gameServerProperties["gameserver.chat.factions.enable"] = "false";
	config.gameServerProperties["gameserver.simple.secondclass.enable"] = "false";
	config.startupTimeout = 10min;
	config.stopTimeout = 3min;
	ScenarioServers servers(config, *environment);

	const JdbcUrl csUrl = JdbcUrl::parse(csDatabaseUrl());
	const std::string chatSchema = "aion_cs_test_chat_" + schemaSuffix(outputDir.string());
	ScenarioDatabase chatDatabase(csDatabaseUrl(), csDatabaseUser(), csDatabasePassword());
	std::optional<SchemaLease> chatLease;

	bool ok = true;
	const auto runCase = [&](std::string_view id, std::string_view title, const std::function<void()>& body) {
		if (!ok)
			cases.skip(id, title, "an earlier case ended with an exception or a fatal failure");
		else
			ok = cases.run(id, title, body);
	};

	ScenarioClient g, a, b;
	const std::string suffix = servers.gameSchema().substr(servers.gameSchema().size() - 8);
	g.label = "G";
	g.account = "chg" + suffix;
	g.name = "Chatwarden";
	g.accessLevel = 9;
	a.label = "A";
	a.account = "cha" + suffix;
	a.name = "Chattera";
	b.label = "B";
	b.account = "chb" + suffix;
	b.name = "Chatterb";

	ok = cases.run("S-0", "the login server, the chat server on its own schema, then the game server start", [&] {
		servers.createSchemas();
		for (const std::string& dropped : chatDatabase.dropAbandonedSchemas("aion_cs_test_chat_", std::chrono::minutes(60)))
			std::cout << "dropped the abandoned chat schema " << dropped << std::endl;
		chatLease.emplace(chatDatabase.lease(chatSchema));
		chatDatabase.recreate(chatSchema, std::filesystem::path(AION_CHATSERVER_JAVA_DIR) / "sql" / "aion_cs.sql");
		cs.prepare(outputDir, csUrl.server + "/" + chatSchema + csUrl.query, csDatabaseUser(), csDatabasePassword());
		servers.startLoginServer();
		cs.start();
		servers.startGameServer();
	});

	runCase("Y1", "the link: \"Gameserver #1 is now online\" in the chat server log, \"Connected to chat server\" in the game server's", [&] {
		EXPECT_TRUE(cs.process->waitForLog("Gameserver #1 is now online", 60s)) << cs.process->readLogTail(4000);
		ASSERT_NE(servers.gameServer(), nullptr);
		EXPECT_TRUE(servers.gameServer()->waitForLog("Connected to chat server", 60s));
	});

	runCase("A0", "three Elyos characters; G's account gets access level 9 while logged out (H-02)", [&] {
		for (ScenarioClient* client : {&g, &a, &b})
			createCharacter(servers, *client);
		servers.loginDatabase().execute(servers.loginSchema(), "UPDATE account_data SET access_level = 9 WHERE name = '" + g.account + "'");
	});

	runCase("Y2", "SM_VERSION_CHECK announces one chat server at 127.0.0.1 and its client port; the channel-chat level is min_level (10)", [&] {
		enterGame(servers, a);
		ASSERT_TRUE(a.versionCheck);
		EXPECT_EQ(a.versionCheck->answer, 0);
		EXPECT_EQ(a.versionCheck->channelChatLevel, CHANNEL_CHAT_LEVEL) << "SM_VERSION_CHECK.java: writeC(GSConfig.CHAT_SERVER_MIN_LEVEL)";
		EXPECT_EQ(a.versionCheck->chatServerCount, 1);
		EXPECT_EQ(a.versionCheck->chatAddress, (std::vector<uint8_t>{127, 0, 0, 1}));
		EXPECT_EQ(a.versionCheck->chatPort, cs.clientPort) << "the chat server's connect address (chatserver.network.client.socket_address)";
	});

	runCase("Y3", "CM_CHAT_AUTH -> SM_CHAT_INIT with a 48-byte token; the fake chat client authenticates with it", [&] {
		const std::vector<uint8_t> token = chatToken(a);
		ASSERT_EQ(token.size(), 48u);
		a.chatClient = std::make_unique<chat::FakeChatClient>(cs.clientPort);
		// CM_PLAYER_AUTH: the identifier "<getName(true)>@AION" and the account name in lower case (chat-server-port.md gate step 5)
		a.chatClient->login(a.playerId, a.name + "@AION", a.account, token);
		EXPECT_FALSE(a.chatClient->socket.isClosed());
	});

	int32_t channelId = 0;
	runCase("Y4", "A and B join one channel; A speaks; both receive Java's SM_CHANNEL_MESSAGE; the chatlog row exists", [&] {
		enterGame(servers, b);
		const std::vector<uint8_t> token = chatToken(b);
		b.chatClient = std::make_unique<chat::FakeChatClient>(cs.clientPort);
		b.chatClient->login(b.playerId, b.name + "@AION", b.account, token);
		const std::string channel = chat::channelIdentifier("public", "poeta", 1, 0);
		channelId = a.chatClient->joinChannel(1, channel);
		EXPECT_EQ(b.chatClient->joinChannel(7, channel), channelId);
		a.chatClient->send(chat::FakeChatClient::buildChannelMessage(channelId, "Hello channel"));
		const chat::Bytes expected = chat::expectedChannelMessage(channelId, a.playerId, a.name + "@AION", "Hello channel");
		EXPECT_EQ(b.chatClient->expectFrame("SM_CHANNEL_MESSAGE"), expected) << chat::hex(expected);
		EXPECT_EQ(a.chatClient->expectFrame("SM_CHANNEL_MESSAGE"), expected) << "the sender is in the channel as well";
		std::optional<std::string> row;
		for (int i = 0; i < 20 && !row; i++) {
			row = chatDatabase.queryString(chatSchema, "SELECT CONCAT(sender, '|', message) FROM chatlog");
			if (!row)
				std::this_thread::sleep_for(250ms);
		}
		EXPECT_EQ(row.value_or("<none>"), a.name + "|Hello channel") << "chatserver.log.chat_to_db";
	});

	runCase("Y5", "G's //gag A 1 test reaches the chat server as CM_PLAYER_GAG (its log line)", [&] {
		enterGame(servers, g);
		g.say("//gag " + a.name + " 1 test");
		// ChatService.gagPlayer logs the minutes of what it received: 60000 ms, Gag.java's Duration.ofMinutes(1).toMillis() (D9: a duration)
		EXPECT_TRUE(cs.process->waitForLog("Player[id=" + std::to_string(a.playerId) + "] was gagged for 1 minutes", 15s))
		  << cs.process->readLogTail(4000);
		g.say("//gag " + a.name + " remove");
		EXPECT_TRUE(cs.process->waitForLog("Player[id=" + std::to_string(a.playerId) + "] was gagged for 0 minutes", 15s))
		  << "unbanPlayer sends gag time 0: " << cs.process->readLogTail(4000);
	});

	// ---- Y6: reconnects ----
	const auto dropAndMeasure = [&](std::chrono::milliseconds downFor) -> std::chrono::milliseconds {
		const std::optional<int32_t> exit = cs.stop();
		EXPECT_EQ(exit.value_or(-1), 0) << "the chat server's stop file";
		const auto dropped = std::chrono::steady_clock::now();
		std::this_thread::sleep_for(downFor);
		cs.start();
		if (!cs.process->waitForLog("Gameserver #1 is now online", 60s))
			throw std::runtime_error("the game server did not come back:\n" + cs.process->readLogTail(4000));
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - dropped);
	};
	runCase("Y6", "the chat server restarts within 5 s: reconnected after 5 s; down for 12 s: the 5 s connect fails, online at 15 s", [&] {
		a.chatClient.reset();
		b.chatClient.reset();
		const size_t reconnectsBefore = servers.gameServer()->findLogLines("Reconnecting to chat server in 5s...", 100).size();
		const std::chrono::milliseconds first = dropAndMeasure(1000ms);
		EXPECT_GE(first, 5000ms - 500ms) << "ChatServer.java:79-81: an authenticated link reconnects after 5 s";
		EXPECT_LE(first, 5000ms + RECONNECT_SLACK);
		EXPECT_EQ(servers.gameServer()->findLogLines("Reconnecting to chat server in 5s...", 100).size(), reconnectsBefore + 1);
		const std::chrono::milliseconds second = dropAndMeasure(12000ms);
		EXPECT_GE(second, 15000ms - 500ms) << "ChatServer.java:56-58: the connect at 5 s fails, the next attempt 10 s later";
		EXPECT_LE(second, 15000ms + RECONNECT_SLACK);
		EXPECT_FALSE(servers.gameServer()->findLogLines("trying again in 10s").empty()) << "the failed attempt names its 10 s retry";
		// a new client is told about the chat server again
		disconnect(b);
		enterGame(servers, b);
		ASSERT_TRUE(b.versionCheck);
		EXPECT_EQ(b.versionCheck->chatServerCount, 1);
		EXPECT_EQ(b.versionCheck->chatPort, cs.clientPort);
	});

	// ---- Y7: logout and shutdown ----
	runCase("Y7a", "B logs out: the chat server closes his chat connection (\"Player[id=...] logged out\")", [&] {
		const std::vector<uint8_t> token = chatToken(b);
		b.chatClient = std::make_unique<chat::FakeChatClient>(cs.clientPort);
		b.chatClient->login(b.playerId, b.name + "@AION", b.account, token);
		disconnect(b);
		EXPECT_TRUE(b.chatClient->socket.waitClosed()) << "CM_PLAYER_LOGOUT closes the player's chat connection";
		EXPECT_TRUE(cs.process->waitForLog("Player[id=" + std::to_string(b.playerId) + "] logged out", 15s)) << cs.process->readLogTail(4000);
	});
	cases.run("Y7b", "the others log out", [&] {
		disconnect(a);
		disconnect(g);
	});
	const size_t reconnectsAtStop = servers.gameServer() ? servers.gameServer()->findLogLines("Reconnecting to chat server", 100).size() : 0;
	const std::optional<int32_t> gameServerExit = servers.stopGameServer();
	const std::optional<int32_t> chatServerExit = cs.stop();
	const std::optional<int32_t> loginServerExit = servers.stopLoginServer();
	cases.run("Y7", "all three exit 0; no reconnect at the game server's shutdown; no unported hits, no ERROR, the census clean", [&] {
		ASSERT_TRUE(gameServerExit) << "the game server did not exit";
		EXPECT_EQ(*gameServerExit, 0);
		EXPECT_EQ(chatServerExit.value_or(-1), 0) << cs.process->readLogTail(4000);
		ASSERT_TRUE(loginServerExit);
		if (*loginServerExit != 98)
			EXPECT_EQ(*loginServerExit, 0);
		EXPECT_EQ(servers.gameServer()->findLogLines("Reconnecting to chat server", 100).size(), reconnectsAtStop)
		  << "ChatServerConnection.cpp: no reconnect once the shutdown is scheduled";
		const std::vector<std::string> unported = servers.readReportLines("unported_trace.txt");
		EXPECT_TRUE(unported.empty()) << "AION_UNPORTED sites were reached on the chat path:\n" << join(unported, "\n");
		EXPECT_TRUE(servers.readReportLines("census.txt").empty()) << join(servers.readReportLines("census.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("lockdep.txt").empty()) << join(servers.readReportLines("lockdep.txt"), "\n");
		const std::map<std::string, std::vector<std::string>> summary = servers.readSummary();
		const auto value = [&](std::string_view key) -> std::string {
			const auto found = summary.find(std::string(key));
			return found == summary.end() || found->second.empty() ? std::string() : found->second[0];
		};
		EXPECT_EQ(value("liveLeaks"), "0") << join(summary.contains("liveLeak") ? summary.at("liveLeak") : std::vector<std::string>{});
		std::vector<std::string> errors;
		for (const std::string& line : servers.gameServer()->findLogLines(" ERROR "))
			errors.push_back("game server: " + line);
		for (int32_t run = 1; run <= cs.runs; run++) {
			std::istringstream log(readFile(cs.dir / ("console_" + std::to_string(run) + ".log")));
			for (std::string line; std::getline(log, line);)
				if (line.find("ERROR") != std::string::npos)
					errors.push_back("chat server run " + std::to_string(run) + ": " + line);
		}
		EXPECT_TRUE(errors.empty()) << "ERROR lines:\n" << join(errors, "\n");
	});

	chatLease.reset();
	finishRun(servers, &chatDatabase, chatSchema, testName);
}

} // namespace

/** `gs.scenario.chat` (m5j-plan.md §10.3, §17.9): the game server's chat server link end to end with a real chat server */
TEST(ChatScenario, Run) {
	runChatGate();
}

} // namespace aion::gameserver::scenario
