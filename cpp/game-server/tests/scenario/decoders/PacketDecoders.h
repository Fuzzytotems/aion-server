#pragma once

// Independent decoders of the server packets the 4.8 client parses strictly (m5a-plan.md D9, F-08, §5.4 V6-V10). Every layout below is written
// from the Java writeImpl methods under game-server/src/com/aionemu/gameserver/network/aion/serverpackets and the helpers they call
// (AbstractPlayerInfoPacket.writePlayerInfo/writeEquippedItems, AionServerPacket.writeS(String,int)/writeDyeInfo, PacketWriteHelper,
// skillinfo/SkillEntryWriter, iteminfo/ItemInfoBlob and its blob entries). A decoder must never include, call or mirror a C++ serverpackets
// header: FakeGameClient already decrypts with the protocol, and the gate can only catch a symmetric width or order error in the port if the
// expectation was written from Java alone.
//
// Every decode function consumes the body exactly and throws DecodeError otherwise. Java constants that carry no data (writeC(5), the zero
// fillers) are verified, so a port that changes one fails the gate instead of silently shifting every later field.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace aion::gameserver::scenario::decoders {

/** A body that does not match the Java layout (wrong width, wrong order, trailing or missing bytes, a changed constant) */
class DecodeError : public std::runtime_error {
public:
	explicit DecodeError(const std::string& message) : std::runtime_error(message) {}
};

/**
 * Little endian reader over one server packet body (the bytes after the 5 header bytes, i.e. what writeImpl wrote). The accessor names are the
 * Java write names: C byte, H short, D int, Q long, F float, S UTF-16LE string.
 */
class BodyReader {
public:
	BodyReader(std::span<const uint8_t> body, std::string_view packetName) : body(body), packetName(packetName) {}

	uint8_t C();
	int8_t Cs();
	uint16_t H();
	int16_t Hs();
	int32_t D();
	int64_t Q();
	float F();
	/** writeS(text): UTF-16LE up to the terminating NUL char */
	std::string S();
	/** writeS(text, fixedLength) of AionServerPacket: always (fixedLength + 1) * 2 bytes, NUL padded */
	std::string S(size_t fixedLength);
	std::vector<uint8_t> B(size_t count);
	void skip(size_t count);
	/** reads count bytes and fails unless every one of them is 0 (Java writeB(new byte[count]) / writeD(0) fillers) */
	void expectZeros(size_t count, std::string_view what);
	/** reads one byte and fails unless it equals value (a literal Java constant) */
	void expectC(uint8_t value, std::string_view what);
	/** reads a short and fails unless it equals value */
	void expectH(uint16_t value, std::string_view what);
	/** reads an int and fails unless it equals value */
	void expectD(int32_t value, std::string_view what);

	size_t offset() const noexcept { return pos; }
	size_t size() const noexcept { return body.size(); }
	size_t remaining() const noexcept { return body.size() - pos; }

	/** fails unless the whole body was consumed (D9: "each decoder must consume the body exactly") */
	void expectFullyConsumed() const;

	[[noreturn]] void fail(std::string_view what) const;

private:
	std::span<const uint8_t> body;
	std::string packetName;
	size_t pos = 0;
};

/** AionServerPacket.writeDyeInfo / PacketWriteHelper.writeDyeInfo: 4 bytes, all zero for a null color */
struct DyeInfo {
	uint8_t status = 0;
	uint8_t r = 0, g = 0, b = 0;

	bool dyed() const noexcept { return status != 0; }
	int32_t rgb() const noexcept { return (static_cast<int32_t>(r) << 16) | (static_cast<int32_t>(g) << 8) | b; }
	bool operator==(const DyeInfo&) const = default;
};

/**
 * The player appearance fields both appearance blocks carry (writePlayerInfo of AbstractPlayerInfoPacket and the block inside SM_PLAYER_INFO).
 * The two blocks write the same fields in a different order: writePlayerInfo starts with the voice as an int, SM_PLAYER_INFO writes it as a
 * byte before the height. Field names follow PlayerAppearance's getters.
 */
struct Appearance {
	int32_t voice = 0;
	int32_t skinRGB = 0, hairRGB = 0, eyeRGB = 0, lipRGB = 0;
	uint8_t face = 0, hair = 0, deco = 0, tattoo = 0, faceContour = 0, expression = 0;
	uint8_t jawLine = 0, forehead = 0;
	uint8_t eyeHeight = 0, eyeSpace = 0, eyeWidth = 0, eyeSize = 0, eyeShape = 0, eyeAngle = 0;
	uint8_t browHeight = 0, browAngle = 0, browShape = 0;
	uint8_t nose = 0, noseBridge = 0, noseWidth = 0, noseTip = 0;
	uint8_t cheek = 0, lipHeight = 0, mouthSize = 0, lipSize = 0, smile = 0, lipShape = 0, jawHeight = 0, chinJut = 0, earShape = 0, headSize = 0;
	uint8_t neck = 0, neckLength = 0, shoulderSize = 0;
	uint8_t torso = 0, chest = 0, waist = 0, hips = 0;
	uint8_t armThickness = 0, handSize = 0, legThickness = 0, footSize = 0, facialRate = 0;
	uint8_t armLength = 0, legLength = 0, shoulders = 0, faceShape = 0;
	float height = 0;

	bool operator==(const Appearance&) const = default;
};

/** one of the 16 fixed visible item entries of writePlayerInfo */
struct VisibleItem {
	/** 0 = not visible, 1 = default (right-hand) slot, 2 = secondary (left-hand) slot */
	uint8_t slotType = 0;
	int32_t itemId = 0;
	int32_t godStoneId = 0;
	DyeInfo color;

	bool operator==(const VisibleItem&) const = default;
};

/** AbstractPlayerInfoPacket.writePlayerInfo: the character list / create character block */
struct PlayerInfoBlock {
	int32_t playerId = 0;
	std::string name;
	int32_t genderId = 0, raceId = 0, classId = 0;
	Appearance appearance;
	int32_t templateId = 0;
	int32_t mapId = 0;
	float x = 0, y = 0, z = 0;
	int32_t heading = 0;
	uint16_t level = 0;
	int32_t titleId = 0;
	int32_t legionId = 0;
	std::string legionName;
	bool legionMember = false;
	int32_t lastOnlineEpochSeconds = 0;
	/** always 16 entries; empty slots have itemId 0 */
	std::array<VisibleItem, 16> visibleItems{};
	int32_t deletionTimeInSeconds = 0;
	uint16_t display = 0;
	int32_t unreadMail = 0;
	int64_t brokerKinah = 0;
	int32_t banStart = 0, banEnd = 0;
	std::string banReason;
};

/** SM_CHARACTER_LIST */
struct CharacterList {
	int32_t playOk2 = 0;
	/** account.size(): the announced character count, compared with characters.size() by the decoder */
	uint8_t characterCount = 0;
	std::vector<PlayerInfoBlock> characters;
};

/** SM_CREATE_CHARACTER (response codes: 0 ok, 10 name used, 11 reserved, 12 other race, 22 open creation window) */
struct CreateCharacter {
	int32_t responseCode = 0;
	/** only for responseCode 0 */
	std::optional<PlayerInfoBlock> player;
};

/** SM_PLAYER_SPAWN */
struct PlayerSpawn {
	/** worldId + instanceId - 1, negated for a personal world */
	int32_t worldChannel = 0;
	int32_t worldId = 0;
	bool personal = false;
	float x = 0, y = 0, z = 0;
	uint8_t heading = 0;
	/** 1 if the world map template has beginner twins */
	uint8_t beginnerTwins = 0;
};

/** AbstractPlayerInfoPacket.writeEquippedItems: one entry per equipped item */
struct EquippedItem {
	int32_t skinTemplateId = 0;
	int32_t godStoneId = 0;
	DyeInfo color;
	uint16_t enchantParam = 0;

	bool operator==(const EquippedItem&) const = default;
};

/**
 * writeEquippedItems writes the ORed slot mask and then one entry per item without a count. The client derives the entry count from the mask:
 * every visible item occupies exactly one bit after the two-handed weapon fix (its SUB_HAND bit is cleared), so the count is the bit count.
 */
struct EquippedItems {
	int32_t mask = 0;
	std::vector<EquippedItem> items;
};

/** SM_PLAYER_INFO branches the decoder cannot see in the bytes */
struct PlayerInfoOptions {
	/** player.isUsingFlightTransporterOrWindstream(): two extra ints (flight path id and distance) after the movement mask */
	bool flightPath = false;
};

/** SM_PLAYER_INFO */
struct PlayerInfo {
	float x = 0, y = 0, z = 0;
	int32_t objectId = 0;
	int32_t templateId = 0;
	int32_t robotId = 0;
	int32_t transformModelId = 0;
	int32_t transformTypeId = 0;
	/** 0x26, or 0 when the target is an enemy */
	uint8_t enemyFlag = 0;
	uint8_t raceId = 0, classId = 0, genderId = 0;
	uint16_t state = 0;
	uint8_t heading = 0;
	std::string name;
	uint16_t titleId = 0;
	uint16_t mentorFlag = 0;
	uint16_t castingSkillId = 0;
	bool legionMember = false;
	int32_t legionId = 0;
	std::string legionName;
	uint8_t hpPercentage = 0;
	uint16_t dp = 0;
	EquippedItems equipment;
	Appearance appearance;
	float scale = 0, gravity = 0;
	float movementSpeed = 0;
	uint16_t attackSpeedBase = 0, attackSpeedCurrent = 0;
	uint8_t portAnimationId = 0;
	std::string storeMessage;
	float vectorX = 0, vectorY = 0, vectorZ = 0;
	float moveX = 0, moveY = 0, moveZ = 0;
	uint8_t movementMask = 0;
	int32_t flightPathId = 0, flightPathDistance = 0;
	uint8_t visualState = 0;
	std::string note;
	uint16_t level = 0;
	uint16_t display = 0, deny = 0;
	uint16_t abyssRankId = 0;
	int32_t targetObjectId = 0;
	int32_t currentTeamId = 0;
	bool mentor = false;
	int32_t houseAddressId = 0;
	/** 1 without membership, else 3 + membership */
	int32_t membership = 0;
	uint8_t conquerorRank = 0, protectorRank = 0;
};

/** skillinfo/SkillEntryWriter: 11 bytes per skill */
struct SkillEntry {
	uint16_t skillId = 0;
	/** 1 for a normal skill, else the skill level */
	uint16_t skillLevel = 0;
	uint8_t professionSkillBarSize = 0;
	int32_t flag = 0;
	uint8_t skillType = 0;

	bool operator==(const SkillEntry&) const = default;
};

/** SM_SKILL_LIST */
struct SkillList {
	/** 1: only list the skills; 0: also update the skill bar and notify */
	bool silentUpdate = false;
	std::vector<SkillEntry> skills;
	int32_t messageId = 0;
	std::string skillNameL10n;
	std::string skillLevelText;
};

/** SM_QUEST_COMPLETED_LIST */
struct QuestCompletedEntry {
	int32_t questId = 0;
	uint8_t completeCount = 0;
	/** 0 if the quest can be repeated, else 1 */
	uint8_t repeatFlag = 0;

	bool operator==(const QuestCompletedEntry&) const = default;
};

struct QuestCompletedList {
	/** 0 = rewrite all entries, 1 = insert new entries */
	uint8_t updateMode = 0;
	std::vector<QuestCompletedEntry> quests;
};

/** iteminfo/GeneralInfoBlobEntry (0x00) */
struct ItemGeneralInfo {
	uint16_t itemMask = 0;
	int64_t count = 0;
	std::string creator;
	int32_t secondsUntilExpiration = 0;
	int32_t temporaryExchangeTimeRemaining = 0;
	/** 3 when account or legion warehouse storability is disabled, else 0 */
	uint16_t sealStatus = 0;
};

/** iteminfo/EnchantInfoBlobEntry (0x0B), 138 bytes */
struct ItemEnchantInfo {
	bool soulBound = false;
	uint8_t enchantLevel = 0;
	int32_t skinTemplateId = 0;
	int8_t optionalSockets = 0;
	int8_t enchantBonus = 0;
	/** Item.MAX_BASIC_STONES manastone item ids by slot (0 = empty) */
	std::array<int32_t, 6> manaStones{};
	int32_t godStoneId = 0;
	DyeInfo color;
	int32_t dyeSecondsLeft = 0;
	int32_t idianStoneId = 0;
	uint8_t polishNumber = 0;
	uint8_t tempering = 0;
	bool amplified = false;
	int32_t buffSkill = 0;
};

/** one item of SM_INVENTORY_INFO */
struct InventoryItem {
	int32_t objectId = 0;
	int32_t templateId = 0;
	std::string l10n;
	/** the blob entry ids in the order ItemInfoBlob.getFullBlob added them */
	std::vector<uint8_t> blobEntryIds;
	std::optional<ItemGeneralInfo> general;
	std::optional<ItemEnchantInfo> enchant;
	/** EQUIPPED_SLOT (0x06): the slot mask the item is equipped in, 0 when it is not equipped */
	std::optional<int64_t> equippedSlotBlob;
	/** the low 16 bits of Item.getEquipmentSlot() written after the blob (0xFFFF for an unequipped item whose slot is -1) */
	uint16_t equipmentSlot = 0;
	bool cloth = false;
};

/** SM_INVENTORY_INFO */
struct InventoryInfo {
	bool firstPacket = false;
	uint8_t npcExpands = 0, questExpands = 0, itemExpands = 0;
	std::vector<InventoryItem> items;
};

/** SM_STATS_INFO. `base` fields are the second half of the packet, everything else is the current value. */
struct StatsInfo {
	int32_t objectId = 0;
	/** GameTime.getTime(): minutes since 1/1/00 00:00:00 */
	int32_t gameTime = 0;
	uint16_t power = 0, health = 0, accuracy = 0, agility = 0, knowledge = 0, will = 0;
	uint16_t waterResistance = 0, windResistance = 0, earthResistance = 0, fireResistance = 0, lightResistance = 0, darkResistance = 0;
	uint16_t level = 0;
	int64_t expNeed = 0, expRecoverable = 0, expShown = 0;
	int32_t maxHp = 0, currentHp = 0;
	int32_t maxMp = 0, currentMp = 0;
	uint16_t maxDp = 0, currentDp = 0;
	int32_t maxFlyTime = 0, currentFp = 0;
	uint8_t flyState = 0, movementMask = 0;
	uint16_t mainHandPAttack = 0, offHandPAttack = 0;
	int32_t pDef = 0;
	uint16_t mainHandMAttack = 0, offHandMAttack = 0;
	int32_t mDef = 0;
	uint16_t mResist = 0;
	float attackRange = 0;
	uint16_t attackSpeed = 0;
	uint16_t evasion = 0, parry = 0, block = 0;
	uint16_t mainHandPCritical = 0, offHandPCritical = 0;
	uint16_t mainHandPAccuracy = 0, offHandPAccuracy = 0;
	uint16_t mAccuracy = 0, mCritical = 0;
	float castingSpeed = 0;
	uint16_t concentration = 0, mBoost = 0, mbResist = 0, healBoost = 0;
	uint16_t pcr = 0, mcr = 0;
	uint16_t physicalCriticalDamageReduce = 0, magicalCriticalDamageReduce = 0;
	int32_t inventoryLimit = 0, inventorySize = 0;
	int32_t classId = 0;
	int64_t currentReposeEnergy = 0, maxReposeEnergy = 0, currentSalvationPercent = 0;

	uint16_t basePower = 0, baseHealth = 0, baseAccuracy = 0, baseAgility = 0, baseKnowledge = 0, baseWill = 0;
	uint16_t baseWaterResistance = 0, baseWindResistance = 0, baseEarthResistance = 0, baseFireResistance = 0, baseLightResistance = 0,
	         baseDarkResistance = 0;
	int32_t baseMaxHp = 0, baseMaxMp = 0;
	uint16_t baseMaxDp = 0;
	int32_t baseFlyTime = 0;
	uint16_t baseMainHandPAttack = 0, baseOffHandPAttack = 0, baseMainHandMAttack = 0, baseOffHandMAttack = 0;
	int32_t basePDef = 0, baseMDef = 0;
	uint16_t baseMResist = 0;
	float baseAttackRange = 0;
	uint16_t baseEvasion = 0, baseParry = 0, baseBlock = 0;
	uint16_t baseMainHandPCritical = 0, baseOffHandPCritical = 0, baseMCritical = 0;
	uint16_t baseMainHandPAccuracy = 0, baseOffHandPAccuracy = 0;
	uint16_t baseMAccuracy = 0;
	uint16_t baseConcentration = 0, baseMBoost = 0, baseMBResist = 0, baseHealBoost = 0;
	uint16_t basePcr = 0, baseMcr = 0;
	uint16_t basePhysicalCriticalDamageReduce = 0, baseMagicalCriticalDamageReduce = 0;
};

/** SM_NPC_INFO (the visibility assertions V1-V3 of m5a-plan.md §5.5 read it) */
struct NpcInfo {
	float x = 0, y = 0, z = 0;
	int32_t objectId = 0;
	/** the template id, written twice (hp gauge / talk properties and visual appearance); the decoder checks that both are equal */
	int32_t templateId = 0;
	/** CreatureType.getId() for the receiving player */
	uint8_t creatureType = 0;
	uint16_t state = 0;
	uint8_t heading = 0;
	int32_t l10nId = 0;
	int32_t titleId = 0;
	/** Summon, Kisk or house npc: the owner's object id or the house address; 0 for a plain npc */
	int32_t creatorId = 0;
	std::string masterName;
	uint8_t hpPercentage = 0;
	int32_t maxHp = 0;
	uint8_t level = 0;
	/** NpcEquippedGear.getItemsMask(), 0 without overridden equipment */
	int32_t equipmentMask = 0;
	/** one template id per set bit of the mask (NpcEquippedGear puts exactly one item into each slot it marks) */
	std::vector<int32_t> equipmentItemIds;
	float boundRadius = 0, height = 0;
	float movementSpeed = 0;
	uint16_t attackSpeedBase = 0, attackSpeedCurrent = 0;
	/** 0x13 for a flag npc, 0x01 for a new spawn, else 0 */
	uint8_t spawnFlag = 0;
	float targetX = 0, targetY = 0, targetZ = 0;
	uint8_t movementMask = 0;
	uint16_t staticId = 0;
	uint8_t visualState = 0;
	uint16_t npcObjectType = 0;
	int32_t targetObjectId = 0;
	int32_t townId = 0;
};

/** SM_GATHERABLE_INFO (V4) */
struct GatherableInfo {
	float x = 0, y = 0, z = 0;
	int32_t objectId = 0;
	int32_t staticId = 0;
	int32_t templateId = 0;
	/** 1 for a gatherable, 9 (open) or 10 (closed) for a static door */
	uint16_t stateFlag = 0;
	uint8_t heading = 0;
	int32_t l10nId = 0;
};

/** SM_QUEST_LIST (V10) */
struct QuestEntry {
	int32_t questId = 0;
	uint8_t status = 0;
	/** quest vars with the flags in the high byte */
	int32_t questVarsAndFlags = 0;
	uint8_t completeCount = 0;

	bool operator==(const QuestEntry&) const = default;
};

struct QuestList {
	std::vector<QuestEntry> quests;
};

/** SM_WAREHOUSE_INFO (V10); the items decode like the SM_INVENTORY_INFO ones without the cloth byte */
struct WarehouseInfo {
	uint8_t warehouseType = 0;
	bool firstPacket = false;
	uint8_t expandLevel = 0;
	/** true for the regular warehouse with at least one item (Java writes 1, 0 there instead of a zero short) */
	bool regularWithItems = false;
	std::vector<InventoryItem> items;
};

/** SM_MACRO_LIST (V10) */
struct MacroEntry {
	uint8_t id = 0;
	std::string xml;

	bool operator==(const MacroEntry&) const = default;
};

struct MacroList {
	int32_t playerObjectId = 0;
	bool clearList = false;
	std::vector<MacroEntry> macros;
};

// ---- block readers (used by several packets) --------------------------------------------------------------------------------------------

DyeInfo readDyeInfo(BodyReader& reader);
/** the appearance of AbstractPlayerInfoPacket.writePlayerInfo (voice first, as an int) */
Appearance readPlayerInfoAppearance(BodyReader& reader);
/** the appearance block of SM_PLAYER_INFO (no voice int, the voice is a byte before the height) */
Appearance readSpawnAppearance(BodyReader& reader);
PlayerInfoBlock readPlayerInfoBlock(BodyReader& reader);
EquippedItems readEquippedItems(BodyReader& reader);

// ---- packets ----------------------------------------------------------------------------------------------------------------------------

CharacterList decodeCharacterList(std::span<const uint8_t> body);
CreateCharacter decodeCreateCharacter(std::span<const uint8_t> body);
PlayerSpawn decodePlayerSpawn(std::span<const uint8_t> body);
PlayerInfo decodePlayerInfo(std::span<const uint8_t> body, const PlayerInfoOptions& options = {});
InventoryInfo decodeInventoryInfo(std::span<const uint8_t> body);
SkillList decodeSkillList(std::span<const uint8_t> body);
StatsInfo decodeStatsInfo(std::span<const uint8_t> body);
QuestCompletedList decodeQuestCompletedList(std::span<const uint8_t> body);
NpcInfo decodeNpcInfo(std::span<const uint8_t> body);
GatherableInfo decodeGatherableInfo(std::span<const uint8_t> body);
QuestList decodeQuestList(std::span<const uint8_t> body);
WarehouseInfo decodeWarehouseInfo(std::span<const uint8_t> body);
MacroList decodeMacroList(std::span<const uint8_t> body);

// ---- prefixes the async-allowed set of §5.9 needs ----------------------------------------------------------------------------------------

/** SM_PLAYER_STATE: the player object id (the packet is 7 bytes: object id, visual state, see state, blinking flag) */
int32_t decodePlayerStateObjectId(std::span<const uint8_t> body);
/** SM_DELETE: the object id (the packet is 5 bytes: object id and animation id) */
int32_t decodeDeleteObjectId(std::span<const uint8_t> body);
/** SM_NPC_INFO: the object id, which follows the three position floats */
int32_t decodeNpcInfoObjectId(std::span<const uint8_t> body);
/** SM_SYSTEM_MESSAGE: the message id, after the chat type, the encoding byte and the sender object id */
int32_t decodeSystemMessageId(std::span<const uint8_t> body);
/**
 * SM_MOVE: the moving creature's object id, the first field of the body (SM_MOVE.java:37). Everything after the position, the heading and the
 * movement mask depends on that mask and on whether the mover has a PlayableMoveController (SM_MOVE.java:44-67), so this is a prefix decoder
 * like decodeNpcInfoObjectId and not one of the D9 decoders that consume the body exactly.
 */
int32_t decodeMoveObjectId(std::span<const uint8_t> body);

/** The fields SM_EMOTION writes before its per-emotion switch (SM_EMOTION.java:94-97), plus the CHANGE_SPEED arm's own two */
struct EmotionHeader {
	int32_t objectId = 0;
	/** EmotionType.getTypeId() (model/EmotionType.java) */
	uint8_t emotionType = 0;
	/** Creature.getState() */
	int32_t state = 0;
	/** CreatureGameStats.getMovementSpeedFloat() */
	float speed = 0;
	/** Stat2.getBase() / getCurrent() of the attack speed; CHANGE_SPEED only (SM_EMOTION.java:170-174), 0 for every other emotion */
	int32_t baseAttackSpeed = 0, currentAttackSpeed = 0;
};

/**
 * m5a-plan.md §5.9, m5b-plan.md D2: **the four emotion types EmoteManager broadcasts for an npc**, and the complete list of them
 * (ai/manager/EmoteManager.cpp, Java EmoteManager.java): WALK from emoteStartWalking, CHANGE_SPEED plus NEUTRALMODE_IN_MOVE from
 * emoteStartIdling / emoteStartReturning / emoteStartFollowing, CHANGE_SPEED plus ATTACKMODE_IN_MOVE from emoteStartAttacking. No other
 * emotion of the 55 reaches a client because of an npc's AI.
 *
 * Three of them - WALK, ATTACKMODE_IN_MOVE and NEUTRALMODE_IN_MOVE - fall into the bare `break` arm of SM_EMOTION's switch
 * (SM_EMOTION.java:98-126), so their bodies are the 11-byte header and nothing else. CHANGE_SPEED has an arm of its own and writes the two
 * attack speeds and a zero byte after the header (SM_EMOTION.java:170-175), 16 bytes in all. decodeEmotionHeader knows both shapes and
 * consumes the body exactly for all four, which is why it can prove their framing and does not try to for the other 51 types.
 */
constexpr uint8_t EMOTION_ATTACKMODE_IN_MOVE = 24;
constexpr uint8_t EMOTION_NEUTRALMODE_IN_MOVE = 25;
constexpr uint8_t EMOTION_WALK = 26;
constexpr uint8_t EMOTION_CHANGE_SPEED = 35;

/** true for one of the four EmoteManager emotion types above */
bool isNpcEmote(uint8_t emotionType);

/**
 * SM_EMOTION: object id, emotion type, state and speed, plus the two attack speeds and the zero byte of the CHANGE_SPEED arm. For an emotion
 * of isNpcEmote the whole body is consumed and a trailing byte fails; for every other type the switch writes a payload this decoder does not
 * model and only the header is read.
 */
EmotionHeader decodeEmotionHeader(std::span<const uint8_t> body);

/** SM_LOOKATOBJECT (SM_LOOKATOBJECT.java:24-26): the whole 9-byte body - who looks, what it looks at (0 for no target) and its heading */
struct LookAtObject {
	int32_t objectId = 0;
	int32_t targetObjectId = 0;
	uint8_t heading = 0;
};
LookAtObject decodeLookAtObject(std::span<const uint8_t> body);

/** SM_ATTACK (SM_ATTACK.java:70-76): the two creatures of one swing */
struct AttackParties {
	int32_t attackerObjectId = 0;
	int32_t targetObjectId = 0;
};
/**
 * The attacker and the target of an SM_ATTACK. It is a prefix decoder: everything after the two HP percentages depends on the attack status,
 * on the critical proc effect and on the per-result shield types (SM_ATTACK.java:80-167), which is G-04's work for the M5b gate.
 */
AttackParties decodeAttackParties(std::span<const uint8_t> body);

/** SM_ATTACK_STATUS (SM_ATTACK_STATUS.java:60): the creature whose HP or MP changed, the first field */
int32_t decodeAttackStatusObjectId(std::span<const uint8_t> body);

} // namespace aion::gameserver::scenario::decoders
