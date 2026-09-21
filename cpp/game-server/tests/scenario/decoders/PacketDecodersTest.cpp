// The F-08 decoders against byte vectors written from the Java field order (m5a-plan.md D9). The builders below are the Java writeImpl
// sequences spelled out once more, field by field, and every case also asserts the body size Java produces: a decoder that reads a field with
// the wrong width or in the wrong order cannot pass both the value assertions and the exact-consumption check.

#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "decoders/PacketDecoders.h"

#include "NetworkTestSupport.h"

namespace aion::gameserver::scenario::decoders {
namespace {

using network::test::PacketWriter;

/** AionServerPacket.writeS(String, int): fixedLength chars plus the terminating NUL char, NUL padded */
void fixedS(PacketWriter& writer, std::string_view text, size_t fixedLength) {
	const std::u16string utf16 = commons::utils::StringUtils::toUtf16(text);
	for (size_t i = 0; i < fixedLength; i++)
		writer.H(i < utf16.size() ? static_cast<int32_t>(utf16[i]) : 0);
	writer.H(0);
}

/** AionServerPacket.writeDyeInfo */
void dyeInfo(PacketWriter& writer, const DyeInfo& dye) {
	writer.C(dye.status).C(dye.r).C(dye.g).C(dye.b);
}

/** a distinct value per appearance field, so a swapped pair fails */
Appearance sampleAppearance() {
	Appearance a;
	a.voice = 3;
	a.skinRGB = 0x00E1C3B4;
	a.hairRGB = 0x00202021;
	a.eyeRGB = 0x00503011;
	a.lipRGB = 0x00A06062;
	uint8_t value = 11;
	for (uint8_t* field : {&a.face, &a.hair, &a.deco, &a.tattoo, &a.faceContour, &a.expression, &a.jawLine, &a.forehead, &a.eyeHeight, &a.eyeSpace,
				 &a.eyeWidth, &a.eyeSize, &a.eyeShape, &a.eyeAngle, &a.browHeight, &a.browAngle, &a.browShape, &a.nose, &a.noseBridge, &a.noseWidth,
				 &a.noseTip, &a.cheek, &a.lipHeight, &a.mouthSize, &a.lipSize, &a.smile, &a.lipShape, &a.jawHeight, &a.chinJut, &a.earShape, &a.headSize,
				 &a.neck, &a.neckLength, &a.shoulderSize, &a.torso, &a.chest, &a.waist, &a.hips, &a.armThickness, &a.handSize, &a.legThickness,
				 &a.footSize, &a.facialRate, &a.armLength, &a.legLength, &a.shoulders, &a.faceShape})
		*field = value++;
	a.height = 1.125f;
	return a;
}

/** the byte run both appearance blocks share (face .. faceShape), Java order */
void appearanceBytes(PacketWriter& writer, const Appearance& a) {
	writer.C(a.face).C(a.hair).C(a.deco).C(a.tattoo).C(a.faceContour).C(a.expression);
	writer.C(5); // always 5 o0
	writer.C(a.jawLine).C(a.forehead);
	writer.C(a.eyeHeight).C(a.eyeSpace).C(a.eyeWidth).C(a.eyeSize).C(a.eyeShape).C(a.eyeAngle);
	writer.C(a.browHeight).C(a.browAngle).C(a.browShape);
	writer.C(a.nose).C(a.noseBridge).C(a.noseWidth).C(a.noseTip);
	writer.C(a.cheek).C(a.lipHeight).C(a.mouthSize).C(a.lipSize).C(a.smile).C(a.lipShape).C(a.jawHeight).C(a.chinJut).C(a.earShape).C(a.headSize);
	writer.C(a.neck).C(a.neckLength).C(a.shoulderSize);
	writer.C(a.torso).C(a.chest).C(a.waist).C(a.hips);
	writer.C(a.armThickness).C(a.handSize).C(a.legThickness);
	writer.C(a.footSize).C(a.facialRate);
	writer.C(0x00);
	writer.C(a.armLength).C(a.legLength).C(a.shoulders).C(a.faceShape);
}

/** AbstractPlayerInfoPacket.writePlayerInfo appearance part */
void playerInfoAppearance(PacketWriter& writer, const Appearance& a) {
	writer.D(a.voice).D(a.skinRGB).D(a.hairRGB).D(a.eyeRGB).D(a.lipRGB);
	appearanceBytes(writer, a);
	writer.C(0x00).C(0x00).C(0x00);
	writer.F(a.height);
}

/** the appearance part of SM_PLAYER_INFO */
void spawnAppearance(PacketWriter& writer, const Appearance& a) {
	writer.D(a.skinRGB).D(a.hairRGB).D(a.eyeRGB).D(a.lipRGB);
	appearanceBytes(writer, a);
	writer.C(0x00);
	writer.C(a.voice);
	writer.F(a.height);
}

/** AbstractPlayerInfoPacket.writePlayerInfo */
void playerInfoBlock(PacketWriter& writer, const PlayerInfoBlock& block) {
	writer.D(block.playerId);
	fixedS(writer, block.name, 25); // CHARNAME_MAX_LENGTH
	writer.D(block.genderId).D(block.raceId).D(block.classId);
	playerInfoAppearance(writer, block.appearance);
	writer.D(block.templateId).D(block.mapId);
	writer.F(block.x).F(block.y).F(block.z);
	writer.D(block.heading);
	writer.H(block.level).H(0);
	writer.D(block.titleId);
	writer.D(block.legionId);
	fixedS(writer, block.legionName, 40);
	writer.H(block.legionMember ? 1 : 0);
	writer.D(block.lastOnlineEpochSeconds);
	for (const VisibleItem& item : block.visibleItems) {
		writer.C(item.slotType).D(item.itemId).D(item.godStoneId);
		dyeInfo(writer, item.color);
	}
	writer.zeros(24).zeros(68);
	writer.D(block.deletionTimeInSeconds);
	writer.H(block.display).H(0);
	writer.D(0).D(block.unreadMail).D(0).D(0);
	writer.Q(block.brokerKinah);
	writer.zeros(20);
	writer.D(block.banStart).D(block.banEnd);
	writer.S(block.banReason);
}

PlayerInfoBlock sampleBlock() {
	PlayerInfoBlock block;
	block.playerId = 0x0BADF00D;
	block.name = "Skalix";
	block.genderId = 0;
	block.raceId = 0;
	block.classId = 0; // WARRIOR
	block.appearance = sampleAppearance();
	block.templateId = 0xA0;
	block.mapId = 210010000;
	block.x = 1212.94f;
	block.y = 1044.85f;
	block.z = 140.76f;
	block.heading = 77;
	block.level = 1;
	block.titleId = 0;
	block.lastOnlineEpochSeconds = 1700000000;
	block.visibleItems[0] = VisibleItem{1, 100000001, 0, DyeInfo{}};
	block.visibleItems[1] = VisibleItem{2, 100000002, 55, DyeInfo{1, 0x11, 0x22, 0x33}};
	block.display = 5;
	block.brokerKinah = 1234567890123LL;
	return block;
}

TEST(PacketDecodersTest, PlayerInfoBlockRoundTripAndSize) {
	const PlayerInfoBlock expected = sampleBlock();
	PacketWriter writer;
	playerInfoBlock(writer, expected);
	// 628 fixed bytes (4 + 52 + 12 + 76 appearance + 124 + 208 visible items + 92 fillers + 24 + 8 + 20 + 8) plus the empty ban reason
	EXPECT_EQ(writer.data.size(), 630u);

	BodyReader reader(writer.data, "block");
	const PlayerInfoBlock decoded = readPlayerInfoBlock(reader);
	reader.expectFullyConsumed();
	EXPECT_EQ(decoded.playerId, expected.playerId);
	EXPECT_EQ(decoded.name, "Skalix");
	EXPECT_EQ(decoded.raceId, 0);
	EXPECT_EQ(decoded.classId, 0);
	EXPECT_EQ(decoded.appearance, expected.appearance);
	EXPECT_EQ(decoded.mapId, 210010000);
	EXPECT_FLOAT_EQ(decoded.x, 1212.94f);
	EXPECT_FLOAT_EQ(decoded.y, 1044.85f);
	EXPECT_FLOAT_EQ(decoded.z, 140.76f);
	EXPECT_EQ(decoded.heading, 77);
	EXPECT_EQ(decoded.level, 1);
	EXPECT_FALSE(decoded.legionMember);
	EXPECT_EQ(decoded.legionName, "");
	EXPECT_EQ(decoded.visibleItems[0], expected.visibleItems[0]);
	EXPECT_EQ(decoded.visibleItems[1], expected.visibleItems[1]);
	EXPECT_EQ(decoded.visibleItems[2].itemId, 0);
	EXPECT_EQ(decoded.display, 5);
	EXPECT_EQ(decoded.brokerKinah, 1234567890123LL);
	EXPECT_EQ(decoded.banReason, "");
}

TEST(PacketDecodersTest, PlayerInfoBlockKeepsTheNameAndLegionNameFixedLength) {
	PlayerInfoBlock expected = sampleBlock();
	expected.name = "Abcdefghijklmnopqrstuvwxy"; // exactly CHARNAME_MAX_LENGTH characters
	expected.legionId = 42;
	expected.legionName = "The Longest Legion Name That Fits Here 1"; // exactly 40 characters
	expected.legionMember = true;
	PacketWriter writer;
	playerInfoBlock(writer, expected);
	EXPECT_EQ(writer.data.size(), 630u); // unchanged: both strings are fixed length

	BodyReader reader(writer.data, "block");
	const PlayerInfoBlock decoded = readPlayerInfoBlock(reader);
	reader.expectFullyConsumed();
	EXPECT_EQ(decoded.name, expected.name);
	EXPECT_EQ(decoded.legionName, expected.legionName);
	EXPECT_TRUE(decoded.legionMember);
	EXPECT_EQ(decoded.legionId, 42);
}

TEST(PacketDecodersTest, CharacterListWithAndWithoutCharacters) {
	PacketWriter empty;
	empty.D(0x11223344).C(0);
	const CharacterList none = decodeCharacterList(empty.data);
	EXPECT_EQ(none.playOk2, 0x11223344);
	EXPECT_EQ(none.characterCount, 0);
	EXPECT_TRUE(none.characters.empty());

	PacketWriter one;
	one.D(7).C(1);
	playerInfoBlock(one, sampleBlock());
	const CharacterList list = decodeCharacterList(one.data);
	ASSERT_EQ(list.characters.size(), 1u);
	EXPECT_EQ(list.characters[0].name, "Skalix");
	EXPECT_EQ(list.characters[0].mapId, 210010000);

	// a body with one byte too many must not pass
	one.C(0);
	EXPECT_THROW(decodeCharacterList(one.data), DecodeError);
}

TEST(PacketDecodersTest, CreateCharacterResponseCodes) {
	PacketWriter window;
	window.D(22); // RESPONSE_OPEN_CREATION_WINDOW: no player info block
	const CreateCharacter opened = decodeCreateCharacter(window.data);
	EXPECT_EQ(opened.responseCode, 22);
	EXPECT_FALSE(opened.player.has_value());

	PacketWriter created;
	created.D(0);
	playerInfoBlock(created, sampleBlock());
	const CreateCharacter ok = decodeCreateCharacter(created.data);
	EXPECT_EQ(ok.responseCode, 0);
	ASSERT_TRUE(ok.player.has_value());
	EXPECT_EQ(ok.player->name, "Skalix");
	EXPECT_EQ(ok.player->appearance, sampleAppearance());

	// an ok response without the block is incomplete
	PacketWriter truncated;
	truncated.D(0);
	EXPECT_THROW(decodeCreateCharacter(truncated.data), DecodeError);
}

TEST(PacketDecodersTest, PlayerSpawn) {
	PacketWriter writer;
	writer.D(210010000).D(210010000).D(0).C(0);
	writer.F(1212.94f).F(1044.85f).F(140.76f).C(77);
	writer.D(0).D(0).D(0).C(1).D(0).C(0);
	ASSERT_EQ(writer.data.size(), 44u);

	const PlayerSpawn spawn = decodePlayerSpawn(writer.data);
	EXPECT_EQ(spawn.worldChannel, 210010000);
	EXPECT_EQ(spawn.worldId, 210010000);
	EXPECT_FALSE(spawn.personal);
	EXPECT_FLOAT_EQ(spawn.x, 1212.94f);
	EXPECT_FLOAT_EQ(spawn.y, 1044.85f);
	EXPECT_FLOAT_EQ(spawn.z, 140.76f);
	EXPECT_EQ(spawn.heading, 77);
	EXPECT_EQ(spawn.beginnerTwins, 1);

	// a personal world sends the negated channel and the personal flag
	PacketWriter personal;
	personal.D(-300100000).D(300100000).D(0).C(1);
	personal.F(1.0f).F(2.0f).F(3.0f).C(0);
	personal.D(0).D(0).D(0).C(0).D(0).C(0);
	const PlayerSpawn instance = decodePlayerSpawn(personal.data);
	EXPECT_EQ(instance.worldChannel, -300100000);
	EXPECT_TRUE(instance.personal);
}

TEST(PacketDecodersTest, PlayerSpawnRejectsAChangedConstantOrTrailingBytes) {
	PacketWriter changed;
	changed.D(1).D(1).D(0).C(0);
	changed.F(0).F(0).F(0).C(0);
	changed.D(0).D(0).D(0).C(0).D(0).C(1); // the 4.7 byte must be 0
	EXPECT_THROW(decodePlayerSpawn(changed.data), DecodeError);

	PacketWriter longBody;
	longBody.D(1).D(1).D(0).C(0);
	longBody.F(0).F(0).F(0).C(0);
	longBody.D(0).D(0).D(0).C(0).D(0).C(0).C(0);
	EXPECT_THROW(decodePlayerSpawn(longBody.data), DecodeError);
}

TEST(PacketDecodersTest, SkillListSilentAndWithMessage) {
	PacketWriter silent;
	silent.H(2).C(1);
	silent.H(30).H(1).C(0).C(0).D(0).C(0);    // a normal skill: the level is always written as 1
	silent.H(1601).H(3).C(0).C(6).D(9).C(1);  // a stigma skill with a profession bar size and a flag
	silent.D(0);
	ASSERT_EQ(silent.data.size(), 3u + 2 * 11 + 4);

	const SkillList list = decodeSkillList(silent.data);
	EXPECT_TRUE(list.silentUpdate);
	ASSERT_EQ(list.skills.size(), 2u);
	EXPECT_EQ(list.skills[0], (SkillEntry{30, 1, 0, 0, 0}));
	EXPECT_EQ(list.skills[1], (SkillEntry{1601, 3, 6, 9, 1}));
	EXPECT_EQ(list.messageId, 0);

	PacketWriter learned;
	learned.H(1).C(0);
	learned.H(30).H(1).C(0).C(0).D(0).C(0);
	learned.D(1300123).S("Aether's Hold").S("2").H(0);
	const SkillList notified = decodeSkillList(learned.data);
	EXPECT_FALSE(notified.silentUpdate);
	EXPECT_EQ(notified.messageId, 1300123);
	EXPECT_EQ(notified.skillNameL10n, "Aether's Hold");
	EXPECT_EQ(notified.skillLevelText, "2");

	// the name and level strings belong to a non-zero message id only
	PacketWriter stray;
	stray.H(0).C(1).D(0).S("x");
	EXPECT_THROW(decodeSkillList(stray.data), DecodeError);
}

TEST(PacketDecodersTest, QuestCompletedListCountIsNegated) {
	PacketWriter none;
	none.C(1).C(0).H(0);
	const QuestCompletedList empty = decodeQuestCompletedList(none.data);
	EXPECT_EQ(empty.updateMode, 0);
	EXPECT_TRUE(empty.quests.empty());

	PacketWriter two;
	two.C(1).C(1).H(-2 & 0xFFFF);
	two.D(1001).C(3).C(1);
	two.D(1002).C(255).C(0);
	const QuestCompletedList list = decodeQuestCompletedList(two.data);
	EXPECT_EQ(list.updateMode, 1);
	ASSERT_EQ(list.quests.size(), 2u);
	EXPECT_EQ(list.quests[0], (QuestCompletedEntry{1001, 3, 1}));
	EXPECT_EQ(list.quests[1], (QuestCompletedEntry{1002, 255, 0}));

	// the leading byte is a Java constant
	PacketWriter wrong;
	wrong.C(0).C(0).H(0);
	EXPECT_THROW(decodeQuestCompletedList(wrong.data), DecodeError);
}

/** SM_STATS_INFO in Java order; every field gets a distinct value so a swapped pair shows up */
std::vector<uint8_t> buildStatsInfo() {
	PacketWriter w;
	w.D(0x0BADF00D).D(500000);
	w.H(100).H(101).H(102).H(103).H(104).H(105);        // power .. will
	w.H(10).H(11).H(12).H(13).H(14).H(15);              // water .. dark resistance
	w.H(1);                                             // level
	w.H(0).H(0).H(0);
	w.Q(2500).Q(0).Q(120);                              // exp need, recoverable, shown
	w.D(0);
	w.D(3000).D(1500);                                  // max hp, current hp
	w.D(400).D(399);                                    // max mp, current mp
	w.H(4000).H(0);                                     // max dp, current dp
	w.D(60000).D(59000);                                // max fly time, current fp
	w.C(0).C(0);                                        // fly state, movement mask
	w.H(31).H(32).H(0);                                 // main/off hand attack
	w.D(120);                                           // pdef
	w.H(33).H(34);                                      // main/off hand magic attack
	w.D(121);                                           // mdef
	w.H(35).H(0);                                       // magic resist
	w.F(1.5f);                                          // attack range
	w.H(1500);                                          // attack speed
	w.H(36).H(37).H(38);                                // evasion, parry, block
	w.H(39).H(40);                                      // main/off hand crit rate
	w.H(41).H(42);                                      // main/off hand accuracy
	w.H(1);
	w.H(43).H(44);                                      // magic accuracy, magic crit
	w.H(0);
	w.F(1.0f);                                          // casting speed
	w.H(0);
	w.H(45).H(46).H(47).H(48);                          // concentration, magic boost, suppression, heal boost
	w.H(0);
	w.H(49).H(50);                                      // strike/spell resist
	w.H(51).H(52);                                      // strike/spell fortitude
	w.D(27).D(3);                                       // inventory limit and size
	w.D(0).D(0);
	w.D(0);                                             // class id (WARRIOR)
	w.H(0).H(0).H(0).H(0);
	w.Q(1000).Q(6000).Q(100);                           // repose energy and salvation
	w.H(0).H(0).H(1).H(0);
	w.H(0).H(0).H(0).H(0);
	w.H(200).H(201).H(202).H(203).H(204).H(205);        // base power .. will
	w.H(20).H(21).H(22).H(23).H(24).H(25);              // base resistances
	w.D(2900).D(390);                                   // base hp, base mp
	w.H(4000).H(21592);                                 // base dp, display_max_point
	w.D(60000);                                         // base fly time
	w.H(131).H(132).H(133).H(134);                      // base attacks
	w.D(110).D(111);                                    // base pdef, mdef
	w.H(135);                                           // base magic resist
	w.F(1.4f);                                          // base attack range
	w.H(0);
	w.H(136).H(137).H(138);                             // base evasion, parry, block
	w.H(139).H(140).H(141);                             // base crit rates
	w.H(0);
	w.H(142).H(143);                                    // base accuracies
	w.H(0);
	w.H(144);                                           // base magic accuracy
	w.H(145).H(146).H(147).H(148);                      // base concentration, boost, suppression, heal boost
	w.H(0);
	w.H(149).H(150);                                    // base strike/spell resist
	w.H(151).H(152);                                    // base fortitudes
	return w.data;
}

TEST(PacketDecodersTest, StatsInfo) {
	const std::vector<uint8_t> body = buildStatsInfo();
	ASSERT_EQ(body.size(), 344u);

	const StatsInfo stats = decodeStatsInfo(body);
	EXPECT_EQ(stats.objectId, 0x0BADF00D);
	EXPECT_EQ(stats.gameTime, 500000);
	EXPECT_EQ(stats.power, 100);
	EXPECT_EQ(stats.will, 105);
	EXPECT_EQ(stats.darkResistance, 15);
	EXPECT_EQ(stats.level, 1);
	EXPECT_EQ(stats.expNeed, 2500);
	EXPECT_EQ(stats.expShown, 120);
	EXPECT_EQ(stats.maxHp, 3000);
	EXPECT_EQ(stats.currentHp, 1500);
	EXPECT_EQ(stats.maxMp, 400);
	EXPECT_EQ(stats.currentMp, 399);
	EXPECT_EQ(stats.maxFlyTime, 60000);
	EXPECT_EQ(stats.currentFp, 59000);
	EXPECT_FLOAT_EQ(stats.attackRange, 1.5f);
	EXPECT_EQ(stats.attackSpeed, 1500);
	EXPECT_FLOAT_EQ(stats.castingSpeed, 1.0f);
	EXPECT_EQ(stats.inventoryLimit, 27);
	EXPECT_EQ(stats.inventorySize, 3);
	EXPECT_EQ(stats.classId, 0);
	EXPECT_EQ(stats.currentReposeEnergy, 1000);
	EXPECT_EQ(stats.maxReposeEnergy, 6000);
	EXPECT_EQ(stats.basePower, 200);
	EXPECT_EQ(stats.baseWill, 205);
	EXPECT_EQ(stats.baseMaxHp, 2900);   // V9: the oracle's PlayerStatCalculator value
	EXPECT_EQ(stats.baseMaxMp, 390);
	EXPECT_EQ(stats.baseMagicalCriticalDamageReduce, 152);
	EXPECT_GE(stats.maxHp, stats.baseMaxHp);
}

TEST(PacketDecodersTest, StatsInfoRejectsAShiftedBody) {
	std::vector<uint8_t> body = buildStatsInfo();
	body.insert(body.begin() + 34, uint8_t{0}); // one extra byte after the level shifts every later field
	EXPECT_THROW(decodeStatsInfo(body), DecodeError);

	body = buildStatsInfo();
	ASSERT_EQ(body[228], 1); // the third 4.3 NA short is the constant 1
	body[228] = 2;
	EXPECT_THROW(decodeStatsInfo(body), DecodeError);
}

/** GeneralInfoBlobEntry (0x00) */
void generalInfoBlob(PacketWriter& w, int64_t count, std::string_view creator) {
	w.C(0x00);
	w.H(0).Q(count).S(creator).C(0).D(0).D(0).D(0).H(0).D(0).H(18);
}

/** EnchantInfoBlobEntry (0x0B), 138 bytes plus the entry id */
void enchantInfoBlob(PacketWriter& w, int32_t skinTemplateId, uint8_t enchantLevel) {
	w.C(0x0B);
	w.C(0).C(enchantLevel).D(skinTemplateId).C(0).C(0);
	for (int i = 0; i < 6; i++) // Item.MAX_BASIC_STONES
		w.D(0);
	w.D(0);              // god stone
	w.zeros(4);          // dye info
	w.C(0).D(0).D(0);    // unknown, 1.5.1.9, dye expiration
	w.D(0).C(0);         // idian stone and polish number
	w.C(0);              // tempering
	w.zeros(18);
	w.zeros(16);         // the plume stat pair block
	w.zeros(16).zeros(16);
	w.D(0);              // 4.7.5
	w.C(0);              // amplified
	w.D(0).D(0).D(0);    // buff skill and two unused skill ids
}

TEST(PacketDecodersTest, InventoryInfoWithAStackableAndAnEquippedItem) {
	PacketWriter blobStack;
	generalInfoBlob(blobStack, 12, "");
	PacketWriter blobWeapon;
	blobWeapon.C(0x06).Q(1);        // EQUIPPED_SLOT: MAIN_HAND
	blobWeapon.C(0x01).Q(1).Q(2);   // SLOTS_WEAPON
	enchantInfoBlob(blobWeapon, 100000001, 0);
	blobWeapon.C(0x10).C(-1).C(0).C(0); // PREMIUM_OPTION of an unidentified item
	generalInfoBlob(blobWeapon, 1, "Skalix");

	PacketWriter w;
	w.C(1).C(0).C(0).C(0).H(2);
	w.D(0x1000).D(182005001).S("Aether Crystal");
	w.H(static_cast<int32_t>(blobStack.data.size())).B(blobStack.data);
	w.H(0).C(0);
	w.D(0x1001).D(100000001).S("Training Sword");
	w.H(static_cast<int32_t>(blobWeapon.data.size())).B(blobWeapon.data);
	w.H(1).C(1);

	const InventoryInfo info = decodeInventoryInfo(w.data);
	EXPECT_TRUE(info.firstPacket);
	EXPECT_EQ(info.npcExpands, 0);
	ASSERT_EQ(info.items.size(), 2u);

	const InventoryItem& stack = info.items[0];
	EXPECT_EQ(stack.objectId, 0x1000);
	EXPECT_EQ(stack.templateId, 182005001);
	EXPECT_EQ(stack.l10n, "Aether Crystal");
	EXPECT_EQ(stack.blobEntryIds, (std::vector<uint8_t>{0x00}));
	ASSERT_TRUE(stack.general.has_value());
	EXPECT_EQ(stack.general->count, 12);
	EXPECT_EQ(stack.general->creator, "");
	EXPECT_FALSE(stack.equippedSlotBlob.has_value());
	EXPECT_EQ(stack.equipmentSlot, 0);
	EXPECT_FALSE(stack.cloth);

	const InventoryItem& weapon = info.items[1];
	EXPECT_EQ(weapon.templateId, 100000001);
	EXPECT_EQ(weapon.blobEntryIds, (std::vector<uint8_t>{0x06, 0x01, 0x0B, 0x10, 0x00}));
	ASSERT_TRUE(weapon.equippedSlotBlob.has_value());
	EXPECT_EQ(*weapon.equippedSlotBlob, 1);
	ASSERT_TRUE(weapon.enchant.has_value());
	EXPECT_EQ(weapon.enchant->skinTemplateId, 100000001);
	ASSERT_TRUE(weapon.general.has_value());
	EXPECT_EQ(weapon.general->count, 1);
	EXPECT_EQ(weapon.general->creator, "Skalix");
	EXPECT_EQ(weapon.equipmentSlot, 1);
	EXPECT_TRUE(weapon.cloth);
}

TEST(PacketDecodersTest, InventoryInfoRejectsABlobSizeThatDoesNotMatchItsEntries) {
	PacketWriter blob;
	generalInfoBlob(blob, 1, "");
	PacketWriter w;
	w.C(1).C(0).C(0).C(0).H(1);
	w.D(1).D(2).S("x");
	w.H(static_cast<int32_t>(blob.data.size()) + 1).B(blob.data); // one byte more than the entries hold
	w.H(0).C(0);
	EXPECT_THROW(decodeInventoryInfo(w.data), DecodeError);

	PacketWriter unknown;
	unknown.C(1).C(0).C(0).C(0).H(1);
	unknown.D(1).D(2).S("x");
	unknown.H(5).C(0x77).D(0); // an entry id ItemInfoBlob does not define
	unknown.H(0).C(0);
	EXPECT_THROW(decodeInventoryInfo(unknown.data), DecodeError);
}

/** SM_PLAYER_INFO of a level 1 character without a legion, a store, a flight path or a target */
std::vector<uint8_t> buildPlayerInfo(const Appearance& appearance, const EquippedItems& equipment) {
	PacketWriter w;
	w.F(1212.94f).F(1044.85f).F(140.76f);
	w.D(0x0BADF00D).D(0xA0).D(0).D(0xA0);
	w.C(0x00);
	w.D(0);       // transform type
	w.C(0x26);    // not an enemy
	w.C(0).C(0).C(0); // race, class, gender
	w.H(0);       // state
	w.D(0).D(0);  // the int before the someState flag, and the flag itself (always 0)
	w.C(77);      // heading
	w.S("Skalix");
	w.H(0).H(0).H(0); // title, mentor flag, casting skill
	w.zeros(12);      // no legion
	w.C(100);         // hp percentage
	w.H(0);           // dp
	w.C(0x00);
	w.D(equipment.mask);
	for (const EquippedItem& item : equipment.items) {
		w.D(item.skinTemplateId).D(item.godStoneId);
		dyeInfo(w, item.color);
		w.H(item.enchantParam).H(0);
	}
	spawnAppearance(w, appearance);
	w.F(0.25f).F(2.0f).F(6.0f); // scale, gravity, movement speed
	w.H(1500).H(1500);          // attack speed base and current
	w.C(0);                     // port animation
	w.S("");                    // no store message
	w.F(0).F(0).F(0);           // movement vector
	w.F(1212.94f).F(1044.85f).F(140.76f);
	w.C(0);                     // movement mask
	w.C(0);                     // visual state
	w.S("");                    // note
	w.H(1);                     // level
	w.H(0).H(0);                // display, deny
	w.H(0).H(0);                // abyss rank, unknown
	w.D(0);                     // target
	w.C(0);                     // suspect id
	w.D(0);                     // team id
	w.C(0);                     // mentor
	w.D(0);                     // house address
	w.D(1);                     // membership
	w.D(1);                     // 4.7
	w.C(3);
	w.C(0).C(0).C(0);           // conqueror rank, protector rank, officer rank icon
	return w.data;
}

TEST(PacketDecodersTest, PlayerInfoOfANewCharacter) {
	EquippedItems equipment;
	equipment.mask = 0x1 | 0x8 | 0x1000; // MAIN_HAND, TORSO and PANTS: three bits, three entries
	equipment.items = {EquippedItem{100000001, 0, DyeInfo{}, 0}, EquippedItem{110000001, 0, DyeInfo{}, 0},
										 EquippedItem{113000001, 0, DyeInfo{1, 1, 2, 3}, 5}};
	const Appearance appearance = sampleAppearance();
	const std::vector<uint8_t> body = buildPlayerInfo(appearance, equipment);

	const PlayerInfo info = decodePlayerInfo(body);
	EXPECT_FLOAT_EQ(info.x, 1212.94f);
	EXPECT_EQ(info.objectId, 0x0BADF00D);
	EXPECT_EQ(info.templateId, 0xA0);
	EXPECT_EQ(info.enemyFlag, 0x26);
	EXPECT_EQ(info.heading, 77);
	EXPECT_EQ(info.name, "Skalix");
	EXPECT_FALSE(info.legionMember);
	EXPECT_EQ(info.hpPercentage, 100);
	EXPECT_EQ(info.equipment.mask, equipment.mask);
	EXPECT_EQ(info.equipment.items, equipment.items);
	EXPECT_EQ(info.appearance, appearance);
	EXPECT_FLOAT_EQ(info.scale, 0.25f);
	EXPECT_FLOAT_EQ(info.gravity, 2.0f);
	EXPECT_FLOAT_EQ(info.movementSpeed, 6.0f);
	EXPECT_EQ(info.attackSpeedBase, 1500);
	EXPECT_EQ(info.storeMessage, "");
	EXPECT_FLOAT_EQ(info.moveZ, 140.76f);
	EXPECT_EQ(info.level, 1);
	EXPECT_EQ(info.membership, 1);
	EXPECT_EQ(info.targetObjectId, 0);
}

TEST(PacketDecodersTest, PlayerInfoEquippedItemCountComesFromTheSlotMask) {
	EquippedItems oneItem;
	oneItem.mask = 0x1; // a two-handed weapon keeps only its MAIN_HAND bit
	oneItem.items = {EquippedItem{100100001, 7, DyeInfo{}, 10}};
	const PlayerInfo info = decodePlayerInfo(buildPlayerInfo(sampleAppearance(), oneItem));
	ASSERT_EQ(info.equipment.items.size(), 1u);
	EXPECT_EQ(info.equipment.items[0].skinTemplateId, 100100001);
	EXPECT_EQ(info.equipment.items[0].godStoneId, 7);
	EXPECT_EQ(info.equipment.items[0].enchantParam, 10);

	// a mask with more bits than entries runs into the rest of the packet
	std::vector<uint8_t> body = buildPlayerInfo(sampleAppearance(), oneItem);
	const size_t maskOffset = 12 + 16 + 1 + 4 + 1 + 3 + 2 + 8 + 1 + (2 * 7) + 6 + 12 + 1 + 2 + 1;
	body[maskOffset] = 0x3; // two bits instead of one
	EXPECT_THROW(decodePlayerInfo(body), DecodeError);
}

TEST(PacketDecodersTest, PlayerInfoLegionAndFlightBranches) {
	EquippedItems none;
	std::vector<uint8_t> body = buildPlayerInfo(sampleAppearance(), none);
	// the 12 zero bytes of "no legion" start after the casting skill id
	const size_t legionOffset = 12 + 16 + 1 + 4 + 1 + 3 + 2 + 8 + 1 + (2 * 7) + 6;
	ASSERT_EQ(body[legionOffset], 0);
	EXPECT_FALSE(decodePlayerInfo(body).legionMember);

	// the same body with a legion id: the emblem bytes and the legion name replace the filler
	std::vector<uint8_t> legion(body.begin(), body.begin() + static_cast<ptrdiff_t>(legionOffset));
	PacketWriter block;
	block.D(1234).C(1).C(2).C(3).C(4).C(5).C(6).S("Daeva Inc");
	legion.insert(legion.end(), block.data.begin(), block.data.end());
	legion.insert(legion.end(), body.begin() + static_cast<ptrdiff_t>(legionOffset + 12), body.end());
	const PlayerInfo member = decodePlayerInfo(legion);
	EXPECT_TRUE(member.legionMember);
	EXPECT_EQ(member.legionId, 1234);
	EXPECT_EQ(member.legionName, "Daeva Inc");

	// a flight path adds two ints after the movement mask, which only the option tells the decoder
	std::vector<uint8_t> withPath;
	const size_t tail = 1 /* visual state */ + 2 /* empty note */ + 2 * 5 + 4 + 1 + 4 + 1 + 4 + 4 + 4 + 1 + 3;
	withPath.assign(body.begin(), body.end() - static_cast<ptrdiff_t>(tail));
	PacketWriter path;
	path.D(101).D(5000);
	withPath.insert(withPath.end(), path.data.begin(), path.data.end());
	withPath.insert(withPath.end(), body.end() - static_cast<ptrdiff_t>(tail), body.end());
	EXPECT_THROW(decodePlayerInfo(withPath), DecodeError);
	const PlayerInfo flying = decodePlayerInfo(withPath, PlayerInfoOptions{true});
	EXPECT_EQ(flying.flightPathId, 101);
	EXPECT_EQ(flying.flightPathDistance, 5000);
}

TEST(PacketDecodersTest, PlayerInfoRejectsAChangedAppearanceConstant) {
	EquippedItems none;
	std::vector<uint8_t> body = buildPlayerInfo(sampleAppearance(), none);
	// the appearance constant 5 sits after the six face bytes of the appearance block
	const size_t appearanceStart = 12 + 16 + 1 + 4 + 1 + 3 + 2 + 8 + 1 + (2 * 7) + 6 + 12 + 1 + 2 + 1 + 4;
	const size_t constantOffset = appearanceStart + 16 + 6;
	ASSERT_EQ(body[constantOffset], 5);
	body[constantOffset] = 6;
	EXPECT_THROW(decodePlayerInfo(body), DecodeError);
}

TEST(PacketDecodersTest, PrefixDecoders) {
	PacketWriter state;
	state.D(0x0BADF00D).C(0).C(0).C(0);
	EXPECT_EQ(decodePlayerStateObjectId(state.data), 0x0BADF00D);
	state.C(0);
	EXPECT_THROW(decodePlayerStateObjectId(state.data), DecodeError);

	PacketWriter deleted;
	deleted.D(778899).C(0);
	EXPECT_EQ(decodeDeleteObjectId(deleted.data), 778899);

	PacketWriter npc;
	npc.F(1.0f).F(2.0f).F(3.0f).D(0x2000).D(0).D(0);
	EXPECT_EQ(decodeNpcInfoObjectId(npc.data), 0x2000);

	PacketWriter message;
	message.C(0).C(0).D(0).D(1300642).C(1).S("120").C(0);
	EXPECT_EQ(decodeSystemMessageId(message.data), 1300642);

	PacketWriter tooShort;
	tooShort.C(0).C(0);
	EXPECT_THROW(decodeSystemMessageId(tooShort.data), DecodeError);
}

TEST(PacketDecodersTest, BodyReaderReportsThePacketAndTheOffset) {
	const std::vector<uint8_t> body = {1, 2, 3};
	BodyReader reader(body, "SM_EXAMPLE");
	EXPECT_EQ(reader.C(), 1);
	try {
		reader.D();
		FAIL() << "expected a DecodeError";
	} catch (const DecodeError& e) {
		const std::string message = e.what();
		EXPECT_NE(message.find("SM_EXAMPLE"), std::string::npos) << message;
		EXPECT_NE(message.find("offset 1"), std::string::npos) << message;
	}
}

} // namespace
} // namespace aion::gameserver::scenario::decoders
