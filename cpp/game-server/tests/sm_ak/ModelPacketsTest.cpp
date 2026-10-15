// P4-16 golden bytes of the server packets that read game objects: the real Player (on stat container doubles, SmAkTestSupport.h), its account,
// common data, lists and position, and the PER_RECIPIENT packets serialized for a real AionConnection whose account and active player are set.
// Expected bodies are written by hand from the Java writeImpl methods. A test whose packet reaches a body of a later chunk skips with the name
// of that body, so it runs as soon as the body is ported.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/animations/ObjectDeleteAnimation.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/BlockedPlayer.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/templates/challenge/ChallengeType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_EFFECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHALLENGE_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CONQUEROR_PROTECTOR.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANK_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ACCOUNT_PROPERTIES.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BLOCK_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CASTSPELL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHANNEL_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHAT_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CREATE_CHARACTER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUBE_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_SETTINGS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FIND_GROUP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORCED_MOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GM_SEARCH.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HEADING_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_EDIT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_OBJECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_REGISTRY.h"
#include "aion/gameserver/network/aion/serverpackets/SM_KEY.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "SmAkTestSupport.h"

namespace aion::gameserver::network::aion::serverpackets::test {
namespace {

using runtime::Ptr;
using runtime::Ref;

/** Runs `statement`; skips the test if it reaches a body another chunk has not ported yet */
#define SKIP_IF_UNPORTED(statement)                                                                                                                  \
	try {                                                                                                                                              \
		statement;                                                                                                                                       \
	} catch (const runtime::UnportedException& unported) {                                                                                             \
		GTEST_SKIP() << unported.what();                                                                                                                 \
	}

TEST_F(PacketTest, GmSearchFormatsNameMapAndIntegerCoordinates) {
	PlayerFixture f = makePlayer(100101, 9101, "Seeker");
	f.player->setPosition(world::WorldPosition::create(210010000, 100.75f, -20.25f, 50.5f, int8_t{7}));
	// Java: "search " + name + " " + worldId + " " + (int) x + " " + (int) y + " " + (int) z, (int) truncating towards zero
	EXPECT_EQ(dataOf(SM_GM_SEARCH(*f.player)), Bytes().S("search Seeker 210010000 100 -20 50").data);
}

TEST_F(PacketTest, VisibleObjectPositionPackets) {
	PlayerFixture f = makePlayer(100102, 9102, "Mover");
	PlayerFixture other = makePlayer(100103, 9103, "Target");
	f.player->setPosition(world::WorldPosition::create(210010000, 1.0f, 2.0f, 3.0f, int8_t{-5}));
	other.player->setPosition(world::WorldPosition::create(210010000, 4.5f, 5.5f, 6.5f, int8_t{9}));
	EXPECT_EQ(dataOf(SM_HEADING_UPDATE(*f.player)), Bytes().D(100102).C(-5).data);
	EXPECT_EQ(dataOf(SM_FORCED_MOVE(*f.player, *other.player)), Bytes().D(100102).D(100103).C(16).F(4.5f).F(5.5f).F(6.5f).data);
	EXPECT_EQ(dataOf(SM_FORCED_MOVE(*f.player, 7, 1.0f, 2.0f, 3.0f)), Bytes().D(100102).D(7).C(16).F(1.0f).F(2.0f).F(3.0f).data);
	// ObjectDeleteAnimation ids: NONE 0, FADE_OUT 1, FADE_OUT_BEAM 2, JUMP_IN 11, DELAYED 19; out of range always NONE
	EXPECT_EQ(dataOf(SM_DELETE(*f.player)), Bytes().D(100102).C(1).data);
	EXPECT_EQ(dataOf(SM_DELETE(*f.player, false)), Bytes().D(100102).C(0).data);
	EXPECT_EQ(dataOf(SM_DELETE(*f.player, model::animations::ObjectDeleteAnimation::DELAYED)), Bytes().D(100102).C(19).data);
	// a position that is not spawned: one channel
	EXPECT_EQ(dataOf(SM_CHANNEL_INFO(f.player->getPosition())), Bytes().D(1).D(1).data);
	// SM_CASTSPELL target types: 0/3/4 the target object, 1 a point, 2 a point and eight unknown ints
	EXPECT_EQ(dataOf(SM_CASTSPELL(*f.player, 1001, 2, 0, 555, 1500, 1.25f, true)),
		Bytes().D(100102).H(1001).C(2).C(0).D(555).H(1500).C(0).F(1.25f).C(1).data);
	EXPECT_EQ(dataOf(SM_CASTSPELL(*f.player, 1001, 2, 1, 7.0f, 8.0f, 9.0f, 1500, 1.0f, false)),
		Bytes().D(100102).H(1001).C(2).C(1).F(7.0f).F(8.0f).F(9.0f).H(1500).C(0).F(1.0f).C(0).data);
	EXPECT_EQ(dataOf(SM_CASTSPELL(*f.player, 1001, 2, 2, 7.0f, 8.0f, 9.0f, 1500, 1.0f, false)),
		Bytes().D(100102).H(1001).C(2).C(2).F(7.0f).F(8.0f).F(9.0f).D(0).D(0).D(0).D(0).D(0).D(0).D(0).D(0).H(1500).C(0).F(1.0f).C(0).data);
	// an unknown target type writes no target block
	EXPECT_EQ(dataOf(SM_CASTSPELL(*f.player, 1001, 2, 5, 555, 1500, 1.0f, false)), Bytes().D(100102).H(1001).C(2).C(5).H(1500).C(0).F(1.0f).C(0).data);
}

TEST_F(PacketTest, CustomSettingsOfAPlayer) {
	PlayerFixture f = makePlayer(100104, 9104, "Styled");
	Ref<model::gameobjects::player::PlayerSettings> settings = model::gameobjects::player::PlayerSettings::create();
	f.player->setPlayerSettings(settings);
	SKIP_IF_UNPORTED(settings->setDisplay(SM_CUSTOM_SETTINGS::HIDE_LEGION_CLOAK));
	SKIP_IF_UNPORTED(settings->setDeny(6));
	// Java: this(player.getObjectId(), 1, display, deny)
	EXPECT_EQ(dataOf(SM_CUSTOM_SETTINGS(*f.player)), Bytes().D(100104).C(1).H(1).H(6).data);
}

TEST_F(PacketTest, AbyssRankPackets) {
	PlayerFixture f = makePlayer(100105, 9105, "Ranked");
	// AbyssRank(dailyAP, weeklyAP, ap, rank, dailyKill, weeklyKill, allKill, maxRank, lastKill, lastAP, lastUpdate, dailyGP, weeklyGP, gp, lastGP)
	Ref<model::gameobjects::player::AbyssRank> rank = model::gameobjects::player::AbyssRank::create(11, 12, 5000, 3, 4, 5, 6, 7, 8, 9,
		commons::utils::currentTimeMillis(), 21, 22, 23, 24);
	f.player->setAbyssRank(rank);
	// rank 3 is AbyssRankEnum.GRADE7_SOLDIER, whose getId() is 3; lastUpdate = now, so doUpdate resets nothing and maxRank 7 >= 3 stays
	EXPECT_EQ(dataOf(SM_ABYSS_RANK_UPDATE(0, *f.player)), Bytes().C(0).D(100105).D(3).data);
	EXPECT_EQ(dataOf(SM_ABYSS_RANK_UPDATE(1, *f.player)), Bytes().C(1).D(100105).D(0).data); // no team
	EXPECT_EQ(dataOf(SM_ABYSS_RANK_UPDATE(2, *f.player)), Bytes().C(2).D(100105).D(0).data); // no mentor
	EXPECT_EQ(dataOf(SM_ABYSS_RANK_UPDATE(3, *f.player)), Bytes().C(3).D(100105).data);
	EXPECT_EQ(dataOf(SM_ABYSS_RANK(*f.player, 42)),
		Bytes()
			.Q(5000) // ap
			.D(23) // currentGp (gp 23 > 0)
			.D(3) // rank id
			.D(42) // rankingListPosition
			.D(0)
			.D(6) // allKill
			.D(7) // maxRank
			.D(4) // dailyKill
			.Q(11) // dailyAP
			.D(21) // dailyGP
			.D(5) // weeklyKill
			.Q(12) // weeklyAP
			.D(22) // weeklyGP
			.D(8) // lastKill
			.Q(9) // lastAP
			.D(24) // lastGP
			.C(0)
			.data);
}

TEST_F(PacketTest, FindGroupWhisperOfAnApplicant) {
	PlayerFixture f = makePlayer(100106, 9106, "Applicant");
	f.commonData->setPlayerClass(model::PlayerClass::RANGER); // id 5
	EXPECT_EQ(dataOf(SM_FIND_GROUP(*f.player)),
		Bytes().C(11).D(100106).D(0).D(0).H(0).C(0).C(5).D(f.player->getLevel()).S(f.player->getName(true)).data);
}

TEST_F(PacketTest, ChatWindowOfAPlayerWithoutTeam) {
	PlayerFixture f = makePlayer(100107, 9107, "Talker");
	f.commonData->setPlayerClass(model::PlayerClass::SORCERER); // id 7
	f.commonData->setNote("hello");
	f.account->setMembership(int8_t{2});
	// not a group request: name, legion name (none), level, class id as short, note, 1, membership
	std::vector<uint8_t> single;
	SKIP_IF_UNPORTED(single = dataOf(SM_CHAT_WINDOW(*f.player, false)));
	EXPECT_EQ(single, Bytes().C(1).S(f.player->getName(true)).S("").C(f.player->getLevel()).H(7).S("hello").D(1).C(2).data);
	std::vector<uint8_t> group;
	SKIP_IF_UNPORTED(group = dataOf(SM_CHAT_WINDOW(*f.player, true)));
	// no group, no alliance: 4, name, 0, class id, level, 0
	EXPECT_EQ(group, Bytes().C(4).S(f.player->getName(true)).D(0).C(7).C(f.player->getLevel()).C(0).data);
}

TEST_F(PacketTest, CubeSizeOfTheInventoryAndWarehouse) {
	PlayerFixture f = makePlayer(100108, 9108, "Carrier");
	f.commonData->setNpcExpands(2);
	f.commonData->setQuestExpands(1);
	f.commonData->setItemExpands(3);
	f.commonData->setWhNpcExpands(4);
	f.commonData->setWhBonusExpands(5);
	std::vector<uint8_t> cube;
	SKIP_IF_UNPORTED(cube = dataOf(SM_CUBE_UPDATE::cubeSize(model::items::storage::StorageType::CUBE, *f.player)));
	// action 0, StorageType.CUBE.ordinal() 0, item count, npc/quest/item expands
	EXPECT_EQ(cube, Bytes().C(0).C(0).D(f.player->getInventory().size()).C(2).C(1).C(3).data);
	std::vector<uint8_t> warehouse;
	SKIP_IF_UNPORTED(warehouse = dataOf(SM_CUBE_UPDATE::cubeSize(model::items::storage::StorageType::REGULAR_WAREHOUSE, *f.player)));
	EXPECT_EQ(warehouse, Bytes().C(0).C(static_cast<int32_t>(model::items::storage::StorageType::REGULAR_WAREHOUSE))
							 .D(f.player->getWarehouse().size())
							 .C(4)
							 .C(5)
							 .C(0)
							 .data);
}

TEST_F(PacketTest, PerRecipientPacketsReadTheConnection) {
	PlayerFixture f = makePlayer(100109, 9109, "Receiver");
	TestConnection con;
	con->setAccount(*f.account);
	ASSERT_TRUE(con->setActivePlayer(Ptr<model::gameobjects::player::Player>(f.player)));

	// SM_ACCOUNT_PROPERTIES: the GM panel byte compares the access level with AdminConfig.GM_PANEL
	const int8_t previousGmPanel = configs::administration::AdminConfig::GM_PANEL.load();
	configs::administration::AdminConfig::GM_PANEL = 3;
	f.account->setAccessLevel(int8_t{2});
	EXPECT_EQ(dataOf(SM_ACCOUNT_PROPERTIES(), con.get()), Bytes().C(0).D(0).H(0).C(0).H(0).C(0).H(0).D(0).D(0).D(0).D(4).data);
	f.account->setAccessLevel(int8_t{3});
	EXPECT_EQ(dataOf(SM_ACCOUNT_PROPERTIES(), con.get()), Bytes().C(1).D(0).H(0).C(0).H(0).C(0).H(0).D(0).D(0).D(0).D(4).data);
	configs::administration::AdminConfig::GM_PANEL = previousGmPanel;

	// SM_BLOCK_LIST: -size as short, 0, then name and reason of each blocked player
	Ref<model::gameobjects::player::BlockList> blockList = model::gameobjects::player::BlockList::create();
	Ref<model::gameobjects::player::BlockedPlayer> blocked = model::gameobjects::player::BlockedPlayer::create(5, "Spammer", "spam");
	blockList->add(*blocked);
	f.player->setBlockList(blockList);
	EXPECT_EQ(dataOf(SM_BLOCK_LIST(), con.get()), Bytes().H(-1).C(0).S("Spammer").S("spam").data);

	// SM_DIALOG_WINDOW with another page than MAIL or TOWN_CHALLENGE_TASK
	EXPECT_EQ(dataOf(SM_DIALOG_WINDOW(700, 5, 1000), con.get()), Bytes().D(700).H(5).D(1000).H(0).H(0).data);

	// SM_FRIEND_LIST without friends and SM_FRIEND_UPDATE of an unknown friend (Java logs and writes nothing)
	f.player->setFriendList(std::make_unique<model::gameobjects::player::FriendList>(*f.player, std::vector<Ptr<model::gameobjects::player::Friend>>{}));
	EXPECT_EQ(dataOf(SM_FRIEND_LIST(), con.get()), Bytes().H(0).C(0).data);
	EXPECT_EQ(dataOf(SM_FRIEND_UPDATE(12345), con.get()), std::vector<uint8_t>{});

	// SM_FRIEND_UPDATE of a friend (offline: the last online time)
	PlayerFixture buddy = makePlayer(100110, 9110, "Buddy");
	buddy.commonData->setPlayerClass(model::PlayerClass::CHANTER); // id 11
	buddy.commonData->setGender(model::Gender::FEMALE);
	buddy.commonData->setMapId(220010000);
	buddy.commonData->setNote("away");
	Ref<model::gameobjects::player::Friend> friend_ = model::gameobjects::player::Friend::create(*buddy.commonData, "memo");
	f.player->getFriendList().addFriend(*friend_);
	std::vector<uint8_t> update;
	SKIP_IF_UNPORTED(update = dataOf(SM_FRIEND_UPDATE(100110), con.get()));
	EXPECT_EQ(update, Bytes()
						  .S("Buddy")
						  .D(buddy.commonData->getLevel())
						  .D(11)
						  .C(1)
						  .D(220010000)
						  .D(buddy.commonData->getLastOnlineEpochSeconds())
						  .S("away")
						  .C(0) // Status.OFFLINE
						  .data);
}

TEST_F(PacketTest, PerRecipientPacketsWithoutConnectionThrowNullPointerException) {
	SM_BLOCK_LIST blockList;
	EXPECT_THROW(blockList.serialize(nullptr), runtime::NullPointerException);
	SM_ACCOUNT_PROPERTIES properties;
	EXPECT_THROW(properties.serialize(nullptr), runtime::NullPointerException);
	SM_KEY key;
	EXPECT_THROW(key.serialize(nullptr), runtime::NullPointerException);
}

TEST_F(PacketTest, HousePacketsReturnEarlyWithoutAnActivePlayer) {
	TestConnection con;
	// Java: if (player == null) return
	EXPECT_EQ(dataOf(SM_HOUSE_EDIT(3), con.get()), std::vector<uint8_t>{});
	EXPECT_EQ(dataOf(SM_HOUSE_REGISTRY(1), con.get()), std::vector<uint8_t>{});
}

TEST_F(PacketTest, KeyPacketWritesTheEnabledCryptKey) {
	TestConnection con;
	// Java: writeD(con.enableCryptKey()) - a random key, 4 bytes
	EXPECT_EQ(dataOf(SM_KEY(), con.get()).size(), 4u);
}

TEST_F(PacketTest, CreateCharacterWithAnErrorWritesOnlyTheCode) {
	TestConnection con;
	EXPECT_EQ(dataOf(SM_CREATE_CHARACTER(nullptr, SM_CREATE_CHARACTER::RESPONSE_NAME_ALREADY_USED), con.get()), Bytes().D(10).data);
}

TEST_F(PacketTest, AbnormalPacketsWithoutEffects) {
	PlayerFixture f = makePlayer(100112, 9112, "Healthy");
	// SM_ABNORMAL_STATE: abnormals, 0, 0, slot, no effects
	EXPECT_EQ(dataOf(SM_ABNORMAL_STATE(std::vector<Ptr<skillengine::model::Effect>>{}, 0x40, 3)), Bytes().D(0x40).D(0).D(0).C(3).H(0).data);
	// SM_ABNORMAL_EFFECT of a player: effect type 2, the given slots (127 = SkillTargetSlot.FULLSLOTS keeps the effects unfiltered)
	EXPECT_EQ(dataOf(SM_ABNORMAL_EFFECT(*f.player, 0x10, std::vector<Ptr<skillengine::model::Effect>>{}, 127)),
		Bytes().D(100112).C(2).D(0).D(0x10).D(0).C(127).H(0).data);
	EXPECT_EQ(dataOf(SM_ABNORMAL_EFFECT(*f.player, 0, std::vector<Ptr<skillengine::model::Effect>>{}, 1)), Bytes().D(100112).C(2).D(0).D(0).D(0).C(1).H(0).data);
}

TEST_F(PacketTest, ChallengeListHeaderOfTheRecipient) {
	PlayerFixture f = makePlayer(100113, 9113, "Challenger");
	TestConnection con;
	ASSERT_TRUE(con->setActivePlayer(Ptr<model::gameobjects::player::Player>(f.player)));
	using model::templates::challenge::ChallengeType;
	// an action without a block: action, owner id, ChallengeType id (LEGION 1, TOWN 2), the recipient's object id
	EXPECT_EQ(dataOf(SM_CHALLENGE_LIST(1, 700, ChallengeType::TOWN, std::vector<Ptr<model::challenge::ChallengeTask>>{}), con.get()),
		Bytes().C(1).D(700).C(2).D(100113).data);
	// action 2 without tasks: the current time in seconds and no entries
	const int64_t before = commons::utils::currentTimeMillis() / 1000;
	std::vector<uint8_t> list = dataOf(SM_CHALLENGE_LIST(2, 701, ChallengeType::LEGION, std::vector<Ptr<model::challenge::ChallengeTask>>{}), con.get());
	const int64_t after = commons::utils::currentTimeMillis() / 1000;
	ASSERT_EQ(list.size(), 16u);
	EXPECT_EQ(std::vector<uint8_t>(list.begin(), list.begin() + 10), Bytes().C(2).D(701).C(1).D(100113).data);
	const int32_t now = list[10] | list[11] << 8 | list[12] << 16 | list[13] << 24;
	EXPECT_GE(now, before);
	EXPECT_LE(now, after);
	EXPECT_EQ(list[14], 0);
	EXPECT_EQ(list[15], 0);
}

TEST_F(PacketTest, ConquerorProtectorOfAPlayer) {
	PlayerFixture f = makePlayer(100114, 9114, "Protector");
	f.player->setPosition(world::WorldPosition::create(210020000, 10.5f, 20.5f, 30.0f, int8_t{0}));
	std::vector<uint8_t> single;
	SKIP_IF_UNPORTED(single = dataOf(SM_CONQUEROR_PROTECTOR(9, *f.player)));
	EXPECT_EQ(single, Bytes().D(9).D(1).D(1).H(1).D(0).D(100114).data);
}

TEST_F(PacketTest, AttackStatusOfAPlayer) {
	PlayerFixture f = makePlayer(100111, 9111, "Victim");
	using TYPE = SM_ATTACK_STATUS::TYPE;
	using LOG = SM_ATTACK_STATUS::LOG;
	int32_t hpPercentage = 0;
	int32_t mpPercentage = 0;
	// the life stats double is read through Creature (Player::getLifeStats casts to PlayerLifeStats, P5-01)
	model::gameobjects::Creature& creature = *f.player;
	SKIP_IF_UNPORTED(hpPercentage = creature.getLifeStats()->getHpPercentage());
	SKIP_IF_UNPORTED(mpPercentage = creature.getLifeStats()->getMpPercentage());
	// DAMAGE: negative value and HP percentage; TYPE.DAMAGE value 7, LOG.SPELLATK 1; critical display code 12
	EXPECT_EQ(dataOf(SM_ATTACK_STATUS(*f.player, TYPE::DAMAGE, 1001, 250, LOG::SPELLATK, true)),
		Bytes().D(100111).D(-250).C(7).C(hpPercentage).H(1001).C(1).C(12).data);
	// USED_MP: negative value and MP percentage (23)
	EXPECT_EQ(dataOf(SM_ATTACK_STATUS(*f.player, TYPE::USED_MP, 0, 30)), Bytes().D(100111).D(-30).C(23).C(mpPercentage).H(0).C(191).C(0).data);
	// HEAL_MP: positive value and MP percentage (19)
	EXPECT_EQ(dataOf(SM_ATTACK_STATUS(*f.player, TYPE::HEAL_MP, 2, 40, LOG::MPHEAL)), Bytes().D(100111).D(40).C(19).C(mpPercentage).H(2).C(4).C(0).data);
	// REGULAR (5) with the default log REGULAR (191); HP is a different constant with the value of DAMAGE but positive
	EXPECT_EQ(dataOf(SM_ATTACK_STATUS(*f.player, 60)), Bytes().D(100111).D(60).C(5).C(hpPercentage).H(0).C(191).C(0).data);
	EXPECT_EQ(dataOf(SM_ATTACK_STATUS(*f.player, TYPE::HP, 3, 70)), Bytes().D(100111).D(70).C(7).C(hpPercentage).H(3).C(191).C(0).data);
}

} // namespace
} // namespace aion::gameserver::network::aion::serverpackets::test
