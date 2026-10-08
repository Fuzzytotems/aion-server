// The M5j stage-0 GM gate (m5j-plan.md §10.2 as refreshed by §17.8; gs.scenario.gm). One login server and one game server as child processes
// on their own test schemas, three Elyos accounts on Poeta online at once:
//   G, access level 9 (seeded into the login schema's account_data before G's first enter world, H-02), the GM;
//   L, access level 1, staff below most commands;
//   P, access level 0, a player.
// The cases (the §10.2 ids where a row is that row, narrowed where §17.8 or this file says so):
//   X1  G enters the world with Java's default login.execute_commands (//invis, //invul, //enemy none, //see): the four commands answer;
//   X3  L's `//kill`: the access text with commands.properties' level 7;
//   X4  P's `//kill`: not a command for a player, so it goes out as NORMAL chat (G reads it) and P is told nothing (AdminCommand.java:47-48);
//   X5  the race byte of the chat line: 0 for G (staff), 1 for P's own echo (SM_MESSAGE.java:140);
//   X6  P whispers G (staff: P's level does not matter) and a name nobody has (STR_NO_SUCH_USER);
//   X7  G gags P: the ban texts, P's line refused with the minutes left and not delivered; `remove` lifts it and P's next line arrives;
//   X8  one command of each stage-0 family against the real services: info (//online), talking (//announce), console (CM_BUILDER_COMMAND
//       `levelup 1`, and a name no console command has), character (//addexp), player (.gmlist from P), monsters (//spawn 210663, then
//       //kill on it);
//   X10 the reports (the M5a Q8 bar), with no Player left alive after the three logouts.
//   X2  every stage-0 command's `help`: the SM_MESSAGE parts equal `oracle.py m5j-commands`' rendering after ChatUtil.split (H-01).
// Rows of §10.2 not run here: X8b (//addtitle, //delete) and X8c, X9's `levelup` is X8's console row, X11 (K-11's riders, part 0.2). The
// help texts and X3's access level come from the oracle (H-01); every other text below is a Java literal or a Java computation of the class
// named beside it, never the port's.
//
// Every expectation is independent of the C++ server code, as in the earlier gates: server packets are read with the decoders of
// tests/scenario/decoders (written from the Java writeImpl methods, m5a-plan.md D9).
//
// This file does not share the other gates' helpers, for the reason M5cScenarioTest.cpp gives: each gate owns its pair of server processes
// and its helpers live in an anonymous namespace. What is duplicated is scaffolding (the case log, the burst collector, the login
// conversation, the report readers), never an assertion.

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

#include <nlohmann/json.hpp>

#include "AsyncAllowed.h"
#include "ChildProcess.h"
#include "FakeLoginClient.h"
#include "GameSession.h"
#include "Oracle.h"
#include "ScenarioDatabase.h"
#include "ScenarioServers.h"
#include "decoders/CombatDecoders.h"
#include "decoders/PacketDecoders.h"
#include "decoders/ProgressionDecoders.h"
#include "decoders/QuestDecoders.h"
#include "decoders/TravelDecoders.h"

#include "aion/commons/utils/StringUtils.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {
namespace {

using namespace std::chrono_literals;
using decoders::DecodeError;
using Packet = GameSession::Packet;

constexpr std::chrono::milliseconds QUIET = 1000ms;
/** How long collectBurst waits for the FIRST packet of an answer (TravelScenarioTest.cpp explains the constant) */
constexpr std::chrono::milliseconds FIRST_REPLY_WAIT = 5000ms;
constexpr std::chrono::milliseconds BURST_LIMIT = 90s;

constexpr int32_t RESPONSE_OK = 0;
constexpr int32_t RESPONSE_OPEN_CREATION_WINDOW = 22;
constexpr uint8_t ENTER_WORLD_OK = 0;

/** ChatType ids (ChatType.java:13, :16, :42, :53) */
constexpr uint8_t CHAT_NORMAL = 0;
constexpr uint8_t CHAT_WHISPER = 4;
constexpr uint8_t CHAT_GOLDEN_YELLOW = 25;
constexpr uint8_t CHAT_BRIGHT_YELLOW_CENTER = 36;

/** SM_SYSTEM_MESSAGE ids (SM_SYSTEM_MESSAGE.java: STR_SKILL_EFFECT_INVISIBLE_BEGIN :237-238, STR_NO_SUCH_USER :12191-12192,
 *  STR_LIST_USER(String) :12296-12297, STR_CAN_CHAT_NOW :12317-12318, STR_INGAME_BLOCK_ENABLE_NO_CHAT :13451-13452,
 *  STR_INGAME_BLOCK_IN_NO_CHAT :13493-13494) */
constexpr int32_t STR_SKILL_EFFECT_INVISIBLE_BEGIN = 1200232;
constexpr int32_t STR_NO_SUCH_USER = 1300627;
constexpr int32_t STR_LIST_USER = 1300641;
constexpr int32_t STR_CAN_CHAT_NOW = 1300644;
constexpr int32_t STR_INGAME_BLOCK_ENABLE_NO_CHAT = 1300808;
constexpr int32_t STR_INGAME_BLOCK_IN_NO_CHAT = 1300814;

/**
 * The stage-0 command set (m5j-plan.md §17.4, the 41 commands the C++ registers with AION_ADMIN_COMMAND, AION_PLAYER_COMMAND and
 * AION_CONSOLE_COMMAND): X2 asks each for its help through the channel a client uses for it
 */
constexpr std::string_view STAGE0_ADMIN[] = {"addexp", "addskill", "addtitle", "ai", "announce", "coords", "damage", "delskill", "delete",
	"dispel", "enemy", "gag", "heal", "info", "invis", "invul", "kick", "kill", "morph", "movie", "npcskill", "online", "removecd", "say", "see",
	"set", "spawn", "speed", "stat", "state", "time", "useskill", "weather", "whisper", "zone"};
constexpr std::string_view STAGE0_PLAYER[] = {"gmlist", "help", "id"};
constexpr std::string_view STAGE0_CONSOLE[] = {"clearusercoolt", "leveldown", "levelup"};
/** the juvenile sparkie of npc_templates.xml:57809 (level 2): no REWARD_AP, so the kill reaches no unported AP variant (m5j-plan.md §17.8 X8) */
constexpr int32_t SPARKIE = 210663;

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

/** every SM_MESSAGE of the packets, decoded */
std::vector<decoders::Message> messagesIn(const std::vector<Packet>& packets) {
	std::vector<decoders::Message> messages;
	for (const Packet& packet : packets)
		if (packet.name == "SM_MESSAGE")
			messages.push_back(decoders::decodeMessage(packet.data));
	return messages;
}

/** every SM_SYSTEM_MESSAGE of the packets, decoded */
std::vector<decoders::SystemMessage> systemMessagesIn(const std::vector<Packet>& packets) {
	std::vector<decoders::SystemMessage> messages;
	for (const Packet& packet : packets)
		if (packet.name == "SM_SYSTEM_MESSAGE")
			messages.push_back(decoders::decodeSystemMessage(packet.data));
	return messages;
}

/** the texts of PacketSendUtility.sendMessage(player, text) (SM_MESSAGE(0, null, text, GOLDEN_YELLOW), PacketSendUtility.java:27-29) */
std::vector<std::string> infoTexts(const std::vector<Packet>& packets) {
	std::vector<std::string> texts;
	for (const decoders::Message& message : messagesIn(packets))
		if (message.chatType == CHAT_GOLDEN_YELLOW && message.senderObjectId == 0 && message.senderName.empty())
			texts.push_back(message.message);
	return texts;
}

bool contains(const std::vector<std::string>& values, std::string_view value) {
	return std::find(values.begin(), values.end(), value) != values.end();
}

std::optional<decoders::SystemMessage> systemMessage(const std::vector<Packet>& packets, int32_t messageId) {
	for (const decoders::SystemMessage& message : systemMessagesIn(packets))
		if (message.messageId == messageId)
			return message;
	return std::nullopt;
}

/**
 * Java ChatUtil.l10n (ChatUtil.java:96-102): "$" followed by the two chars (id << 1 | 1) & 0xFFFF and (id << 1 | 1) >>> 16, as the client reads
 * them from writeS (UTF-16), here in the UTF-8 BodyReader.S() returns
 */
std::string l10n(int32_t l10nId) {
	const int32_t id = l10nId << 1 | 1;
	std::u16string text = u"$";
	text += static_cast<char16_t>(id & 0xFFFF);
	text += static_cast<char16_t>((static_cast<uint32_t>(id) >> 16) & 0xFFFF);
	return commons::utils::StringUtils::toUtf8(text);
}

/**
 * Java ChatUtil.charName (ChatUtil.java:87-89) of a player whose Player.getName(true) (Player.java:248-255) is `name` with the custom tag of
 * his access level: gameserver.administration.customtags (admin.properties:18, the shipped default) is "%s" for level 1 and
 * "\u00BBAdmin\u00AB\uE04A%s" for level 9; a player of level 0 has no tag
 */
std::string charName(std::string_view name, int32_t accessLevel = 0) {
	std::string tagged(name);
	if (accessLevel == 9)
		tagged = commons::utils::StringUtils::toUtf8(std::u16string(u"\u00BBAdmin\u00AB\uE04A")) + tagged;
	return "[charname:" + tagged + ";1 1 1]";
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

/** Reads until a decoded SM_MESSAGE satisfies `wanted`, recording everything on the way. @return it, or nullopt after `timeout` */
std::optional<decoders::Message> waitForMessage(GameSession& session, const std::function<bool(const decoders::Message&)>& wanted,
	std::chrono::milliseconds timeout = 10s) {
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
		if (packet->name != "SM_MESSAGE")
			continue;
		const decoders::Message message = decoders::decodeMessage(packet->data);
		if (wanted(message))
			return message;
	}
}

/** Reads until an SM_SYSTEM_MESSAGE of that id arrives, recording everything on the way. @return it, or nullopt after `timeout` */
std::optional<decoders::SystemMessage> waitForSystemMessage(GameSession& session, int32_t messageId, std::chrono::milliseconds timeout = 10s) {
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
		if (packet->name != "SM_SYSTEM_MESSAGE")
			continue;
		const decoders::SystemMessage message = decoders::decodeSystemMessage(packet->data);
		if (message.messageId == messageId)
			return message;
	}
}

// ---- the clients ----------------------------------------------------------------------------------------------------------------------

struct ScenarioClient {
	std::string label;
	std::string account;
	std::string password = "gmGatePassword1";
	std::string name;
	int32_t accessLevel = 0;
	std::unique_ptr<FakeLoginClient> login;
	std::unique_ptr<GameSession> game;
	FakeLoginClient::SessionKey key;
	int32_t playerId = 0;
	AnnouncedNpcs npcs;
	AsyncAllowed async = AsyncAllowed::m5aDefault();
	float x = 0, y = 0, z = 0;
	int32_t worldId = 0;

	size_t mark() const { return game ? game->recorded().size() : 0; }
	std::vector<Packet> since(size_t from) const { return game ? slice(*game, from) : std::vector<Packet>{}; }
	/** a chat line or a typed chat command: CM_CHAT_MESSAGE_PUBLIC NORMAL */
	void say(std::string_view text) { game->send(GameSession::CM_CHAT_MESSAGE_PUBLIC, GameSession::buildGmCommand(text)); }
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

/** CM_QUIT(0): the character leaves the world (if it is in one) and the connection ends */
void disconnect(ScenarioClient& client) {
	if (!client.game)
		return;
	client.game->send(GameSession::CM_QUIT, GameSession::buildCM_QUIT(false));
	waitFor(*client.game, "SM_QUIT_RESPONSE", 30s);
	if (!client.game->waitClosed(30s))
		throw std::runtime_error(client.label + ": the socket stayed open after CM_QUIT(0)");
	client.npcs.follow(nullptr);
	client.game.reset();
	client.login.reset();
}

/** a fresh account's first login: an Elyos Warrior is created on Poeta, and the account logs out again */
void createCharacter(ScenarioServers& servers, ScenarioClient& client) {
	const decoders::CharacterList list = logIn(servers, client);
	EXPECT_EQ(list.characterCount, 0) << client.label << ": a fresh account must have no character";
	NewCharacter character;
	character.name = client.name;
	character.asmodian = false;
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

/**
 * H-02 (m5j-plan.md §17.6): the account's access level, written while no client of it is connected. Gate accounts are created at their
 * first login (loginserver.accounts.autocreate, ScenarioServers.cpp), so the row exists after createCharacter; the login server reads
 * account_data.access_level at the next login and hands it to the game server with the account (login-server/sql/aion_ls.sql:12)
 */
void seedAccessLevel(const ScenarioServers& servers, const ScenarioClient& client) {
	servers.loginDatabase().execute(servers.loginSchema(), "UPDATE account_data SET access_level = " + std::to_string(client.accessLevel) +
	                                                         " WHERE name = '" + client.account + "'");
	const std::optional<int64_t> stored = servers.loginDatabase().queryLong(
		servers.loginSchema(), "SELECT access_level FROM account_data WHERE name = '" + client.account + "'");
	if (!stored || *stored != client.accessLevel)
		throw std::runtime_error(client.label + ": account_data.access_level of " + client.account + " is not " + std::to_string(client.accessLevel));
}

/** a new login, the character into the world and CM_LEVEL_READY. @return everything from CM_ENTER_WORLD to a quiet second after it */
std::vector<Packet> enterGame(ScenarioServers& servers, ScenarioClient& client) {
	logIn(servers, client);
	client.game->send(GameSession::CM_MAY_LOGIN_INTO_GAME, GameSession::buildCM_MAY_LOGIN_INTO_GAME());
	expectNext(*client.game, "SM_MAY_LOGIN_INTO_GAME", client.async);
	std::this_thread::sleep_for(1500ms);
	const size_t from = client.mark();
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
	client.game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
	collectBurst(*client.game, client.async);
	collectFor(*client.game, 1000ms);
	return client.since(from);
}

/** the object id of an SM_NPC_INFO of `templateId` within `range` m of (x, y) recorded since `from` */
std::optional<int32_t> npcNear(const ScenarioClient& client, size_t from, int32_t templateId, float x, float y, double range) {
	for (const Packet& packet : client.since(from)) {
		if (packet.name != "SM_NPC_INFO")
			continue;
		try {
			const decoders::NpcInfo npc = decoders::decodeNpcInfo(packet.data);
			if (npc.templateId == templateId && distance2d(npc.x, npc.y, x, y) <= range)
				return npc.objectId;
		} catch (const DecodeError&) {
		}
	}
	return std::nullopt;
}

// ---- the reports -----------------------------------------------------------------------------------------------------------------------

enum class AllowlistSection { HitAtLeastOnce, HitNever, NotPinned };

struct AllowlistEntry {
	std::string site;
	AllowlistSection section = AllowlistSection::NotPinned;
};

/** Reads tests/scenario/gm_partial_allowlist.txt with its three sections ("# --- SECTION A/B/C" marker lines, as the M5c list) */
std::vector<AllowlistEntry> readAllowlist() {
	std::vector<AllowlistEntry> entries;
	std::ifstream in(AION_SCENARIO_GM_PARTIAL_ALLOWLIST, std::ios::binary);
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

// ---- the gate ---------------------------------------------------------------------------------------------------------------------------

void runGmGate() {
	const std::string testName = "gs.scenario.gm";
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
	const std::filesystem::path outputDir = std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "gm";
	std::optional<Oracle> oracle = Oracle::fromEnvironment(outputDir / "oracle");
	if (!oracle) {
		if (required)
			ADD_FAILURE() << testName << " was not configured and AION_SCENARIO_REQUIRE is set: no Python interpreter for tools/oracle (AION_TEST_PYTHON)";
		else
			GTEST_SKIP() << testName << ": skipped (no Python interpreter for tools/oracle: set AION_TEST_PYTHON)";
		return;
	}
	// H-01: the commands' levels, aliases, help parts and access texts as the Java builds them
	std::vector<std::string> oracleArguments{"m5j-commands", "--alias"};
	for (std::string_view alias : STAGE0_ADMIN)
		oracleArguments.push_back("//" + std::string(alias));
	for (std::string_view alias : STAGE0_PLAYER)
		oracleArguments.push_back("." + std::string(alias));
	for (std::string_view alias : STAGE0_CONSOLE)
		oracleArguments.emplace_back(alias);
	const nlohmann::json commandsReport = nlohmann::json::parse(oracle->run(oracleArguments));
	const nlohmann::json& commands = commandsReport.at("commands");

	CaseLog cases;
	struct ReportPrinter {
		const CaseLog& cases;
		const std::string& testName;
		~ReportPrinter() { std::cout << cases.report(testName) << std::flush; }
	} printer{cases, testName};

	// the M5a profile with the keys §17.8 pins: every assertion below reads one of them or their Java default
	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.schemaPrefix = "gm";
	config.gameServerProperties["gameserver.npcshouts.enable"] = "false";
	config.gameServerProperties["gameserver.rates.drop"] = "0";
	// admin.properties:101 and :105, custom.properties:16, :20 and :32 (their Java defaults), the chat server off (X7 checks the game server's ban)
	config.gameServerProperties["gameserver.administration.login.execute_commands"] = "//invis, //invul, //enemy none, //see";
	config.gameServerProperties["gameserver.administration.login.print_revision"] = "9";
	config.gameServerProperties["gameserver.chat.factions.enable"] = "false";
	config.gameServerProperties["gameserver.chat.whisper.level"] = "10";
	config.gameServerProperties["gameserver.chatserver.enable"] = "false";
	config.gameServerProperties["gameserver.simple.secondclass.enable"] = "false";
	config.startupTimeout = 10min;
	config.stopTimeout = 3min;
	ScenarioServers servers(config, *environment);

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

	ScenarioClient g, l, p;
	const std::string suffix = servers.gameSchema().substr(servers.gameSchema().size() - 8);
	g.label = "G";
	g.account = "gmg" + suffix;
	g.name = "Wardeng";
	g.accessLevel = 9;
	l.label = "L";
	l.account = "gml" + suffix;
	l.name = "Helperl";
	l.accessLevel = 1;
	p.label = "P";
	p.account = "gmp" + suffix;
	p.name = "Plainp";
	p.accessLevel = 0;

	runCase("A0", "three Elyos characters are created; G's and L's accounts get access levels 9 and 1 while they are logged out (H-02)", [&] {
		for (ScenarioClient* client : {&g, &l, &p})
			createCharacter(servers, *client);
		seedAccessLevel(servers, g);
		seedAccessLevel(servers, l);
	});

	// ---- X1: the login commands ----
	runCase("X1", "G enters the world: the default login.execute_commands answer (//invis, //invul, //enemy none, //see)", [&] {
		const std::vector<Packet> burst = enterGame(servers, g);
		const std::vector<std::string> texts = infoTexts(burst);
		// Invul.java:25 sendInfo(l10n(293440)), Enemy.java:50 "You are now neutral to everyone.", See.java:23 sendInfo(l10n(288645))
		EXPECT_TRUE(contains(texts, l10n(293440))) << "//invul: " << join(texts, " | ");
		EXPECT_TRUE(contains(texts, "You are now neutral to everyone.")) << "//enemy none: " << join(texts, " | ");
		EXPECT_TRUE(contains(texts, l10n(288645))) << "//see: " << join(texts, " | ");
		// Invis.java:29: STR_SKILL_EFFECT_INVISIBLE_BEGIN; with SM_PLAYER_STATE broadcast to himself
		EXPECT_TRUE(systemMessage(burst, STR_SKILL_EFFECT_INVISIBLE_BEGIN)) << "//invis: " << join(namesOf(burst));
		bool ownState = false;
		for (const Packet& packet : burst)
			if (packet.name == "SM_PLAYER_STATE" && decoders::decodePlayerStateObjectId(packet.data) == g.playerId)
				ownState = true;
		EXPECT_TRUE(ownState) << "//invis broadcasts SM_PLAYER_STATE to himself (toSelf)";
		for (const std::string& text : texts)
			EXPECT_FALSE(text.starts_with("<Error while executing command>")) << "a login command failed: " << text;
	});

	// ---- X2: every stage-0 command's help ----
	runCase("X2", "G asks every stage-0 command for `help`: the messages are the oracle's parts after ChatUtil.split", [&] {
		const auto ask = [&](const std::string& aliasWithPrefix, const std::function<void()>& send) {
			const nlohmann::json& command = commands.at(aliasWithPrefix);
			std::vector<std::string> expected;
			for (const auto& part : command.at("help"))
				expected.push_back(part.get<std::string>());
			ASSERT_FALSE(expected.empty()) << aliasWithPrefix;
			const size_t from = g.mark();
			send();
			// the parts are GOLDEN_YELLOW sendMessage texts in order; other messages (the login announcement) may be interleaved
			const auto deadline = std::chrono::steady_clock::now() + 10s;
			std::vector<std::string> got;
			while (std::chrono::steady_clock::now() < deadline) {
				got.clear();
				bool started = false;
				for (const std::string& text : infoTexts(g.since(from))) {
					if (!started && !text.starts_with("Command: "))
						continue;
					started = true;
					got.push_back(text);
				}
				if (got.size() >= expected.size())
					break;
				collectFor(*g.game, 200ms);
			}
			got.resize(std::min(got.size(), expected.size()));
			EXPECT_EQ(got, expected) << command.at("aliasWithPrefix").get<std::string>() << " help (" << command.at("javaFile").get<std::string>() << ")";
		};
		for (std::string_view alias : STAGE0_ADMIN)
			ask("//" + std::string(alias), [&] { g.say("//" + std::string(alias) + " help"); });
		for (std::string_view alias : STAGE0_PLAYER)
			ask("." + std::string(alias), [&] { g.say("." + std::string(alias) + " help"); });
		for (std::string_view alias : STAGE0_CONSOLE)
			ask(std::string(alias), [&] { g.game->send(GameSession::CM_BUILDER_COMMAND, GameSession::buildCM_BUILDER_COMMAND(std::string(alias) + " help")); });
	});

	// ---- X3: an access level below the command's ----
	runCase("X3", "L (access level 1) types //kill: the access text with kill's level 7, nothing else happens", [&] {
		enterGame(servers, l);
		const size_t from = l.mark();
		l.say("//kill");
		// AdminCommand.java:40-41 with commands.properties' level of kill, both from the oracle (H-01)
		const std::string expected = commands.at("//kill").at("accessMessage").get<std::string>();
		const std::optional<decoders::Message> answer = waitForMessage(*l.game, [&](const decoders::Message& m) {
			return m.chatType == CHAT_GOLDEN_YELLOW && m.senderObjectId == 0 && m.message == expected;
		});
		EXPECT_TRUE(answer) << "no \"" << expected << "\": " << join(infoTexts(l.since(from)), " | ");
		collectFor(*l.game, 1000ms);
		for (const decoders::Message& message : messagesIn(l.since(from)))
			EXPECT_FALSE(message.chatType == CHAT_NORMAL && message.message == "//kill") << "a staff member's refused command is no chat line";
		// the command did not run: //kill without a target answers its syntax (Kill.java:32-34, sendInfo(player) -> "Syntax:...")
		for (const std::string& text : infoTexts(l.since(from)))
			EXPECT_FALSE(text.starts_with("Syntax:")) << "//kill ran for L (AdminCommand.process went past validateAccess): " << text;
	});

	// ---- X4 / X5: a player's command is a chat line ----
	runCase("X4", "P (access level 0) types //kill: it goes out as NORMAL chat; G reads it with race byte 0, P's echo has 1; P is told nothing", [&] {
		enterGame(servers, p);
		const size_t gFrom = g.mark();
		const size_t pFrom = p.mark();
		p.say("//kill");
		const std::optional<decoders::Message> echo = waitForMessage(*p.game, [&](const decoders::Message& m) {
			return m.chatType == CHAT_NORMAL && m.senderObjectId == p.playerId;
		});
		ASSERT_TRUE(echo) << "P's own line did not come back: " << join(namesOf(p.since(pFrom)));
		EXPECT_EQ(echo->message, "//kill");
		EXPECT_EQ(echo->senderName, p.name);
		EXPECT_EQ(echo->senderRace, 1) << "SM_MESSAGE.java:140: a non-staff reader gets Race.ELYOS id 0 + 1";
		const std::optional<decoders::Message> heard = waitForMessage(*g.game, [&](const decoders::Message& m) {
			return m.chatType == CHAT_NORMAL && m.senderObjectId == p.playerId;
		});
		ASSERT_TRUE(heard) << "G did not read P's line: " << join(namesOf(g.since(gFrom)));
		EXPECT_EQ(heard->message, "//kill");
		EXPECT_EQ(heard->senderRace, 0) << "SM_MESSAGE.java:140: a staff reader gets 0";
		collectFor(*p.game, 500ms);
		for (const std::string& text : infoTexts(p.since(pFrom)))
			EXPECT_FALSE(text.starts_with("<You need access level")) << "a player cannot probe for commands (AdminCommand.java:47-48)";
	});

	// ---- X6: whispers ----
	runCase("X6", "P whispers G (staff: P's level 1 is no obstacle) and a name nobody has (STR_NO_SUCH_USER)", [&] {
		const size_t gFrom = g.mark();
		p.game->send(GameSession::CM_CHAT_MESSAGE_WHISPER, GameSession::buildCM_CHAT_MESSAGE_WHISPER(g.name, "psst"));
		const std::optional<decoders::Message> whisper = waitForMessage(*g.game, [&](const decoders::Message& m) {
			return m.chatType == CHAT_WHISPER && m.senderObjectId == p.playerId;
		});
		ASSERT_TRUE(whisper) << "G was whispered nothing: " << join(namesOf(g.since(gFrom)));
		EXPECT_EQ(whisper->message, "psst");
		p.game->send(GameSession::CM_CHAT_MESSAGE_WHISPER, GameSession::buildCM_CHAT_MESSAGE_WHISPER("Nobodyhere", "psst"));
		const std::optional<decoders::SystemMessage> noSuchUser = waitForSystemMessage(*p.game, STR_NO_SUCH_USER);
		ASSERT_TRUE(noSuchUser) << "no STR_NO_SUCH_USER";
		EXPECT_EQ(noSuchUser->params, std::vector<std::string>{"Nobodyhere"});
	});

	// ---- X7: the gag ----
	runCase("X7", "G gags P for 1 minute: P is told so and his line is refused with 1 minute left; `remove` lifts it and his next line arrives", [&] {
		const size_t pFrom = p.mark();
		g.say("//gag " + p.name + " 1 test");
		const std::optional<decoders::SystemMessage> enabled = waitForSystemMessage(*p.game, STR_INGAME_BLOCK_ENABLE_NO_CHAT);
		ASSERT_TRUE(enabled) << "Gag.java:56: no STR_INGAME_BLOCK_ENABLE_NO_CHAT for P: " << join(namesOf(p.since(pFrom)));
		EXPECT_EQ(enabled->params, std::vector<std::string>{"1"});
		EXPECT_TRUE(waitForMessage(*p.game, [](const decoders::Message& m) { return m.chatType == CHAT_GOLDEN_YELLOW && m.message == "test"; }))
			<< "Gag.java:57: sendInfo(player, reason)";
		const std::string gagged = charName(p.name) + " is now gagged for 1 minute(s).";
		EXPECT_TRUE(waitForMessage(*g.game, [&](const decoders::Message& m) { return m.chatType == CHAT_GOLDEN_YELLOW && m.message == gagged; }))
			<< "Gag.java:58";

		const size_t gMuted = g.mark();
		p.say("let me speak");
		const std::optional<decoders::SystemMessage> refused = waitForSystemMessage(*p.game, STR_INGAME_BLOCK_IN_NO_CHAT);
		ASSERT_TRUE(refused) << "PlayerRestrictions.canChat: no STR_INGAME_BLOCK_IN_NO_CHAT";
		EXPECT_EQ(refused->params, std::vector<std::string>{"1"}) << "ChatBanService.getBanMinutes: ceil(millis left / 60000)";
		collectFor(*g.game, 1500ms);
		for (const decoders::Message& message : messagesIn(g.since(gMuted)))
			EXPECT_NE(message.senderObjectId, p.playerId) << "a gagged line reached G: " << message.message;

		g.say("//gag " + p.name + " remove");
		EXPECT_TRUE(waitForSystemMessage(*p.game, STR_CAN_CHAT_NOW)) << "ChatBanService.unbanPlayer: STR_CAN_CHAT_NOW";
		const std::string unbanned = "Unbanned " + charName(p.name) + " from all chats.";
		EXPECT_TRUE(waitForMessage(*g.game, [&](const decoders::Message& m) { return m.chatType == CHAT_GOLDEN_YELLOW && m.message == unbanned; }))
			<< "Gag.java:40";
		p.say("free again");
		EXPECT_TRUE(waitForMessage(*g.game, [&](const decoders::Message& m) {
			return m.chatType == CHAT_NORMAL && m.senderObjectId == p.playerId && m.message == "free again";
		})) << "the line after the unban reaches G";
	});

	// ---- X8: one command of each family ----
	runCase("X8a", "info: //online counts the three Elyos (STR_LIST_USER)", [&] {
		g.say("//online");
		const std::optional<decoders::SystemMessage> listed = waitForSystemMessage(*g.game, STR_LIST_USER);
		ASSERT_TRUE(listed) << "no STR_LIST_USER";
		EXPECT_EQ(listed->params, std::vector<std::string>{"3 (3 Elyos / 0 Asmos)"}) << "Online.java:31-32";
	});

	runCase("X8b", "talking: //announce a reaches all three as a BRIGHT_YELLOW_CENTER notice", [&] {
		g.say("//announce a Hello all");
		for (ScenarioClient* client : {&g, &l, &p})
			EXPECT_TRUE(waitForMessage(*client->game, [](const decoders::Message& m) {
				return m.chatType == CHAT_BRIGHT_YELLOW_CENTER && m.senderObjectId == 0 && m.message == "Announce: Hello all";
			})) << client->label << " read no \"Announce: Hello all\" (Announce.java:35, :47-49)";
	});

	runCase("X8c", "console: CM_BUILDER_COMMAND `levelup 1` raises G to level 2; an unknown name is not implemented", [&] {
		g.game->send(GameSession::CM_BUILDER_COMMAND, GameSession::buildCM_BUILDER_COMMAND("levelup 1"));
		const std::string set = "Set " + charName(g.name, g.accessLevel) + "'s level to 2"; // Levelup.java:31
		EXPECT_TRUE(waitForMessage(*g.game, [&](const decoders::Message& m) { return m.chatType == CHAT_GOLDEN_YELLOW && m.message == set; }))
			<< "no \"" << set << "\"";
		g.game->send(GameSession::CM_BUILDER_COMMAND, GameSession::buildCM_BUILDER_COMMAND("nosuchcommand"));
		EXPECT_TRUE(waitForMessage(*g.game, [](const decoders::Message& m) {
			return m.chatType == CHAT_GOLDEN_YELLOW && m.message == "The command nosuchcommand is not implemented.";
		})) << "ChatProcessor.java:78";
	});

	runCase("X8d", "character: //addexp 100 adds 100 to G's exp (SM_STATUPDATE_EXP) and says so", [&] {
		collectFor(*g.game, 500ms);
		std::optional<int64_t> before;
		for (const Packet& packet : g.game->recorded())
			if (packet.name == "SM_STATUPDATE_EXP")
				before = decoders::decodeStatUpdateExp(packet.data).currentExp;
		ASSERT_TRUE(before) << "no SM_STATUPDATE_EXP since G's login (the levelup of X8c sends one)";
		const size_t from = g.mark();
		g.say("//addexp 100");
		const std::string added = "You added 100 exp points to " + charName(g.name, g.accessLevel) + "."; // AddExp.java:27
		EXPECT_TRUE(waitForMessage(*g.game, [&](const decoders::Message& m) { return m.chatType == CHAT_GOLDEN_YELLOW && m.message == added; }))
			<< "no \"" << added << "\"";
		collectFor(*g.game, 500ms);
		std::optional<int64_t> after;
		for (const Packet& packet : g.since(from))
			if (packet.name == "SM_STATUPDATE_EXP")
				after = decoders::decodeStatUpdateExp(packet.data).currentExp;
		ASSERT_TRUE(after) << "no SM_STATUPDATE_EXP after //addexp";
		EXPECT_EQ(*after, *before + 100);
	});

	runCase("X8e", "player: P's .gmlist lists G and L as online GMs", [&] {
		p.say(".gmlist");
		const std::optional<decoders::Message> list = waitForMessage(*p.game, [](const decoders::Message& m) {
			return m.chatType == CHAT_GOLDEN_YELLOW && m.message.starts_with("GMs online (");
		});
		ASSERT_TRUE(list) << "no \"GMs online (\" (GmList.java:26)";
		EXPECT_TRUE(list->message.starts_with("GMs online (2):")) << list->message;
		EXPECT_NE(list->message.find("\n\t" + charName(g.name, g.accessLevel) + " (online)"), std::string::npos) << list->message;
		EXPECT_NE(list->message.find("\n\t" + charName(l.name, l.accessLevel) + " (online)"), std::string::npos) << list->message;
	});

	runCase("X8f", "monsters: //spawn 210663 puts a juvenile sparkie at G's feet; //kill on it kills it", [&] {
		const size_t from = g.mark();
		g.say("//spawn " + std::to_string(SPARKIE));
		collectFor(*g.game, 2000ms);
		const std::optional<int32_t> sparkie = npcNear(g, from, SPARKIE, g.x, g.y, 3.0);
		ASSERT_TRUE(sparkie) << "no SM_NPC_INFO of " << SPARKIE << " within 3 m of G (SpawnNpc.java:61-63): " << join(namesOf(g.since(from)));
		g.game->send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(*sparkie));
		collectFor(*g.game, 500ms);
		const size_t killed = g.mark();
		g.say("//kill");
		const std::optional<decoders::Message> text = waitForMessage(*g.game, [](const decoders::Message& m) {
			return m.chatType == CHAT_GOLDEN_YELLOW && (m.message.starts_with("Killed ") || m.message.starts_with("Couldn't kill "));
		});
		ASSERT_TRUE(text) << "no answer of //kill (Kill.java:46, :48)";
		EXPECT_TRUE(text->message.starts_with("Killed ")) << text->message;
		EXPECT_NE(text->message.find(std::to_string(SPARKIE)), std::string::npos) << "ChatUtil.path(npc, true) names the id: " << text->message;
		collectFor(*g.game, 2000ms);
		bool died = false;
		for (const Packet& packet : g.since(killed)) {
			if (packet.name != "SM_EMOTION")
				continue;
			try {
				const decoders::Emotion emotion = decoders::decodeEmotion(packet.data);
				died = died || (emotion.emotionType == decoders::EMOTION_DIE && emotion.senderObjectId == *sparkie);
			} catch (const DecodeError&) {
			}
		}
		EXPECT_TRUE(died) << "no SM_EMOTION(DIE) of the sparkie: " << join(namesOf(g.since(killed)));
	});

	// ---- X10: reports and shutdown (the M5a Q8 bar) ----
	cases.run("X10a", "all three log out", [&] {
		disconnect(p);
		disconnect(l);
		disconnect(g);
	});
	const std::optional<int32_t> gameServerExit = servers.stopGameServer();
	const std::chrono::system_clock::time_point serverUpTo = std::chrono::system_clock::now();
	const std::optional<int32_t> loginServerExit = servers.stopLoginServer();
	cases.run("X10", "reports: no AION_UNPORTED, the allow-list, no ERROR, the census, no Player alive", [&] {
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
		EXPECT_TRUE(unported.empty()) << "AION_UNPORTED sites were reached on the GM path:\n" << join(unported, "\n") << cronNote;
		const std::vector<AllowlistEntry> allowlist = readAllowlist();
		ASSERT_FALSE(allowlist.empty()) << "tests/scenario/gm_partial_allowlist.txt is empty or missing";
		std::map<std::string, int64_t> hitsByEntry;
		for (const PartialHit& hit : readPartialHits(servers)) {
			bool allowed = false;
			for (const AllowlistEntry& entry : allowlist)
				if (allowlistEntryMatches(entry.site, hit.site)) {
					allowed = true;
					hitsByEntry[entry.site] += hit.hits;
				}
			EXPECT_TRUE(allowed) << "the AION_PARTIAL site " << hit.site << " is not in tests/scenario/gm_partial_allowlist.txt (" << hit.line << ")";
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
		// the GAG task of X7 pins P until it ran or was cut (ChatBanService.registerUnban): a live leak names the Player
		EXPECT_EQ(value("liveLeaks"), "0") << join(summary.contains("liveLeak") ? summary.at("liveLeak") : std::vector<std::string>{});
		const auto notPorted = summary.find("notPortedClientPacket");
		if (notPorted != summary.end())
			ADD_FAILURE() << "the GM path sent client packets that are not ported: " << join(notPorted->second);
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

/** `gs.scenario.gm` (m5j-plan.md §10.2, §17.8): the GM login, the access rules, chat, whisper, the gag and one command of each stage-0 family */
TEST(GmScenario, Run) {
	runGmGate();
}

} // namespace aion::gameserver::scenario
