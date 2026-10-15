// The M5j gate, stages 1 and 2 (m5j-plan.md §10.4, gs.scenario.m5j; §18.1 CP5, G-11; §18.3 CP5, G-21). One login server and one game server as child processes on
// their own test schemas, five accounts:
//   G,  access level 9, Elyos: the GM of //addtitle, //sprison, //rprison and //ranking (its access level seeded into the login schema, H-02);
//   A,  access level 0, Elyos, the creation spawn on Poeta; seeded a level-10 Daeva (a Gladiator) with 1000 AP before Z14;
//   B,  access level 0, Elyos, seeded a level-10 Daeva (a Gladiator, quest 1006 COMPLETE, m5c-plan.md D5's recipe) 1 m from A, because a
//       non-Daeva cannot whisper a non-staff player (§3.6) or search (gameserver.search.player.level);
//   C,  access level 0, Asmodian, on Ishalgen: the other race of the friend request and of the search;
//   G2, access level 9, Asmodian, seeded a level-13 Daeva 1 m from A: Z14's PvP winner.
// The cases, in the order the state they need allows (the §10.4 ids; each row narrowed or changed where it says so):
//   Z1  A asks C (another race: TARGET_NOT_FOUND) and B to be friends; B accepts the question; both get the list and TARGET_ADDED; two
//       `friends` rows; B quits (A: SM_FRIEND_UPDATE offline, SM_FRIEND_NOTIFY logout) and comes back (online, login, B's list has A);
//   Z3  A's note (SM_UPDATE_NOTE to A and to B, who sees A; B's SM_FRIEND_LIST carries it), A's macro (SM_MACRO_RESULT), G's //addtitle on A
//       and A's CM_TITLE_SET of the owned and of an unowned title; A relogs: the note reaches B in SM_FRIEND_UPDATE, the macro is in
//       SM_MACRO_LIST, the title in SM_TITLE_INFO; the DB rows;
//   Z4  A's search is refused below level 10 (STR_CANT_WHO_LEVEL); B's finds A and not C (another race); B views A's details (the oracle's
//       equipped item count of a new Elyos Warrior);
//   Z2  A cannot block a friend (STR_BLOCKLIST_NO_BUDDY), deletes B as friend, blocks B: B's line no longer reaches A (it did before), B's
//       whisper answers STR_YOU_EXCLUDED(A); B blocks A, and A's whisper answers STR_CANT_WHISPER_LEVEL("10"), not STR_YOU_EXCLUDED - the
//       level arm comes first (§3.6);
//   Z5  A asks B for a duel: B's question, A's withdraw question; B accepts: SM_DUEL started to both;
//   Z6  B attacks A until A would die (CHANGED from §10.4, where A fights B: a level-1 A cannot bring a level-10 Daeva down within the gate's
//       time; the loser is the case's subject either way): A receives no SM_DIE, A's HP is set to Java's 33 % floor, and the SM_DUEL results
//       are lost (A) and won (B);
//   Z7  G: //sprison B 1 test (the three parameters SPrison.java reads: two answer the syntax) -> B in LF_PRISON, the prison text, the
//       punishment row; B's chat line is refused with STR_INGAME_BLOCK_IN_NO_CHAT(1); //rprison B -> B out (the bind point on Poeta), the row
//       gone, STR_CAN_CHAT_NOW;
//   Z8  A: CM_ABYSS_RANKING_PLAYERS(0) twice - the list (no ranked player: no packet), then the short answer with the cache's update time;
//       G: //ranking update runs the update (its two log lines, no ERROR) and resets the flags: A's third request is the list again, the
//       fourth the short answer with an update time at least the first's;
//   Z9  (stage 2) G's //add gives A the oracle's ride item (m5j-items); A uses it: the casting animation, STR_USE_ITEM, SM_EMOTION(CHANGE_SPEED)
//       and SM_EMOTION(RIDE, npcId) and the closing SM_ITEM_USAGE_ANIMATION to A and to G (the watcher); the second use dismounts (RIDE_END).
//       The gate switches gameserver.ride.restriction.enable off: Poeta has no RIDE flag (the oracle's map block);
//   Z11 (stage 2) the oracle's pet egg from //add; CM_PET ADOPT -> SM_PET ADOPT with the pet's specialties and the player_pets row; CM_PET SPAWN
//       -> SM_PET SPAWN to A and G; A relogs: SM_PET LOAD_PETS carries the pet (CHANGED order: Z11 before Z10, so the kisk case's death
//       comes last before Z14's relog);
//   Z10 (stage 2) the oracle's kisk item from //add; A uses it: the kisk's SM_NPC_INFO (A its creator), KiskAI's bind question, A accepts:
//       STR_BINDSTONE_REGISTER, SM_KISK_UPDATE and the kisk bind point; G targets A and types //kill: SM_DIE offers the kisk; CM_REVIVE(KISK)
//       brings A back at the kisk with one resurrection fewer; G's //delete on the kisk removes it (KiskAI.handleDespawned's
//       STR_BINDSTONE_IS_REMOVED, removeKisk's SM_KISK_UPDATE), so no Kisk outlives the run (Z13's live counts);
//   Z14 A relogs as a level-10 Daeva with 1000 AP; G2 enters beside A, types //enemy cancel and //kill on A: A dies, A's AP and G2's AP change
//       by the oracle's calculatePvPApLost / calculatePvpApGained (m5j-social), SM_ABYSS_RANK and the AP system messages to both;
//   Z13 the reports (the M5a Q8 bar), with no Player alive after the logouts.
// Z15 (S-13, Legion Dominion and the windstream) is lane B's chunk and not run. Every text, level and id comes from the oracles
// (`m5j-social`, `m5j-items`, `m5a-creation`) or is a Java literal cited beside it, never the port's.
//
// Every expectation is independent of the C++ server code, as in the earlier gates: server packets are read with the decoders of
// tests/scenario/decoders (written from the Java writeImpl methods, m5a-plan.md D9). This file does not share the other gates' helpers, for the
// reason M5cScenarioTest.cpp gives: what is duplicated from GmScenarioTest.cpp is scaffolding, never an assertion.

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
#include "decoders/EconomyDecoders.h"
#include "decoders/ItemDecoders.h"
#include "decoders/PacketDecoders.h"
#include "decoders/PetKiskDecoders.h"
#include "decoders/ProgressionDecoders.h"
#include "decoders/SkillDecoders.h"
#include "decoders/SocialDecoders.h"
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
/** how long a case listens for a packet that must NOT come */
constexpr std::chrono::milliseconds SILENCE = 2000ms;

constexpr int32_t RESPONSE_OK = 0;
constexpr int32_t RESPONSE_OPEN_CREATION_WINDOW = 22;
constexpr uint8_t ENTER_WORLD_OK = 0;

/** ChatType ids (ChatType.java:13, :16, :42) */
constexpr uint8_t CHAT_NORMAL = 0;
constexpr uint8_t CHAT_WHISPER = 4;
constexpr uint8_t CHAT_GOLDEN_YELLOW = 25;

/** WorldMapType.POETA and LF_PRISON (WorldMapType.java), TeleportService.teleportToPrison's spot (TeleportService.java:297-299) */
constexpr int32_t POETA = 210010000;
constexpr int32_t LF_PRISON = 510010000;
constexpr float PRISON_X = 275, PRISON_Y = 239, PRISON_Z = 49;
/** the Daeva class of the seeds (an advanced class of the Warrior, as the travel gate's) */
constexpr std::string_view DAEVA_CLASS = "GLADIATOR";
/** the levels of the seeds: B and A level 10, G2 level 13 - the PvP level difference 3 has an arm in both formulas (StatFunctions.java:139, :159) */
constexpr int32_t B_LEVEL = 10;
constexpr int32_t A_PVP_LEVEL = 10;
constexpr int32_t G2_LEVEL = 13;
/** A's AP before Z14: inside GRADE9_SOLDIER (AbyssRankEnum: GRADE8 needs 1200), and above its points lost */
constexpr int32_t A_SEED_AP = 1000;
/** CreatureLifeStats.setCurrentHpPercent's percentage in PlayerController.onDie's duel arm (PlayerController.java:279-286) */
constexpr int64_t DUEL_FLOOR_PERCENT = 33;

/** the macro of Z3 (CM_MACRO_CREATE: a position and the client's XML, stored as given) */
constexpr uint8_t MACRO_POSITION = 3;
constexpr std::string_view MACRO_XML = "<macro><command>/s hello</command></macro>";
constexpr std::string_view NOTE = "the stage one gate";
/** Z11's pet name: letters only (NameConfig.PET_NAME_PATTERN's default [a-zA-Z]{2,16}), capitalised as Util.convertName leaves it */
constexpr std::string_view PET_NAME = "Kittyj";

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

std::vector<Packet> ofName(const std::vector<Packet>& packets, std::string_view name) {
	std::vector<Packet> found;
	for (const Packet& packet : packets)
		if (packet.name == name)
			found.push_back(packet);
	return found;
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

std::vector<decoders::Message> messagesIn(const std::vector<Packet>& packets) {
	std::vector<decoders::Message> messages;
	for (const Packet& packet : packets)
		if (packet.name == "SM_MESSAGE")
			messages.push_back(decoders::decodeMessage(packet.data));
	return messages;
}

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

std::vector<int32_t> systemMessageIds(const std::vector<Packet>& packets) {
	std::vector<int32_t> ids;
	for (const decoders::SystemMessage& message : systemMessagesIn(packets))
		ids.push_back(message.messageId);
	return ids;
}

std::string idsText(const std::vector<int32_t>& ids) {
	std::vector<std::string> texts;
	for (int32_t id : ids)
		texts.push_back(std::to_string(id));
	return join(texts);
}

/** Java ChatUtil.l10n (ChatUtil.java:96-102), in the UTF-8 BodyReader.S() returns (GmScenarioTest.cpp explains the form) */
std::string l10n(int32_t l10nId) {
	const int32_t id = l10nId << 1 | 1;
	std::u16string text = u"$";
	text += static_cast<char16_t>(id & 0xFFFF);
	text += static_cast<char16_t>((static_cast<uint32_t>(id) >> 16) & 0xFFFF);
	return commons::utils::StringUtils::toUtf8(text);
}

/** Java ChatUtil.charName (ChatUtil.java:87-89) with admin.properties' shipped customtags (GmScenarioTest.cpp's charName) */
std::string charName(std::string_view name, int32_t accessLevel = 0) {
	std::string tagged(name);
	if (accessLevel == 9)
		tagged = commons::utils::StringUtils::toUtf8(std::u16string(u"»Admin«")) + tagged;
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

/**
 * Reads until a packet named `name` for which `wanted` answers true arrives, recording everything on the way; the packets already recorded
 * from `from` on are looked at first. @return it, or nullopt after `timeout` or a close
 */
std::optional<Packet> waitForPacket(GameSession& session, size_t from, std::string_view name, const std::function<bool(const Packet&)>& wanted,
	std::chrono::milliseconds timeout = 15s) {
	for (const Packet& packet : slice(session, from))
		if (packet.name == name && wanted(packet))
			return packet;
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
		if (packet->name == name && wanted(*packet))
			return packet;
	}
}

/** Reads until a packet with that name arrives and records everything on the way. @throws std::runtime_error on timeout or close */
Packet waitFor(GameSession& session, std::string_view name, std::chrono::milliseconds timeout = 15s) {
	std::optional<Packet> packet = waitForPacket(session, session.recorded().size(), name, [](const Packet&) { return true; }, timeout);
	if (!packet)
		throw std::runtime_error("timeout waiting for " + std::string(name) + (session.client.socket.isClosed() ? " (the connection closed)" : ""));
	return *packet;
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

/** an SM_SYSTEM_MESSAGE of that id from `from` on, waiting up to `timeout` */
std::optional<decoders::SystemMessage> waitForSystemMessage(GameSession& session, size_t from, int32_t messageId,
	std::chrono::milliseconds timeout = 10s) {
	std::optional<Packet> packet = waitForPacket(session, from, "SM_SYSTEM_MESSAGE", [&](const Packet& p) {
		return decoders::decodeSystemMessage(p.data).messageId == messageId;
	}, timeout);
	if (!packet)
		return std::nullopt;
	return decoders::decodeSystemMessage(packet->data);
}

/** an SM_MESSAGE `wanted` accepts from `from` on, waiting up to `timeout` */
std::optional<decoders::Message> waitForMessage(GameSession& session, size_t from, const std::function<bool(const decoders::Message&)>& wanted,
	std::chrono::milliseconds timeout = 10s) {
	std::optional<Packet> packet = waitForPacket(session, from, "SM_MESSAGE", [&](const Packet& p) { return wanted(decoders::decodeMessage(p.data)); },
	                                             timeout);
	if (!packet)
		return std::nullopt;
	return decoders::decodeMessage(packet->data);
}

/** a GOLDEN_YELLOW sendMessage text from `from` on */
bool waitForInfo(GameSession& session, size_t from, std::string_view text, std::chrono::milliseconds timeout = 10s) {
	return waitForMessage(session, from, [&](const decoders::Message& m) {
		return m.chatType == CHAT_GOLDEN_YELLOW && m.senderObjectId == 0 && m.message == text;
	}, timeout).has_value();
}

// ---- the clients ----------------------------------------------------------------------------------------------------------------------

struct ScenarioClient {
	std::string label;
	std::string account;
	std::string password = "m5jGatePassword1";
	std::string name;
	int32_t accessLevel = 0;
	bool asmodian = false;
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
	void say(std::string_view text) { game->send(GameSession::CM_CHAT_MESSAGE_PUBLIC, GameSession::buildGmCommand(text)); }
	void send(int32_t opcode, const std::vector<uint8_t>& body) { game->send(opcode, body); }
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

/** a fresh account's first login: a Warrior of the client's race is created at its creation spawn, and the account logs out again */
void createCharacter(ScenarioServers& servers, ScenarioClient& client) {
	const decoders::CharacterList list = logIn(servers, client);
	EXPECT_EQ(list.characterCount, 0) << client.label << ": a fresh account must have no character";
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
	disconnect(client);
}

/** H-02: the account's access level, written while no client of it is connected (GmScenarioTest.cpp's seedAccessLevel) */
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

/**
 * A teleport without animation (TeleportService.sendLoc with TeleportAnimation.NONE runs the SpawnTask at once): SM_PLAYER_SPAWN of the
 * destination, then the client's CM_LEVEL_READY spawns the character there. @return the destination's spawn
 */
decoders::PlayerSpawn followInstantTeleport(ScenarioClient& client, size_t from) {
	std::optional<Packet> spawn = waitForPacket(*client.game, from, "SM_PLAYER_SPAWN", [](const Packet&) { return true; }, 20s);
	if (!spawn)
		throw std::runtime_error(client.label + ": no SM_PLAYER_SPAWN: " + join(namesOf(client.since(from))));
	const decoders::PlayerSpawn spawned = decoders::decodePlayerSpawn(spawn->data);
	client.game->send(GameSession::CM_LEVEL_READY, GameSession::buildCM_LEVEL_READY());
	collectFor(*client.game, 1500ms);
	client.x = spawned.x;
	client.y = spawned.y;
	client.z = spawned.z;
	client.worldId = spawned.worldId;
	return spawned;
}

std::optional<decoders::StatsInfo> lastStats(const ScenarioClient& client) {
	std::optional<decoders::StatsInfo> stats;
	for (const Packet& packet : client.since(0))
		if (packet.name == "SM_STATS_INFO") {
			try {
				stats = decoders::decodeStatsInfo(packet.data);
			} catch (const DecodeError&) {
			}
		}
	return stats;
}

// ---- the reports -----------------------------------------------------------------------------------------------------------------------

enum class AllowlistSection { HitAtLeastOnce, HitNever, NotPinned };

struct AllowlistEntry {
	std::string site;
	AllowlistSection section = AllowlistSection::NotPinned;
};

/** Reads tests/scenario/m5j_partial_allowlist.txt with its three sections ("# --- SECTION A/B/C" marker lines, as the M5c list) */
std::vector<AllowlistEntry> readAllowlist() {
	std::vector<AllowlistEntry> entries;
	std::ifstream in(AION_SCENARIO_M5J_PARTIAL_ALLOWLIST, std::ios::binary);
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

void runM5jGate() {
	const std::string testName = "gs.scenario.m5j";
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
	const std::filesystem::path outputDir = std::filesystem::path(AION_SCENARIO_OUTPUT_DIR) / "m5j";
	std::optional<Oracle> oracle = Oracle::fromEnvironment(outputDir / "oracle");
	if (!oracle) {
		if (required)
			ADD_FAILURE() << testName << " was not configured and AION_SCENARIO_REQUIRE is set: no Python interpreter for tools/oracle (AION_TEST_PYTHON)";
		else
			GTEST_SKIP() << testName << ": skipped (no Python interpreter for tools/oracle: set AION_TEST_PYTHON)";
		return;
	}

	// the gate's keys over the M5a profile (§10.1: the factions key and the whisper level explicitly, at their Java defaults)
	const std::map<std::string, std::string> gateKeys{
		{"gameserver.npcshouts.enable", "false"},
		{"gameserver.rates.drop", "0"},
		{"gameserver.administration.login.execute_commands", "//invis, //invul, //enemy none, //see"},
		{"gameserver.chat.factions.enable", "false"},
		{"gameserver.chat.whisper.level", "10"},
		{"gameserver.chatserver.enable", "false"},
		{"gameserver.simple.secondclass.enable", "false"},
		// stage 2 (Z9): Poeta's map and zones have no RIDE flag, so RideAction.canAct's zone arm would refuse every spot; m5j-items models
		// the map's zones, not the spot's (§18.3 CP5)
		{"gameserver.ride.restriction.enable", "false"},
	};
	// the oracles read the server's own keys as their profile, never the owner's config/mygs.properties (M5dScenarioTest.cpp's pattern)
	std::filesystem::create_directories(outputDir);
	const std::filesystem::path profileFile = outputDir / "m5j_oracle_profile.properties";
	{
		std::map<std::string, std::string> keys = ScenarioServers::m5aProfile();
		for (const auto& [key, value] : gateKeys)
			keys[key] = value;
		std::ofstream out(profileFile, std::ios::binary | std::ios::trunc);
		for (const auto& [key, value] : keys)
			out << key << " = " << value << "\n";
	}
	const nlohmann::json social = nlohmann::json::parse(oracle->run(
		{"m5j-social", "--profile", profileFile.string(), "--message", "STR_YOU_EXCLUDED", "STR_CANT_WHISPER_LEVEL", "STR_CANT_WHO_LEVEL",
	     "STR_BLOCKLIST_NO_BUDDY", "STR_INGAME_BLOCK_IN_NO_CHAT", "STR_CAN_CHAT_NOW", "STR_MSG_GET_CASH_TITLE", "STR_DUEL_REQUESTED",
	     "STR_MSG_COMBAT_MY_ABYSS_POINT_GAIN", "STR_MSG_USE_ABYSSPOINT", "--question", "STR_BUDDYLIST_ADD_BUDDY_REQUEST",
	     "STR_DUEL_DO_YOU_ACCEPT_REQUEST", "STR_DUEL_DO_YOU_WITHDRAW_REQUEST", "--daeva-level", std::to_string(B_LEVEL), std::to_string(G2_LEVEL),
	     "--pvp-kill", std::to_string(A_SEED_AP) + "," + std::to_string(A_PVP_LEVEL) + ",0," + std::to_string(G2_LEVEL)}));
	const nlohmann::json creation = nlohmann::json::parse(oracle->run({"m5a-creation", "--race", "ELYOS", "--class", "WARRIOR"}));
	// stage 2: the ride, kisk and pet items a level-1 Elyos Warrior may use, Poeta's kisk zones (m5j-items, H-21)
	const nlohmann::json items = nlohmann::json::parse(oracle->run({"m5j-items", "--profile", profileFile.string(), "--class", "WARRIOR", "--race",
		"ELYOS", "--level", "1", "--map", std::to_string(POETA), "--message", "STR_USE_ITEM", "STR_BINDSTONE_REGISTER", "STR_BINDSTONE_IS_REMOVED", "--question",
		"STR_ASK_REGISTER_BINDSTONE"}));
	const auto emotionId = [&](const char* name) { return items.at("emotions").at(name).get<int32_t>(); };
	const auto message = [&](const char* name) {
		return (social.at("messages").contains(name) ? social : items).at("messages").at(name).get<int32_t>();
	};
	const auto question = [&](const char* name) {
		return (social.at("questions").contains(name) ? social : items).at("questions").at(name).get<int32_t>();
	};
	const int32_t whisperLevel = social.at("config").at("gameserver.chat.whisper.level").at("value").get<int32_t>();
	const int32_t searchLevel = social.at("config").at("gameserver.search.player.level").at("value").get<int32_t>();
	const nlohmann::json& daeva = social.at("daeva");
	const nlohmann::json& kill = social.at("pvpKill");
	int32_t equippedItems = 0; // SM_VIEW_PLAYER_DETAILS: getEquippedItemsWithoutStigma of a new Elyos Warrior
	for (const nlohmann::json& item : creation.at("items"))
		if (item.at("equipped").get<bool>())
			equippedItems++;
	const nlohmann::json& ownedTitle = social.at("titles").at("ELYOS").at(0);
	const nlohmann::json& unownedTitle = social.at("titles").at("ELYOS").at(1);

	CaseLog cases;
	struct ReportPrinter {
		const CaseLog& cases;
		const std::string& testName;
		~ReportPrinter() { std::cout << cases.report(testName) << std::flush; }
	} printer{cases, testName};

	ScenarioServers::Config config;
	config.gameServerExecutable = AION_GAME_SERVER_EXECUTABLE;
	config.loginServerExecutable = AION_LOGIN_SERVER_EXECUTABLE;
	config.gameServerJavaDir = AION_GAMESERVER_JAVA_DIR;
	config.loginServerJavaDir = AION_LOGINSERVER_JAVA_DIR;
	config.outputDir = outputDir;
	config.schemaPrefix = "m5j";
	for (const auto& [key, value] : gateKeys)
		config.gameServerProperties[key] = value;
	config.startupTimeout = 10min;
	config.stopTimeout = 3min;
	ScenarioServers servers(config, *environment);
	const ScenarioDatabase& database = servers.gameDatabase();
	const auto schema = [&] { return servers.gameSchema(); };
	const auto count = [&](const std::string& sql) { return database.queryLong(schema(), sql).value_or(-1); };

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

	ScenarioClient g, a, b, c, g2;
	const std::string suffix = servers.gameSchema().substr(servers.gameSchema().size() - 8);
	const auto setUp = [&](ScenarioClient& client, const char* label, const char* prefix, const char* name, int32_t accessLevel, bool asmodian) {
		client.label = label;
		client.account = prefix + suffix;
		client.name = name;
		client.accessLevel = accessLevel;
		client.asmodian = asmodian;
	};
	setUp(g, "G", "m5jg", "Wardenj", 9, false);
	setUp(a, "A", "m5ja", "Alphaj", 0, false);
	setUp(b, "B", "m5jb", "Bravoj", 0, false);
	setUp(c, "C", "m5jc", "Charlij", 0, true);
	setUp(g2, "G2", "m5jh", "Wardtwoj", 9, true);
	float spotX = 0, spotY = 0, spotZ = 0;

	/** m5c-plan.md D5's Daeva seed (TravelScenarioTest.cpp's form): the advanced class, the level's exp, the ascension quest COMPLETE */
	const auto seedDaeva = [&](const ScenarioClient& client, int32_t level, const std::string& position) {
		const std::string id = std::to_string(client.playerId);
		const std::string race = client.asmodian ? "ASMODIANS" : "ELYOS";
		database.execute(schema(), "UPDATE players SET player_class = '" + std::string(DAEVA_CLASS) +
		                             "', exp = " + std::to_string(daeva.at("byLevel").at(std::to_string(level)).at("exp").get<int64_t>()) + position +
		                             " WHERE id = " + id);
		database.execute(schema(), "INSERT INTO player_quests (player_id, quest_id, status, complete_count) VALUES (" + id + ", " +
		                             std::to_string(daeva.at("ascensionQuests").at(race).get<int32_t>()) + ", 'COMPLETE', 1)");
	};
	const auto at = [&](float dx, float dy) {
		return ", world_id = " + std::to_string(POETA) + ", x = " + std::to_string(spotX + dx) + ", y = " + std::to_string(spotY + dy) +
		       ", z = " + std::to_string(spotZ) + ", heading = 0";
	};

	runCase("A0", "five characters are created; G's and G2's accounts get access level 9; B is seeded a level-10 Daeva 1 m from A and G2 a "
	              "level-13 Daeva beside A (both while logged out)", [&] {
		for (ScenarioClient* client : {&g, &a, &b, &c, &g2})
			createCharacter(servers, *client);
		seedAccessLevel(servers, g);
		seedAccessLevel(servers, g2);
		const std::vector<std::vector<std::optional<std::string>>> rows =
			database.queryRows(schema(), "SELECT world_id, x, y, z FROM players WHERE id = " + std::to_string(a.playerId), 4);
		ASSERT_EQ(rows.size(), 1u);
		ASSERT_EQ(rows[0][0].value_or(""), std::to_string(POETA)) << "an Elyos character is created on Poeta (PlayerInitialData)";
		spotX = std::stof(rows[0][1].value_or("0"));
		spotY = std::stof(rows[0][2].value_or("0"));
		spotZ = std::stof(rows[0][3].value_or("0"));
		seedDaeva(b, B_LEVEL, at(1.0f, 0.0f));
		seedDaeva(g2, G2_LEVEL, at(-1.0f, 0.0f));
	});

	runCase("A1", "G, A, B and C enter the world; B enters as a level-10 Daeva", [&] {
		enterGame(servers, g);
		enterGame(servers, a);
		const std::vector<Packet> burst = enterGame(servers, b);
		std::optional<decoders::StatsInfo> stats = lastStats(b);
		ASSERT_TRUE(stats) << "no SM_STATS_INFO in B's enter world";
		EXPECT_EQ(stats->level, B_LEVEL) << "the Daeva seed (m5c-plan.md D5)";
		enterGame(servers, c);
		EXPECT_EQ(c.worldId, 220010000) << "an Asmodian character is created on Ishalgen";
	});

	// ---- Z1: friends ----
	runCase("Z1", "A asks C (another race) and B to be friends; B accepts; two rows; B's quit and return reach A", [&] {
		size_t aFrom = a.mark();
		a.send(GameSession::CM_FRIEND_ADD, GameSession::buildCM_FRIEND_ADD(c.name, "hello"));
		std::optional<Packet> refused = waitForPacket(*a.game, aFrom, "SM_FRIEND_RESPONSE", [](const Packet&) { return true; });
		ASSERT_TRUE(refused) << "CM_FRIEND_ADD of another race: no SM_FRIEND_RESPONSE";
		EXPECT_EQ(decoders::decodeFriendResponse(refused->data).code, decoders::FRIEND_RESPONSE_TARGET_NOT_FOUND) << "CM_FRIEND_ADD.java: the race arm";

		const size_t bFrom = b.mark();
		aFrom = a.mark();
		a.send(GameSession::CM_FRIEND_ADD, GameSession::buildCM_FRIEND_ADD(b.name, "be my friend"));
		std::optional<Packet> asked = waitForPacket(*b.game, bFrom, "SM_QUESTION_WINDOW", [&](const Packet& p) {
			return decoders::decodeQuestionWindow(p.data).code == question("STR_BUDDYLIST_ADD_BUDDY_REQUEST");
		});
		ASSERT_TRUE(asked) << "B was not asked (SM_QUESTION_WINDOW STR_BUDDYLIST_ADD_BUDDY_REQUEST): " << join(namesOf(b.since(bFrom)));
		const decoders::QuestionWindow window = decoders::decodeQuestionWindow(asked->data);
		EXPECT_EQ(window.senderId, a.playerId);
		EXPECT_EQ(window.params[0], a.name);
		EXPECT_EQ(window.params[1], "be my friend");
		b.send(GameSession::CM_QUESTION_RESPONSE, GameSession::buildCM_QUESTION_RESPONSE(question("STR_BUDDYLIST_ADD_BUDDY_REQUEST"), 1));
		for (auto [self, other] : {std::pair{&a, &b}, std::pair{&b, &a}}) {
			const size_t from = self == &a ? aFrom : bFrom;
			std::optional<Packet> added = waitForPacket(*self->game, from, "SM_FRIEND_RESPONSE", [&](const Packet& p) {
				return decoders::decodeFriendResponse(p.data).code == decoders::FRIEND_RESPONSE_TARGET_ADDED;
			});
			ASSERT_TRUE(added) << self->label << ": no SM_FRIEND_RESPONSE TARGET_ADDED (SocialService.makeFriends)";
			EXPECT_EQ(decoders::decodeFriendResponse(added->data).name, other->name);
			std::optional<Packet> list = waitForPacket(*self->game, from, "SM_FRIEND_LIST", [&](const Packet& p) {
				return decoders::decodeFriendList(p.data).find(other->playerId) != nullptr;
			}, 5s);
			ASSERT_TRUE(list) << self->label << ": no SM_FRIEND_LIST with " << other->label;
			const decoders::FriendList decoded = decoders::decodeFriendList(list->data);
			const decoders::FriendEntry* entry = decoded.find(other->playerId);
			EXPECT_EQ(entry->name, other->name);
			EXPECT_EQ(entry->status, decoders::FRIEND_STATUS_ONLINE);
		}
		EXPECT_EQ(count("SELECT COUNT(*) FROM friends WHERE (player = " + std::to_string(a.playerId) + " AND friend = " + std::to_string(b.playerId) +
		                ") OR (player = " + std::to_string(b.playerId) + " AND friend = " + std::to_string(a.playerId) + ")"),
		          2)
			<< "FriendListDAO.addFriends: one row each way";

		aFrom = a.mark();
		disconnect(b);
		std::optional<Packet> offline = waitForPacket(*a.game, aFrom, "SM_FRIEND_UPDATE", [&](const Packet& p) {
			const decoders::FriendUpdate u = decoders::decodeFriendUpdate(p.data);
			return !u.empty && u.name == b.name && u.status == decoders::FRIEND_STATUS_OFFLINE;
		});
		EXPECT_TRUE(offline) << "B's quit: FriendList.setStatus(OFFLINE) sends SM_FRIEND_UPDATE to A: " << join(namesOf(a.since(aFrom)));
		EXPECT_TRUE(waitForPacket(*a.game, aFrom, "SM_FRIEND_NOTIFY", [&](const Packet& p) {
			const decoders::FriendNotify n = decoders::decodeFriendNotify(p.data);
			return n.name == b.name && n.code == decoders::FRIEND_NOTIFY_LOGOUT;
		}, 5s)) << "SM_FRIEND_NOTIFY LOGOUT";

		aFrom = a.mark();
		const std::vector<Packet> burst = enterGame(servers, b);
		const Packet* list = firstOfName(burst, "SM_FRIEND_LIST");
		ASSERT_NE(list, nullptr) << "B's enter world has no SM_FRIEND_LIST (PlayerEnterWorldService.java:284)";
		EXPECT_NE(decoders::decodeFriendList(list->data).find(a.playerId), nullptr) << "the friendship was loaded from the DB";
		// the status byte is not asserted: FriendList.setStatus(ONLINE) runs before World.storeObject (PlayerEnterWorldService.java:191, :197),
		// and Friend.getStatus answers OFFLINE while World.getPlayer does not find the friend - which the packet's write races
		EXPECT_TRUE(waitForPacket(*a.game, aFrom, "SM_FRIEND_UPDATE", [&](const Packet& p) {
			const decoders::FriendUpdate u = decoders::decodeFriendUpdate(p.data);
			return !u.empty && u.name == b.name;
		}, 5s)) << "B's enter world: SM_FRIEND_UPDATE to A";
		EXPECT_TRUE(waitForPacket(*a.game, aFrom, "SM_FRIEND_NOTIFY", [&](const Packet& p) {
			const decoders::FriendNotify n = decoders::decodeFriendNotify(p.data);
			return n.name == b.name && n.code == decoders::FRIEND_NOTIFY_LOGIN;
		}, 5s)) << "SM_FRIEND_NOTIFY LOGIN";
	});

	// ---- Z3: note, macro, title ----
	runCase("Z3", "A's note reaches A and B, the macro is stored, G gives A a title which A displays (an unowned one is ignored); a relog "
	              "keeps all three", [&] {
		size_t aFrom = a.mark();
		size_t bFrom = b.mark();
		a.send(GameSession::CM_SET_NOTE, GameSession::buildCM_SET_NOTE(NOTE));
		for (ScenarioClient* reader : {&a, &b}) {
			const size_t from = reader == &a ? aFrom : bFrom;
			std::optional<Packet> note = waitForPacket(*reader->game, from, "SM_UPDATE_NOTE", [&](const Packet& p) {
				return decoders::decodeUpdateNote(p.data).objectId == a.playerId;
			});
			ASSERT_TRUE(note) << reader->label << ": no SM_UPDATE_NOTE (broadcastPacketAndReceive, CM_SET_NOTE.java)";
			EXPECT_EQ(decoders::decodeUpdateNote(note->data).note, NOTE);
		}
		std::optional<Packet> friendList = waitForPacket(*b.game, bFrom, "SM_FRIEND_LIST", [&](const Packet& p) {
			const decoders::FriendList decoded = decoders::decodeFriendList(p.data);
			const decoders::FriendEntry* entry = decoded.find(a.playerId);
			return entry != nullptr && entry->note == NOTE;
		}, 5s);
		EXPECT_TRUE(friendList) << "A's friend B gets SM_FRIEND_LIST with the note (CM_SET_NOTE.java)";

		aFrom = a.mark();
		a.send(GameSession::CM_MACRO_CREATE, GameSession::buildCM_MACRO_CREATE(MACRO_POSITION, MACRO_XML));
		EXPECT_TRUE(waitForPacket(*a.game, aFrom, "SM_MACRO_RESULT", [](const Packet&) { return true; })) << "CM_MACRO_CREATE.java: SM_MACRO_RESULT";

		const size_t gFrom = g.mark();
		aFrom = a.mark();
		const int32_t titleId = ownedTitle.at("id").get<int32_t>();
		const std::string titleL10n = l10n(ownedTitle.at("nameId").get<int32_t>());
		g.say("//addtitle " + std::to_string(titleId) + " " + a.name);
		EXPECT_TRUE(waitForInfo(*g.game, gFrom, "Added title \"" + titleL10n + "\" to " + charName(a.name))) << "AddTitle.java: " <<
			join(infoTexts(g.since(gFrom)), " | ");
		EXPECT_TRUE(waitForInfo(*a.game, aFrom, charName(g.name, g.accessLevel) + " gave you the title \"" + titleL10n + "\"")) << "AddTitle.java";
		EXPECT_TRUE(waitForSystemMessage(*a.game, aFrom, message("STR_MSG_GET_CASH_TITLE"))) << "TitleList.addTitle";

		aFrom = a.mark();
		bFrom = b.mark();
		a.send(GameSession::CM_TITLE_SET, GameSession::buildCM_TITLE_SET(static_cast<uint16_t>(titleId)));
		std::optional<Packet> self = waitForPacket(*a.game, aFrom, "SM_TITLE_INFO", [](const Packet& p) {
			return decoders::decodeTitleInfo(p.data).action == decoders::TITLE_ACTION_SELF_SET;
		});
		ASSERT_TRUE(self) << "TitleList.setDisplayTitle: SM_TITLE_INFO(titleId)";
		EXPECT_EQ(decoders::decodeTitleInfo(self->data).titleId, titleId);
		std::optional<Packet> broad = waitForPacket(*b.game, bFrom, "SM_TITLE_INFO", [&](const Packet& p) {
			const decoders::TitleInfo info = decoders::decodeTitleInfo(p.data);
			return info.action == decoders::TITLE_ACTION_BROAD_SET && info.playerObjectId == a.playerId;
		}, 5s);
		ASSERT_TRUE(broad) << "B sees A's title (SM_TITLE_INFO(owner, titleId), broadcastPacketAndReceive)";
		EXPECT_EQ(decoders::decodeTitleInfo(broad->data).titleId, titleId);
		aFrom = a.mark();
		const int32_t unownedId = unownedTitle.at("id").get<int32_t>();
		a.send(GameSession::CM_TITLE_SET, GameSession::buildCM_TITLE_SET(static_cast<uint16_t>(unownedId)));
		collectFor(*a.game, SILENCE);
		for (const Packet& packet : ofName(a.since(aFrom), "SM_TITLE_INFO"))
			EXPECT_NE(decoders::decodeTitleInfo(packet.data).titleId, unownedId) << "CM_TITLE_SET.java: a title the list does not contain is ignored";

		// the relog: the note in B's SM_FRIEND_UPDATE, the macro in SM_MACRO_LIST, the displayed title in SM_TITLE_INFO
		disconnect(a);
		EXPECT_EQ(database.queryString(schema(), "SELECT note FROM players WHERE id = " + std::to_string(a.playerId)).value_or(""), NOTE);
		EXPECT_EQ(count("SELECT COUNT(*) FROM player_macrosses WHERE player_id = " + std::to_string(a.playerId) + " AND `order` = " +
		                std::to_string(MACRO_POSITION)),
		          1);
		EXPECT_EQ(count("SELECT COUNT(*) FROM player_titles WHERE player_id = " + std::to_string(a.playerId) + " AND title_id = " +
		                std::to_string(titleId)),
		          1);
		bFrom = b.mark();
		const std::vector<Packet> burst = enterGame(servers, a);
		bool macroListed = false;
		for (const Packet& packet : ofName(burst, "SM_MACRO_LIST"))
			for (const decoders::MacroEntry& macro : decoders::decodeMacroList(packet.data).macros)
				macroListed = macroListed || (macro.id == MACRO_POSITION && macro.xml == MACRO_XML);
		EXPECT_TRUE(macroListed) << "A's enter world: SM_MACRO_LIST with the macro (PlayerEnterWorldService.java:490-492)";
		bool titleShown = false;
		for (const Packet& packet : ofName(burst, "SM_TITLE_INFO")) {
			const decoders::TitleInfo info = decoders::decodeTitleInfo(packet.data);
			titleShown = titleShown || (info.action == decoders::TITLE_ACTION_SELF_SET && info.titleId == titleId);
		}
		EXPECT_TRUE(titleShown) << "A's enter world: SM_TITLE_INFO(pcd.getTitleId()) (PlayerEnterWorldService.java:239)";
		std::optional<Packet> online = waitForPacket(*b.game, bFrom, "SM_FRIEND_UPDATE", [&](const Packet& p) {
			const decoders::FriendUpdate u = decoders::decodeFriendUpdate(p.data);
			return !u.empty && u.name == a.name; // the status byte races World.storeObject (Z1)
		}, 5s);
		ASSERT_TRUE(online) << "A's enter world: SM_FRIEND_UPDATE to B";
		EXPECT_EQ(decoders::decodeFriendUpdate(online->data).note, NOTE) << "the note was loaded back";
	});

	// ---- Z4: search and details ----
	runCase("Z4", "A cannot search below level 10; B finds A but not C; B views A's equipped items", [&] {
		size_t aFrom = a.mark();
		a.send(GameSession::CM_PLAYER_SEARCH, GameSession::buildCM_PLAYER_SEARCH(b.name));
		const std::optional<decoders::SystemMessage> low = waitForSystemMessage(*a.game, aFrom, message("STR_CANT_WHO_LEVEL"));
		ASSERT_TRUE(low) << "CM_PLAYER_SEARCH.java: a level-1 searcher is refused";
		EXPECT_EQ(low->params, std::vector<std::string>{std::to_string(searchLevel)});
		EXPECT_TRUE(ofName(a.since(aFrom), "SM_PLAYER_SEARCH").empty());

		size_t bFrom = b.mark();
		b.send(GameSession::CM_PLAYER_SEARCH, GameSession::buildCM_PLAYER_SEARCH(a.name));
		const Packet found = waitFor(*b.game, "SM_PLAYER_SEARCH");
		const std::vector<decoders::PlayerSearchEntry> entries = decoders::decodePlayerSearch(found.data);
		ASSERT_EQ(entries.size(), 1u) << "the name matches A alone";
		EXPECT_EQ(entries[0].name, a.name) << "toFactionPrefixedName: no prefix for a reader who is no staff";
		EXPECT_EQ(entries[0].worldId, POETA);
		EXPECT_EQ(entries[0].level, 1);
		b.send(GameSession::CM_PLAYER_SEARCH, GameSession::buildCM_PLAYER_SEARCH(c.name));
		const std::vector<decoders::PlayerSearchEntry> other = decoders::decodePlayerSearch(waitFor(*b.game, "SM_PLAYER_SEARCH").data);
		EXPECT_TRUE(other.empty()) << "another race is not found (gameserver.search.factions.mode "
		                           << social.at("config").at("gameserver.search.factions.mode").at("value") << ")";

		bFrom = b.mark();
		b.send(GameSession::CM_VIEW_PLAYER_DETAILS, GameSession::buildCM_VIEW_PLAYER_DETAILS(a.playerId));
		std::optional<Packet> details = waitForPacket(*b.game, bFrom, "SM_VIEW_PLAYER_DETAILS", [](const Packet&) { return true; });
		ASSERT_TRUE(details) << "CM_VIEW_PLAYER_DETAILS.java: no SM_VIEW_PLAYER_DETAILS";
		const decoders::ViewPlayerDetailsHead head = decoders::decodeViewPlayerDetailsHead(details->data);
		EXPECT_EQ(head.targetObjectId, a.playerId);
		EXPECT_EQ(head.itemCount, equippedItems) << "the equipped items of a new Elyos Warrior (m5a-creation)";
	});

	// ---- Z2: block and the whisper arms ----
	runCase("Z2", "A blocks B after deleting the friendship: B's chat no longer reaches A; B's whisper is excluded; with B blocking A as well, "
	              "A's whisper answers the level, not the block", [&] {
		size_t aFrom = a.mark();
		b.say("before the block");
		EXPECT_TRUE(waitForMessage(*a.game, aFrom, [&](const decoders::Message& m) {
			return m.chatType == CHAT_NORMAL && m.senderObjectId == b.playerId && m.message == "before the block";
		})) << "the control: B's line reaches A while nothing is blocked";

		aFrom = a.mark();
		a.send(GameSession::CM_BLOCK_ADD, GameSession::buildCM_BLOCK_ADD(b.name, "spam"));
		EXPECT_TRUE(waitForSystemMessage(*a.game, aFrom, message("STR_BLOCKLIST_NO_BUDDY"))) << "CM_BLOCK_ADD.java: a friend cannot be blocked";
		const size_t bFrom = b.mark();
		aFrom = a.mark();
		a.send(GameSession::CM_FRIEND_DEL, GameSession::buildCM_FRIEND_DEL(b.name));
		std::optional<Packet> removed = waitForPacket(*a.game, aFrom, "SM_FRIEND_RESPONSE", [](const Packet& p) {
			return decoders::decodeFriendResponse(p.data).code == decoders::FRIEND_RESPONSE_TARGET_REMOVED;
		});
		ASSERT_TRUE(removed) << "SocialService.deleteFriend: TARGET_REMOVED";
		EXPECT_TRUE(waitForPacket(*b.game, bFrom, "SM_FRIEND_NOTIFY", [&](const Packet& p) {
			const decoders::FriendNotify n = decoders::decodeFriendNotify(p.data);
			return n.code == decoders::FRIEND_NOTIFY_DELETED && n.name == a.name;
		}, 5s)) << "SM_FRIEND_NOTIFY DELETED to B";
		EXPECT_EQ(count("SELECT COUNT(*) FROM friends WHERE player IN (" + std::to_string(a.playerId) + ", " + std::to_string(b.playerId) + ")"), 0);

		aFrom = a.mark();
		a.send(GameSession::CM_BLOCK_ADD, GameSession::buildCM_BLOCK_ADD(b.name, "spam"));
		std::optional<Packet> blocked = waitForPacket(*a.game, aFrom, "SM_BLOCK_RESPONSE", [](const Packet&) { return true; });
		ASSERT_TRUE(blocked) << "SocialService.addBlockedUser: SM_BLOCK_RESPONSE";
		EXPECT_EQ(decoders::decodeBlockResponse(blocked->data).code, decoders::BLOCK_RESPONSE_BLOCK_SUCCESSFUL);
		std::optional<Packet> blockList = waitForPacket(*a.game, aFrom, "SM_BLOCK_LIST", [](const Packet&) { return true; }, 5s);
		ASSERT_TRUE(blockList);
		EXPECT_EQ(decoders::decodeBlockList(blockList->data).blocked, (std::vector<std::pair<std::string, std::string>>{{b.name, "spam"}}));
		EXPECT_EQ(count("SELECT COUNT(*) FROM blocks WHERE player = " + std::to_string(a.playerId) + " AND blocked_player = " +
		                std::to_string(b.playerId) + " AND reason = 'spam'"),
		          1);

		aFrom = a.mark();
		b.say("after the block");
		collectFor(*a.game, SILENCE);
		for (const decoders::Message& m : messagesIn(a.since(aFrom)))
			EXPECT_NE(m.senderObjectId, b.playerId) << "a blocked player's line reached A: " << m.message;

		size_t bWhisper = b.mark();
		b.send(GameSession::CM_CHAT_MESSAGE_WHISPER, GameSession::buildCM_CHAT_MESSAGE_WHISPER(a.name, "psst"));
		const std::optional<decoders::SystemMessage> excluded = waitForSystemMessage(*b.game, bWhisper, message("STR_YOU_EXCLUDED"));
		ASSERT_TRUE(excluded) << "CM_CHAT_MESSAGE_WHISPER.java: the block arm: " << idsText(systemMessageIds(b.since(bWhisper)));
		EXPECT_EQ(excluded->params, std::vector<std::string>{a.name});

		bWhisper = b.mark();
		b.send(GameSession::CM_BLOCK_ADD, GameSession::buildCM_BLOCK_ADD(a.name, "back"));
		EXPECT_TRUE(waitForPacket(*b.game, bWhisper, "SM_BLOCK_RESPONSE", [](const Packet& p) {
			return decoders::decodeBlockResponse(p.data).code == decoders::BLOCK_RESPONSE_BLOCK_SUCCESSFUL;
		})) << "B blocks A";
		aFrom = a.mark();
		a.send(GameSession::CM_CHAT_MESSAGE_WHISPER, GameSession::buildCM_CHAT_MESSAGE_WHISPER(b.name, "psst"));
		const std::optional<decoders::SystemMessage> level = waitForSystemMessage(*a.game, aFrom, message("STR_CANT_WHISPER_LEVEL"));
		ASSERT_TRUE(level) << "the level arm (A is level 1): " << idsText(systemMessageIds(a.since(aFrom)));
		EXPECT_EQ(level->params, std::vector<std::string>{std::to_string(whisperLevel)});
		collectFor(*a.game, 500ms);
		const std::vector<int32_t> ids = systemMessageIds(a.since(aFrom));
		EXPECT_EQ(std::count(ids.begin(), ids.end(), message("STR_YOU_EXCLUDED")), 0) << "the level arm comes before the block arm (§3.6)";
	});

	// ---- Z5 / Z6: the duel ----
	runCase("Z5", "A asks B for a duel: B is asked, A gets the withdraw question; B accepts: SM_DUEL started to both", [&] {
		size_t aFrom = a.mark();
		size_t bFrom = b.mark();
		a.send(GameSession::CM_DUEL_REQUEST, GameSession::buildCM_DUEL_REQUEST(b.playerId));
		std::optional<Packet> asked = waitForPacket(*b.game, bFrom, "SM_QUESTION_WINDOW", [&](const Packet& p) {
			return decoders::decodeQuestionWindow(p.data).code == question("STR_DUEL_DO_YOU_ACCEPT_REQUEST");
		});
		ASSERT_TRUE(asked) << "DuelService.onDuelRequest: B's question: " << join(namesOf(b.since(bFrom)));
		EXPECT_EQ(decoders::decodeQuestionWindow(asked->data).params[0], a.name);
		const std::optional<decoders::SystemMessage> requested = waitForSystemMessage(*b.game, bFrom, message("STR_DUEL_REQUESTED"), 5s);
		ASSERT_TRUE(requested) << "STR_DUEL_REQUESTED to B";
		EXPECT_EQ(requested->params, std::vector<std::string>{a.name});
		EXPECT_TRUE(waitForPacket(*a.game, aFrom, "SM_QUESTION_WINDOW", [&](const Packet& p) {
			return decoders::decodeQuestionWindow(p.data).code == question("STR_DUEL_DO_YOU_WITHDRAW_REQUEST");
		})) << "DuelService.confirmDuelWith: A's withdraw question (same race: not enemies)";

		aFrom = a.mark();
		bFrom = b.mark();
		b.send(GameSession::CM_QUESTION_RESPONSE, GameSession::buildCM_QUESTION_RESPONSE(question("STR_DUEL_DO_YOU_ACCEPT_REQUEST"), 1));
		for (auto [self, other] : {std::pair{&a, &b}, std::pair{&b, &a}}) {
			std::optional<Packet> started = waitForPacket(*self->game, self == &a ? aFrom : bFrom, "SM_DUEL", [](const Packet& p) {
				return decoders::decodeDuel(p.data).type == decoders::DUEL_TYPE_STARTED;
			});
			ASSERT_TRUE(started) << self->label << ": no SM_DUEL_STARTED (DuelService.startDuel)";
			EXPECT_EQ(decoders::decodeDuel(started->data).requesterObjectId, other->playerId) << "SM_DUEL_STARTED carries the opponent";
		}
	});

	runCase("Z6", "B attacks A until A would die: A gets no SM_DIE and keeps Java's 33 % floor; the results are lost and won", [&] {
		const std::optional<decoders::StatsInfo> stats = lastStats(b);
		ASSERT_TRUE(stats) << "no SM_STATS_INFO for B";
		const size_t aFrom = a.mark();
		const size_t bFrom = b.mark();
		b.send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(a.playerId));
		waitFor(*b.game, "SM_TARGET_SELECTED", 10s);
		const GameSession::FightOutcome outcome = b.game->fightUntil(
			a.playerId, std::chrono::milliseconds(stats->attackSpeed),
			[](const Packet& packet) { return packet.name == "SM_DUEL" && decoders::decodeDuel(packet.data).type == decoders::DUEL_TYPE_RESULT; }, 120s,
			80);
		ASSERT_TRUE(outcome.done) << "the duel did not end within " << outcome.elapsed.count() << " ms (" << outcome.attacksSent
		                          << " CM_ATTACK): " << join(namesOf(b.since(bFrom)));
		std::optional<Packet> won = waitForPacket(*b.game, bFrom, "SM_DUEL", [](const Packet& p) {
			return decoders::decodeDuel(p.data).type == decoders::DUEL_TYPE_RESULT;
		}, 1s);
		ASSERT_TRUE(won);
		const decoders::Duel bResult = decoders::decodeDuel(won->data);
		EXPECT_EQ(bResult.resultId, decoders::DUEL_RESULT_WON);
		EXPECT_EQ(bResult.messageId, decoders::DUEL_WON_MESSAGE);
		EXPECT_EQ(bResult.name, a.name);
		std::optional<Packet> lost = waitForPacket(*a.game, aFrom, "SM_DUEL", [](const Packet& p) {
			return decoders::decodeDuel(p.data).type == decoders::DUEL_TYPE_RESULT;
		}, 5s);
		ASSERT_TRUE(lost) << "A: no SM_DUEL result (DuelService.loseDuel)";
		const decoders::Duel aResult = decoders::decodeDuel(lost->data);
		EXPECT_EQ(aResult.resultId, decoders::DUEL_RESULT_LOST);
		EXPECT_EQ(aResult.messageId, decoders::DUEL_LOST_MESSAGE);
		EXPECT_EQ(aResult.name, b.name);
		collectFor(*a.game, SILENCE);
		EXPECT_TRUE(ofName(a.since(aFrom), "SM_DIE").empty()) << "PlayerController.onDie's duel arm returns before the resurrection options";
		// setCurrentHpPercent(33): (int) ((long) maxHp * 33 / 100) (CreatureLifeStats.java:339-341), sent as A's SM_STATUPDATE_HP
		bool floored = false;
		int32_t lastHp = -1;
		for (const Packet& packet : ofName(a.since(aFrom), "SM_STATUPDATE_HP")) {
			const decoders::StatUpdateHp hp = decoders::decodeStatUpdateHp(packet.data);
			floored = floored || hp.currentHp == static_cast<int32_t>(static_cast<int64_t>(hp.maxHp) * DUEL_FLOOR_PERCENT / 100);
			lastHp = hp.currentHp;
		}
		EXPECT_TRUE(floored) << "no SM_STATUPDATE_HP at 33 % of A's max HP: " << join(namesOf(a.since(aFrom)));
		EXPECT_GT(lastHp, 0) << "A is alive";
		for (const Packet& packet : ofName(a.since(aFrom), "SM_STATUPDATE_MP")) {
			const decoders::StatUpdateMp mp = decoders::decodeStatUpdateMp(packet.data);
			EXPECT_GE(static_cast<int64_t>(mp.currentMp) * 100, static_cast<int64_t>(mp.maxMp) * DUEL_FLOOR_PERCENT - 100) << "A's MP floor";
		}
		std::cout << "Z6: B won the duel in " << outcome.elapsed.count() << " ms, " << outcome.attacksSent << " attacks" << std::endl;
	});

	// ---- Z7: prison ----
	runCase("Z7", "G: //sprison B 1 test puts B into LF_PRISON with a punishment row and refuses his chat; //rprison B brings him out", [&] {
		size_t bFrom = b.mark();
		size_t gFrom = g.mark();
		g.say("//sprison " + b.name + " 1 test");
		const decoders::PlayerSpawn prison = followInstantTeleport(b, bFrom);
		EXPECT_EQ(prison.worldId, LF_PRISON) << "TeleportService.teleportToPrison (Elyos)";
		EXPECT_NEAR(prison.x, PRISON_X, 0.01);
		EXPECT_NEAR(prison.y, PRISON_Y, 0.01);
		EXPECT_NEAR(prison.z, PRISON_Z, 0.01);
		EXPECT_TRUE(waitForInfo(*b.game, bFrom, "You have been teleported to prison for a time of 1 minutes.\n If you disconnect the time stops and "
		                                         "the timer of the prison'll see at your next login."))
			<< "PunishmentService.setIsInPrison: " << join(infoTexts(b.since(bFrom)), " | ");
		EXPECT_TRUE(waitForInfo(*g.game, gFrom, "Player " + b.name + " sent to prison for 1 because Test.")) << "SPrison.java (Util.convertName)";
		// PunishmentService.setIsInPrison passes the minutes to ChatBanService.banPlayer(player, durationMillis): a 1 ms gag whose GAG task
		// unbans B at once (registerUnban), with STR_CAN_CHAT_NOW - Java's behaviour, the correction proposed with CP2
		EXPECT_TRUE(waitForSystemMessage(*b.game, bFrom, message("STR_CAN_CHAT_NOW"), 5s)) << "the 1 ms prison gag's unban";
		EXPECT_EQ(database.queryString(schema(), "SELECT reason FROM player_punishments WHERE player_id = " + std::to_string(b.playerId) +
		                                           " AND punishment_type = 'PRISON'")
		            .value_or("<no row>"),
		          "Test");
		bFrom = b.mark();
		b.say("let me out");
		const std::optional<decoders::SystemMessage> refused = waitForSystemMessage(*b.game, bFrom, message("STR_INGAME_BLOCK_IN_NO_CHAT"));
		ASSERT_TRUE(refused) << "PlayerRestrictions.canChat's prison arm";
		EXPECT_EQ(refused->params, std::vector<std::string>{"1"}) << "getPrisonDurationSeconds() / 60 + 1 within the first minute";

		bFrom = b.mark();
		gFrom = g.mark();
		g.say("//rprison " + b.name);
		const decoders::PlayerSpawn out = followInstantTeleport(b, bFrom);
		EXPECT_EQ(out.worldId, POETA) << "TeleportService.moveToBindLocation: a new character's bind point is on Poeta";
		EXPECT_TRUE(waitForInfo(*b.game, bFrom, "You come out of prison.")) << "PunishmentService.setIsInPrison(false)";
		collectFor(*b.game, 500ms);
		const std::vector<int32_t> afterRelease = systemMessageIds(b.since(bFrom));
		EXPECT_EQ(std::count(afterRelease.begin(), afterRelease.end(), message("STR_CAN_CHAT_NOW")), 0)
			<< "ChatBanService.unbanPlayer: the 1 ms gag is gone already, so chatBans.remove finds nothing to announce";
		EXPECT_TRUE(waitForInfo(*g.game, gFrom, "Player " + b.name + " removed from prison.")) << "RPrison.java";
		EXPECT_EQ(count("SELECT COUNT(*) FROM player_punishments WHERE player_id = " + std::to_string(b.playerId)), 0)
			<< "PlayerPunishmentsDAO.unpunishPlayer";
	});

	// ---- Z8: the abyss ranking ----
	runCase("Z8", "CM_ABYSS_RANKING_PLAYERS: the list, then the short answer; //ranking update resets the flags", [&] {
		const auto request = [&]() {
			const size_t from = a.mark();
			a.send(GameSession::CM_ABYSS_RANKING_PLAYERS, GameSession::buildCM_ABYSS_RANKING_PLAYERS(0));
			collectFor(*a.game, SILENCE);
			std::vector<decoders::AbyssRankingPlayers> answers;
			for (const Packet& packet : ofName(a.since(from), "SM_ABYSS_RANKING_PLAYERS"))
				answers.push_back(decoders::decodeAbyssRankingPlayers(packet.data));
			return answers;
		};
		const auto isShort = [](const decoders::AbyssRankingPlayers& p) { return p.page == 0 && p.endFlag == 0 && p.players.empty(); };
		const std::vector<decoders::AbyssRankingPlayers> first = request();
		for (const decoders::AbyssRankingPlayers& page : first)
			EXPECT_FALSE(isShort(page)) << "the first request is the cache's list (AbyssRankingCache.getPlayers), never the short answer";
		const std::vector<decoders::AbyssRankingPlayers> second = request();
		ASSERT_EQ(second.size(), 1u) << "the list was marked sent: one short answer";
		EXPECT_TRUE(isShort(second[0]));
		EXPECT_EQ(second[0].race, 0) << "Race.ELYOS";
		EXPECT_GT(second[0].lastUpdate, 0) << "AbyssRankingCache.getLastUpdate: the startup's refresh time in seconds";

		const size_t logFrom = servers.gameServer()->findLogLines("AbyssRankUpdateService: Finished in").size();
		g.say("//ranking update");
		bool finished = false;
		for (int i = 0; i < 60 && !finished; i++) {
			finished = servers.gameServer()->findLogLines("AbyssRankUpdateService: Finished in").size() > logFrom;
			if (!finished)
				collectFor(*a.game, 250ms);
		}
		ASSERT_TRUE(finished) << "AbyssRankUpdateService.performUpdate did not finish within 15 s (the 1000 ms delay, then the update)";
		EXPECT_FALSE(servers.gameServer()->findLogLines("AbyssRankUpdateService: Executing rank update...").empty());
		const std::vector<decoders::AbyssRankingPlayers> third = request();
		for (const decoders::AbyssRankingPlayers& page : third)
			EXPECT_FALSE(isShort(page)) << "reloadRankings reset the flag (resetAbyssRankListUpdated): the list again, not the short answer";
		const std::vector<decoders::AbyssRankingPlayers> fourth = request();
		ASSERT_EQ(fourth.size(), 1u);
		EXPECT_TRUE(isShort(fourth[0]));
		EXPECT_GE(fourth[0].lastUpdate, second[0].lastUpdate) << "refreshCache's new update time";
	});

	// ---- the stage-2 cases (m5j-plan.md §10.4 Z9-Z11, §18.3 CP5): A as he left Z8, a level-1 Warrior on Poeta; G watches ----
	/** the item G's //add gives A: its object id from A's SM_INVENTORY_ADD_ITEM */
	const auto giveA = [&](int32_t itemId) -> int32_t {
		const size_t aFrom = a.mark();
		const size_t gFrom = g.mark();
		g.say("//add " + a.name + " " + std::to_string(itemId));
		std::optional<Packet> added = waitForPacket(*a.game, aFrom, "SM_INVENTORY_ADD_ITEM", [&](const Packet& p) {
			const decoders::InventoryAddItem add = decoders::decodeInventoryAddItem(p.data);
			return !add.items.empty() && add.items[0].templateId == itemId;
		}, 10s);
		if (!added)
			throw std::runtime_error("//add " + std::to_string(itemId) + ": A got no SM_INVENTORY_ADD_ITEM: " + join(namesOf(a.since(aFrom))) +
			                         " | G: " + join(infoTexts(g.since(gFrom)), " | "));
		EXPECT_TRUE(waitForMessage(*g.game, gFrom, [&](const decoders::Message& m) {
			return m.chatType == CHAT_GOLDEN_YELLOW && m.message.starts_with("You gave 1 x ");
		}, 5s)) << "Add.java: G's 'You gave' line";
		return decoders::decodeInventoryAddItem(added->data).items[0].objectId;
	};
	const auto emotionOf = [](const Packet& p) { return decoders::decodeEmotion(p.data); };
	/** SM_PET decoded; an undecodable body is a failure that names its bytes, and the waits go on */
	const auto petOf = [](const Packet& p) -> std::optional<decoders::Pet> {
		try {
			return decoders::decodePet(p.data);
		} catch (const DecodeError& e) {
			std::string hex;
			for (uint8_t byte : p.data)
				hex += "0123456789abcdef"[byte >> 4], hex += "0123456789abcdef"[byte & 15];
			ADD_FAILURE() << e.what() << " (" << p.data.size() << " bytes: " << hex << ")";
			return std::nullopt;
		}
	};

	// ---- Z9: a ride ----
	runCase("Z9", "A mounts the oracle's ride item: after its casting delay A and G see CHANGE_SPEED, RIDE(npcId) and the closing item "
	              "animation; the second use dismounts (CHANGE_SPEED, RIDE_END)", [&] {
		const nlohmann::json& ride = items.at("ride");
		const int32_t rideItem = ride.at("itemId").get<int32_t>();
		const int32_t rideNpc = ride.at("npcId").get<int32_t>();
		const int32_t objectId = giveA(rideItem);
		size_t aFrom = a.mark();
		size_t gFrom = g.mark();
		a.send(GameSession::CM_USE_ITEM, GameSession::buildCM_USE_ITEM(objectId));
		if (ride.at("castingDelay").get<int32_t>() > 0) {
			std::optional<Packet> opening = waitForPacket(*a.game, aFrom, "SM_ITEM_USAGE_ANIMATION", [&](const Packet& p) {
				const decoders::ItemUsageAnimation animation = decoders::decodeItemUsageAnimation(p.data);
				return animation.itemObjectId == objectId && animation.end == 0;
			}, 10s);
			ASSERT_TRUE(opening) << "RideAction.act: the casting animation: " << join(namesOf(a.since(aFrom)));
			EXPECT_EQ(decoders::decodeItemUsageAnimation(opening->data).time, ride.at("castingDelay").get<int32_t>());
		}
		const std::optional<decoders::SystemMessage> used = waitForSystemMessage(*a.game, aFrom, message("STR_USE_ITEM"), 15s);
		ASSERT_TRUE(used) << "RideAction.finishUse: STR_USE_ITEM: " << join(namesOf(a.since(aFrom)));
		for (ScenarioClient* client : {&a, &g}) {
			const size_t from = client == &a ? aFrom : gFrom;
			const auto emotion = [&](int32_t type, int32_t target) {
				return waitForPacket(*client->game, from, "SM_EMOTION", [&](const Packet& p) {
					const decoders::Emotion e = emotionOf(p);
					return e.senderObjectId == a.playerId && e.emotionType == type && e.targetObjectId == target;
				}, 10s);
			};
			EXPECT_TRUE(emotion(emotionId("CHANGE_SPEED"), 0)) << client->label << ": SM_EMOTION(CHANGE_SPEED, 0, 0) (RideAction.java:152)";
			EXPECT_TRUE(emotion(emotionId("RIDE"), rideNpc)) << client->label << ": SM_EMOTION(RIDE, 0, " << rideNpc << ") (RideAction.java:153)";
			EXPECT_TRUE(waitForPacket(*client->game, from, "SM_ITEM_USAGE_ANIMATION", [&](const Packet& p) {
				const decoders::ItemUsageAnimation animation = decoders::decodeItemUsageAnimation(p.data);
				return animation.playerObjectId == a.playerId && animation.itemId == rideItem && animation.end == 1 && animation.time == 0;
			}, 10s)) << client->label << ": SM_ITEM_USAGE_ANIMATION(..., 0, 1, 1) (RideAction.java:154-155)";
		}

		aFrom = a.mark();
		gFrom = g.mark();
		a.send(GameSession::CM_USE_ITEM, GameSession::buildCM_USE_ITEM(objectId));
		for (ScenarioClient* client : {&a, &g}) {
			const size_t from = client == &a ? aFrom : gFrom;
			EXPECT_TRUE(waitForPacket(*client->game, from, "SM_EMOTION", [&](const Packet& p) {
				const decoders::Emotion e = emotionOf(p);
				return e.senderObjectId == a.playerId && e.emotionType == emotionId("RIDE_END");
			}, 10s)) << client->label << ": the dismount's SM_EMOTION(RIDE_END) (PlayerActions.java:56): " << join(namesOf(client->since(from)));
		}
		EXPECT_FALSE(firstOfName(a.since(aFrom), "SM_ITEM_USAGE_ANIMATION")) << "act's first arm only unsets the mode: no casting";
	});

	// ---- Z11: a toy pet ----
	runCase("Z11", "A adopts the oracle's pet from its egg (CM_PET ADOPT: SM_PET ADOPT with the pet's specialties), summons it (SM_PET SPAWN), "
	               "relogs: the player_pets row and SM_PET LOAD_PETS carry it", [&] {
		const nlohmann::json& pet = items.at("pet");
		const int32_t petId = pet.at("petId").get<int32_t>();
		const int32_t eggObjectId = giveA(pet.at("eggItemId").get<int32_t>());
		size_t aFrom = a.mark();
		a.send(GameSession::CM_PET, GameSession::buildCM_PET_ADOPT(eggObjectId, petId, 0, PET_NAME));
		std::optional<Packet> adopted = waitForPacket(*a.game, aFrom, "SM_PET", [&](const Packet& p) {
			const std::optional<decoders::Pet> decoded = petOf(p);
			return decoded && decoded->action == decoders::PET_ACTION_ADOPT;
		}, 10s);
		ASSERT_TRUE(adopted) << "PetAdoptionService.addPet: SM_PET(ADOPT): " << join(namesOf(a.since(aFrom)));
		const decoders::PetData data = *decoders::decodePet(adopted->data).adopted;
		EXPECT_EQ(data.name, PET_NAME) << "Util.convertName of a capitalised name";
		EXPECT_EQ(data.templateId, petId);
		EXPECT_EQ(data.masterObjectId, a.playerId);
		EXPECT_EQ(data.secondsUntilExpiration, 0) << "an egg without minutes";
		std::vector<int32_t> specialties;
		for (const decoders::PetFunction& function : data.functions)
			specialties.push_back(function.id);
		EXPECT_EQ(specialties, pet.at("writtenSpecialtyIds").get<std::vector<int32_t>>()) << "SM_PET.writePetData's specialties";
		EXPECT_EQ(count("SELECT COUNT(*) FROM player_pets WHERE player_id = " + std::to_string(a.playerId) + " AND template_id = " +
		                std::to_string(petId) + " AND id = " + std::to_string(data.objectId)), 1) << "PlayerPetsDAO.insertPlayerPet";

		aFrom = a.mark();
		const size_t gFrom = g.mark();
		a.send(GameSession::CM_PET, GameSession::buildCM_PET(GameSession::PET_SPAWN, petId));
		for (ScenarioClient* client : {&a, &g}) {
			std::optional<Packet> spawned = waitForPacket(*client->game, client == &a ? aFrom : gFrom, "SM_PET", [&](const Packet& p) {
				const std::optional<decoders::Pet> decoded = petOf(p);
				return decoded && decoded->action == decoders::PET_ACTION_SPAWN && decoded->spawn->objectId == data.objectId;
			}, 10s);
			ASSERT_TRUE(spawned) << client->label << ": SM_PET(SPAWN) of the summoned pet (PlayerController.see)";
			const decoders::PetSpawn spawn = *decoders::decodePet(spawned->data).spawn;
			EXPECT_EQ(spawn.templateId, petId);
			EXPECT_EQ(spawn.masterObjectId, a.playerId);
			EXPECT_EQ(spawn.name, PET_NAME);
		}

		disconnect(a);
		const std::vector<Packet> burst = enterGame(servers, a);
		std::optional<decoders::Pet> loaded;
		for (const Packet& packet : burst)
			if (packet.name == "SM_PET")
				if (std::optional<decoders::Pet> decoded = petOf(packet); decoded && decoded->action == decoders::PET_ACTION_LOAD_PETS)
					loaded = std::move(decoded);
		ASSERT_TRUE(loaded) << "PetService.onPlayerLogin: SM_PET(LOAD_PETS) in the enter world";
		const std::vector<decoders::PetData> pets = loaded->pets;
		ASSERT_EQ(pets.size(), 1u);
		EXPECT_EQ(pets[0].objectId, data.objectId) << "the pet persists (player_pets)";
		EXPECT_EQ(pets[0].name, PET_NAME);
		EXPECT_EQ(pets[0].templateId, petId);
	});

	// ---- Z10: a kisk ----
	runCase("Z10", "A puts the oracle's kisk (SM_NPC_INFO of it with A as creator), binds through its question (SM_KISK_UPDATE, the kisk "
	               "bind point); G's //kill on A offers the kisk revive, which brings A back at the kisk", [&] {
		const nlohmann::json& kisk = items.at("kisk");
		const int32_t kiskNpc = kisk.at("npcId").get<int32_t>();
		const int32_t objectId = giveA(kisk.at("itemId").get<int32_t>());
		size_t aFrom = a.mark();
		a.send(GameSession::CM_USE_ITEM, GameSession::buildCM_USE_ITEM(objectId));
		std::optional<Packet> npc = waitForPacket(*a.game, aFrom, "SM_NPC_INFO", [&](const Packet& p) {
			return decoders::decodeNpcInfo(p.data).templateId == kiskNpc;
		}, std::chrono::milliseconds(kisk.at("castingDelay").get<int32_t>()) + 15s);
		ASSERT_TRUE(npc) << "ToyPetSpawnAction.finishUse: the kisk's SM_NPC_INFO: " << join(namesOf(a.since(aFrom)));
		const decoders::NpcInfo kiskInfo = decoders::decodeNpcInfo(npc->data);
		EXPECT_EQ(kiskInfo.creatorId, a.playerId) << "Kisk: setCreatorId(owner)";
		EXPECT_LT(std::hypot(kiskInfo.x - a.x, kiskInfo.y - a.y), 0.5f) << "spawned at A's position";
		std::optional<Packet> asked = waitForPacket(*a.game, aFrom, "SM_QUESTION_WINDOW", [&](const Packet& p) {
			return decoders::decodeQuestionWindow(p.data).code == question("STR_ASK_REGISTER_BINDSTONE");
		}, 10s);
		ASSERT_TRUE(asked) << "members > 1: KiskAI.handleDialogStart's question (ToyPetSpawnAction.java:110-111)";

		aFrom = a.mark();
		a.send(GameSession::CM_QUESTION_RESPONSE, GameSession::buildCM_QUESTION_RESPONSE(question("STR_ASK_REGISTER_BINDSTONE"), 1));
		ASSERT_TRUE(waitForSystemMessage(*a.game, aFrom, message("STR_BINDSTONE_REGISTER"), 10s)) << "KiskService.onBind";
		std::optional<Packet> update = waitForPacket(*a.game, aFrom, "SM_KISK_UPDATE", [](const Packet&) { return true; }, 5s);
		ASSERT_TRUE(update) << "Kisk.addPlayer: broadcastKiskUpdate";
		const decoders::KiskUpdate before = decoders::decodeKiskUpdate(update->data);
		EXPECT_EQ(before.kiskObjectId, kiskInfo.objectId);
		EXPECT_EQ(before.currentMembers, 1);
		EXPECT_EQ(before.maxMembers, kisk.at("maxMembers").get<int32_t>());
		EXPECT_EQ(before.remainingResurrects, kisk.at("maxResurrects").get<int32_t>());
		EXPECT_EQ(before.useMask, kisk.at("useMask").get<int32_t>());
		EXPECT_GT(before.remainingLifetimeSeconds, kisk.at("lifetimeSeconds").get<int32_t>() - 60);
		std::optional<Packet> bindPoint = waitForPacket(*a.game, aFrom, "SM_BIND_POINT_INFO", [](const Packet&) { return true; }, 5s);
		ASSERT_TRUE(bindPoint) << "TeleportService.sendKiskBindPoint";
		EXPECT_EQ(decoders::decodeBindPointInfo(bindPoint->data).type, 4) << "4: kisk";
		EXPECT_EQ(decoders::decodeBindPointInfo(bindPoint->data).kiskObjectId, kiskInfo.objectId);

		g.send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(a.playerId));
		waitFor(*g.game, "SM_TARGET_SELECTED", 10s);
		aFrom = a.mark();
		const size_t gFrom = g.mark();
		g.say("//kill");
		ASSERT_TRUE(waitForInfo(*g.game, gFrom, "Killed player: " + charName(a.name))) << "Kill.java: " << join(infoTexts(g.since(gFrom)), " | ");
		std::optional<Packet> died = waitForPacket(*a.game, aFrom, "SM_DIE", [](const Packet&) { return true; }, 10s);
		ASSERT_TRUE(died) << "A died";
		const decoders::Die offer = decoders::decodeDie(died->data);
		EXPECT_GT(offer.remainingKiskTimeSeconds, 0) << "SM_DIE offers the kisk revive (Kisk.getRemainingLifetime)";
		EXPECT_LE(offer.remainingKiskTimeSeconds, kisk.at("lifetimeSeconds").get<int32_t>());

		aFrom = a.mark();
		a.send(GameSession::CM_REVIVE, GameSession::buildCM_REVIVE(GameSession::KISK_REVIVE));
		std::optional<Packet> used = waitForPacket(*a.game, aFrom, "SM_KISK_UPDATE", [&](const Packet& p) {
			return decoders::decodeKiskUpdate(p.data).remainingResurrects == before.remainingResurrects - 1;
		}, 10s);
		EXPECT_TRUE(used) << "Kisk.resurrectionUsed: one resurrection fewer";
		// TeleportService.teleportTo on the same map is spawnOnSameMap (TeleportService.java:208-219): SM_CHANNEL_INFO, then A's own SM_PLAYER_INFO
		// at the kisk, no SM_PLAYER_SPAWN
		std::optional<Packet> respawned = waitForPacket(*a.game, aFrom, "SM_PLAYER_INFO", [&](const Packet& p) {
			return decoders::decodePlayerInfo(p.data).objectId == a.playerId;
		}, 10s);
		ASSERT_TRUE(respawned) << "PlayerReviveService.kiskRevive: teleportTo(kisk): " << join(namesOf(a.since(aFrom)));
		const decoders::PlayerInfo at = decoders::decodePlayerInfo(respawned->data);
		EXPECT_LT(std::hypot(at.x - kiskInfo.x, at.y - kiskInfo.y), 0.5f) << "A stands at the kisk";
		a.x = at.x;
		a.y = at.y;
		a.z = at.z;
		EXPECT_FALSE(firstOfName(a.since(aFrom), "SM_DIE")) << "A is alive";

		// G removes the kisk (Delete.java: a single-time spawn is not saved, SpawnsData.java:211): KiskAI.handleDespawned -> removeKisk
		g.send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(kiskInfo.objectId));
		waitFor(*g.game, "SM_TARGET_SELECTED", 10s);
		collectFor(*a.game, 1500ms); // the respawn's own SM_KISK_UPDATE (PlayerController.see of A's kisk) comes before the mark
		aFrom = a.mark();
		g.say("//delete");
		EXPECT_TRUE(waitForPacket(*a.game, aFrom, "SM_DELETE", [&](const Packet& p) {
			return decoders::decodeDelete(p.data).objectId == kiskInfo.objectId;
		}, 10s)) << "the kisk leaves A's view";
		ASSERT_TRUE(waitForSystemMessage(*a.game, aFrom, message("STR_BINDSTONE_IS_REMOVED"), 10s)) << "KiskAI.handleDespawned";
		// KiskAI.handleDespawned calls removeKisk (the creator's SM_KISK_UPDATE) before it broadcasts STR_BINDSTONE_IS_REMOVED
		bool updatedBeforeRemoved = false;
		for (const Packet& packet : a.since(aFrom)) {
			if (packet.name == "SM_SYSTEM_MESSAGE" && decoders::decodeSystemMessage(packet.data).messageId == message("STR_BINDSTONE_IS_REMOVED"))
				break;
			updatedBeforeRemoved = updatedBeforeRemoved || packet.name == "SM_KISK_UPDATE";
		}
		EXPECT_TRUE(updatedBeforeRemoved) << "KiskService.removeKisk: the creator's SM_KISK_UPDATE: " << join(namesOf(a.since(aFrom)));
	});

	// ---- Z14: a PvP kill ----
	runCase("Z14", "A (a level-10 Daeva with 1000 AP) and G2 (a level-13 Asmodian Daeva): G2's //kill on A pays the oracle's AP", [&] {
		disconnect(a);
		ASSERT_EQ(count("SELECT COUNT(*) FROM abyss_rank WHERE player_id = " + std::to_string(a.playerId)), 1) << "A's abyss_rank row";
		seedDaeva(a, A_PVP_LEVEL, at(0.0f, 0.0f));
		database.execute(schema(), "UPDATE abyss_rank SET ap = " + std::to_string(A_SEED_AP) + " WHERE player_id = " + std::to_string(a.playerId));
		const std::vector<Packet> aBurst = enterGame(servers, a);
		const std::vector<Packet> g2Burst = enterGame(servers, g2);
		const auto enterRank = [](const std::vector<Packet>& burst) {
			const Packet* rank = firstOfName(burst, "SM_ABYSS_RANK");
			return rank == nullptr ? std::optional<decoders::AbyssRank>() : decoders::decodeAbyssRank(rank->data);
		};
		const std::optional<decoders::AbyssRank> aBefore = enterRank(aBurst);
		const std::optional<decoders::AbyssRank> g2Before = enterRank(g2Burst);
		ASSERT_TRUE(aBefore && g2Before) << "SM_ABYSS_RANK in the enter worlds (PlayerEnterWorldService.java:290)";
		ASSERT_EQ(aBefore->ap, kill.at("victim").at("apBefore").get<int64_t>()) << "A's seeded AP";
		ASSERT_EQ(g2Before->ap, kill.at("winner").at("apBefore").get<int64_t>());
		ASSERT_EQ(g2.worldId, POETA) << "G2 enters at its seed beside A";
		// CM_LEVEL_READY started A's protection (PlayerController.startProtectionActiveTask: BLINKING, 60 s), under which onAttack returns at
		// once (PlayerController.java:439-440); a step ends it (CM_MOVE.java:140-141)
		a.send(GameSession::CM_MOVE, GameSession::buildCM_MOVE(a.x + 0.3f, a.y, a.z, 0, 0));
		collectFor(*a.game, 500ms);

		size_t g2From = g2.mark();
		g2.say("//enemy cancel");
		ASSERT_TRUE(waitForInfo(*g2.game, g2From, "You appear regular to everyone again.")) << "Enemy.java: the login command's neutrality ends";
		g2.send(GameSession::CM_TARGET_SELECT, GameSession::buildCM_TARGET_SELECT(a.playerId));
		waitFor(*g2.game, "SM_TARGET_SELECTED", 10s);
		const size_t aFrom = a.mark();
		g2From = g2.mark();
		g2.say("//kill");
		ASSERT_TRUE(waitForInfo(*g2.game, g2From, "Killed player: " + charName(a.name))) << "Kill.java: " << join(infoTexts(g2.since(g2From)), " | ");
		EXPECT_TRUE(waitForPacket(*a.game, aFrom, "SM_DIE", [](const Packet&) { return true; }, 10s)) << "A died (the resurrection options)";

		const auto rankAfter = [](ScenarioClient& client, size_t from, int64_t ap) {
			return waitForPacket(*client.game, from, "SM_ABYSS_RANK", [&](const Packet& p) { return decoders::decodeAbyssRank(p.data).ap == ap; }, 10s);
		};
		const int64_t aExpected = kill.at("victim").at("apAfter").get<int64_t>();
		const int64_t g2Expected = kill.at("winner").at("apAfter").get<int64_t>();
		EXPECT_TRUE(rankAfter(a, aFrom, aExpected)) << "A's SM_ABYSS_RANK with " << aExpected << " AP (calculatePvPApLost)";
		EXPECT_TRUE(rankAfter(g2, g2From, g2Expected)) << "G2's SM_ABYSS_RANK with " << g2Expected << " AP (calculatePvpApGained)";
		const std::optional<decoders::SystemMessage> used = waitForSystemMessage(*a.game, aFrom, message("STR_MSG_USE_ABYSSPOINT"), 2s);
		ASSERT_TRUE(used) << "AbyssPointsService.addAp: a loss is STR_MSG_USE_ABYSSPOINT(-added)";
		EXPECT_EQ(used->params, std::vector<std::string>{std::to_string(kill.at("victim").at("apLost").get<int64_t>())});
		const std::optional<decoders::SystemMessage> gained = waitForSystemMessage(*g2.game, g2From, message("STR_MSG_COMBAT_MY_ABYSS_POINT_GAIN"), 2s);
		ASSERT_TRUE(gained) << "AbyssPointsService.addAp: STR_MSG_COMBAT_MY_ABYSS_POINT_GAIN(added)";
		EXPECT_EQ(gained->params, std::vector<std::string>{std::to_string(kill.at("winner").at("apGained").get<int64_t>())});
		for (const Packet& packet : ofName(g2.since(g2From), "SM_ABYSS_RANK"))
			EXPECT_EQ(decoders::decodeAbyssRank(packet.data).allKill, 1) << "PvpService.doReward: incrementAllKills";
	});

	// ---- Z13: reports and shutdown (the M5a Q8 bar) ----
	cases.run("Z13a", "everyone logs out (G2 after its login announcement ran)", [&] {
		// GMService.scheduleBroadcastLogin's 15 s task holds G2 until it ran (GMService.java:84-98): a logout and a shutdown inside those 15 s
		// would leave it pending at the census. It answers either way, "has been announced" or "has not been announced"
		if (g2.game)
			EXPECT_TRUE(waitForMessage(*g2.game, 0, [](const decoders::Message& m) {
				return m.chatType == CHAT_GOLDEN_YELLOW && m.message.starts_with("Your login ha");
			}, 25s)) << "G2: the login announcement task did not run";
		for (ScenarioClient* client : {&g2, &c, &b})
			disconnect(*client);
		// A logs out dead: leaveWorld's bindRevive respawns him at his bind point on the same map (TeleportService.spawnOnSameMap). Measured:
		// with the stop right after the last logout the final census names A (refcount 1, no pending task); with 3 s between the logouts it is
		// empty, and an A revived before his logout is never named. Inferred, not traced: the holder is the short-lived queue spawnOnSameMap's
		// updateZone fills (ZoneUpdateService, an AbstractFIFOPeriodicTaskManager of 500 ms, as in Java). So G stays 2 s longer
		disconnect(a);
		collectFor(*g.game, 2000ms);
		disconnect(g);
	});
	const std::optional<int32_t> gameServerExit = servers.stopGameServer();
	const std::chrono::system_clock::time_point serverUpTo = std::chrono::system_clock::now();
	const std::optional<int32_t> loginServerExit = servers.stopLoginServer();
	cases.run("Z13", "reports: no AION_UNPORTED, the allow-list, no ERROR, the census, no Player alive", [&] {
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
		EXPECT_TRUE(unported.empty()) << "AION_UNPORTED sites were reached on the stage-1 path:\n" << join(unported, "\n") << cronNote;
		const std::vector<AllowlistEntry> allowlist = readAllowlist();
		ASSERT_FALSE(allowlist.empty()) << "tests/scenario/m5j_partial_allowlist.txt is empty or missing";
		std::map<std::string, int64_t> hitsByEntry;
		for (const PartialHit& hit : readPartialHits(servers)) {
			bool allowed = false;
			for (const AllowlistEntry& entry : allowlist)
				if (allowlistEntryMatches(entry.site, hit.site)) {
					allowed = true;
					hitsByEntry[entry.site] += hit.hits;
				}
			EXPECT_TRUE(allowed) << "the AION_PARTIAL site " << hit.site << " is not in tests/scenario/m5j_partial_allowlist.txt (" << hit.line << ")";
		}
		for (const AllowlistEntry& entry : allowlist) {
			if (entry.section == AllowlistSection::HitAtLeastOnce)
				EXPECT_GT(hitsByEntry[entry.site], 0) << "the section A row " << entry.site << " was never hit";
			else if (entry.section == AllowlistSection::HitNever)
				EXPECT_EQ(hitsByEntry[entry.site], 0) << "the section B row " << entry.site << " was hit " << hitsByEntry[entry.site] << " times";
		}
		EXPECT_TRUE(servers.readReportLines("census.txt").empty())
			<< "the final census reports leaks:\n" << join(servers.readReportLines("census.txt"), "\n") << "\n(the characters' object ids: G "
			<< g.playerId << ", A " << a.playerId << ", B " << b.playerId << ", C " << c.playerId << ", G2 " << g2.playerId << ")";
		EXPECT_TRUE(servers.readReportLines("lockdep.txt").empty()) << join(servers.readReportLines("lockdep.txt"), "\n");
		EXPECT_TRUE(servers.readReportLines("watchdog.txt").empty()) << join(servers.readReportLines("watchdog.txt"), "\n");
		const std::map<std::string, std::vector<std::string>> summary = servers.readSummary();
		const auto value = [&](std::string_view key) -> std::string {
			const auto found = summary.find(std::string(key));
			return found == summary.end() || found->second.empty() ? std::string() : found->second[0];
		};
		EXPECT_EQ(value("started"), "true");
		EXPECT_EQ(value("exitCode"), "0");
		// the duel's draw task, the prison task and the gag pin their players until they ran or were cut: a live leak names the Player
		EXPECT_EQ(value("liveLeaks"), "0") << join(summary.contains("liveLeak") ? summary.at("liveLeak") : std::vector<std::string>{});
		const auto notPorted = summary.find("notPortedClientPacket");
		if (notPorted != summary.end())
			ADD_FAILURE() << "the stage-1 path sent client packets that are not ported: " << join(notPorted->second);
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

/** `gs.scenario.m5j` (m5j-plan.md §10.4, stages 1 and 2): friends, blocks and whispers, note, macro and title, search, the duel, prison, the
 *  abyss ranking, a ride, a toy pet, a kisk and a PvP kill's AP */
TEST(M5jScenario, Run) {
	runM5jGate();
}

} // namespace aion::gameserver::scenario
