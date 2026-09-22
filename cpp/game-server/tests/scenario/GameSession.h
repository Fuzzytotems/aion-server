#pragma once

// GameSession (m5a-plan.md F-04): one game client connection of the scenario over FakeGameClient (tests/support): the client packet builders
// of the scripted path (layouts written from the Java readImpl methods, the leading fields only: the frame adds nothing, the server ignores
// what it does not read), a fixed MAC address and HDD serial, and a recorder of every server packet with its Java class name
// (ServerPacketsOpcodes). The bodies are decoded by the independent decoders of F-08 (stage 2), never with the server's packet classes.
//
// ONE DEPENDENCY ON THE SERVER, worth knowing before a §5.8 sequence failure is blamed on a chunk: the CM opcodes this class sends are written
// out from the Java AionClientPacketFactory (below), but nameOf() maps a RECEIVED opcode to a packet name through the server's own generated
// table (ServerPacketsOpcodes.gen.h). A packet class given the wrong opcode relative to Java's AionServerPacketsOpcodes would therefore be
// mislabelled identically on both sides and the §5.8 match would still succeed. What restores the independence is separate opcode-parity
// coverage that the gate does not depend on: tests/network_crypt/GeneratedOpcodesTest.cpp, tests/sm_ak/ServerPacketsAKOpcodeTest.cpp and
// tests/sm_lz/OpcodesAndSupportTest.cpp. A full ctest runs them; a bare `ctest -R ^gs.scenario.m5a$` does not.
//
// Second thing to know before blaming a chunk: collectUntilQuiet ends a burst at the first 1 s gap. A measured enter-world burst takes about
// 70 ms, so the headroom is large, but a 1 s stall on a loaded machine truncates the burst, and the gate then fails with a sequence error that
// looks like a port defect.

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "FakeGameClient.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {

/** The appearance fields of AbstractCharacterEditPacket.readAppearance in their wire order (unsigned bytes unless noted) */
struct CharacterAppearance {
	int32_t voice = 0;
	int32_t skinRGB = 0x00E1C3B4;
	int32_t hairRGB = 0x00202020;
	int32_t eyeRGB = 0x00503010;
	int32_t lipRGB = 0x00A06060;
	uint8_t face = 1, hair = 1, deco = 0, tattoo = 0, faceContour = 0, expression = 0;
	uint8_t unknown4 = 4; // "always 4"
	uint8_t jawLine = 0, forehead = 0;
	uint8_t eyeHeight = 0, eyeSpace = 0, eyeWidth = 0, eyeSize = 0, eyeShape = 0, eyeAngle = 0;
	uint8_t browHeight = 0, browAngle = 0, browShape = 0;
	uint8_t nose = 0, noseBridge = 0, noseWidth = 0, noseTip = 0;
	uint8_t cheek = 0, lipHeight = 0, mouthSize = 0, lipSize = 0, smile = 0, lipShape = 0, jawHeight = 0, chinJut = 0, earShape = 0, headSize = 0;
	uint8_t neck = 0, neckLength = 0;
	uint8_t shoulderSize = 0;
	uint8_t torso = 0, chest = 0, waist = 0, hips = 0;
	uint8_t armThickness = 0;
	uint8_t handSize = 0, legThickness = 0;
	uint8_t footSize = 0, facialRate = 0;
	uint8_t unknown0 = 0; // "always 0"
	uint8_t armLength = 0, legLength = 0, shoulders = 0, faceShape = 0;
	uint8_t unknownA = 0, unknownB = 0, unknownC = 0;
	float height = 1.0f;

	void writeTo(network::test::PacketWriter& writer) const;
};

/** CM_CREATE_CHARACTER's character (AbstractCharacterEditPacket.readBasicInfo) */
struct NewCharacter {
	std::string name;
	bool female = false;
	bool asmodian = false;
	int32_t playerClassId = WARRIOR;
	CharacterAppearance appearance;

	static constexpr int32_t WARRIOR = 0; // PlayerClass.WARRIOR id
	static constexpr int32_t MAGE = 6;    // PlayerClass.MAGE id
};

class GameSession {
public:
	/** Java opcodes (AionClientPacketFactory) of the client packets the scenario sends */
	static constexpr int32_t CM_VERSION_CHECK = 0;
	static constexpr int32_t CM_QUIT = 3;
	static constexpr int32_t CM_ENTER_WORLD = 8;
	static constexpr int32_t CM_LEVEL_READY = 9;
	static constexpr int32_t CM_TIME_CHECK = 18;
	static constexpr int32_t CM_PING = 44;
	static constexpr int32_t CM_MOVE = 48;
	static constexpr int32_t CM_SECURITY_TOKEN = 92;
	static constexpr int32_t CM_GAMEGUARD = 104;
	static constexpr int32_t CM_L2AUTH_LOGIN_CHECK = 149;
	static constexpr int32_t CM_CHARACTER_LIST = 150;
	static constexpr int32_t CM_CREATE_CHARACTER = 151;
	static constexpr int32_t CM_CHECK_NICKNAME = 177;
	static constexpr int32_t CM_MAY_LOGIN_INTO_GAME = 186;
	static constexpr int32_t CM_MAC_ADDRESS = 189;
	/** the two in-world packets of item C-01 a real client sends without any user action (AionClientPacketFactory packets[12] and packets[163]) */
	static constexpr int32_t CM_CUSTOM_SETTINGS = 12;
	static constexpr int32_t CM_SUBZONE_CHANGE = 163;
	/** the three packets of an M5b fight (AionClientPacketFactory packets[5], [31] and [32]; m5b-plan.md G-02) */
	static constexpr int32_t CM_REVIVE = 5;
	static constexpr int32_t CM_TARGET_SELECT = 31;
	static constexpr int32_t CM_ATTACK = 32;

	/** ReviveType ids, which are what CM_REVIVE carries (model/gameobjects/player/ReviveType.java; note that 5 and 7 are no revive type) */
	static constexpr uint8_t BIND_REVIVE = 0;
	static constexpr uint8_t REBIRTH_REVIVE = 1;
	static constexpr uint8_t ITEM_SELF_REVIVE = 2;
	static constexpr uint8_t SKILL_REVIVE = 3;
	static constexpr uint8_t KISK_REVIVE = 4;
	static constexpr uint8_t INSTANCE_REVIVE = 6;
	static constexpr uint8_t OBELISK_REVIVE = 8;

	/**
	 * The margin PlayerController.attackTarget allows on the attack interval: a CM_ATTACK that arrives less than `attackSpeed - 300` ms after
	 * the previous one is answered with SM_ATTACK_RESPONSE.STOP_WITHOUT_MESSAGE and nothing else happens (PlayerController.java:424-426,
	 * `milis - lastAttackMillis + 300 < attackSpeed`). fightUntil therefore paces at the full attack speed.
	 */
	static constexpr std::chrono::milliseconds ATTACK_INTERVAL_TOLERANCE{300};

	/** the MAC address (LoginServer.java MAC pattern) and HDD serial every scenario client sends */
	static constexpr std::string_view MAC_ADDRESS = "0A-1B-2C-3D-4E-5F";
	static constexpr std::string_view HDD_SERIAL = "M5ASCENARIO0001";
	/** the client version CM_VERSION_CHECK sends (SM_VERSION_CHECK echoes it; 206 is below INTERNAL_VERSION like the 4.8 client) */
	static constexpr uint16_t CLIENT_VERSION = 206;

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

	// ---- client packet bodies (Java readImpl order) ----
	static std::vector<uint8_t> buildCM_VERSION_CHECK(uint16_t clientVersion = CLIENT_VERSION);
	static std::vector<uint8_t> buildCM_L2AUTH_LOGIN_CHECK(int32_t playOk2, int32_t playOk1, int32_t accountId, int32_t loginOk);
	static std::vector<uint8_t> buildCM_MAC_ADDRESS(std::string_view macAddress = MAC_ADDRESS, std::string_view hddSerial = HDD_SERIAL);
	static std::vector<uint8_t> buildCM_TIME_CHECK(int32_t nanoTime);
	static std::vector<uint8_t> buildCM_CHARACTER_LIST(int32_t playOk2);
	static std::vector<uint8_t> buildCM_PING();
	static std::vector<uint8_t> buildCM_GAMEGUARD(std::span<const uint8_t> data);
	static std::vector<uint8_t> buildCM_SECURITY_TOKEN();
	static std::vector<uint8_t> buildCM_CHECK_NICKNAME(std::string_view nick);
	static std::vector<uint8_t> buildCM_CREATE_CHARACTER(int32_t accountId, std::string_view accountName, const NewCharacter& character, uint8_t type);
	static std::vector<uint8_t> buildCM_MAY_LOGIN_INTO_GAME();
	static std::vector<uint8_t> buildCM_ENTER_WORLD(int32_t objectId);
	static std::vector<uint8_t> buildCM_LEVEL_READY();
	/** CM_MOVE with type POSITION | MANUAL | ABSOLUTE (0xE0): position, heading, type, target position */
	static std::vector<uint8_t> buildCM_MOVE(float x, float y, float z, int8_t heading, int8_t type, float x2, float y2, float z2);
	/** CM_MOVE without POSITION|MANUAL, GLIDE and VEHICLE data (e.g. a stop move with type 0) */
	static std::vector<uint8_t> buildCM_MOVE(float x, float y, float z, int8_t heading, int8_t type);
	static std::vector<uint8_t> buildCM_QUIT(bool stayConnected);
	/** CM_CUSTOM_SETTINGS.readImpl: display, deny (both readUH) */
	static std::vector<uint8_t> buildCM_CUSTOM_SETTINGS(uint16_t display, uint16_t deny);
	/** CM_SUBZONE_CHANGE.readImpl: one readC ("always 1") */
	static std::vector<uint8_t> buildCM_SUBZONE_CHANGE(uint8_t unk);
	/** CM_TARGET_SELECT.readImpl: readD targetObjectId (0 unselects), readC selectTargetOfTarget */
	static std::vector<uint8_t> buildCM_TARGET_SELECT(int32_t targetObjectId, bool selectTargetOfTarget = false);
	/**
	 * CM_ATTACK.readImpl: readD targetObjectId, readUC attackno, readUH time, readUC type. Only the object id and `time` are used -
	 * runImpl looks the id up in the knownlist and calls attackTarget(creature, time, false), which passes `time` on to the DelayedOnAttack
	 * of CreatureController.attackTarget; `attackno` and `type` are read and dropped (CM_ATTACK.java:36-59).
	 */
	static std::vector<uint8_t> buildCM_ATTACK(int32_t targetObjectId, uint8_t attackNo = 0, uint16_t time = 0, uint8_t type = 0);
	/** CM_REVIVE.readImpl: one readUC reviveId, which must be a ReviveType id (CM_REVIVE.java:32-63 throws IllegalArgumentException otherwise) */
	static std::vector<uint8_t> buildCM_REVIVE(uint8_t reviveId = BIND_REVIVE);

	network::test::FakeGameClient client;

private:
	Packet record(const network::test::FakeGameClient::ServerPacket& packet);

	std::vector<Packet> packets;
};

} // namespace aion::gameserver::scenario
