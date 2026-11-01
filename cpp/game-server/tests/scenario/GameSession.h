#pragma once

// GameSession (m5a-plan.md F-04): one game client connection of the scenario over FakeGameClient (tests/support): the client packet builders
// of the scripted path (ClientPackets of the client protocol library, cpp/client-protocol: layouts written from the Java readImpl methods, the
// leading fields only), a fixed MAC address and HDD serial, and a recorder of every server packet with its Java class name
// (ServerPacketsOpcodes). The bodies are decoded by the independent decoders of F-08 (stage 2, also in the client protocol library), never
// with the server's packet classes.
//
// ONE DEPENDENCY ON THE SERVER, worth knowing before a §5.8 sequence failure is blamed on a chunk: the CM opcodes this class sends are written
// out from the Java AionClientPacketFactory (ClientPackets), but nameOf() maps a RECEIVED opcode to a packet name through a table generated
// from the same Java file as the server's own (ClientOpcodes.gen.h, written by tools/gen/opcodes.py next to ServerPacketsOpcodes.gen.h). A
// packet class given the wrong opcode relative to Java's AionServerPacketsOpcodes would therefore be mislabelled identically on both sides
// and the §5.8 match would still succeed. What restores the independence is separate opcode-parity coverage that the gate does not depend on: tests/network_crypt/GeneratedOpcodesTest.cpp, tests/sm_ak/ServerPacketsAKOpcodeTest.cpp and
// tests/sm_lz/OpcodesAndSupportTest.cpp. A full ctest runs them; a bare `ctest -R ^gs.scenario.m5a$` does not.
//
// Second thing to know before blaming a chunk: collectUntilQuiet ends a burst at the first 1 s gap. A measured enter-world burst takes about
// 70 ms, so the headroom is large, but a 1 s stall on a loaded machine truncates the burst, and the gate then fails with a sequence error that
// looks like a port defect.

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "FakeGameClient.h"
#include "aion/clientprotocol/ClientPackets.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {

using clientprotocol::CharacterAppearance;
using clientprotocol::NewCharacter;

/** The client packet builders and opcodes (GameSession::buildCM_X, GameSession::CM_X) come from the client protocol library's ClientPackets */
class GameSession : public clientprotocol::ClientPackets {
public:
	/**
	 * The margin PlayerController.attackTarget allows on the attack interval: a CM_ATTACK that arrives less than `attackSpeed - 300` ms after
	 * the previous one is answered with SM_ATTACK_RESPONSE.STOP_WITHOUT_MESSAGE and nothing else happens (PlayerController.java:424-426,
	 * `milis - lastAttackMillis + 300 < attackSpeed`). fightUntil therefore paces at the full attack speed.
	 */
	static constexpr std::chrono::milliseconds ATTACK_INTERVAL_TOLERANCE{300};

	/** One recorded server packet */
	struct Packet {
		int32_t opcode = -1;
		std::string name;
		std::vector<uint8_t> data;
		std::chrono::steady_clock::time_point receivedAt;
	};

	explicit GameSession(uint16_t port);

	/** reads SM_KEY (recorded as SM_KEY) */
	int32_t readKey(std::chrono::milliseconds timeout = std::chrono::seconds(10));

	void send(int32_t opcode, std::span<const uint8_t> data);

	/** @return the next server packet (recorded), std::nullopt on timeout or close */
	std::optional<Packet> next(std::chrono::milliseconds timeout);

	/** reads until a packet with that name arrived (every packet on the way is recorded). @throws std::runtime_error on timeout or close */
	Packet expect(std::string_view name, std::chrono::milliseconds timeout = std::chrono::seconds(10));

	/** reads packets until none arrives for `quiet` (or `limit` passed) and returns the packets read by this call */
	std::vector<Packet> collectUntilQuiet(std::chrono::milliseconds quiet, std::chrono::milliseconds limit = std::chrono::seconds(60));

	const std::vector<Packet>& recorded() const noexcept { return packets; }

	/** the names of recorded packets [from, end) */
	std::vector<std::string> names(size_t from = 0) const;

	bool waitClosed(std::chrono::milliseconds timeout);

	/** "SM_X" for a known opcode, "SM_UNKNOWN_<opcode>" otherwise */
	static std::string nameOf(int32_t opcode);

	/** What one fightUntil call did */
	struct FightOutcome {
		/** the predicate answered true for one of the packets this call read */
		bool done = false;
		int32_t attacksSent = 0;
		/** the index in recorded() of the first packet this call recorded, so the caller can read the fight back packet by packet */
		size_t firstPacket = 0;
		std::chrono::milliseconds elapsed{0};
		/** the connection closed while fighting (the character was kicked, the server died) */
		bool closed = false;
	};

	/** Answers true for the packet that ends the fight; it sees every server packet from the call on, in arrival order, exactly once */
	using FightPredicate = std::function<bool(const Packet&)>;

	/**
	 * Sends CM_ATTACK(targetObjectId) every `attackSpeed` ms - the value SM_STATS_INFO carries for this character, which is the *minimum*
	 * interval the server accepts up to ATTACK_INTERVAL_TOLERANCE (m5b-plan.md G-02) - and records every server packet that arrives in
	 * between, feeding each to `done`. Returns when `done` answers true, when `timeout` has passed, when `maxAttacks` attacks have been sent
	 * and one more interval has been read, or when the connection closes. The first attack goes out immediately, so the caller stops moving
	 * and selects its target first (PlayerController.attackTarget widens the range check while the attacker is in move).
	 *
	 * @param attackNo is the running count of this call, truncated to a byte: CM_ATTACK reads it and never uses it (CM_ATTACK.java:38)
	 */
	FightOutcome fightUntil(int32_t targetObjectId, std::chrono::milliseconds attackSpeed, const FightPredicate& done,
		std::chrono::milliseconds timeout, int32_t maxAttacks = 60, uint8_t attackType = 0);

	/**
	 * A client packet castAndWait sends once while the cast is running, `after` the CM_CASTSPELL went out - X5's CM_MOVE 300 ms into a cast
	 * (m5b2-plan.md §10.3), which PlayerController.onStartMove answers by cancelling it. It is not sent if the cast has ended before.
	 */
	struct CastInterruption {
		std::chrono::milliseconds after{0};
		int32_t opcode = 0;
		std::vector<uint8_t> body;
	};

	/** What one castAndWait call saw; the indices are into recorded() */
	struct CastOutcome {
		/** the first packet this call recorded */
		size_t firstPacket = 0;
		/** when the CM_CASTSPELL was sent: X4's "SM_CASTSPELL_RESULT not before 1,800 ms later" is measured from the SM_CASTSPELL, not from here */
		std::chrono::steady_clock::time_point sentAt;
		/** the caster's first SM_CASTSPELL for the skill (Skill.startCast) */
		std::optional<size_t> castSpell;
		/** the caster's SM_CASTSPELL_RESULT for the skill (Skill.sendCastSpellEnd); it ends the wait */
		std::optional<size_t> castSpellResult;
		/** the caster's SM_SKILL_CANCEL for the skill (PlayerController.cancelCurrentSkill); it ends the wait */
		std::optional<size_t> skillCancel;
		/** when the interruption was sent, if it was */
		std::optional<std::chrono::steady_clock::time_point> interruptionSentAt;
		std::chrono::milliseconds elapsed{0};
		/** the connection closed while waiting */
		bool closed = false;

		bool ended() const noexcept { return castSpellResult.has_value() || skillCancel.has_value(); }
	};

	/**
	 * castAndWait(skillId, timeout) of m5b2-plan.md G-02: sends CM_CASTSPELL(request) and records every server packet until the cast of
	 * `request.spellId` by `casterObjectId` ends - its SM_CASTSPELL_RESULT or its SM_SKILL_CANCEL - or `timeout` passes or the connection closes.
	 * The caster's object id is part of the match because a fight is full of other casts: npc 210133 casts its own skill at the character
	 * (§10.3 X9), and an SM_CASTSPELL of the monster must not end the character's wait. The three packets are decoded with the independent
	 * decoders of decoders/SkillDecoders.h, so a body that does not match its Java writeImpl fails the call with DecodeError.
	 *
	 * A refused cast (PlayerRestrictions.canUseSkill, a condition, no target) sends neither packet: the call then runs into its timeout and
	 * `ended()` is false, with whatever the server sent instead (an SM_SYSTEM_MESSAGE) recorded from firstPacket on.
	 */
	CastOutcome castAndWait(int32_t casterObjectId, const CastRequest& request, std::chrono::milliseconds timeout,
		const std::optional<CastInterruption>& interruption = std::nullopt);

	/** What one talk call read */
	struct TalkOutcome {
		/** the index in recorded() of the first packet this call recorded (recorded().size() when it read none) */
		size_t firstPacket = 0;
		/** the packets this call read, in arrival order: recorded() from firstPacket on */
		std::vector<Packet> packets;
		/** the connection was closed when the call ended */
		bool closed = false;
	};

	/**
	 * talk(npcObjectId, action, questId) of m5d-plan.md G-02, one step of a quest conversation (§10.2 C6-C14): sends
	 * CM_DIALOG_SELECT(npcObjectId, dialogActionId, 0, 0, questId) and reads until no packet arrives for `quiet`, or `limit` has passed, or the
	 * connection closes (collectUntilQuiet). DialogService answers a quest action inside the packet's runImpl, so every packet of the answer -
	 * SM_QUEST_ACTION, SM_NEARBY_QUESTS, the reward's item and exp packets and the SM_DIALOG_WINDOW - arrives in one burst, in the order the
	 * server sent it, which is what the §10.3 order patterns (Y3, Y5, Y6, Y11, Y13) read. A packet that belongs to something else (an
	 * SM_NEARBY_QUESTS of a spawn, §10.6 (d)) can join the burst; the caller's pattern allows it.
	 *
	 * The npc id 0 is the quest journal's form (C10b: `CM_DIALOG_SELECT(target 0, 108, 1102)`, CM_DIALOG_SELECT.java:75-100).
	 */
	TalkOutcome talk(int32_t npcObjectId, uint16_t dialogActionId, int32_t questId, std::chrono::milliseconds quiet = std::chrono::seconds(1),
		std::chrono::milliseconds limit = std::chrono::seconds(10));

	network::test::FakeGameClient client;

private:
	Packet record(const network::test::FakeGameClient::ServerPacket& packet);

	std::vector<Packet> packets;
};

} // namespace aion::gameserver::scenario
