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

	/** the two packets of an M5b-2 cast (AionClientPacketFactory packets[33] and [35]; m5b2-plan.md G-02) */
	static constexpr int32_t CM_CASTSPELL = 33;
	static constexpr int32_t CM_REMOVE_ALTERED_STATE = 35;

	/**
	 * the nine packets of M5b-3's loot and items (m5b3-plan.md §2.7, G-02): AionClientPacketFactory packets[37], [38], [74], [116], [154],
	 * [155], [156], [157] and [178] (AionClientPacketFactory.java:65, 66, 102, 144, 182-185, 206). 4.8 sockets a godstone with CM_MANASTONE
	 * action 4; CM_GODSTONE_SOCKET (packets[91]) is commented out there (:119)
	 */
	static constexpr int32_t CM_USE_ITEM = 37;
	static constexpr int32_t CM_EQUIP_ITEM = 38;
	static constexpr int32_t CM_MANASTONE = 74;
	static constexpr int32_t CM_DELETE_ITEM = 116;
	static constexpr int32_t CM_START_LOOT = 154;
	static constexpr int32_t CM_LOOT_ITEM = 155;
	static constexpr int32_t CM_MOVE_ITEM = 156;
	static constexpr int32_t CM_SPLIT_ITEM = 157;
	static constexpr int32_t CM_REPLACE_ITEM = 178;

	/**
	 * the four packets of talking to an npc (m5c-plan.md §2.1, G-02): AionClientPacketFactory packets[50], [52], [53] and [54]
	 * (AionClientPacketFactory.java:78, 80-82)
	 */
	static constexpr int32_t CM_QUESTION_RESPONSE = 50;
	static constexpr int32_t CM_SHOW_DIALOG = 52;
	static constexpr int32_t CM_CLOSE_DIALOG = 53;
	static constexpr int32_t CM_DIALOG_SELECT = 54;

	/** abandoning a quest (m5d-plan.md D-03, G-02): AionClientPacketFactory packets[80], "C_GIVE_UP_QUEST" (AionClientPacketFactory.java:108) */
	static constexpr int32_t CM_DELETE_QUEST = 80;
	/**
	 * the end of a cutscene (P6-Q prologue, owner answer 4 of 2026-09-29): AionClientPacketFactory packets[81], "C_QUIT_CUTSCENE"
	 * (AionClientPacketFactory.java:109)
	 */
	static constexpr int32_t CM_PLAY_MOVIE_END = 81;

	/**
	 * the stage-1 packets of M5c (m5c-plan.md K-01, K-02, G-02): the shop, the exchange, the private store, the mail and identification
	 * (AionClientPacketFactory packets[51], [63], [64], [66]-[69], [119], [120], [132]-[134], [136], [137], [235], [236] and [238],
	 * AionClientPacketFactory.java:79, 91-92, 94-97, 147-148, 160-162, 164-165, 263-264, 266), and stage 2's two crafting packets
	 * (packets[89] and [141], :117, :169). There is no packets[65] (the exchange's) and packets[135] (C_MAIL_SETREAD) is commented out
	 */
	static constexpr int32_t CM_BUY_ITEM = 51;
	static constexpr int32_t CM_EXCHANGE_REQUEST = 63;
	static constexpr int32_t CM_EXCHANGE_ADD_ITEM = 64;
	static constexpr int32_t CM_EXCHANGE_ADD_KINAH = 66;
	static constexpr int32_t CM_EXCHANGE_LOCK = 67;
	static constexpr int32_t CM_EXCHANGE_OK = 68;
	static constexpr int32_t CM_EXCHANGE_CANCEL = 69;
	static constexpr int32_t CM_RECIPE_DELETE = 89;
	static constexpr int32_t CM_PRIVATE_STORE = 119;
	static constexpr int32_t CM_PRIVATE_STORE_NAME = 120;
	static constexpr int32_t CM_SEND_MAIL = 132;
	static constexpr int32_t CM_CHECK_MAIL_LIST = 133;
	static constexpr int32_t CM_READ_MAIL = 134;
	static constexpr int32_t CM_GET_MAIL_ATTACHMENT = 136;
	static constexpr int32_t CM_DELETE_MAIL = 137;
	static constexpr int32_t CM_CRAFT = 141;
	/** M5e's packets (m5e-plan.md G-02; AionClientPacketFactory.java:62, 149, 262) */
	static constexpr int32_t CM_TOGGLE_SKILL_DEACTIVATE = 34;
	static constexpr int32_t CM_SUMMON_COMMAND = 121;
	static constexpr int32_t CM_USE_CHARGE_SKILL = 234;
	static constexpr int32_t CM_TUNE = 235;
	static constexpr int32_t CM_SELECT_DECOMPOSABLE = 236;
	static constexpr int32_t CM_TUNE_RESULT = 238;

	/**
	 * CM_BUY_ITEM's tradeActionId (CM_BUY_ITEM.java:75-88): what the entries' ids mean - a private store's INDEX (0), an inventory object id to
	 * sell (1) or a repurchase object id (2), a template id to buy (13-16), a pet's (17)
	 */
	static constexpr int16_t TRADE_PRIVATE_STORE = 0;
	static constexpr int16_t TRADE_SELL = 1;
	static constexpr int16_t TRADE_REPURCHASE = 2;
	static constexpr int16_t TRADE_BUY = 13;
	static constexpr int16_t TRADE_BUY_ABYSS = 14;
	static constexpr int16_t TRADE_BUY_REWARD = 15;
	static constexpr int16_t TRADE_BUY_GENERAL = 16;
	static constexpr int16_t TRADE_SELL_TO_PET = 17;
	/** the audit bounds CM_BUY_ITEM.readImpl checks (CM_BUY_ITEM.java:53, 69): at most 36 entries, each count at most 20000 */
	static constexpr uint16_t BUY_ITEM_MAX_ENTRIES = 36;
	static constexpr int64_t BUY_ITEM_MAX_COUNT = 20000;

	/** CM_GET_MAIL_ATTACHMENT's attachmentType (CM_GET_MAIL_ATTACHMENT.java:25, "0 - item , 1 - kinah") */
	static constexpr uint8_t MAIL_ATTACHMENT_ITEM = 0;
	static constexpr uint8_t MAIL_ATTACHMENT_KINAH = 1;
	/** CM_SEND_MAIL's idLetterType, LetterType.getId() (LetterType.java:8-10) */
	static constexpr uint8_t LETTER_NORMAL = 0;
	static constexpr uint8_t LETTER_EXPRESS = 1;
	/** CM_CRAFT's first byte: 129 is the morph substances' arm, which skips the target check (CM_CRAFT.java:53) */
	static constexpr uint8_t CRAFT_UNK_MORPH = 129;

	/**
	 * CM_QUESTION_RESPONSE's answer (CM_QUESTION_RESPONSE.java:30, "y/n"): RequestResponseHandler.handle denies on 0 and accepts on any other
	 * value (RequestResponseHandler.java:28-33)
	 */
	static constexpr uint8_t ANSWER_NO = 0;
	static constexpr uint8_t ANSWER_YES = 1;

	/** CM_START_LOOT's action (CM_START_LOOT.java runImpl): 0 opens the drop list (requestDropList), 1 closes it (closeDropList) */
	static constexpr uint8_t LOOT_OPEN = 0;
	static constexpr uint8_t LOOT_CLOSE = 1;
	/** CM_EQUIP_ITEM's action (CM_EQUIP_ITEM.java:30, "0/1/2 = equip/unequip/switch weapons") */
	static constexpr uint8_t EQUIP = 0;
	static constexpr uint8_t UNEQUIP = 1;
	static constexpr uint8_t SWITCH_WEAPONS = 2;
	/** CM_MANASTONE's actionType for a godstone (CM_MANASTONE.java runImpl: ItemSocketService.socketGodstone) */
	static constexpr uint8_t MANASTONE_SOCKET_GODSTONE = 4;
	/** CM_MANASTONE's arm that reads a slot and an npc instead of two item ids (CM_MANASTONE.java:51-56, removeManastone) */
	static constexpr uint8_t MANASTONE_REMOVE = 3;

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

	/**
	 * The fields CM_CASTSPELL.readImpl reads, in its order (CM_CASTSPELL.java:36-71): readUH spellid, readUC level, readUC targetType, the target
	 * arm, readUH hitTime, readD unk. runImpl passes spellid, targetType, x/y/z, hitTime and level on to PlayerController.useSkill
	 * (CM_CASTSPELL.java:108); the object id of the 0/3/4 arm and `unk` are read and dropped - the first target is the player's current target
	 * (PlayerController.useSkill: SkillEngine.getSkillFor(player, template, player.getTarget())), which is why the gate selects it first.
	 */
	struct CastRequest {
		uint16_t spellId = 0;
		/** the level the client claims; Skill takes the level from the player's skill list, not from here */
		uint8_t level = 1;
		/** 0, 3 and 4 read an object id, 1 reads x/y/z, 2 reads x/y/z and eight more floats, and any other value reads no arm at all */
		uint8_t targetType = 0;
		int32_t targetObjectId = 0;
		float x = 0, y = 0, z = 0;
		/** the client's hit time, which PlayerController.useSkill hands to Skill.setClientHitTime */
		uint16_t hitTime = 0;
		int32_t unk = 0;
	};

	/**
	 * A client packet castAndWait sends once while the cast is running, `after` the CM_CASTSPELL went out - X5's CM_MOVE 300 ms into a cast
	 * (m5b2-plan.md §10.3), which PlayerController.onStartMove answers by cancelling it. It is not sent if the cast has ended before.
	 */
	struct CastInterruption {
		std::chrono::milliseconds after{0};
		int32_t opcode = 0;
		std::vector<uint8_t> body;
	};

	/**
	 * The fields CM_MANASTONE.readImpl reads, in its order (CM_MANASTONE.java:39-58): readUC actionType, readUC targetFusedSlot, readD
	 * targetItemUniqueId, then the arm of the action - 1, 2, 4 and 8 read the stone and the supplement (readD, readD), 3 reads readUC slotNum, a
	 * dropped readC and readH, and readD npcObjId - and any other action reads no arm (the switch has no default).
	 */
	struct ManastoneRequest {
		uint8_t actionType = MANASTONE_SOCKET_GODSTONE;
		uint8_t targetFusedSlot = 0;
		int32_t targetItemUniqueId = 0;
		/** arms 1, 2, 4, 8 */
		int32_t stoneUniqueId = 0;
		int32_t supplementUniqueId = 0;
		/** arm 3 */
		uint8_t slotNum = 0;
		int32_t npcObjId = 0;
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
	/** CM_CASTSPELL.readImpl (CM_CASTSPELL.java:36-71), every target arm; the eight extra floats of arm 2 are written as 0 */
	static std::vector<uint8_t> buildCM_CASTSPELL(const CastRequest& request);
	/**
	 * CM_CASTSPELL for an object target, the form the gate casts with (m5b2-plan.md §10.2: `CM_CASTSPELL(1282, 1, 0, objId, hitTime)`).
	 * @throws std::invalid_argument for target type 1 or 2, which read a point instead: use the CastRequest form
	 */
	static std::vector<uint8_t> buildCM_CASTSPELL(uint16_t spellId, uint8_t level, uint8_t targetType, int32_t targetObjectId, uint16_t hitTime = 0);
	/**
	 * CM_REMOVE_ALTERED_STATE.readImpl: readUH skillId, then two readC the server drops (CM_REMOVE_ALTERED_STATE.java:24-28, "seen 1 with
	 * skillId 3573"). runImpl ends the player's effect of that skill unless it is a DEBUFF (:31-42) - the client's right click on a buff icon.
	 */
	static std::vector<uint8_t> buildCM_REMOVE_ALTERED_STATE(uint16_t skillId, uint8_t unk1 = 0, uint8_t unk2 = 0);

	// ---- M5b-3's loot and item packets (m5b3-plan.md §2.7, G-02), each the Java readImpl field order ----
	/** CM_START_LOOT.readImpl (CM_START_LOOT.java:35-38): readD targetObjectId, readC action (LOOT_OPEN, LOOT_CLOSE) */
	static std::vector<uint8_t> buildCM_START_LOOT(int32_t targetObjectId, uint8_t action = LOOT_OPEN);
	/** CM_LOOT_ITEM.readImpl (CM_LOOT_ITEM.java:23-26): readD targetObjectId, readUC index (the entry's DropItem index, not its position) */
	static std::vector<uint8_t> buildCM_LOOT_ITEM(int32_t targetObjectId, uint8_t index);
	/**
	 * CM_USE_ITEM.readImpl (CM_USE_ITEM.java:38-52): readD uniqueItemId, readC type, then readD targetItemId for type 2, syncId for type 5 or
	 * indexReturn for type 6 - `extra` - and nothing for any other type (a potion is type 0)
	 */
	static std::vector<uint8_t> buildCM_USE_ITEM(int32_t uniqueItemId, int8_t type = 0, int32_t extra = 0);
	/**
	 * CM_MOVE_ITEM.readImpl (CM_MOVE_ITEM.java:25-30): readD itemObjId, readC source, readC destination (0 cube, 1 regular, 2 account, 3 legion
	 * warehouse), readH slot (-1 merges into a stack of the same item, ItemMoveService.moveItem)
	 */
	static std::vector<uint8_t> buildCM_MOVE_ITEM(int32_t itemObjId, uint8_t source, uint8_t destination, int16_t slot);
	/**
	 * CM_SPLIT_ITEM.readImpl (CM_SPLIT_ITEM.java:27-34): readD sourceItemObjId, readQ itemAmount, readC sourceStorageType, readD
	 * destinationItemObjId (0: a new stack), readC destinationStorageType, readH slotNum
	 */
	static std::vector<uint8_t> buildCM_SPLIT_ITEM(int32_t sourceItemObjId, int64_t itemAmount, uint8_t sourceStorageType, int32_t destinationItemObjId,
		uint8_t destinationStorageType, int16_t slotNum);
	/** CM_REPLACE_ITEM.readImpl (CM_REPLACE_ITEM.java:25-30): readC sourceStorageType, readD sourceItemObjId, readC replaceStorageType, readD replaceItemObjId */
	static std::vector<uint8_t> buildCM_REPLACE_ITEM(uint8_t sourceStorageType, int32_t sourceItemObjId, uint8_t replaceStorageType, int32_t replaceItemObjId);
	/** CM_MANASTONE.readImpl, every arm (ManastoneRequest) */
	static std::vector<uint8_t> buildCM_MANASTONE(const ManastoneRequest& request);
	/**
	 * CM_MANASTONE for the arms that read two item ids (1, 2, 4, 8) - the gate's `CM_MANASTONE(4, 0, sword, stone, 0)` (m5b3-plan.md §10.2 L6).
	 * @throws std::invalid_argument for action 3, which reads a slot and an npc instead: use the ManastoneRequest form
	 */
	static std::vector<uint8_t> buildCM_MANASTONE(uint8_t actionType, uint8_t targetFusedSlot, int32_t targetItemUniqueId, int32_t stoneUniqueId,
		int32_t supplementUniqueId = 0);
	/** CM_EQUIP_ITEM.readImpl (CM_EQUIP_ITEM.java:29-33): readC action (EQUIP, UNEQUIP, SWITCH_WEAPONS), readQ slotRead, readD itemObjId */
	static std::vector<uint8_t> buildCM_EQUIP_ITEM(uint8_t action, int64_t slot, int32_t itemObjId);
	/** CM_DELETE_ITEM.readImpl (CM_DELETE_ITEM.java:26-28): readD itemObjectId */
	static std::vector<uint8_t> buildCM_DELETE_ITEM(int32_t itemObjectId);

	// ---- M5c's dialog packets (m5c-plan.md §2.1, G-02), each the Java readImpl field order ----
	/** CM_SHOW_DIALOG.readImpl (CM_SHOW_DIALOG.java:23-25): readD targetObjectId */
	static std::vector<uint8_t> buildCM_SHOW_DIALOG(int32_t targetObjectId);
	/** CM_CLOSE_DIALOG.readImpl (CM_CLOSE_DIALOG.java:24-26): readD targetObjectId */
	static std::vector<uint8_t> buildCM_CLOSE_DIALOG(int32_t targetObjectId);
	/**
	 * CM_DIALOG_SELECT.readImpl (CM_DIALOG_SELECT.java:47-54): readD targetObjectId, readUH dialogActionId, readUH extendedRewardIndex, readUH
	 * lastPage, readD questId, readUH unk ("unk 4.7"). A function dialog - the gate's `CM_DIALOG_SELECT(798007, BUY = 2)` - leaves the last four at 0
	 */
	static std::vector<uint8_t> buildCM_DIALOG_SELECT(int32_t targetObjectId, uint16_t dialogActionId, uint16_t extendedRewardIndex = 0,
		uint16_t lastPage = 0, int32_t questId = 0, uint16_t unk = 0);
	/**
	 * CM_QUESTION_RESPONSE.readImpl (CM_QUESTION_RESPONSE.java:27-36): readD questionid, readUC response, a dropped readC ("unk 0x00 - 0x01 ?")
	 * and readH, readD senderid, then a dropped readD and readH. runImpl answers by questionid alone (ResponseRequester.respond): the sender id
	 * is read and never used, so it defaults to 0
	 */
	static std::vector<uint8_t> buildCM_QUESTION_RESPONSE(int32_t questionId, uint8_t response, int32_t senderId = 0);

	// ---- M5d's quest packet (m5d-plan.md D-03, G-02) ----
	/**
	 * CM_DELETE_QUEST.readImpl (CM_DELETE_QUEST.java:23-25): readD questId. runImpl stops a timer quest's timer (an SM_QUEST_ACTION TIMER 0)
	 * and calls QuestService.abandonQuest (:28-37) - the gate's `CM_DELETE_QUEST(1103)` (m5d-plan.md §10.2 C14)
	 */
	static std::vector<uint8_t> buildCM_DELETE_QUEST(int32_t questId);
	/**
	 * CM_PLAY_MOVIE_END.readImpl (CM_PLAY_MOVIE_END.java:33-40): readC type, readD targetObjectId, readD questId, readD movieId, readC (unknown),
	 * readC (0: canSkip). A real client sends it with the fields of the SM_PLAY_MOVIE whose cutscene ended or was skipped (the last two bytes
	 * written as 0). Until it arrives the server drops every CM_MOVE: SM_PLAY_MOVIE.java:28 sets WATCHING_CUTSCENE, CM_MOVE.java:159-161 drops
	 * a move while it is set, and CM_PLAY_MOVIE_END.java:52 alone clears it
	 */
	static std::vector<uint8_t> buildCM_PLAY_MOVIE_END(uint8_t type, int32_t targetObjectId, int32_t questId, int32_t movieId);

	// ---- M5c's stage-1 packets (m5c-plan.md K-01, K-02, G-02) and stage 2's crafting packets, each the Java readImpl field order ----
	/** one entry of CM_BUY_ITEM (CM_BUY_ITEM.java:65-66): readD itemId (see TRADE_*), readQ count */
	struct BuyItemEntry {
		int32_t itemId = 0;
		int64_t count = 0;
	};
	/**
	 * CM_BUY_ITEM.readImpl (CM_BUY_ITEM.java:47-90): readD sellerObjId, readH tradeActionId, readUH amount = entries.size(), then each entry.
	 * The gate's `CM_BUY_ITEM(798007, 13, [(162000052, 2)])` (m5c-plan.md C5). Nothing is clamped: 37 entries or a count above 20000 are sent as
	 * given, which is how a test reaches the audit arms
	 * @throws std::invalid_argument for more than 65535 entries (readUH cannot carry them)
	 */
	static std::vector<uint8_t> buildCM_BUY_ITEM(int32_t sellerObjectId, int16_t tradeActionId, std::span<const BuyItemEntry> entries);
	/** CM_EXCHANGE_REQUEST.readImpl (CM_EXCHANGE_REQUEST.java:34-36): readD targetObjectId */
	static std::vector<uint8_t> buildCM_EXCHANGE_REQUEST(int32_t targetObjectId);
	/** CM_EXCHANGE_ADD_ITEM.readImpl (CM_EXCHANGE_ADD_ITEM.java:23-26): readD itemObjId, readD itemCount - an int, not a long */
	static std::vector<uint8_t> buildCM_EXCHANGE_ADD_ITEM(int32_t itemObjectId, int32_t itemCount);
	/** CM_EXCHANGE_ADD_KINAH.readImpl (CM_EXCHANGE_ADD_KINAH.java:21-23): readQ kinahCount */
	static std::vector<uint8_t> buildCM_EXCHANGE_ADD_KINAH(int64_t kinahCount);
	/** CM_EXCHANGE_LOCK, CM_EXCHANGE_OK and CM_EXCHANGE_CANCEL read nothing (their readImpl is empty): an empty body */
	static std::vector<uint8_t> buildCM_EXCHANGE_LOCK();
	static std::vector<uint8_t> buildCM_EXCHANGE_OK();
	static std::vector<uint8_t> buildCM_EXCHANGE_CANCEL();
	/** one item of CM_PRIVATE_STORE (CM_PRIVATE_STORE.java:27-30): readD itemObjId, readD itemId, readUH count, readQ price (of one item) */
	struct PrivateStoreItem {
		int32_t itemObjectId = 0;
		int32_t itemId = 0;
		uint16_t count = 0;
		int64_t price = 0;
	};
	/**
	 * CM_PRIVATE_STORE.readImpl (CM_PRIVATE_STORE.java:23-33): readUH itemCount, then each item; an empty list closes the store
	 * @throws std::invalid_argument for more than 65535 items
	 */
	static std::vector<uint8_t> buildCM_PRIVATE_STORE(std::span<const PrivateStoreItem> items);
	/** CM_PRIVATE_STORE_NAME.readImpl (CM_PRIVATE_STORE_NAME.java:27-29): readS name */
	static std::vector<uint8_t> buildCM_PRIVATE_STORE_NAME(std::string_view name);
	/**
	 * CM_SEND_MAIL.readImpl (CM_SEND_MAIL.java:29-37): readS recipientName, readS title, readS message, readD itemObjId (0 for none), readQ
	 * itemCount, readQ kinahCount, readUC idLetterType (LETTER_NORMAL, LETTER_EXPRESS). The gate's `CM_SEND_MAIL(B, "m5c", "gate", potion
	 * stack, 5, 200, NORMAL)` (m5c-plan.md C11)
	 */
	static std::vector<uint8_t> buildCM_SEND_MAIL(std::string_view recipientName, std::string_view title, std::string_view message,
		int32_t itemObjectId, int64_t itemCount, int64_t kinahCount, uint8_t letterType = LETTER_NORMAL);
	/** CM_CHECK_MAIL_LIST.readImpl (CM_CHECK_MAIL_LIST.java:22-24): readC, `== 1` lists only the unread express letters */
	static std::vector<uint8_t> buildCM_CHECK_MAIL_LIST(bool expressOnly = false);
	/** CM_READ_MAIL.readImpl (CM_READ_MAIL.java:22-24): readD mailObjId */
	static std::vector<uint8_t> buildCM_READ_MAIL(int32_t letterObjectId);
	/** CM_GET_MAIL_ATTACHMENT.readImpl (CM_GET_MAIL_ATTACHMENT.java:23-26): readD mailObjId, readC attachmentType (MAIL_ATTACHMENT_*) */
	static std::vector<uint8_t> buildCM_GET_MAIL_ATTACHMENT(int32_t letterObjectId, uint8_t attachmentType);
	/**
	 * CM_DELETE_MAIL.readImpl (CM_DELETE_MAIL.java:22-28): readUH count, then per letter readD mailObjId and a dropped readC (written 0)
	 * @throws std::invalid_argument for more than 65535 letters
	 */
	static std::vector<uint8_t> buildCM_DELETE_MAIL(std::span<const int32_t> letterObjectIds);
	/** CM_TUNE.readImpl (CM_TUNE.java:25-28): readD itemObjectId, readD tuningScrollObjectId - 0 identifies without a scroll (C15) */
	static std::vector<uint8_t> buildCM_TUNE(int32_t itemObjectId, int32_t tuningScrollObjectId = 0);
	/** CM_TUNE_RESULT.readImpl (CM_TUNE_RESULT.java:28-31): readD itemObjectId, readC `== 1` hasAccepted */
	static std::vector<uint8_t> buildCM_TUNE_RESULT(int32_t itemObjectId, bool accepted);
	/** CM_SELECT_DECOMPOSABLE.readImpl (CM_SELECT_DECOMPOSABLE.java:38-42): readD objectId, readD unk, readUC index */
	static std::vector<uint8_t> buildCM_SELECT_DECOMPOSABLE(int32_t objectId, int32_t unk, uint8_t index);
	/** one material of CM_CRAFT (CM_CRAFT.java:40): readD itemId, readQ count - the key and value of Java's materialsData map */
	struct CraftMaterial {
		int32_t itemId = 0;
		int64_t count = 0;
	};
	/**
	 * CM_CRAFT.readImpl (CM_CRAFT.java:32-41): readUC unk, readD targetTemplateId, readD recipeId, readD targetObjId, readUH materialsCount,
	 * readUC craftType - the count comes BEFORE the craft type and the materials after both. The gate's
	 * `CM_CRAFT(0, 150000009, 155001381, oven, {152001001: 1, 169400096: 2}, 0)` (m5c-plan.md C19)
	 * @throws std::invalid_argument for more than 65535 materials
	 */
	static std::vector<uint8_t> buildCM_CRAFT(uint8_t unk, int32_t targetTemplateId, int32_t recipeId, int32_t targetObjectId,
		std::span<const CraftMaterial> materials, uint8_t craftType = 0);
	/** CM_RECIPE_DELETE.readImpl (CM_RECIPE_DELETE.java:21-23): readD recipeId */
	static std::vector<uint8_t> buildCM_RECIPE_DELETE(int32_t recipeId);

	// ---- M5e's progression packets (m5e-plan.md §2.11, G-02), each the Java readImpl field order ----
	/** MovementMask.GLIDE and FALL (MovementMask.java:16, 21): what buildCM_MOVE_GLIDE writes, GlideFlag.NONE (0) its default flag */
	static constexpr int8_t MOVE_GLIDE = 0x04;
	static constexpr int8_t MOVE_FALL = 0x08;
	/**
	 * CM_MOVE with the glide bit (CM_MOVE.java:40-66): x, y, z, heading, type = GLIDE | `extraType`, then - type has no POSITION|MANUAL pair,
	 * so no target point - readC glideFlag (and readUC geyserLocationId only for GlideFlag.GEYSER, which this builder refuses: a geyser is a
	 * windstream). runImpl calls FlyController.switchToGliding (CM_MOVE.java:95-98), which refuses a non-Daeva with STR_GLIDE_ONLY_DEVA_CAN
	 * (FlyController.java:137-141); a later move with the FALL bit ends the glide (onStopGliding, :157-159)
	 * @throws std::invalid_argument for a type carrying POSITION and MANUAL or VEHICLE, or a geyser flag
	 */
	static std::vector<uint8_t> buildCM_MOVE_GLIDE(float x, float y, float z, int8_t heading, int8_t extraType = 0, uint8_t glideFlag = 0);
	/**
	 * CM_TOGGLE_SKILL_DEACTIVATE.readImpl (CM_TOGGLE_SKILL_DEACTIVATE.java:24-28): readUH skillId, then two readH the server drops (written 0).
	 * runImpl removes the effect of a toggle or a stance and audits anything else (:31-42)
	 */
	static std::vector<uint8_t> buildCM_TOGGLE_SKILL_DEACTIVATE(uint16_t skillId);
	/**
	 * CM_USE_CHARGE_SKILL.readImpl reads nothing (CM_USE_CHARGE_SKILL.java:20-21): an empty body. runImpl releases the casting charge skill
	 * with the time since its cast started (:24-31)
	 */
	static std::vector<uint8_t> buildCM_USE_CHARGE_SKILL();
	/** SummonMode ids (SummonMode.java:8-11): what CM_SUMMON_COMMAND's mode byte selects */
	static constexpr uint8_t SUMMON_ATTACK = 0;
	static constexpr uint8_t SUMMON_GUARD = 1;
	static constexpr uint8_t SUMMON_REST = 2;
	static constexpr uint8_t SUMMON_RELEASE = 3;
	/** CM_SUMMON_COMMAND.readImpl (CM_SUMMON_COMMAND.java:26-31): readUC mode, two readD the server drops (written 0), readD targetObjId */
	static std::vector<uint8_t> buildCM_SUMMON_COMMAND(uint8_t mode, int32_t targetObjectId = 0);

	network::test::FakeGameClient client;

private:
	Packet record(const network::test::FakeGameClient::ServerPacket& packet);

	std::vector<Packet> packets;
};

} // namespace aion::gameserver::scenario
