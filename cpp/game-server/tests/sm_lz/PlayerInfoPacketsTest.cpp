// SM_PLAYER_INFO, AbstractPlayerInfoPacket.writePlayerInfo (the character list entry of SM_CHARACTER_LIST/SM_CREATE_CHARACTER), writeEquippedItems
// and SM_UPDATE_PLAYER_APPEARANCE: golden bytes written by hand from the Java writeImpls for real Players on stat container doubles. The
// branches: the race sent for enemies (ENEMY_OF_ALL_PLAYERS: the viewer's opposite race) and for NEUTRAL_TO_ALL_PLAYERS (the viewer's race), the
// enemy flag byte, the ABSOLUTE movement mask (a vector from the target point, the flag cleared), the membership value, the Conqueror/Protector
// ranks, the character ban info (stored, expired, faction switch cooldown), the visible items (16 slots), and the equipment mask with the sub hand
// bit removed for two-handed weapons.
//
// Test doubles: the service calls and the P5-01 stat reads go through detail::PacketLookups (LookupsGuard); Legion's constructor is unported
// (P5-10), so no legion member is tested (the 12 zero bytes of a player without legion are).

#include "SmLzTestSupport.h"

#include <chrono>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/controllers/movement/MovementMask.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/account/CharacterBanInfo.h"
#include "aion/gameserver/model/animations/ArrivalAnimation.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/templates/cp/CPType.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/AbstractPlayerInfoPacket.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UPDATE_PLAYER_APPEARANCE.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/CPInfo.h"

namespace aion::gameserver::network::aion::serverpackets::testing {
namespace {

using model::gameobjects::player::CustomPlayerState;
using runtime::Ref;

/** Distinct values for every appearance field, set in the setter order of PlayerAppearance */
void setAppearance(model::gameobjects::player::PlayerAppearance& a) {
	a.setFace(1);
	a.setHair(2);
	a.setDeco(3);
	a.setTattoo(4);
	a.setFaceContour(5);
	a.setExpression(6);
	a.setJawLine(7);
	a.setSkinRGB(0x112233);
	a.setHairRGB(0x445566);
	a.setEyeRGB(0x778899);
	a.setLipRGB(0xAABBCC);
	a.setFaceShape(8);
	a.setForehead(9);
	a.setEyeHeight(10);
	a.setEyeSpace(11);
	a.setEyeWidth(12);
	a.setEyeSize(13);
	a.setEyeShape(14);
	a.setEyeAngle(15);
	a.setBrowHeight(16);
	a.setBrowAngle(17);
	a.setBrowShape(18);
	a.setNose(19);
	a.setNoseBridge(20);
	a.setNoseWidth(21);
	a.setNoseTip(22);
	a.setCheek(23);
	a.setLipHeight(24);
	a.setMouthSize(25);
	a.setLipSize(26);
	a.setSmile(27);
	a.setLipShape(28);
	a.setJawHeigh(29);
	a.setChinJut(30);
	a.setEarShape(31);
	a.setHeadSize(32);
	a.setNeck(33);
	a.setNeckLength(34);
	a.setShoulders(35);
	a.setShoulderSize(36);
	a.setTorso(37);
	a.setChest(38);
	a.setWaist(39);
	a.setHips(40);
	a.setArmThickness(41);
	a.setArmLength(42);
	a.setHandSize(43);
	a.setLegThickness(44);
	a.setLegLength(45);
	a.setFootSize(46);
	a.setFacialRate(47);
	a.setVoice(48);
	a.setHeight(1.25f);
}

/** The appearance bytes from `face` to `headSize` (writeC(getFace()) .. writeC(getHeadSize()), with the constant 5 after getExpression()) */
Bytes& faceToHeadSize(Bytes& b) {
	b.C(1).C(2).C(3).C(4).C(5).C(6).C(5).C(7).C(9).C(10).C(11).C(12).C(13).C(14).C(15).C(16).C(17).C(18).C(19).C(20).C(21).C(22).C(23);
	return b.C(24).C(25).C(26).C(27).C(28).C(29).C(30).C(31).C(32);
}

/** neck .. facialRate, 0, armLength, legLength, shoulders, faceShape (writePlayerInfo and SM_PLAYER_INFO) */
Bytes& neckToFaceShape(Bytes& b) {
	return b.C(33).C(34).C(36).C(37).C(38).C(39).C(40).C(41).C(43).C(44).C(46).C(47).C(0).C(42).C(45).C(35).C(8);
}

detail::AttackSpeedValues attackSpeed(model::gameobjects::Creature&) {
	return {1500, 1200};
}
int32_t hpPercentage(model::gameobjects::Creature&) {
	return 85;
}
float movementSpeed(model::gameobjects::Creature&) {
	return 5.0f;
}
runtime::Ptr<model::house::House> noHouse(model::gameobjects::player::Player&) {
	return nullptr;
}
runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo> noCpInfo(model::gameobjects::player::Player&) {
	return nullptr;
}

/** The CP info a captureless lookup returns (reset by the test that sets it) */
Ref<services::conquerorAndProtectorSystem::CPInfo> conqueror;

/**
 * A second character of the account (Java: the account data PlayerDAO/AccountService build for the character list): an Elyos female gladiator
 * named "Shown" with the appearance of setAppearance and title 12, the given ban info and visible items
 */
model::account::PlayerAccountData& addCharacter(model::account::Account& account, int32_t objectId, runtime::Ptr<model::account::CharacterBanInfo> cbi,
	std::vector<Ref<model::account::PlayerAccountData::VisibleItem>> visibleItems) {
	Ref<model::gameobjects::player::PlayerCommonData> commonData = model::gameobjects::player::PlayerCommonData::create(objectId);
	commonData->setName("Shown");
	commonData->setRace(model::Race::ELYOS);
	commonData->setPlayerClass(model::PlayerClass::GLADIATOR);
	commonData->setGender(model::Gender::FEMALE);
	commonData->setTitleId(12);
	Ref<model::gameobjects::player::PlayerAppearance> appearance = model::gameobjects::player::PlayerAppearance::create();
	setAppearance(*appearance);
	account.addPlayerAccountData(std::make_unique<model::account::PlayerAccountData>(account, *commonData, *appearance, cbi, std::move(visibleItems)));
	return *account.getPlayerAccountData(objectId);
}

class PlayerInfoPacketsTest : public PacketTest {
protected:
	void SetUp() override {
		PacketTest::SetUp();
		lookups.attackSpeed = &attackSpeed;
		lookups.hpPercentage = &hpPercentage;
		lookups.movementSpeedFloat = &movementSpeed;
		lookups.activeHouseOfPlayer = &noHouse;
		lookups.cpInfoForCurrentMap = &noCpInfo;
		detail::setPacketLookupsForTests(&lookups);
	}

	/** A player at (3, 4, 0) with heading 7, distinct appearance values, settings, abyss rank 1 and title 12 */
	PlayerFixture shownPlayer(int32_t objectId, model::Race race) {
		PlayerFixture f = makePlayer(objectId, objectId + 1000, "Shown", race);
		setAppearance(*f.appearance);
		f.commonData->setPlayerClass(model::PlayerClass::GLADIATOR);
		f.commonData->setGender(model::Gender::FEMALE);
		f.commonData->setTitleId(12);
		f.commonData->setDp(300); // not a starting class, not online: stored without a packet
		f.commonData->setNote("note");
		f.player->setPosition(world::WorldPosition::create(210010000, 3.0f, 4.0f, 0.0f, int8_t{7}));
		Ref<model::gameobjects::player::PlayerSettings> settings = model::gameobjects::player::PlayerSettings::create();
		settings->setDisplay(4);
		settings->setDeny(2);
		f.player->setPlayerSettings(settings);
		f.player->setAbyssRank(model::gameobjects::player::AbyssRank::create(0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0));
		f.player->setPortAnimation(model::animations::ArrivalAnimation::LANDING);
		return f;
	}

	detail::PacketLookupsForTests lookups;
};

/**
 * The expected SM_PLAYER_INFO body of shownPlayer (female gladiator, template 100000 + raceId * 2 + 1) for the given race byte, enemy flag,
 * movement vector and mask, membership value and CP ranks
 */
Bytes expectedPlayerInfo(int32_t objectId, int32_t templateId, int32_t raceByte, bool enemy, float vx, float vy, float vz, int32_t movementMask,
	int32_t membershipValue, int32_t conquerorRank, int32_t protectorRank) {
	Bytes b;
	b.header(32).F(3.0f).F(4.0f).F(0.0f).D(objectId).D(templateId).D(0 /* robot */).D(templateId /* transform model: the template */).C(0);
	b.D(0 /* TransformType.NONE */).C(enemy ? 0 : 0x26).C(raceByte).C(1 /* GLADIATOR */).C(1 /* FEMALE */).H(1 /* CreatureState.ACTIVE */);
	b.D(0).D(0 /* someState */).C(7).S("Shown").H(12).H(0 /* no mentor flag */).H(0 /* not casting */);
	b.zeros(12); // no legion
	b.C(85).H(300).C(0);
	b.D(0); // writeEquippedItems: no item, mask 0
	b.D(0x112233).D(0x445566).D(0x778899).D(0xAABBCC);
	faceToHeadSize(b);
	neckToFaceShape(b);
	b.C(0).C(48 /* voice */).F(1.25f).F(0.25f).F(2.0f).F(5.0f).H(1500).H(1200);
	b.C(2 /* ArrivalAnimation.LANDING */).S("" /* no store */);
	b.F(vx).F(vy).F(vz).F(3.0f).F(4.0f).F(0.0f).C(movementMask);
	b.C(0 /* visual state */).S("note").H(0 /* level */).H(4 /* display */).H(2 /* deny */).H(1 /* abyss rank */).H(0);
	b.D(0 /* no target */).C(0).D(0 /* no team */).C(0 /* not mentor */).D(0 /* no house */);
	b.D(membershipValue).D(1).C(3).C(conquerorRank).C(protectorRank).C(0);
	return b;
}

TEST_F(PlayerInfoPacketsTest, PlayerInfoOfTheViewerHimself) {
	PACKET_TEST_SCOPE;
	PlayerFixture shown = shownPlayer(100001, model::Race::ASMODIANS);
	TestConnection connection;
	connection.get()->setActivePlayer(runtime::Ptr<model::gameobjects::player::Player>(*shown.player));
	SM_PLAYER_INFO packet(*shown.player);
	EXPECT_EQ(packet.recipients(), AionServerPacket::Recipients::PER_RECIPIENT);
	// not an enemy of himself: raceId = player.getRace().getRaceId() = 1; template 100000 + 1 * 2 + 1
	EXPECT_EQ(serialized(packet, connection.get()), expectedPlayerInfo(100001, 100003, 1, false, 0, 0, 0, 0, 1, 0, 0).data);
}

TEST_F(PlayerInfoPacketsTest, PlayerInfoForEnemiesAndNeutralPlayers) {
	PACKET_TEST_SCOPE;
	PlayerFixture viewer = makePlayer(100010, 9010, "Viewer", model::Race::ELYOS);
	PlayerFixture shown = shownPlayer(100011, model::Race::ELYOS);
	TestConnection connection;
	connection.get()->setActivePlayer(runtime::Ptr<model::gameobjects::player::Player>(*viewer.player));

	// ENEMY_OF_ALL_PLAYERS: isEnemy is true, the race sent is the viewer's opposite race (ASMODIANS, 1) although the player is an Elyos
	shown.player->setCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS);
	SM_PLAYER_INFO enemyPacket(*shown.player, true);
	EXPECT_EQ(serialized(enemyPacket, connection.get()), expectedPlayerInfo(100011, 100001, 1, true, 0, 0, 0, 0, 1, 0, 0).data);

	// NEUTRAL_TO_ALL_PLAYERS on either side: the viewer's own race; the enemy flag of the constructor stays independent of it
	shown.player->setCustomState(CustomPlayerState::NEUTRAL_TO_ALL_PLAYERS);
	viewer.commonData->setRace(model::Race::ASMODIANS);
	SM_PLAYER_INFO neutralPacket(*shown.player);
	// isEnemy (ENEMY_OF_ALL_PLAYERS) gives the viewer's opposite race ELYOS, then NEUTRAL_TO_ALL_PLAYERS replaces it with the viewer's race 1
	EXPECT_EQ(serialized(neutralPacket, connection.get()), expectedPlayerInfo(100011, 100001, 1, false, 0, 0, 0, 0, 1, 0, 0).data);
}

TEST_F(PlayerInfoPacketsTest, PlayerInfoWithAbsoluteMovementMembershipAndConquerorRank) {
	PACKET_TEST_SCOPE;
	PlayerFixture shown = shownPlayer(100020, model::Race::ELYOS);
	shown.account->setMembership(int8_t{2});
	TestConnection connection;
	connection.get()->setActivePlayer(runtime::Ptr<model::gameobjects::player::Player>(*shown.player));
	// ABSOLUTE: the vector from (3, 4, 0) to the target point (0, 0, 0), normalized (Vector3f.normalizeLocal: x * (1f / FastMath.sqrt(25))) and
	// multiplied by the movement speed 5; the mask is sent without the ABSOLUTE bit
	shown.player->getMoveController()->movementMask.set(static_cast<int8_t>(controllers::movement::MovementMask::ABSOLUTE | 0x04));
	const float inverse = 1.0f / static_cast<float>(std::sqrt(25.0));
	float vx = -3.0f * inverse;
	float vy = -4.0f * inverse;
	vx *= 5.0f;
	vy *= 5.0f;
	float vz = 0.0f * inverse * 5.0f;
	conqueror = services::conquerorAndProtectorSystem::CPInfo::create(model::templates::cp::CPType::CONQUEROR, *shown.player);
	conqueror->setRank(3);
	lookups.cpInfoForCurrentMap = [](model::gameobjects::player::Player&) { return runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo>(conqueror); };
	SM_PLAYER_INFO packet(*shown.player);
	// membership 2: writeD(0x03 + 2); CONQUEROR rank 3, protector rank 0
	EXPECT_EQ(serialized(packet, connection.get()), expectedPlayerInfo(100020, 100001, 0, false, vx, vy, vz, 0x04, 5, 3, 0).data);
	conqueror.reset();
}

TEST_F(PlayerInfoPacketsTest, PlayerInfoWithoutActivePlayerWritesNothing) {
	PACKET_TEST_SCOPE;
	PlayerFixture shown = shownPlayer(100030, model::Race::ELYOS);
	TestConnection connection;
	SM_PLAYER_INFO packet(*shown.player);
	EXPECT_EQ(serialized(packet, connection.get()), Bytes().header(32).data) << "Java returns before the first write";
	EXPECT_THROW(static_cast<void>(packet.serialize(nullptr)), runtime::NullPointerException);
}

/** A subclass exposing the protected writers of AbstractPlayerInfoPacket (SM_CHARACTER_LIST's opcode 64 is irrelevant for the bytes) */
class TestPlayerInfoPacket final : public AbstractPlayerInfoPacket {
public:
	TestPlayerInfoPacket(model::account::PlayerAccountData* accountData, std::vector<runtime::Ptr<model::gameobjects::Item>> items)
		: AbstractPlayerInfoPacket(opcodeOf<SM_PLAYER_INFO>), accountData(accountData), items(std::move(items)) {}
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }

protected:
	void writeImpl(AionConnection* con) override {
		if (accountData != nullptr)
			writePlayerInfo(*accountData, con);
		else
			writeEquippedItems(items);
	}

private:
	model::account::PlayerAccountData* accountData;
	std::vector<runtime::Ptr<model::gameobjects::Item>> items;
};

runtime::Ptr<model::team::legion::LegionMember> noLegionMember(model::gameobjects::player::PlayerCommonData&) {
	return nullptr;
}
int32_t displaySettings(int32_t playerId) {
	return playerId == 100040 ? 5 : 0;
}
bool unreadMail(int32_t) {
	return true;
}
int64_t earnedKinah(model::gameobjects::player::PlayerCommonData&) {
	return 123456789012LL;
}
std::optional<int32_t> factionSwitchCooldown(model::Race, AionConnection*) {
	return 30;
}

/** writePlayerInfo from the player id to the visible items, for the account data of shownPlayer (common data at its default position) */
Bytes& playerInfoHead(Bytes& b, int32_t playerId, int32_t templateId) {
	b.D(playerId).S("Shown", 25).D(1 /* FEMALE */).D(0 /* ELYOS */).D(1 /* GLADIATOR */).D(48 /* voice */);
	b.D(0x112233).D(0x445566).D(0x778899).D(0xAABBCC);
	faceToHeadSize(b);
	neckToFaceShape(b);
	b.C(0).C(0).C(0).F(1.25f).D(templateId).D(0 /* map */).F(0.0f).F(0.0f).F(0.0f).D(0 /* heading */).H(0 /* level */).H(0).D(12 /* title */);
	return b.D(0).S("", 40).H(0).D(0 /* never online */);
}

TEST_F(PlayerInfoPacketsTest, CharacterListEntryWithVisibleItemsBanInfoAndBrokerKinah) {
	PACKET_TEST_SCOPE;
	Ref<model::account::Account> account = model::account::Account::create(9040);
	lookups.legionMemberOfCommonData = &noLegionMember;
	lookups.displaySettings = &displaySettings;
	lookups.unreadMail = &unreadMail;
	lookups.earnedKinahFromSoldItems = &earnedKinah;
	// two visible items: the second without a color; slots 3..15 are written as zeros
	using VisibleItem = model::account::PlayerAccountData::VisibleItem;
	std::vector<Ref<VisibleItem>> visibleItems{VisibleItem::create(int8_t{1}, 100000001, 168000001, 0x0A0B0C),
		VisibleItem::create(int8_t{2}, 100000002, 0, std::nullopt)};
	int64_t nowSeconds = commons::utils::currentTimeMillis() / 1000;
	// a ban info that ends in an hour is sent; the reason is a plain string
	Ref<model::account::CharacterBanInfo> ban = model::account::CharacterBanInfo::create(nowSeconds - 10, 3600, "abuse");
	model::account::PlayerAccountData& banned = addCharacter(*account, 100040, ban, visibleItems);
	TestConnection connection;
	TestPlayerInfoPacket packet(&banned, {});
	Bytes expected;
	expected.header(32);
	playerInfoHead(expected, 100040, 100001);
	expected.C(1).D(100000001).D(168000001).dye(0x0A0B0C);
	expected.C(2).D(100000002).D(0).dye(std::nullopt);
	for (int i = 2; i < 16; i++)
		expected.C(0).D(0).D(0).dye(std::nullopt);
	expected.D(0).D(0).D(0).D(0).D(0).D(0).zeros(68).D(banned.getDeletionTimeInSeconds()).H(5).H(0).D(0).D(1).D(0).D(0).Q(123456789012LL);
	expected.D(0).D(0).D(0).D(0).D(0).D(static_cast<int32_t>(nowSeconds - 10)).D(static_cast<int32_t>(nowSeconds - 10 + 3600)).S("abuse");
	EXPECT_EQ(serialized(packet, connection.get()), expected.data);
}

TEST_F(PlayerInfoPacketsTest, CharacterListEntryBanInfoExpiryAndFactionSwitchCooldown) {
	PACKET_TEST_SCOPE;
	Ref<model::account::Account> account = model::account::Account::create(9041);
	lookups.legionMemberOfCommonData = &noLegionMember;
	lookups.displaySettings = &displaySettings;
	lookups.unreadMail = [](int32_t) { return false; };
	lookups.earnedKinahFromSoldItems = [](model::gameobjects::player::PlayerCommonData&) { return int64_t{0}; };
	lookups.factionSwitchCooldownTime = &factionSwitchCooldown;
	int64_t nowSeconds = commons::utils::currentTimeMillis() / 1000;
	Ref<model::account::CharacterBanInfo> oldBan = model::account::CharacterBanInfo::create(nowSeconds - 100, 50, "old");
	model::account::PlayerAccountData& expired = addCharacter(*account, 100041, oldBan, {});
	TestConnection connection;

	Bytes tail;
	for (int i = 0; i < 16; i++)
		tail.C(0).D(0).D(0).dye(std::nullopt);
	tail.D(0).D(0).D(0).D(0).D(0).D(0).zeros(68).D(expired.getDeletionTimeInSeconds()).H(0).H(0).D(0).D(0).D(0).D(0).Q(0).D(0).D(0).D(0).D(0).D(0);

	// an expired ban info is not sent (restriction mode NONE: no cooldown check)
	TestPlayerInfoPacket expiredPacket(&expired, {});
	Bytes withoutBan;
	withoutBan.header(32);
	playerInfoHead(withoutBan, 100041, 100001).raw(tail.data).D(0).D(0).S("");
	EXPECT_EQ(serialized(expiredPacket, connection.get()), withoutBan.data);

	// SAME_FACTION with a cooldown: a new ban info of 61 seconds with Java's text (" " is EE 80 A6 in UTF-8)
	using configs::main::SecurityConfig;
	SecurityConfig::MULTI_CLIENTING_RESTRICTION_MODE = SecurityConfig::MultiClientingRestrictionMode::SAME_FACTION;
	SecurityConfig::MULTI_CLIENTING_FACTION_SWITCH_COOLDOWN_MINUTES = 15;
	TestPlayerInfoPacket cooldownPacket(&expired, {});
	std::vector<uint8_t> bytes = serialized(cooldownPacket, connection.get());
	SecurityConfig::MULTI_CLIENTING_RESTRICTION_MODE = SecurityConfig::MultiClientingRestrictionMode::NONE;
	SecurityConfig::MULTI_CLIENTING_FACTION_SWITCH_COOLDOWN_MINUTES = 0;
	Bytes head;
	head.header(32);
	playerInfoHead(head, 100041, 100001).raw(tail.data);
	ASSERT_GT(bytes.size(), head.data.size() + 8);
	EXPECT_EQ(std::vector<uint8_t>(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(head.data.size())), head.data);
	auto readInt = [&bytes](size_t offset) {
		return static_cast<int32_t>(bytes[offset] | bytes[offset + 1] << 8 | bytes[offset + 2] << 16 | static_cast<uint32_t>(bytes[offset + 3]) << 24);
	};
	int32_t start = readInt(head.data.size());
	EXPECT_LE(std::abs(static_cast<int64_t>(start) - nowSeconds), 2);
	EXPECT_EQ(readInt(head.data.size() + 4), start + 61);
	EXPECT_EQ(std::vector<uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(head.data.size()) + 8, bytes.end()),
		Bytes().S("\n\n\n\xEE\x80\xA6 15 minute cooldown between switching factions\n\n\n\n\n\n\n").data);
}

TEST_F(PlayerInfoPacketsTest, EquippedItemsMaskDropsTheSubHandOfTwoHandedWeapons) {
	PACKET_TEST_SCOPE;
	xml::LoadContext context;
	const auto* sword = xml::bindString<model::templates::item::ItemTemplate>(context, R"(<item_template id="100000001" name="sword"/>)").release();
	const auto* armor = xml::bindString<model::templates::item::ItemTemplate>(context, R"(<item_template id="110000001" name="armor"/>)").release();
	using namespace model::items::detail;
	// the first item is a two-handed weapon (main and sub hand): the sub hand bit is removed after it, the torso bit of the next item is kept
	Ref<model::gameobjects::Item> twoHanded = model::gameobjects::Item::create(900001, sword, 1, true, SLOT_MAIN_HAND | SLOT_SUB_HAND);
	Ref<model::gameobjects::Item> torso = model::gameobjects::Item::create(900002, armor, 1, true, SLOT_TORSO);
	torso->setItemColor(0x00FF7F);
	TestPlayerInfoPacket packet(nullptr, {runtime::Ptr<model::gameobjects::Item>(*twoHanded), runtime::Ptr<model::gameobjects::Item>(*torso)});
	Bytes expected;
	expected.header(32).D(static_cast<int32_t>(SLOT_MAIN_HAND | SLOT_TORSO));
	expected.D(100000001).D(0).dye(std::nullopt).H(twoHanded->getItemEnchantParam()).H(0);
	expected.D(110000001).D(0).dye(0x00FF7F).H(torso->getItemEnchantParam()).H(0);
	EXPECT_EQ(serialized(packet), expected.data);

	// SM_UPDATE_PLAYER_APPEARANCE: writeD(playerId) and writeEquippedItems(items); a sub hand item after the weapon keeps its bit
	Ref<model::gameobjects::Item> shield = model::gameobjects::Item::create(900003, armor, 1, true, SLOT_SUB_HAND);
	SM_UPDATE_PLAYER_APPEARANCE appearance(100050, {runtime::Ptr<model::gameobjects::Item>(*twoHanded), runtime::Ptr<model::gameobjects::Item>(*shield)});
	Bytes expectedAppearance;
	expectedAppearance.header(36).D(100050).D(static_cast<int32_t>(SLOT_MAIN_HAND | SLOT_SUB_HAND));
	expectedAppearance.D(100000001).D(0).dye(std::nullopt).H(twoHanded->getItemEnchantParam()).H(0);
	expectedAppearance.D(110000001).D(0).dye(std::nullopt).H(shield->getItemEnchantParam()).H(0);
	EXPECT_EQ(serialized(appearance), expectedAppearance.data);
}

} // namespace
} // namespace aion::gameserver::network::aion::serverpackets::testing
