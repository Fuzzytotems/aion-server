#include "decoders/PacketDecoders.h"

#include <bit>
#include <utility>

#include "aion/commons/utils/StringUtils.h"

namespace aion::gameserver::scenario::decoders {

namespace {

/** ItemInfoBlob.ItemBlobType entry ids */
enum class BlobType : uint8_t {
	GENERAL_INFO = 0x00,
	SLOTS_WEAPON = 0x01,
	SLOTS_ARMOR = 0x02,
	SLOTS_SHIELD = 0x03,
	SLOTS_ACCESSORY = 0x04,
	SLOTS_ARROW = 0x05,
	EQUIPPED_SLOT = 0x06,
	STIGMA_INFO = 0x07,
	STIGMA_SHARD = 0x08,
	STAT_BONUSES = 0x0A,
	ENCHANT_INFO = 0x0B,
	SLOTS_WING = 0x0D,
	COMPOSITE_ITEM = 0x0E,
	CONDITIONING_INFO = 0x0F,
	PREMIUM_OPTION = 0x10,
	POLISH_INFO = 0x11,
	WRAP_INFO = 0x12,
	PLUME_INFO = 0x13,
};

/** Item.MAX_BASIC_STONES */
constexpr size_t MAX_BASIC_STONES = 6;
/** AbstractPlayerInfoPacket.CHARNAME_MAX_LENGTH */
constexpr size_t CHARNAME_MAX_LENGTH = 25;
/** the fixed legion name length of writePlayerInfo */
constexpr size_t LEGION_NAME_LENGTH = 40;

} // namespace

// ---- BodyReader -------------------------------------------------------------------------------------------------------------------------

void BodyReader::fail(std::string_view what) const {
	throw DecodeError(packetName + ": " + std::string(what) + " at offset " + std::to_string(pos) + " of " + std::to_string(body.size()));
}

uint8_t BodyReader::C() {
	if (remaining() < 1)
		fail("body ends before a byte");
	return body[pos++];
}

int8_t BodyReader::Cs() {
	return static_cast<int8_t>(C());
}

uint16_t BodyReader::H() {
	if (remaining() < 2)
		fail("body ends before a short");
	const uint16_t value = static_cast<uint16_t>(body[pos] | static_cast<uint16_t>(body[pos + 1]) << 8);
	pos += 2;
	return value;
}

int16_t BodyReader::Hs() {
	return static_cast<int16_t>(H());
}

int32_t BodyReader::D() {
	if (remaining() < 4)
		fail("body ends before an int");
	const uint32_t value = static_cast<uint32_t>(body[pos]) | static_cast<uint32_t>(body[pos + 1]) << 8 | static_cast<uint32_t>(body[pos + 2]) << 16 |
	                       static_cast<uint32_t>(body[pos + 3]) << 24;
	pos += 4;
	return static_cast<int32_t>(value);
}

int64_t BodyReader::Q() {
	const uint64_t low = static_cast<uint32_t>(D());
	const uint64_t high = static_cast<uint32_t>(D());
	return static_cast<int64_t>(low | high << 32);
}

float BodyReader::F() {
	return std::bit_cast<float>(static_cast<uint32_t>(D()));
}

std::string BodyReader::S() {
	std::u16string text;
	for (char16_t c; (c = static_cast<char16_t>(H())) != 0;)
		text += c;
	return commons::utils::StringUtils::toUtf8(text);
}

std::string BodyReader::S(size_t fixedLength) {
	// AionServerPacket.writeS(String, int): fixedLength chars plus the terminating NUL char, NUL padded, or (fixedLength + 1) * 2 zero bytes
	std::u16string text;
	bool ended = false;
	for (size_t i = 0; i <= fixedLength; i++) {
		const char16_t c = static_cast<char16_t>(H());
		if (c == 0)
			ended = true;
		else if (!ended)
			text += c;
		else
			fail("a fixed length string continues after its terminating NUL char");
	}
	return commons::utils::StringUtils::toUtf8(text);
}

std::vector<uint8_t> BodyReader::B(size_t count) {
	if (remaining() < count)
		fail("body ends before " + std::to_string(count) + " bytes");
	std::vector<uint8_t> bytes(body.begin() + static_cast<ptrdiff_t>(pos), body.begin() + static_cast<ptrdiff_t>(pos + count));
	pos += count;
	return bytes;
}

void BodyReader::skip(size_t count) {
	if (remaining() < count)
		fail("body ends before " + std::to_string(count) + " skipped bytes");
	pos += count;
}

void BodyReader::expectZeros(size_t count, std::string_view what) {
	const size_t start = pos;
	for (size_t i = 0; i < count; i++) {
		if (C() != 0) {
			pos = start + i;
			fail(std::string(what) + ": byte " + std::to_string(i) + " of " + std::to_string(count) + " is not 0");
		}
	}
}

void BodyReader::expectC(uint8_t value, std::string_view what) {
	const size_t start = pos;
	if (const uint8_t read = C(); read != value) {
		pos = start;
		fail(std::string(what) + ": expected " + std::to_string(value) + ", got " + std::to_string(read));
	}
}

void BodyReader::expectH(uint16_t value, std::string_view what) {
	const size_t start = pos;
	if (const uint16_t read = H(); read != value) {
		pos = start;
		fail(std::string(what) + ": expected " + std::to_string(value) + ", got " + std::to_string(read));
	}
}

void BodyReader::expectD(int32_t value, std::string_view what) {
	const size_t start = pos;
	if (const int32_t read = D(); read != value) {
		pos = start;
		fail(std::string(what) + ": expected " + std::to_string(value) + ", got " + std::to_string(read));
	}
}

void BodyReader::expectFullyConsumed() const {
	if (remaining() != 0)
		fail(std::to_string(remaining()) + " bytes left after the last field");
}

// ---- blocks -----------------------------------------------------------------------------------------------------------------------------

DyeInfo readDyeInfo(BodyReader& reader) {
	DyeInfo dye;
	dye.status = reader.C();
	dye.r = reader.C();
	dye.g = reader.C();
	dye.b = reader.C();
	return dye;
}

namespace {

/** the byte run both appearance blocks share, from the face to the facial rate (writePlayerInfo and SM_PLAYER_INFO write it identically) */
void readAppearanceBytes(BodyReader& reader, Appearance& appearance) {
	appearance.face = reader.C();
	appearance.hair = reader.C();
	appearance.deco = reader.C();
	appearance.tattoo = reader.C();
	appearance.faceContour = reader.C();
	appearance.expression = reader.C();
	reader.expectC(5, "the appearance constant after the expression"); // writeC(5) // always 5 o0
	appearance.jawLine = reader.C();
	appearance.forehead = reader.C();
	appearance.eyeHeight = reader.C();
	appearance.eyeSpace = reader.C();
	appearance.eyeWidth = reader.C();
	appearance.eyeSize = reader.C();
	appearance.eyeShape = reader.C();
	appearance.eyeAngle = reader.C();
	appearance.browHeight = reader.C();
	appearance.browAngle = reader.C();
	appearance.browShape = reader.C();
	appearance.nose = reader.C();
	appearance.noseBridge = reader.C();
	appearance.noseWidth = reader.C();
	appearance.noseTip = reader.C();
	appearance.cheek = reader.C();
	appearance.lipHeight = reader.C();
	appearance.mouthSize = reader.C();
	appearance.lipSize = reader.C();
	appearance.smile = reader.C();
	appearance.lipShape = reader.C();
	appearance.jawHeight = reader.C();
	appearance.chinJut = reader.C();
	appearance.earShape = reader.C();
	appearance.headSize = reader.C();
	appearance.neck = reader.C();
	appearance.neckLength = reader.C();
	appearance.shoulderSize = reader.C();
	appearance.torso = reader.C();
	appearance.chest = reader.C();
	appearance.waist = reader.C();
	appearance.hips = reader.C();
	appearance.armThickness = reader.C();
	appearance.handSize = reader.C();
	appearance.legThickness = reader.C();
	appearance.footSize = reader.C();
	appearance.facialRate = reader.C();
	reader.expectC(0, "the appearance filler after the facial rate");
	appearance.armLength = reader.C();
	appearance.legLength = reader.C();
	appearance.shoulders = reader.C();
	appearance.faceShape = reader.C();
}

} // namespace

Appearance readPlayerInfoAppearance(BodyReader& reader) {
	Appearance appearance;
	appearance.voice = reader.D();
	appearance.skinRGB = reader.D();
	appearance.hairRGB = reader.D();
	appearance.eyeRGB = reader.D();
	appearance.lipRGB = reader.D();
	readAppearanceBytes(reader, appearance);
	reader.expectC(0, "the access level filler"); // always 0 may be acessLevel
	reader.expectC(0, "the 0xC7 filler");         // sometimes 0xC7 (199) for all chars, else 0
	reader.expectC(0, "the 0x04 filler");         // sometimes 0x04 (4) for all chars, else 0
	appearance.height = reader.F();
	return appearance;
}

Appearance readSpawnAppearance(BodyReader& reader) {
	Appearance appearance;
	appearance.skinRGB = reader.D();
	appearance.hairRGB = reader.D();
	appearance.eyeRGB = reader.D();
	appearance.lipRGB = reader.D();
	readAppearanceBytes(reader, appearance);
	reader.expectC(0, "the filler after the face shape");
	appearance.voice = reader.C();
	appearance.height = reader.F();
	return appearance;
}

PlayerInfoBlock readPlayerInfoBlock(BodyReader& reader) {
	PlayerInfoBlock block;
	block.playerId = reader.D();
	block.name = reader.S(CHARNAME_MAX_LENGTH);
	block.genderId = reader.D();
	block.raceId = reader.D();
	block.classId = reader.D();
	block.appearance = readPlayerInfoAppearance(reader);
	block.templateId = reader.D();
	block.mapId = reader.D();
	block.x = reader.F();
	block.y = reader.F();
	block.z = reader.F();
	block.heading = reader.D();
	block.level = reader.H();
	reader.expectH(0, "the unknown short after the level");
	block.titleId = reader.D();
	block.legionId = reader.D();
	block.legionName = reader.S(LEGION_NAME_LENGTH);
	block.legionMember = reader.H() != 0;
	block.lastOnlineEpochSeconds = reader.D();
	for (VisibleItem& item : block.visibleItems) {
		item.slotType = reader.C();
		item.itemId = reader.D();
		item.godStoneId = reader.D();
		item.color = readDyeInfo(reader);
	}
	reader.expectZeros(24, "the six int fillers after the visible items"); // writeD(0) x2 plus four 4.5 ints
	reader.expectZeros(68, "the 4.7 filler");
	block.deletionTimeInSeconds = reader.D();
	block.display = reader.H();
	reader.expectH(0, "the short after the display setting");
	reader.expectD(0, "the total mail count");
	block.unreadMail = reader.D();
	reader.expectD(0, "the express mail count");
	reader.expectD(0, "the blackcloud mail count");
	block.brokerKinah = reader.Q();
	reader.expectZeros(20, "the five int fillers before the ban info");
	block.banStart = reader.D();
	block.banEnd = reader.D();
	block.banReason = reader.S();
	return block;
}

EquippedItems readEquippedItems(BodyReader& reader) {
	EquippedItems equipment;
	equipment.mask = reader.D();
	// writeEquippedItems writes no count: the mask has one bit per item (the SUB_HAND bit of a two-handed weapon is cleared again)
	const int count = std::popcount(static_cast<uint32_t>(equipment.mask));
	for (int i = 0; i < count; i++) {
		EquippedItem item;
		item.skinTemplateId = reader.D();
		item.godStoneId = reader.D();
		item.color = readDyeInfo(reader);
		item.enchantParam = reader.H();
		reader.expectH(0, "the 4.7 short of an equipped item");
		equipment.items.push_back(item);
	}
	return equipment;
}

// ---- packets ----------------------------------------------------------------------------------------------------------------------------

CharacterList decodeCharacterList(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_CHARACTER_LIST");
	CharacterList list;
	list.playOk2 = reader.D();
	list.characterCount = reader.C();
	for (uint8_t i = 0; i < list.characterCount; i++)
		list.characters.push_back(readPlayerInfoBlock(reader));
	reader.expectFullyConsumed();
	return list;
}

CreateCharacter decodeCreateCharacter(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_CREATE_CHARACTER");
	CreateCharacter response;
	response.responseCode = reader.D();
	if (response.responseCode == 0) // SM_CREATE_CHARACTER.RESPONSE_OK
		response.player = readPlayerInfoBlock(reader);
	reader.expectFullyConsumed();
	return response;
}

PlayerSpawn decodePlayerSpawn(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_PLAYER_SPAWN");
	PlayerSpawn spawn;
	spawn.worldChannel = reader.D();
	spawn.worldId = reader.D();
	reader.expectD(0, "the unknown int after the world id");
	spawn.personal = reader.C() != 0;
	spawn.x = reader.F();
	spawn.y = reader.F();
	spawn.z = reader.F();
	spawn.heading = reader.C();
	reader.expectZeros(8, "the two 2.5 ints");
	reader.expectD(0, "the victory pledge state");
	spawn.beginnerTwins = reader.C();
	reader.expectD(0, "the 4.0 int");
	reader.expectC(0, "the 4.7 byte");
	reader.expectFullyConsumed();
	return spawn;
}

PlayerInfo decodePlayerInfo(std::span<const uint8_t> body, const PlayerInfoOptions& options) {
	BodyReader reader(body, "SM_PLAYER_INFO");
	PlayerInfo info;
	info.x = reader.F();
	info.y = reader.F();
	info.z = reader.F();
	info.objectId = reader.D();
	info.templateId = reader.D();
	info.robotId = reader.D();
	info.transformModelId = reader.D();
	reader.expectC(0, "the 2.0 byte after the transform model");
	info.transformTypeId = reader.D();
	info.enemyFlag = reader.C();
	info.raceId = reader.C();
	info.classId = reader.C();
	info.genderId = reader.C();
	info.state = reader.H();
	reader.expectD(0, "the int before the someState flag");
	// Java's someState local is always false, so the 13 bytes it would add are never written
	reader.expectD(0, "the someState flag");
	info.heading = reader.C();
	info.name = reader.S();
	info.titleId = reader.H();
	info.mentorFlag = reader.H();
	info.castingSkillId = reader.H();
	// the legion branch writes the legion id first and a legion id is never 0, so a 0 int is Java's 12 zero bytes for a player without a legion
	info.legionId = reader.D();
	if (info.legionId == 0) {
		reader.expectZeros(8, "the filler of a player without a legion");
	} else {
		info.legionMember = true;
		reader.skip(6); // emblem id, emblem type and the four emblem color bytes
		info.legionName = reader.S();
	}
	info.hpPercentage = reader.C();
	info.dp = reader.H();
	reader.expectC(0, "the unknown byte after the dp");
	info.equipment = readEquippedItems(reader);
	info.appearance = readSpawnAppearance(reader);
	info.scale = reader.F();
	info.gravity = reader.F();
	info.movementSpeed = reader.F();
	info.attackSpeedBase = reader.H();
	info.attackSpeedCurrent = reader.H();
	info.portAnimationId = reader.C();
	info.storeMessage = reader.S();
	info.vectorX = reader.F();
	info.vectorY = reader.F();
	info.vectorZ = reader.F();
	info.moveX = reader.F();
	info.moveY = reader.F();
	info.moveZ = reader.F();
	info.movementMask = reader.C();
	if (options.flightPath) {
		info.flightPathId = reader.D();
		info.flightPathDistance = reader.D();
	}
	info.visualState = reader.C();
	info.note = reader.S();
	info.level = reader.H();
	info.display = reader.H();
	info.deny = reader.H();
	info.abyssRankId = reader.H();
	reader.expectH(0, "the unknown short after the abyss rank");
	info.targetObjectId = reader.D();
	reader.expectC(0, "the suspect id");
	info.currentTeamId = reader.D();
	info.mentor = reader.C() != 0;
	info.houseAddressId = reader.D();
	info.membership = reader.D();
	reader.expectD(1, "the 4.7 int");
	reader.expectC(3, "the elyos/asmodian byte"); // 3 or 5, 3 is the common value
	info.conquerorRank = reader.C();
	info.protectorRank = reader.C();
	reader.expectC(0, "the officer rank icon");
	reader.expectFullyConsumed();
	return info;
}

namespace {

/** one ItemInfoBlob entry; the entry id was already read */
void readBlobEntry(BodyReader& reader, BlobType type, InventoryItem& item) {
	switch (type) {
		case BlobType::GENERAL_INFO: {
			ItemGeneralInfo general;
			general.itemMask = reader.H();
			general.count = reader.Q();
			general.creator = reader.S();
			reader.expectC(0, "the byte after the item creator");
			general.secondsUntilExpiration = reader.D();
			reader.expectD(0, "the int after the disappear time");
			general.temporaryExchangeTimeRemaining = reader.D();
			general.sealStatus = reader.H();
			reader.expectD(0, "the remaining unsealing time");
			reader.expectH(18, "the 4.7.5 short of the general item info");
			item.general = general;
			break;
		}
		case BlobType::ENCHANT_INFO: {
			ItemEnchantInfo enchant;
			enchant.soulBound = reader.C() != 0;
			enchant.enchantLevel = reader.C();
			enchant.skinTemplateId = reader.D();
			enchant.optionalSockets = reader.Cs();
			enchant.enchantBonus = reader.Cs();
			for (size_t i = 0; i < MAX_BASIC_STONES; i++)
				enchant.manaStones[i] = reader.D();
			enchant.godStoneId = reader.D();
			enchant.color = readDyeInfo(reader);
			reader.expectC(0, "the byte after the dye info");
			reader.expectD(0, "the 1.5.1.9 int");
			enchant.dyeSecondsLeft = reader.D();
			enchant.idianStoneId = reader.D();
			enchant.polishNumber = reader.C();
			enchant.tempering = reader.C();
			reader.expectZeros(18, "the unknown block after the tempering level");
			reader.skip(16); // the plume stat block (statId/value pairs 1 and 2)
			reader.expectZeros(16, "the statId/value pairs 3 and 4");
			reader.expectZeros(16, "the statId/value pairs 5 and 6");
			reader.expectD(0, "the 4.7.5 int of the enchant info");
			enchant.amplified = reader.C() != 0;
			enchant.buffSkill = reader.D();
			reader.expectZeros(8, "the two unused skill ids");
			item.enchant = enchant;
			break;
		}
		case BlobType::EQUIPPED_SLOT:
			item.equippedSlotBlob = reader.Q();
			break;
		case BlobType::SLOTS_WEAPON:
		case BlobType::SLOTS_ACCESSORY:
		case BlobType::SLOTS_WING:
			reader.skip(16); // primary and secondary slot masks
			break;
		case BlobType::SLOTS_ARMOR:
		case BlobType::SLOTS_SHIELD:
			reader.skip(20); // primary and secondary slot masks plus the dye info
			break;
		case BlobType::SLOTS_ARROW:
			// ArrowInfoBlobEntry writes two longs but reports 8 bytes, so the blob's declared size is 8 too small (Java bug)
			reader.skip(16);
			break;
		case BlobType::PLUME_INFO:
			reader.skip(32);
			break;
		case BlobType::COMPOSITE_ITEM:
			reader.skip(4 + MAX_BASIC_STONES * 4 + 2);
			break;
		case BlobType::STIGMA_INFO:
			reader.skip(306);
			break;
		case BlobType::STIGMA_SHARD:
		case BlobType::CONDITIONING_INFO:
		case BlobType::POLISH_INFO:
			reader.skip(4);
			break;
		case BlobType::STAT_BONUSES:
			reader.skip(7); // stone mask, value and the rate flag
			break;
		case BlobType::PREMIUM_OPTION:
			reader.skip(3);
			break;
		case BlobType::WRAP_INFO:
			reader.skip(1);
			break;
		default:
			reader.fail("unknown item info blob entry id " + std::to_string(static_cast<int>(type)));
	}
}

void readItemInfoBlob(BodyReader& reader, InventoryItem& item) {
	const size_t declaredSize = reader.H();
	const size_t end = reader.offset() + declaredSize;
	if (declaredSize > reader.remaining())
		reader.fail("the item info blob announces " + std::to_string(declaredSize) + " bytes, more than the body holds");
	while (reader.offset() < end) {
		const uint8_t entryId = reader.C();
		item.blobEntryIds.push_back(entryId);
		readBlobEntry(reader, static_cast<BlobType>(entryId), item);
	}
	if (reader.offset() != end)
		reader.fail("the item info blob entries overrun its announced size of " + std::to_string(declaredSize) + " bytes");
}

} // namespace

InventoryInfo decodeInventoryInfo(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_INVENTORY_INFO");
	InventoryInfo info;
	info.firstPacket = reader.C() != 0;
	info.npcExpands = reader.C();
	info.questExpands = reader.C();
	info.itemExpands = reader.C();
	const uint16_t count = reader.H();
	for (uint16_t i = 0; i < count; i++) {
		InventoryItem item;
		item.objectId = reader.D();
		item.templateId = reader.D();
		item.l10n = reader.S();
		readItemInfoBlob(reader, item);
		item.equipmentSlot = reader.H();
		item.cloth = reader.C() != 0;
		info.items.push_back(std::move(item));
	}
	reader.expectFullyConsumed();
	return info;
}

SkillList decodeSkillList(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_SKILL_LIST");
	SkillList list;
	const uint16_t count = reader.H();
	list.silentUpdate = reader.C() != 0;
	for (uint16_t i = 0; i < count; i++) {
		SkillEntry entry;
		entry.skillId = reader.H();
		entry.skillLevel = reader.H();
		reader.expectC(0, "the byte after the skill level");
		entry.professionSkillBarSize = reader.C();
		entry.flag = reader.D();
		entry.skillType = reader.C();
		list.skills.push_back(entry);
	}
	list.messageId = reader.D();
	if (list.messageId != 0) {
		list.skillNameL10n = reader.S();
		list.skillLevelText = reader.S();
		reader.expectH(0, "the short after the skill level text");
	}
	reader.expectFullyConsumed();
	return list;
}

QuestCompletedList decodeQuestCompletedList(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_QUEST_COMPLETED_LIST");
	QuestCompletedList list;
	reader.expectC(1, "the leading byte"); // unk, always 1 (when 0, no entries change)
	list.updateMode = reader.C();
	// writeH(-questStates.size() & 0xFFFF): the entry count is written negated
	const uint16_t negated = reader.H();
	const size_t count = static_cast<size_t>((0x10000u - negated) & 0xFFFFu);
	for (size_t i = 0; i < count; i++) {
		QuestCompletedEntry entry;
		entry.questId = reader.D();
		entry.completeCount = reader.C();
		entry.repeatFlag = reader.C();
		list.quests.push_back(entry);
	}
	reader.expectFullyConsumed();
	return list;
}

StatsInfo decodeStatsInfo(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_STATS_INFO");
	StatsInfo stats;
	stats.objectId = reader.D();
	stats.gameTime = reader.D();
	stats.power = reader.H();
	stats.health = reader.H();
	stats.accuracy = reader.H();
	stats.agility = reader.H();
	stats.knowledge = reader.H();
	stats.will = reader.H();
	stats.waterResistance = reader.H();
	stats.windResistance = reader.H();
	stats.earthResistance = reader.H();
	stats.fireResistance = reader.H();
	stats.lightResistance = reader.H();
	stats.darkResistance = reader.H();
	stats.level = reader.H();
	reader.expectZeros(6, "the three dynamic shorts after the level");
	stats.expNeed = reader.Q();
	stats.expRecoverable = reader.Q();
	stats.expShown = reader.Q();
	reader.expectD(0, "the int before the max hp");
	stats.maxHp = reader.D();
	stats.currentHp = reader.D();
	stats.maxMp = reader.D();
	stats.currentMp = reader.D();
	stats.maxDp = reader.H();
	stats.currentDp = reader.H();
	stats.maxFlyTime = reader.D();
	stats.currentFp = reader.D();
	stats.flyState = reader.C();
	stats.movementMask = reader.C();
	stats.mainHandPAttack = reader.H();
	stats.offHandPAttack = reader.H();
	reader.expectH(0, "the 3.0 short after the off hand attack");
	stats.pDef = reader.D();
	stats.mainHandMAttack = reader.H();
	stats.offHandMAttack = reader.H();
	stats.mDef = reader.D();
	stats.mResist = reader.H();
	reader.expectH(0, "the 3.0 short after the magic resist");
	stats.attackRange = reader.F();
	stats.attackSpeed = reader.H();
	stats.evasion = reader.H();
	stats.parry = reader.H();
	stats.block = reader.H();
	stats.mainHandPCritical = reader.H();
	stats.offHandPCritical = reader.H();
	stats.mainHandPAccuracy = reader.H();
	stats.offHandPAccuracy = reader.H();
	reader.expectH(1, "the short after the off hand accuracy");
	stats.mAccuracy = reader.H();
	stats.mCritical = reader.H();
	reader.expectH(0, "the short after the magic critical rate");
	stats.castingSpeed = reader.F();
	reader.expectH(0, "the 3.5 short after the casting speed");
	stats.concentration = reader.H();
	stats.mBoost = reader.H();
	stats.mbResist = reader.H();
	stats.healBoost = reader.H();
	reader.expectH(0, "the short after the heal boost");
	stats.pcr = reader.H();
	stats.mcr = reader.H();
	stats.physicalCriticalDamageReduce = reader.H();
	stats.magicalCriticalDamageReduce = reader.H();
	stats.inventoryLimit = reader.D();
	stats.inventorySize = reader.D();
	reader.expectZeros(8, "the two ints after the inventory size");
	stats.classId = reader.D();
	reader.expectZeros(8, "the two 3.0 and two 3.5 shorts");
	stats.currentReposeEnergy = reader.Q();
	stats.maxReposeEnergy = reader.Q();
	stats.currentSalvationPercent = reader.Q();
	reader.expectH(0, "the first 4.3 NA short");
	reader.expectH(0, "the second 4.3 NA short");
	reader.expectH(1, "the third 4.3 NA short");
	reader.expectH(0, "the fourth 4.3 NA short");
	reader.expectZeros(8, "the four 4.8 shorts");
	stats.basePower = reader.H();
	stats.baseHealth = reader.H();
	stats.baseAccuracy = reader.H();
	stats.baseAgility = reader.H();
	stats.baseKnowledge = reader.H();
	stats.baseWill = reader.H();
	stats.baseWaterResistance = reader.H();
	stats.baseWindResistance = reader.H();
	stats.baseEarthResistance = reader.H();
	stats.baseFireResistance = reader.H();
	stats.baseLightResistance = reader.H();
	stats.baseDarkResistance = reader.H();
	stats.baseMaxHp = reader.D();
	stats.baseMaxMp = reader.D();
	stats.baseMaxDp = reader.H();
	reader.expectH(21592, "the display_max_point short");
	stats.baseFlyTime = reader.D();
	stats.baseMainHandPAttack = reader.H();
	stats.baseOffHandPAttack = reader.H();
	stats.baseMainHandMAttack = reader.H();
	stats.baseOffHandMAttack = reader.H();
	stats.basePDef = reader.D();
	stats.baseMDef = reader.D();
	stats.baseMResist = reader.H();
	stats.baseAttackRange = reader.F();
	reader.expectH(0, "the 3.5 short after the base attack range");
	stats.baseEvasion = reader.H();
	stats.baseParry = reader.H();
	stats.baseBlock = reader.H();
	stats.baseMainHandPCritical = reader.H();
	stats.baseOffHandPCritical = reader.H();
	stats.baseMCritical = reader.H();
	reader.expectH(0, "the short after the base magical critical rate");
	stats.baseMainHandPAccuracy = reader.H();
	stats.baseOffHandPAccuracy = reader.H();
	reader.expectH(0, "the short after the base off hand accuracy");
	stats.baseMAccuracy = reader.H();
	stats.baseConcentration = reader.H();
	stats.baseMBoost = reader.H();
	stats.baseMBResist = reader.H();
	stats.baseHealBoost = reader.H();
	reader.expectH(0, "the short after the base heal boost");
	stats.basePcr = reader.H();
	stats.baseMcr = reader.H();
	stats.basePhysicalCriticalDamageReduce = reader.H();
	stats.baseMagicalCriticalDamageReduce = reader.H();
	reader.expectFullyConsumed();
	return stats;
}

NpcInfo decodeNpcInfo(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_NPC_INFO");
	NpcInfo npc;
	npc.x = reader.F();
	npc.y = reader.F();
	npc.z = reader.F();
	npc.objectId = reader.D();
	npc.templateId = reader.D(); // hp gauge and talk properties
	const int32_t visualTemplateId = reader.D();
	if (visualTemplateId != npc.templateId)
		reader.fail("the two template ids of SM_NPC_INFO differ (" + std::to_string(npc.templateId) + " and " + std::to_string(visualTemplateId) + ")");
	npc.creatureType = reader.C();
	npc.state = reader.H();
	npc.heading = reader.C();
	npc.l10nId = reader.D();
	npc.titleId = reader.D();
	reader.expectH(0, "the short after the title id");
	reader.expectC(0, "the byte after the title id");
	reader.expectD(0, "the int after the title id");
	npc.creatorId = reader.D();
	npc.masterName = reader.S();
	npc.hpPercentage = reader.C();
	npc.maxHp = reader.D();
	npc.level = reader.C();
	npc.equipmentMask = reader.D();
	// SM_NPC_INFO writes no entry count: NpcEquippedGear.init puts one item into each slot of the mask, so the bit count is the entry count
	const int32_t entries = std::popcount(static_cast<uint32_t>(npc.equipmentMask));
	for (int32_t i = 0; i < entries; i++) {
		npc.equipmentItemIds.push_back(reader.D());
		reader.expectD(0, "the first int after an npc equipment item");
		reader.expectD(0, "the second int after an npc equipment item");
		reader.expectH(0, "the short after an npc equipment item");
		reader.expectH(0, "the 4.7 short after an npc equipment item");
	}
	npc.boundRadius = reader.F();
	npc.height = reader.F();
	npc.movementSpeed = reader.F();
	npc.attackSpeedBase = reader.H();
	npc.attackSpeedCurrent = reader.H();
	npc.spawnFlag = reader.C();
	npc.targetX = reader.F();
	npc.targetY = reader.F();
	npc.targetZ = reader.F();
	npc.movementMask = reader.C();
	npc.staticId = reader.H();
	reader.expectZeros(8, "the eight unknown bytes after the static id");
	npc.visualState = reader.C();
	npc.npcObjectType = reader.H();
	reader.expectC(0, "the byte after the npc object type");
	npc.targetObjectId = reader.D();
	npc.townId = reader.D();
	reader.expectD(0, "the 4.7.5 int of SM_NPC_INFO");
	reader.expectFullyConsumed();
	return npc;
}

GatherableInfo decodeGatherableInfo(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_GATHERABLE_INFO");
	GatherableInfo info;
	info.x = reader.F();
	info.y = reader.F();
	info.z = reader.F();
	info.objectId = reader.D();
	info.staticId = reader.D();
	info.templateId = reader.D();
	info.stateFlag = reader.H(); // 1, or 9 / 10 for an open / closed static door
	if (info.stateFlag != 1 && info.stateFlag != 9 && info.stateFlag != 10)
		reader.fail("unexpected state flag " + std::to_string(info.stateFlag));
	info.heading = reader.C();
	info.l10nId = reader.D();
	reader.expectH(0, "the first unknown short");
	reader.expectH(0, "the second unknown short");
	reader.expectH(0, "the third unknown short");
	reader.expectC(100, "the trailing unknown byte");
	reader.expectFullyConsumed();
	return info;
}

QuestList decodeQuestList(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_QUEST_LIST");
	QuestList list;
	reader.expectH(1, "the leading short");
	// writeH(-questStates.size() & 0xFFFF): the entry count is written negated, like SM_QUEST_COMPLETED_LIST
	const uint16_t negated = reader.H();
	const size_t count = static_cast<size_t>((0x10000u - negated) & 0xFFFFu);
	for (size_t i = 0; i < count; i++) {
		QuestEntry entry;
		entry.questId = reader.D();
		entry.status = reader.C();
		entry.questVarsAndFlags = reader.D();
		entry.completeCount = reader.C();
		list.quests.push_back(entry);
	}
	reader.expectFullyConsumed();
	return list;
}

WarehouseInfo decodeWarehouseInfo(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_WAREHOUSE_INFO");
	WarehouseInfo info;
	info.warehouseType = reader.C();
	info.firstPacket = reader.C() != 0;
	info.expandLevel = reader.C();
	// the regular warehouse with items writes writeC(1), writeC(0), every other case writeH(0): two bytes either way
	const uint16_t branch = reader.H();
	if (branch != 0 && branch != 1)
		reader.fail("unexpected warehouse branch marker " + std::to_string(branch));
	info.regularWithItems = branch == 1;
	const uint16_t count = reader.H();
	for (uint16_t i = 0; i < count; i++) {
		InventoryItem item;
		item.objectId = reader.D();
		item.templateId = reader.D();
		reader.expectC(0, "the item info byte of a warehouse item");
		item.l10n = reader.S();
		readItemInfoBlob(reader, item);
		item.equipmentSlot = reader.H(); // SM_WAREHOUSE_INFO writes no cloth byte
		info.items.push_back(std::move(item));
	}
	reader.expectFullyConsumed();
	return info;
}

MacroList decodeMacroList(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_MACRO_LIST");
	MacroList list;
	list.playerObjectId = reader.D();
	list.clearList = reader.C() != 0;
	const uint16_t negated = reader.H(); // writeH(-macros.size())
	const size_t count = static_cast<size_t>((0x10000u - negated) & 0xFFFFu);
	for (size_t i = 0; i < count; i++) {
		MacroEntry entry;
		entry.id = reader.C();
		entry.xml = reader.S();
		list.macros.push_back(std::move(entry));
	}
	reader.expectFullyConsumed();
	return list;
}

// ---- prefixes ---------------------------------------------------------------------------------------------------------------------------

int32_t decodePlayerStateObjectId(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_PLAYER_STATE");
	const int32_t objectId = reader.D();
	reader.skip(3); // visual state, see state and the blinking flag
	reader.expectFullyConsumed();
	return objectId;
}

int32_t decodeDeleteObjectId(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_DELETE");
	const int32_t objectId = reader.D();
	reader.skip(1); // animation id
	reader.expectFullyConsumed();
	return objectId;
}

int32_t decodeNpcInfoObjectId(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_NPC_INFO");
	reader.skip(12); // x, y, z
	return reader.D();
}

int32_t decodeSystemMessageId(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_SYSTEM_MESSAGE");
	reader.skip(2); // chat type and the text encoding byte
	reader.skip(4); // sender object id
	return reader.D();
}

int32_t decodeMoveObjectId(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_MOVE");
	return reader.D(); // creature.getObjectId(), SM_MOVE.java:37
}

NpcMove decodeNpcMove(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_MOVE");
	NpcMove move;
	move.objectId = reader.D(); // SM_MOVE.java:37-41
	move.x = reader.F();
	move.y = reader.F();
	move.z = reader.F();
	move.heading = reader.C();
	move.movementMask = reader.C(); // :43
	// :45-55: POSITION and MANUAL write the target; for an npc (pmc == null) always the move controller's getTargetX2/Y2/Z2
	if ((move.movementMask & MOVEMENT_MASK_POSITION) != 0 && (move.movementMask & MOVEMENT_MASK_MANUAL) != 0)
		move.target = std::array<float, 3>{reader.F(), reader.F(), reader.F()};
	// :56-61: GLIDE writes the glide flag, 0 without a PlayableMoveController, and no geyser id then; :62-68 the vehicle arm needs a pmc
	if ((move.movementMask & MOVEMENT_MASK_GLIDE) != 0)
		reader.expectC(0, "SM_MOVE glide flag of an npc (pmc == null writes 0)");
	reader.expectFullyConsumed();
	return move;
}

bool isNpcEmote(uint8_t emotionType) {
	return emotionType == EMOTION_ATTACKMODE_IN_MOVE || emotionType == EMOTION_NEUTRALMODE_IN_MOVE || emotionType == EMOTION_WALK ||
		emotionType == EMOTION_CHANGE_SPEED;
}

EmotionHeader decodeEmotionHeader(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_EMOTION");
	EmotionHeader emotion;
	emotion.objectId = reader.D();
	emotion.emotionType = reader.C();
	emotion.state = reader.H();
	emotion.speed = reader.F();
	if (emotion.emotionType == EMOTION_CHANGE_SPEED) {
		// SM_EMOTION.java:170-175, the "emote startloop" arm
		emotion.baseAttackSpeed = reader.H();
		emotion.currentAttackSpeed = reader.H();
		reader.expectC(0, "the byte after the two attack speeds (SM_EMOTION.java:174, \"new 4.0\")");
		reader.expectFullyConsumed();
	} else if (isNpcEmote(emotion.emotionType)) {
		reader.expectFullyConsumed(); // SM_EMOTION.java:98-126: WALK, ATTACKMODE_IN_MOVE and NEUTRALMODE_IN_MOVE write nothing after the header
	}
	return emotion;
}

LookAtObject decodeLookAtObject(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_LOOKATOBJECT");
	LookAtObject look;
	look.objectId = reader.D();
	look.targetObjectId = reader.D();
	look.heading = reader.C();
	reader.expectFullyConsumed();
	return look;
}

AttackParties decodeAttackParties(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_ATTACK");
	AttackParties parties;
	parties.attackerObjectId = reader.D();
	reader.skip(1); // attackno
	reader.skip(2); // time
	reader.skip(2); // attackTypeAnimation and attackHandAnimation
	parties.targetObjectId = reader.D();
	return parties;
}

int32_t decodeAttackStatusObjectId(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_ATTACK_STATUS");
	return reader.D(); // creature.getObjectId()
}

} // namespace aion::gameserver::scenario::decoders
