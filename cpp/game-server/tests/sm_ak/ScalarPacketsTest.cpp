// P4-16 golden bytes of the server packets whose writeImpl writes constructor values, static data templates or small model records: the
// expected bodies are written by hand from the Java writeImpl methods (field widths and order, conditional blocks, UTF-16 strings, Java null
// strings written as a lone 0 char) and the Java enum constructor arguments.

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/dao/AbyssRankDAO.h"
#include "aion/gameserver/dao/BookmarkDAO.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/DuelResult.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Passport.h"
#include "aion/gameserver/model/account/PassportsList.h"
#include "aion/gameserver/model/animations/ActionAnimation.h"
#include "aion/gameserver/model/gameobjects/player/emotion/Emotion.h"
#include "aion/gameserver/model/house/PlayerScript.h"
#include "aion/gameserver/model/items/ItemCooldown.h"
#include "aion/gameserver/model/templates/gather/GatherableTemplate.bind.h"
#include "aion/gameserver/model/templates/gather/GatherableTemplate.h"
#include "aion/gameserver/model/templates/gather/Material.bind.h"
#include "aion/gameserver/model/templates/gather/Material.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/ResultedItem.bind.h"
#include "aion/gameserver/model/templates/item/ResultedItem.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANKING_LEGIONS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANKING_PLAYERS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ACTION_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_AFTER_SIEGE_LOCINFO_475.h"
#include "aion/gameserver/network/aion/serverpackets/SM_AFTER_TIME_CHECK_4_7_5.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_READY_CHECK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ASCENSION_MORPH.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATREIAN_PASSPORT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOGInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPEInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BIND_POINT_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BIND_POINT_TELEPORT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BLOCK_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BROKER_SERVICE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CAPTCHA.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHANNEL_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHARACTER_SELECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHAT_INIT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CLOSE_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CONQUEROR_PROTECTOR.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CRAFT_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CRAFT_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUBE_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_PACKET.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_PACKET_PacketElementTypeInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_SETTINGS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_CHARACTER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_HOUSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_HOUSE_OBJECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_WAREHOUSE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DP_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DUEL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ENTER_WORLD_CHECK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EXCHANGE_ADD_KINAH.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EXCHANGE_CONFIRMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EXCHANGE_REQUEST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FIND_GROUP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FIRST_SHOW_DECOMPOSABLE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FLY_TIME.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORTRESS_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_NOTIFY.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GAMEGUARD.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GATHER_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GATHER_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GF_WEBSHOP_TOKEN_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GM_BOOKMARK_ADD.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_DATA_EXCHANGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_LOOT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_ACQUIRE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_PAY_RENT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_SCRIPTS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_TELEPORT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ICON_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_COUNT_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_STAGE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_COOLDOWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemDeleteType.h"
#include "SmAkTestSupport.h"

namespace aion::gameserver::network::aion::serverpackets::test {
namespace {

using runtime::Ptr;
using runtime::Ref;

/** Static data bound from XML text is kept for the process like the DataManager holders keep it */
template <class T>
const T* bindStatic(std::string_view xml) {
	xml::LoadContext context;
	return xml::bindString<T>(context, xml).release();
}

/** Java ChatUtil.l10n(id): "$" + (char) (id * 2 + 1) + (char) ((id * 2 + 1) >>> 16) */
std::u16string l10nOf(int32_t id) {
	const uint32_t encoded = static_cast<uint32_t>(id) << 1 | 1;
	return std::u16string{u'$', static_cast<char16_t>(encoded & 0xFFFF), static_cast<char16_t>(encoded >> 16)};
}

TEST_F(PacketTest, ConstantBodies) {
	EXPECT_EQ(dataOf(SM_AFTER_SIEGE_LOCINFO_475()), Bytes().H(0).C(0).data);
	EXPECT_EQ(dataOf(SM_AFTER_TIME_CHECK_4_7_5()), Bytes().H(1).D(0).data);
	EXPECT_EQ(dataOf(SM_ENTER_WORLD_CHECK()), Bytes().C(0).C(0).C(0).data);
	// Msg ids: OK(0) .. REENTRY_TIME(6)
	EXPECT_EQ(dataOf(SM_ENTER_WORLD_CHECK(SM_ENTER_WORLD_CHECK::Msg::BOTH_FACTIONS)), Bytes().C(3).C(0).C(0).data);
	EXPECT_EQ(dataOf(SM_ENTER_WORLD_CHECK(SM_ENTER_WORLD_CHECK::Msg::REENTRY_TIME)), Bytes().C(6).C(0).C(0).data);
}

TEST_F(PacketTest, IntegerFieldPackets) {
	EXPECT_EQ(dataOf(SM_DELETE_HOUSE(0x11223344)), Bytes().D(0x11223344).data);
	EXPECT_EQ(dataOf(SM_EXCHANGE_CONFIRMATION(2)), Bytes().C(2).data);
	EXPECT_EQ(dataOf(SM_DELETE_HOUSE_OBJECT(-5)), Bytes().D(-5).data);
	EXPECT_EQ(dataOf(SM_FORTRESS_INFO(1011, true)), Bytes().D(1011).C(1).data);
	EXPECT_EQ(dataOf(SM_FORTRESS_INFO(1011, false)), Bytes().D(1011).C(0).data);
	EXPECT_EQ(dataOf(SM_FRIEND_STATUS(3)), Bytes().C(3).data);
	EXPECT_EQ(dataOf(SM_HOUSE_PAY_RENT(4)), Bytes().C(0).C(4).data);
	EXPECT_EQ(dataOf(SM_ASCENSION_MORPH(1)), Bytes().C(1).C(0).data);
	EXPECT_EQ(dataOf(SM_EXCHANGE_ADD_KINAH(0x123456789ALL, 1)), Bytes().C(1).Q(0x123456789ALL).data);
	EXPECT_EQ(dataOf(SM_FLY_TIME(60, 120)), Bytes().D(60).D(120).data);
	EXPECT_EQ(dataOf(SM_HOUSE_TELEPORT(1100, 77)), Bytes().D(1100).D(77).data);
	EXPECT_EQ(dataOf(SM_ALLIANCE_READY_CHECK(42, 5)), Bytes().D(42).C(5).data);
	EXPECT_EQ(dataOf(SM_DP_INFO(9, 3000)), Bytes().D(9).H(3000).data);
	EXPECT_EQ(dataOf(SM_INSTANCE_COUNT_INFO(300030000, 7)), Bytes().D(300030000).D(7).D(1).data);
	EXPECT_EQ(dataOf(SM_ICON_INFO(10, true)), Bytes().D(0).D(10).C(1).data);
	EXPECT_EQ(dataOf(SM_HOUSE_ACQUIRE(5, 1200, false)), Bytes().D(5).D(1200).D(0).data);
	EXPECT_EQ(dataOf(SM_HOUSE_ACQUIRE(5, 1200, true)), Bytes().D(5).D(1200).D(1).data);
	EXPECT_EQ(dataOf(SM_INSTANCE_STAGE_INFO(2, 3, 4)), Bytes().C(2).D(0).H(3).H(4).data);
	EXPECT_EQ(dataOf(SM_CRAFT_ANIMATION(1, 2, 40001, 1)), Bytes().D(1).D(2).H(40001).C(1).data);
	EXPECT_EQ(dataOf(SM_GATHER_ANIMATION(1, 2, 30002, 5)), Bytes().D(1).D(2).H(30002).C(5).data);
	EXPECT_EQ(dataOf(SM_ATTACK_RESPONSE::STOP_OBSTACLE_IN_THE_WAY(3)), Bytes().C(5).C(3).data);
	EXPECT_EQ(dataOf(SM_ATTACK_RESPONSE::TARGET_IN_DIFFERENT_AREA(1)), Bytes().C(1).C(1).data);
	EXPECT_EQ(dataOf(SM_GROUP_LOOT(10, 20, 186000001, 3, 30, 2, 0x100000063LL, 4)),
		Bytes().D(10).D(4).D(3).D(186000001).C(0).C(0).C(0).D(30).C(2).D(20).D(0x63).data); // (int) luck keeps the low 32 bits
	EXPECT_EQ(dataOf(SM_CUSTOM_SETTINGS(15, 1, SM_CUSTOM_SETTINGS::HIDE_HELMET | SM_CUSTOM_SETTINGS::HIDE_PLUME, 2)),
		Bytes().D(15).C(1).H(12).H(2).data);
}

TEST_F(PacketTest, ItemDeletePacketsWriteTheDeleteTypeMask) {
	using services::item::ItemPacketService_ItemDeleteType;
	EXPECT_EQ(dataOf(SM_DELETE_ITEM(700)), Bytes().D(700).C(0).data); // DEFAULT(0)
	EXPECT_EQ(dataOf(SM_DELETE_ITEM(700, ItemPacketService_ItemDeleteType::DECOMPOSE)), Bytes().D(700).C(0x66).data);
	EXPECT_EQ(dataOf(SM_DELETE_ITEM(700, ItemPacketService_ItemDeleteType::PUT_TO_EXCHANGE)), Bytes().D(700).C(0x26).data);
	EXPECT_EQ(dataOf(SM_DELETE_WAREHOUSE_ITEM(2, 701, ItemPacketService_ItemDeleteType::SELL)), Bytes().C(2).D(701).C(0x1F).data);
	EXPECT_EQ(dataOf(SM_DELETE_WAREHOUSE_ITEM(1, 702, ItemPacketService_ItemDeleteType::REGISTER)), Bytes().C(1).D(702).C(0x78).data);
}

TEST_F(PacketTest, ConditionalBlocks) {
	// SM_BIND_POINT_TELEPORT: action 1 adds locId, action 3 locId and cooldown, others nothing
	EXPECT_EQ(dataOf(SM_BIND_POINT_TELEPORT(1, 7, 8, 9)), Bytes().C(1).D(7).D(8).data);
	EXPECT_EQ(dataOf(SM_BIND_POINT_TELEPORT(3, 7, 8, 9)), Bytes().C(3).D(7).D(8).D(9).data);
	EXPECT_EQ(dataOf(SM_BIND_POINT_TELEPORT(0, 7, 8, 9)), Bytes().C(0).D(7).data);
	// SM_DELETE_CHARACTER: 0 object id writes 0x10, 0, 0
	EXPECT_EQ(dataOf(SM_DELETE_CHARACTER(123, 456)), Bytes().D(0).D(123).D(456).data);
	EXPECT_EQ(dataOf(SM_DELETE_CHARACTER(0, 456)), Bytes().D(0x10).D(0).D(0).data);
	// SM_CHARACTER_SELECT: type 2 adds the passkey block with PASSKEY_WRONG_MAXCOUNT
	EXPECT_EQ(dataOf(SM_CHARACTER_SELECT(0)), Bytes().C(0).data);
	EXPECT_EQ(dataOf(SM_CHARACTER_SELECT(1)), Bytes().C(1).data);
	const int32_t maxCount = configs::main::SecurityConfig::PASSKEY_WRONG_MAXCOUNT.load();
	EXPECT_EQ(dataOf(SM_CHARACTER_SELECT(2, int16_t{3}, 2)), Bytes().C(2).H(3).C(1).D(2).D(maxCount).data);
	EXPECT_EQ(dataOf(SM_CHARACTER_SELECT(2, int16_t{0}, 0)), Bytes().C(2).H(0).C(0).D(0).D(maxCount).data);
	// SM_CAPTCHA: type 1 image, type 3 result
	const std::array<uint8_t, 3> image{0xAA, 0xBB, 0xCC};
	EXPECT_EQ(dataOf(SM_CAPTCHA(2, image)), Bytes().C(1).C(2).D(3).B(image).data);
	EXPECT_EQ(dataOf(SM_CAPTCHA(true, 3000)), Bytes().C(3).H(1).D(3000).data);
	// SM_GROUP_DATA_EXCHANGE: the unk byte only for action != 1
	const std::array<uint8_t, 2> data{1, 2};
	EXPECT_EQ(dataOf(SM_GROUP_DATA_EXCHANGE(data)), Bytes().C(1).D(2).B(data).data);
	EXPECT_EQ(dataOf(SM_GROUP_DATA_EXCHANGE(data, 2, 9)), Bytes().C(2).C(9).D(2).B(data).data);
	EXPECT_EQ(dataOf(SM_GROUP_DATA_EXCHANGE(data, 1, 9)), Bytes().C(1).D(2).B(data).data);
	EXPECT_EQ(dataOf(SM_CHAT_INIT(data)), Bytes().D(2).B(data).data);
	// SM_CUBE_UPDATE.stigmaSlots: action 6 has no size block
	EXPECT_EQ(dataOf(SM_CUBE_UPDATE::stigmaSlots(4)), Bytes().C(6).C(4).data);
}

TEST_F(PacketTest, GameGuardWritesSizeZeroBytes) {
	EXPECT_EQ(dataOf(SM_GAMEGUARD(3)), Bytes().D(3).zeros(3).data);
	EXPECT_EQ(dataOf(SM_GAMEGUARD(0)), Bytes().D(0).data);
	// Java: new byte[-1] throws NegativeArraySizeException after writeD
	SM_GAMEGUARD negative(-1);
	EXPECT_THROW(negative.serialize(nullptr), commons::utils::IllegalArgumentException);
}

TEST_F(PacketTest, StringPackets) {
	EXPECT_EQ(dataOf(SM_EXCHANGE_REQUEST("Trader")), Bytes().S("Trader").data);
	EXPECT_EQ(dataOf(SM_BLOCK_RESPONSE(SM_BLOCK_RESPONSE::LIST_FULL, "Name")), Bytes().S("Name").C(3).data);
	EXPECT_EQ(dataOf(SM_FRIEND_NOTIFY(SM_FRIEND_NOTIFY::DELETED, "Buddy")), Bytes().S("Buddy").C(2).data);
	// non-ASCII names are written as UTF-16 (the client's code units)
	EXPECT_EQ(dataOf(SM_EXCHANGE_REQUEST("\xC3\x84sir")), Bytes().S(u"Äsir").data);
	// writeS(token, 32): 32 chars zero padded plus the 0 char
	EXPECT_EQ(dataOf(SM_GF_WEBSHOP_TOKEN_RESPONSE("abc")), Bytes().S("abc", 32).data);
	EXPECT_EQ(dataOf(SM_GF_WEBSHOP_TOKEN_RESPONSE(std::string(40, 'x'))), Bytes().S(std::string(32, 'x'), 32).data);
}

TEST_F(PacketTest, FriendResponses) {
	EXPECT_EQ(dataOf(SM_FRIEND_RESPONSE::TARGET_ADDED("Alice")), Bytes().S("Alice").C(0).data);
	EXPECT_EQ(dataOf(SM_FRIEND_RESPONSE::REQUEST_ALREADY_RECEIVED("Bob")), Bytes().S("Bob").C(0x13).data);
	// cached constants carry an empty name ("" in Java's this("", messageType))
	EXPECT_EQ(dataOf(*SM_FRIEND_RESPONSE::TARGET_OFFLINE), Bytes().S("").C(1).data);
	EXPECT_EQ(dataOf(*SM_FRIEND_RESPONSE::CLOSE_SEND_REQUEST_WINDOW), Bytes().S("").C(0x11).data);
	// a shared constant stays unchanged by serialization: the same bytes again
	EXPECT_EQ(dataOf(*SM_FRIEND_RESPONSE::TARGET_OFFLINE), Bytes().S("").C(1).data);
}

TEST_F(PacketTest, CloseQuestionWindowWritesThreeParameters) {
	// the fourth parameter is not written, missing ones are Java null (a lone 0 char)
	EXPECT_EQ(dataOf(SM_CLOSE_QUESTION_WINDOW::STR_DUEL_HE_REJECT_DUEL("Rival")), Bytes().D(0).D(1300097).S("Rival").S("").S("").data);
	EXPECT_EQ(dataOf(SM_CLOSE_QUESTION_WINDOW::CLOSE_QUESTION_WINDOW()), Bytes().D(0).D(0).S("").S("").S("").data);
	// String.valueOf of the Object... parameters
	EXPECT_EQ(dataOf(SM_CLOSE_QUESTION_WINDOW(5, "a", 12, true, "ignored")), Bytes().D(0).D(5).S("a").S("12").S("true").data);
}

TEST_F(PacketTest, DuelPackets) {
	// DuelResult: DUEL_WON(1300098, 2), DUEL_LOST(1300099, 0), DUEL_DRAW(1300100, 1)
	EXPECT_EQ(dataOf(SM_DUEL::SM_DUEL_STARTED(55)), Bytes().C(0).D(55).data);
	EXPECT_EQ(dataOf(SM_DUEL::SM_DUEL_RESULT(model::DuelResult::DUEL_WON, "Winner")), Bytes().C(1).C(2).D(1300098).S("Winner").data);
	EXPECT_EQ(dataOf(SM_DUEL::SM_DUEL_RESULT(model::DuelResult::DUEL_DRAW, "Nobody")), Bytes().C(1).C(1).D(1300100).S("Nobody").data);
}

TEST_F(PacketTest, AnimationPackets) {
	// ActionAnimation: LEVEL_UP(0), UNK(1), BIND_KISK(2), REPAIR_GATE(3), CRAFT_LEVEL_UP(4), CLASS_CHANGE(4)
	EXPECT_EQ(dataOf(SM_ACTION_ANIMATION(9, model::animations::ActionAnimation::BIND_KISK)), Bytes().D(9).H(2).D(0).data);
	EXPECT_EQ(dataOf(SM_ACTION_ANIMATION(9, model::animations::ActionAnimation::CLASS_CHANGE, 55)), Bytes().D(9).H(4).D(55).data);
	// SM_ITEM_USAGE_ANIMATION: the short constructor sets end 1 and unk3 1, unk2 defaults to 1
	EXPECT_EQ(dataOf(SM_ITEM_USAGE_ANIMATION(1, 2, 3)), Bytes().D(1).D(1).D(2).D(3).D(0).C(1).C(0).C(0).C(1).D(1).data);
	EXPECT_EQ(dataOf(SM_ITEM_USAGE_ANIMATION(1, 2, 3, 500, 0)), Bytes().D(1).D(1).D(2).D(3).D(500).C(0).C(0).C(0).C(1).D(0).data);
	EXPECT_EQ(dataOf(SM_ITEM_USAGE_ANIMATION(1, 4, 2, 3, 500, 1, 7)), Bytes().D(1).D(4).D(2).D(3).D(500).C(1).C(0).C(0).C(1).D(7).data);
	EXPECT_EQ(dataOf(SM_ITEM_USAGE_ANIMATION(1, 4, 2, 3, 500, 1, 7, 8, 9, 10)), Bytes().D(1).D(4).D(2).D(3).D(500).C(1).C(7).C(8).C(9).D(10).data);
}

TEST_F(PacketTest, EmotionWithoutCreature) {
	// SM_EMOTION(objId, type, state): speed 0; EmotionType SIT(2) has no extra block, DIE(18) writes the target (0 here)
	EXPECT_EQ(dataOf(SM_EMOTION(77, model::EmotionType::SIT, 3)), Bytes().D(77).C(2).H(3).F(0).data);
	EXPECT_EQ(dataOf(SM_EMOTION(77, model::EmotionType::DIE, 0)), Bytes().D(77).C(18).H(0).F(0).D(0).data);
	EXPECT_EQ(dataOf(SM_EMOTION(77, model::EmotionType::RESURRECT, 0)), Bytes().D(77).C(19).H(0).F(0).D(0).data);
	// RIDE(15) without a target writes only the three floats
	EXPECT_EQ(dataOf(SM_EMOTION(77, model::EmotionType::RIDE, 0)), Bytes().D(77).C(15).H(0).F(0).F(0x3F).F(0x3F).F(0x40).data);
	// CHANGE_SPEED(35) writes the attack speeds (0 without a creature)
	EXPECT_EQ(dataOf(SM_EMOTION(77, model::EmotionType::CHANGE_SPEED, 0)), Bytes().D(77).C(35).H(0).F(0).H(0).H(0).C(0).data);
}

TEST_F(PacketTest, ConquerorProtectorBuffPackets) {
	EXPECT_EQ(dataOf(SM_CONQUEROR_PROTECTOR(1, 2, 300)), Bytes().D(1).D(1).D(1).H(1).D(2).D(300).data);
	EXPECT_EQ(dataOf(SM_CONQUEROR_PROTECTOR(8, 3)), Bytes().D(8).D(1).D(1).H(1).D(3).D(0).data);
	// type 4 intruder scan without intruders
	EXPECT_EQ(dataOf(SM_CONQUEROR_PROTECTOR(std::vector<Ptr<model::gameobjects::player::Player>>{}, false)), Bytes().D(4).D(1).D(1).H(0).data);
	EXPECT_EQ(dataOf(SM_CONQUEROR_PROTECTOR(std::vector<Ptr<model::gameobjects::player::Player>>{}, true)), Bytes().D(5).D(1).D(1).H(0).data);
	// other types write only the header
	EXPECT_EQ(dataOf(SM_CONQUEROR_PROTECTOR(2, 5, 6)), Bytes().D(2).D(1).D(1).data);
}

TEST_F(PacketTest, BindPointInfo) {
	EXPECT_EQ(dataOf(SM_BIND_POINT_INFO(210010000, 1.5f, 2.5f, -3.5f)), Bytes().C(0).C(1).D(210010000).F(1.5f).F(2.5f).F(-3.5f).D(0).data);
	// no kisk: type 4 with zeros clears the display
	EXPECT_EQ(dataOf(SM_BIND_POINT_INFO(Ptr<model::gameobjects::Kisk>())), Bytes().C(4).C(1).D(0).F(0).F(0).F(0).D(0).data);
}

TEST_F(PacketTest, ChannelInfoWithoutPosition) {
	EXPECT_EQ(dataOf(SM_CHANNEL_INFO(nullptr)), Bytes().D(1).D(1).data);
}

TEST_F(PacketTest, FindGroupRemovalsAndMaskIds) {
	EXPECT_EQ(dataOf(SM_FIND_GROUP(1234, int8_t{2}, int8_t{0}, int8_t{0}, int8_t{16})), Bytes().C(1).D(1234).C(2).C(0).C(0).C(16).data);
	EXPECT_EQ(dataOf(SM_FIND_GROUP(4321)), Bytes().C(5).D(4321).data);
	EXPECT_EQ(dataOf(SM_FIND_GROUP(std::vector<int32_t>{107, 108})), Bytes().C(26).H(2).D(107).D(108).data);
	// an action without a block writes only the action
	EXPECT_EQ(dataOf(SM_FIND_GROUP(2, std::vector<Ptr<model::gameobjects::findGroup::FindGroupEntry>>{})), Bytes().C(2).data);
	// action 0 with no recruitments: two sizes and the current time in seconds
	const int64_t before = commons::utils::currentTimeMillis() / 1000;
	std::vector<uint8_t> recruitments = dataOf(SM_FIND_GROUP(0, std::vector<Ptr<model::gameobjects::findGroup::FindGroupEntry>>{}));
	const int64_t after = commons::utils::currentTimeMillis() / 1000;
	ASSERT_EQ(recruitments.size(), 9u);
	EXPECT_EQ(std::vector<uint8_t>(recruitments.begin(), recruitments.begin() + 5), Bytes().C(0).H(0).H(0).data);
	const int32_t lastUpdate = recruitments[5] | recruitments[6] << 8 | recruitments[7] << 16 | recruitments[8] << 24;
	EXPECT_GE(lastUpdate, before);
	EXPECT_LE(lastUpdate, after);
}

TEST_F(PacketTest, BrokerServiceWithoutItems) {
	// CANCEL_REGISTERED_ITEM(4), SHOW_SETTLED_ICON(5), REMOVE_SETTLED_ICON(6) as writeH, SHOW_SELL_WINDOW(7), REGISTER_ITEM(3)
	EXPECT_EQ(dataOf(SM_BROKER_SERVICE(int8_t{1}, 186000002)), Bytes().C(4).C(1).D(186000002).data);
	// overloads that differ in integer width and bool need exactly typed arguments (Java callers pass a long settledKinah)
	EXPECT_EQ(dataOf(SM_BROKER_SERVICE(true, int64_t{5000})), Bytes().C(5).Q(5000).D(0).H(0).H(1).C(0).data);
	EXPECT_EQ(dataOf(SM_BROKER_SERVICE(false, int64_t{5000})), Bytes().H(6).data);
	EXPECT_EQ(dataOf(SM_BROKER_SERVICE(int8_t{2}, 100, 30, 40)), Bytes().C(7).C(2).D(100).D(0).D(0).C(3).Q(30).Q(40).data);
	EXPECT_EQ(dataOf(SM_BROKER_SERVICE(1)), Bytes().C(3).C(1).zeros(174).H(255).zeros(7).data);
	// the lists without entries
	EXPECT_EQ(dataOf(SM_BROKER_SERVICE(std::vector<Ptr<model::gameobjects::BrokerItem>>{})), Bytes().C(1).D(0).H(0).data);
	EXPECT_EQ(dataOf(SM_BROKER_SERVICE(std::vector<Ptr<model::gameobjects::BrokerItem>>{}, 7, 2)), Bytes().C(0).D(7).C(0).H(2).H(0).data);
	EXPECT_EQ(dataOf(SM_BROKER_SERVICE(std::vector<Ptr<model::gameobjects::BrokerItem>>{}, 20, 1, 900)),
		Bytes().C(5).Q(900).D(20).H(1).C(0).H(0).data);
	// REGISTER_ITEM with message 0 from the (int message) constructor: Java never assigns brokerItems, so brokerItems.getFirst() on the null field
	// throws NullPointerException (SM_BROKER_SERVICE.java:39-42 and writeRegisterItem)
	SM_BROKER_SERVICE noItem(0);
	EXPECT_THROW(noItem.serialize(nullptr), runtime::NullPointerException);
}

TEST_F(PacketTest, CustomPacketElements) {
	using PacketElementType = SM_CUSTOM_PACKET::PacketElementType;
	SM_CUSTOM_PACKET packet(0x1FF);
	packet.addElement(PacketElementType::D, "0x10"); // Integer.decode: hexadecimal
	packet.addElement(PacketElementType::H, "-2");
	packet.addElement(PacketElementType::C, "#7F");
	packet.addElement(PacketElementType::B, "3"); // Integer.valueOf: new byte[3]
	packet.addElement(PacketElementType::F, "1.5");
	packet.addElement(PacketElementType::DF, "-0.25");
	packet.addElement(PacketElementType::Q, "010"); // Long.decode: octal
	Ref<SM_CUSTOM_PACKET::PacketElement> text = SM_CUSTOM_PACKET::PacketElement::create(PacketElementType::S, "hi");
	packet.addElement(*text);
	EXPECT_EQ(dataOf(packet), Bytes().D(16).H(-2).C(0x7F).zeros(3).F(1.5f).DF(-0.25).Q(8).S("hi").data);
	// invalid numbers throw NumberFormatException like Integer.decode
	SM_CUSTOM_PACKET invalid(1);
	invalid.addElement(PacketElementType::D, "abc");
	EXPECT_THROW(invalid.serialize(nullptr), commons::utils::NumberFormatException);
	// PacketElementType.getByCode
	EXPECT_EQ(getByCode(u'e'), PacketElementType::DF);
	EXPECT_EQ(getByCode(u's'), PacketElementType::S);
	EXPECT_EQ(getByCode(u'x'), std::nullopt);
}

TEST_F(PacketTest, AttackStatusEnumValuesFollowTheJavaConstructors) {
	using TYPE = SM_ATTACK_STATUS_TYPE;
	using LOG = SM_ATTACK_STATUS_LOG;
	EXPECT_EQ(getValue(TYPE::TYPE1), 1);
	EXPECT_EQ(getValue(TYPE::TYPE25), 25);
	EXPECT_EQ(getValue(TYPE::NATURAL_HP), 3);
	EXPECT_EQ(getValue(TYPE::REGULAR), 5);
	EXPECT_EQ(getValue(TYPE::DAMAGE), 7);
	EXPECT_EQ(getValue(TYPE::HP), 7);
	EXPECT_EQ(getValue(TYPE::DROWNING), 12);
	EXPECT_EQ(getValue(TYPE::DAMAGE_MP), 20);
	EXPECT_EQ(getValue(TYPE::ABSORBED_MP), 20);
	EXPECT_EQ(getValue(TYPE::FP_DAMAGE), 26);
	EXPECT_EQ(getValue(TYPE::NATURAL_FP), 27);
	EXPECT_EQ(getValue(LOG::SPELLATK), 1);
	EXPECT_EQ(getValue(LOG::PROCATKINSTANT), 93);
	EXPECT_EQ(getValue(LOG::SPELLATKDRAIN), 132);
	EXPECT_EQ(getValue(LOG::REGULAR), 191);
}

TEST_F(PacketTest, AbyssRankingPackets) {
	Ref<dao::AbyssRankDAO::RankingListLegion> legion =
		dao::AbyssRankDAO::RankingListLegion::create(1, 3, 500, "Legion", model::Race::ASMODIANS, 5, 0x100000001LL, 42);
	// Race.getRaceId: ELYOS 0, ASMODIANS 1
	EXPECT_EQ(dataOf(SM_ABYSS_RANKING_LEGIONS(1700000000, std::vector<Ptr<dao::AbyssRankDAO::RankingListLegion>>{legion}, model::Race::ASMODIANS)),
		Bytes().D(1).D(1700000000).D(1).D(1).H(1).D(1).D(3).D(500).D(1).C(5).D(42).Q(0x100000001LL).S("Legion", 40).data);
	EXPECT_EQ(dataOf(SM_ABYSS_RANKING_LEGIONS(99, model::Race::ELYOS)), Bytes().D(0).D(99).D(0).D(0).H(0).data);

	Ref<dao::AbyssRankDAO::RankingListPlayer> player = dao::AbyssRankDAO::RankingListPlayer::create(2, 4, 600, "Hero", model::Race::ELYOS, 65, 12,
		123456, 789, 0, model::PlayerClass::CLERIC, model::Gender::FEMALE, "Guild");
	// PlayerClass.CLERIC id 10, Gender.FEMALE 1; CHARNAME_MAX_LENGTH 25
	EXPECT_EQ(dataOf(SM_ABYSS_RANKING_PLAYERS(1000, std::vector<Ptr<dao::AbyssRankDAO::RankingListPlayer>>{player}, model::Race::ELYOS, 3, true)),
		Bytes().D(0).D(1000).D(3).D(0x7F).H(1).D(2).D(12).D(4).D(600).D(0).D(10).C(1).C(0).C(0).C(0).Q(123456).D(789).H(65).S("Hero", 25).S("Guild", 42)
			.data);
	EXPECT_EQ(dataOf(SM_ABYSS_RANKING_PLAYERS(5, model::Race::ASMODIANS)), Bytes().D(1).D(5).D(0).D(0).H(0).data);
}

TEST_F(PacketTest, BookmarkAndCooldownsAndEmotions) {
	Ref<dao::BookmarkDAO::Bookmark> bookmark = dao::BookmarkDAO::Bookmark::create("home", 110010000, 1.0f, 2.0f, 3.0f);
	EXPECT_EQ(dataOf(SM_GM_BOOKMARK_ADD(*bookmark)), Bytes().S("home").D(110010000).F(1.0f).F(2.0f).F(3.0f).data);

	const int64_t now = commons::utils::currentTimeMillis();
	Ref<model::items::ItemCooldown> running = model::items::ItemCooldown::create(now + 3600 * 1000LL, 60);
	Ref<model::items::ItemCooldown> expired = model::items::ItemCooldown::create(now - 5000, 30);
	std::unordered_map<int32_t, Ptr<model::items::ItemCooldown>> cooldowns{{7, running}, {3, expired}};
	std::vector<uint8_t> cooldownData = dataOf(SM_ITEM_COOLDOWN(cooldowns));
	// group 3 (expired: 0 s left), then group 7 with (reuseTime - now) / 1000 seconds, a second less if the clock ticked past a boundary
	std::vector<uint8_t> expected = Bytes().H(2).H(3).D(0).D(30).H(7).D(3600).D(60).data;
	std::vector<uint8_t> expectedLate = Bytes().H(2).H(3).D(0).D(30).H(7).D(3599).D(60).data;
	EXPECT_TRUE(cooldownData == expected || cooldownData == expectedLate);

	Ref<model::gameobjects::player::emotion::Emotion> emotion = model::gameobjects::player::emotion::Emotion::create(10, 0);
	EXPECT_EQ(dataOf(SM_EMOTION_LIST(int8_t{0}, std::vector<Ptr<model::gameobjects::player::emotion::Emotion>>{emotion})),
		Bytes().C(0).H(1).D(10).H(0).data); // expire time 0: never expires, 0 seconds
}

TEST_F(PacketTest, AtreianPassport) {
	Ref<model::account::PassportsList> passports = model::account::PassportsList::create();
	Ref<model::account::Passport> taken =
		model::account::Passport::create(3, true, commons::database::Timestamp(std::chrono::milliseconds(1700000000123LL)));
	Ref<model::account::Passport> available =
		model::account::Passport::create(4, false, commons::database::Timestamp(std::chrono::milliseconds(1700086400999LL)));
	passports->addPassport(*taken);
	passports->addPassport(*available);
	const commons::database::Date created{std::chrono::year{2024}, std::chrono::month{2}, std::chrono::day{29}};
	// RewardStatus ids: UPCOMING 0, AVAILABLE 1, TAKEN 2, EXPIRED 3; arrive date in seconds
	EXPECT_EQ(dataOf(SM_ATREIAN_PASSPORT(*passports, 7, created)),
		Bytes().H(2024).H(2).H(29).H(2).D(3).D(7).D(2).D(1700000000).D(4).D(7).D(1).D(1700086400).data);
	// a passport without arrive date: Java NullPointerException
	Ref<model::account::PassportsList> broken = model::account::PassportsList::create();
	Ref<model::account::Passport> noDate = model::account::Passport::create(5, false, std::nullopt);
	broken->addPassport(*noDate);
	SM_ATREIAN_PASSPORT brokenPacket(*broken, 0, created);
	EXPECT_THROW(brokenPacket.serialize(nullptr), runtime::NullPointerException);
}

TEST_F(PacketTest, HouseScripts) {
	Ref<runtime::Array<int8_t>> bytes = runtime::Array<int8_t>::make(3);
	(*bytes)[0].set(int8_t{1});
	(*bytes)[1].set(int8_t{-1});
	(*bytes)[2].set(int8_t{7});
	Ref<model::house::PlayerScript> withData = model::house::PlayerScript::create(2, bytes, 99);
	Ref<model::house::PlayerScript> empty = model::house::PlayerScript::create(5, nullptr, 0);
	// Java: script == null ? Collections.emptyList() : ...
	EXPECT_EQ(dataOf(SM_HOUSE_SCRIPTS(1100, Ptr<model::house::PlayerScript>())), Bytes().D(1100).H(0).data);
	try {
		static_cast<void>(withData->hasData());
	} catch (const runtime::UnportedException&) {
		GTEST_SKIP() << "PlayerScript::hasData is not ported yet (P5-11)";
	}
	const std::array<uint8_t, 3> content{1, 0xFF, 7};
	const std::array<uint8_t, 8> padding{0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD}; // SCRIPT_PADDING: eight times -51
	EXPECT_EQ(dataOf(SM_HOUSE_SCRIPTS(1100, std::vector<Ptr<model::house::PlayerScript>>{withData, empty})),
		Bytes().D(1100).H(2).C(2).H(8 + 3 + 8).D(3 + 8).D(99).B(content).B(padding).C(5).H(0).data);
	EXPECT_EQ(SM_HOUSE_SCRIPTS::DYNAMIC_BODY_PART_SIZE_CALCULATOR(*withData), 11 + 3 + 8);
	EXPECT_EQ(SM_HOUSE_SCRIPTS::DYNAMIC_BODY_PART_SIZE_CALCULATOR(*empty), 3);
}

TEST_F(PacketTest, TemplatePackets) {
	const model::templates::item::ItemTemplate* item =
		bindStatic<model::templates::item::ItemTemplate>(R"(<item_template id="152000001" desc="1000" level="10"/>)");
	// SM_CRAFT_UPDATE: skill 40009 always has a delay of 1000; the l10n string is "$" + (char) 2001 + (char) 0
	EXPECT_EQ(dataOf(SM_CRAFT_UPDATE(40009, item, 5, 6, 0, 1100, 3000)),
		Bytes().H(40009).C(0).D(152000001).D(5).D(6).D(1100).D(1000).D(1330048).S(l10nOf(1000)).data);
	EXPECT_EQ(dataOf(SM_CRAFT_UPDATE(40001, item, 5, 6, 2, 1100, 3000)), Bytes().H(40001).C(2).D(152000001).D(5).D(6).D(1100).D(3000).D(0).S("").data);
	EXPECT_EQ(dataOf(SM_CRAFT_UPDATE(40001, item, 5, 6, 4, 1100, 3000)),
		Bytes().H(40001).C(4).D(152000001).D(5).D(6).D(1100).D(3000).D(1330051).S("").data);
	EXPECT_EQ(dataOf(SM_CRAFT_UPDATE(40001, item, 5, 6, 7, 1100, 3000)),
		Bytes().H(40001).C(7).D(152000001).D(5).D(6).D(1100).D(3000).D(1330050).S(l10nOf(1000)).data);
	// action 8 writes no message block
	EXPECT_EQ(dataOf(SM_CRAFT_UPDATE(40001, item, 5, 6, 8, 1100, 3000)), Bytes().H(40001).C(8).D(152000001).D(5).D(6).D(1100).D(3000).data);

	const model::templates::gather::GatherableTemplate* gatherable =
		bindStatic<model::templates::gather::GatherableTemplate>(R"(<gatherable_template id="400010000" harvestSkill="30002"><materials/><exmaterials/></gatherable_template>)");
	const model::templates::gather::Material* material =
		bindStatic<model::templates::gather::Material>(R"(<material itemid="152000100" nameid="2500" rate="100"/>)");
	// action 0: STR_EXTRACT_GATHER_START_1_BASIC (1330077) with the material l10n
	EXPECT_EQ(dataOf(SM_GATHER_UPDATE(gatherable, material, 1, 2, 0, 800, 50)),
		Bytes().H(30002).C(0).D(152000100).D(1).D(2).D(800).D(50).D(1330077).S(l10nOf(2500)).data);
	EXPECT_EQ(dataOf(SM_GATHER_UPDATE(gatherable, material, 1, 2, 1, 800, 50)), Bytes().H(30002).C(1).D(152000100).D(1).D(2).D(800).D(50).D(0).S("").data);
	EXPECT_EQ(dataOf(SM_GATHER_UPDATE(gatherable, material, 1, 2, 5, 800, 50)),
		Bytes().H(30002).C(5).D(152000100).D(1).D(2).D(800).D(50).D(1330080).S(l10nOf(2500)).data); // CANCEL (1330080)
	EXPECT_EQ(dataOf(SM_GATHER_UPDATE(gatherable, material, 1, 2, 6, 800, 50)),
		Bytes().H(30002).C(6).D(152000100).D(1).D(2).D(800).D(50).D(1330078).S(l10nOf(2500)).data); // SUCCESS
	EXPECT_EQ(dataOf(SM_GATHER_UPDATE(gatherable, material, 1, 2, 7, 800, 50)),
		Bytes().H(30002).C(7).D(152000100).D(1).D(2).D(800).D(50).D(1330079).S(l10nOf(2500)).data); // FAIL
	EXPECT_EQ(dataOf(SM_GATHER_UPDATE(gatherable, material, 1, 2, 8, 800, 50)),
		Bytes().H(30002).C(8).D(152000100).D(1).D(2).D(800).D(50).D(1330074).S(l10nOf(2500)).data); // OCCUPIED_BY_OTHER
	EXPECT_EQ(dataOf(SM_GATHER_UPDATE(gatherable, material, 1, 2, 4, 800, 50)), Bytes().H(30002).C(4).D(152000100).D(1).D(2).D(800).D(50).data);

	// ResultedItem.afterUnmarshal requires the reward item templates as XmlIDs of the same load
	xml::LoadContext decomposables;
	decomposables.registerXmlId("100", *bindStatic<model::templates::item::ItemTemplate>(R"(<item_template id="100"/>)"));
	decomposables.registerXmlId("200", *bindStatic<model::templates::item::ItemTemplate>(R"(<item_template id="200"/>)"));
	const model::templates::item::ResultedItem* first =
		xml::bindString<model::templates::item::ResultedItem>(decomposables, R"(<item id="100" min_count="2"/>)").release();
	const model::templates::item::ResultedItem* second = xml::bindString<model::templates::item::ResultedItem>(decomposables, R"(<item id="200"/>)").release();
	EXPECT_EQ(dataOf(SM_FIRST_SHOW_DECOMPOSABLE(88, {first, second})),
		Bytes().D(88).D(0).C(2).C(0).D(100).D(2).C(0).C(0).C(0).C(1).C(1).D(200).D(1).C(0).C(0).C(0).C(1).data);
}

} // namespace
} // namespace aion::gameserver::network::aion::serverpackets::test
