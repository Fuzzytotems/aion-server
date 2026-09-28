// Golden bytes of the P4-17 server packets that write only their own scalar and string fields, written by hand from each Java writeImpl (field
// order and widths, UTF-16 strings with the terminator, Java null strings as the lone terminator) with the opcode of ServerPacketsOpcodes.java.

#include "SmLzTestSupport.h"

#include <array>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/model/EventTheme.h"
#include "aion/gameserver/model/animations/ObjectDeleteAnimation.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/gameobjects/PetAction.h"
#include "aion/gameserver/model/gameobjects/PetSpecialFunction.h"
#include "aion/gameserver/model/team/legion/LegionRank.h"
#include "aion/gameserver/model/templates/mail/MailMessage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_L2AUTH_LOGIN_CHECK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEARN_RECIPE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEAVE_GROUP_MEMBER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_EDIT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_LEAVE_MEMBER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_SEND_EMBLEM_DATA.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_NICKNAME.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_SELF_INTRO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_TITLE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LOOT_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MACRO_RESULT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MAY_LOGIN_INTO_GAME.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MEGAPHONE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_NEARBY_QUESTS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_NICKNAME_CHECK_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_NPC_ASSEMBLER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PACKAGE_INFO_NOTIFY.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PET.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PING_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PONG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_POSITION_SELF.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTIONNAIRE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_REPEAT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUIT_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECALLED_BY_OTHER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECEIVE_BIDS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECIPE_DELETE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECIPE_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECONNECT_KEY.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RESTORE_CHARACTER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RIFT_ANNOUNCE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SECURITY_TOKEN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SHOW_BRAND.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SIEGE_LOCATION_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_ACTIVATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATS_STATUS_UNK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATUPDATE_DP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATUPDATE_EXP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATUPDATE_HP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATUPDATE_MP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_OWNER_REMOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_PANEL_REMOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_USESKILL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TARGET_SELECTED.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TELEPORT_MAP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TIME_CHECK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TITLE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UI_SETTINGS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UNWRAP_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UPGRADE_ARCADE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_USE_OBJECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_VERSION_CHECK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_WINDSTREAM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_WINDSTREAM_ANNOUNCE.h"

namespace aion::gameserver::network::aion::serverpackets::testing {
namespace {

std::vector<uint8_t> bytesOf(AionServerPacket&& packet) {
	return serialized(packet);
}

TEST(ScalarPacketsTest, LegionTexts) {
	// SM_LEARN_RECIPE: writeD(recipeId) writeC(0)
	EXPECT_EQ(bytesOf(SM_LEARN_RECIPE(155000001)), Bytes().header(241).D(155000001).C(0).data);
	// SM_LEAVE_GROUP_MEMBER: writeD(0) writeC(0) writeD(0x3F) writeD(0) writeH(0)
	EXPECT_EQ(bytesOf(SM_LEAVE_GROUP_MEMBER()), Bytes().header(247).D(0).C(0).D(0x3F).D(0).H(0).data);
	// SM_LEGION_LEAVE_MEMBER: writeD(playerObjId) writeC(0) writeD(0) writeD(msgId) writeS(name) writeS(name1)
	EXPECT_EQ(bytesOf(SM_LEGION_LEAVE_MEMBER(1300001, 100001, "Kiko")), Bytes().header(112).D(100001).C(0).D(0).D(1300001).S("Kiko").S("").data);
	EXPECT_EQ(bytesOf(SM_LEGION_LEAVE_MEMBER(1300002, 7, "A", "B")), Bytes().header(112).D(7).C(0).D(0).D(1300002).S("A").S("B").data);
	EXPECT_EQ(bytesOf(SM_LEGION_UPDATE_NICKNAME(42, "Nick")), Bytes().header(11).D(42).S("Nick").data);
	EXPECT_EQ(bytesOf(SM_LEGION_UPDATE_SELF_INTRO(43, "\xC3\xA9t\xC3\xA9")), Bytes().header(119).D(43).S("\xC3\xA9t\xC3\xA9").data);
	// SM_LEGION_UPDATE_TITLE: writeD writeD writeS writeC(rank.getRankId()) - LegionRank ids 0..4
	EXPECT_EQ(bytesOf(SM_LEGION_UPDATE_TITLE(1, 2, "Guild", model::team::legion::LegionRank::CENTURION)),
		Bytes().header(114).D(1).D(2).S("Guild").C(2).data);
	EXPECT_EQ(bytesOf(SM_LEGION_UPDATE_TITLE(1, 0, "", model::team::legion::LegionRank::VOLUNTEER)), Bytes().header(114).D(1).D(0).S("").C(4).data);
	// SM_LEGION_SEND_EMBLEM_DATA: writeD(size) writeB(data)
	const std::vector<uint8_t> emblem{1, 2, 0xFF};
	EXPECT_EQ(bytesOf(SM_LEGION_SEND_EMBLEM_DATA(3, emblem)), Bytes().header(214).D(3).C(1).C(2).C(0xFF).data);
}

TEST(ScalarPacketsTest, LegionEditWithoutLegion) {
	// types that write no legion field: 0x06 disband time, 0x07/0x08 nothing, unknown types only the type byte
	EXPECT_EQ(bytesOf(SM_LEGION_EDIT(0x06, 1700000000)), Bytes().header(158).C(6).D(1700000000).data);
	EXPECT_EQ(bytesOf(SM_LEGION_EDIT(0x07)), Bytes().header(158).C(7).data);
	EXPECT_EQ(bytesOf(SM_LEGION_EDIT(0x08)), Bytes().header(158).C(8).data);
	EXPECT_EQ(bytesOf(SM_LEGION_EDIT(0x0A)), Bytes().header(158).C(0x0A).data);
}

TEST(ScalarPacketsTest, LoginAndSession) {
	EXPECT_EQ(bytesOf(SM_MAY_LOGIN_INTO_GAME()), Bytes().header(137).D(0).data);
	EXPECT_EQ(bytesOf(SM_NICKNAME_CHECK_RESPONSE(0x0A)), Bytes().header(233).C(0x0A).data);
	EXPECT_EQ(bytesOf(SM_PACKAGE_INFO_NOTIFY()), Bytes().header(266).H(1).C(3).D(0).data);
	EXPECT_EQ(bytesOf(SM_PING_RESPONSE()), Bytes().header(128).C(4).data);
	EXPECT_EQ(bytesOf(SM_PONG()), Bytes().header(142).C(0).C(0).data);
	EXPECT_EQ(bytesOf(SM_QUIT_RESPONSE()), Bytes().header(98).D(1).C(0).D(-1).data);
	EXPECT_EQ(bytesOf(SM_QUIT_RESPONSE(true)), Bytes().header(98).D(2).C(0).D(-1).data);
	EXPECT_EQ(bytesOf(SM_RECONNECT_KEY(-559038737)), Bytes().header(255).C(0).D(-559038737).data);
	EXPECT_EQ(bytesOf(SM_RESTORE_CHARACTER(100001, true)), Bytes().header(203).D(0).D(100001).data);
	EXPECT_EQ(bytesOf(SM_RESTORE_CHARACTER(100001, false)), Bytes().header(203).D(0x10).D(100001).data);
	// SM_SECURITY_TOKEN: writeC(0) writeB(token) writeB(new byte[token.length])
	const std::vector<uint8_t> token{0xAB, 0xCD};
	EXPECT_EQ(bytesOf(SM_SECURITY_TOKEN(token)), Bytes().header(152).C(0).C(0xAB).C(0xCD).zeros(2).data);
	// SM_UI_SETTINGS: writeC(type) writeH(0x1C00) writeB(data) and zero padding up to 0x1C00 bytes
	const std::vector<uint8_t> settings{1, 2, 3};
	EXPECT_EQ(bytesOf(SM_UI_SETTINGS(settings, 1)), Bytes().header(30).C(1).H(0x1C00).C(1).C(2).C(3).zeros(0x1C00 - 3).data);
	const std::vector<uint8_t> full(0x1C00, uint8_t{7});
	EXPECT_EQ(bytesOf(SM_UI_SETTINGS(full, 2)), Bytes().header(30).C(2).H(0x1C00).raw(full).data);
	// SM_TIME_CHECK: writeD(serverUpTime) writeD(nanoTime); the uptime is the process uptime in milliseconds
	SM_TIME_CHECK timeCheck(12345);
	std::vector<uint8_t> timeCheckBytes = serialized(timeCheck);
	ASSERT_EQ(timeCheckBytes.size(), 5u + 8u);
	int32_t upTime = timeCheckBytes[5] | timeCheckBytes[6] << 8 | timeCheckBytes[7] << 16 | timeCheckBytes[8] << 24;
	EXPECT_GE(upTime, 0);
	EXPECT_EQ(std::vector<uint8_t>(timeCheckBytes.begin() + 9, timeCheckBytes.end()), Bytes().D(12345).data);
}

TEST(ScalarPacketsTest, VersionCheckWithAnotherVersionAnswersOne) {
	// version != INTERNAL_VERSION: only writeC(1) after the config reads
	EXPECT_EQ(bytesOf(SM_VERSION_CHECK(206, model::EventTheme::NONE)), Bytes().header(0).C(1).data);
}

TEST(ScalarPacketsTest, MacroResultStaticPackets) {
	// the two shared static packets: serialize only reads code (hub-headers.md §12)
	EXPECT_EQ(serialized(SM_MACRO_RESULT::SM_MACRO_CREATED), Bytes().header(232).C(0).data);
	EXPECT_EQ(serialized(SM_MACRO_RESULT::SM_MACRO_DELETED), Bytes().header(232).C(1).data);
	EXPECT_EQ(serialized(SM_MACRO_RESULT::SM_MACRO_CREATED), Bytes().header(232).C(0).data) << "a second serialization is identical";
}

TEST(ScalarPacketsTest, Megaphone) {
	// writeS(senderName) writeS(message) writeD(itemId) writeC(senderFaction.id): NONE -1, ELYOS 0, ASMODIANS 1
	EXPECT_EQ(bytesOf(SM_MEGAPHONE(SM_MEGAPHONE::FactionLabel::NONE, "GM", "hi", 188100000)),
		Bytes().header(285).S("GM").S("hi").D(188100000).C(-1).data);
	EXPECT_EQ(bytesOf(SM_MEGAPHONE(SM_MEGAPHONE::FactionLabel::ASMODIANS, "A", "", 1)), Bytes().header(285).S("A").S("").D(1).C(1).data);
	EXPECT_EQ(bytesOf(SM_MEGAPHONE(SM_MEGAPHONE::FactionLabel::ELYOS, "E", "x", 2)), Bytes().header(285).S("E").S("x").D(2).C(0).data);
}

TEST(ScalarPacketsTest, MotionActions) {
	// action 2: writeH(motionId) writeD(remainingTime); 5: writeH writeC(type); 6: writeH; 7 with an empty map: 5x writeH(0); 1 with no motion
	EXPECT_EQ(bytesOf(SM_MOTION(int16_t{3}, int32_t{3600})), Bytes().header(148).C(2).H(3).D(3600).data);
	EXPECT_EQ(bytesOf(SM_MOTION(int16_t{4}, int8_t{2})), Bytes().header(148).C(5).H(4).C(2).data);
	EXPECT_EQ(bytesOf(SM_MOTION(int16_t{5})), Bytes().header(148).C(6).H(5).data);
	EXPECT_EQ(bytesOf(SM_MOTION(100001, std::unordered_map<int32_t, runtime::Ptr<model::gameobjects::player::motion::Motion>>{})),
		Bytes().header(148).C(7).D(100001).H(0).H(0).H(0).H(0).H(0).data);
	EXPECT_EQ(bytesOf(SM_MOTION(std::vector<runtime::Ptr<model::gameobjects::player::motion::Motion>>{})), Bytes().header(148).C(1).H(0).data);
}

TEST(ScalarPacketsTest, NearbyQuestsInJavaHashMapOrder) {
	// writeC(0) writeH(-size & 0xFFFF), then the keys of the HashMap (quest id | 1 << 17 for a value > 0). Java HashMap<Integer, Integer> with 16
	// buckets iterates by key & 15 (Integer.hashCode spreads no high bits below 65536), and a bucket in insertion (here: ascending) order.
	const std::unordered_map<int32_t, int32_t> quests{{1017, 0}, {1002, 1}, {1033, 0}};
	// buckets: 1002 & 15 = 10, 1017 & 15 = 9, 1033 & 15 = 9 -> 1017, 1033 (bucket 9), 1002 (bucket 10)
	EXPECT_EQ(bytesOf(SM_NEARBY_QUESTS(quests)), Bytes().header(127).C(0).H(0xFFFD).D(1017).D(1033).D(1002 | 1 << 17).data);
	EXPECT_EQ(bytesOf(SM_NEARBY_QUESTS({})), Bytes().header(127).C(0).H(0).data);
}

TEST(ScalarPacketsTest, NpcAssemblerWithoutNpc) {
	EXPECT_EQ(bytesOf(SM_NPC_ASSEMBLER(nullptr)), Bytes().header(10).D(0).data);
}

TEST(ScalarPacketsTest, PetActionsWithoutModelObjects) {
	using model::gameobjects::PetAction;
	// writeH(action.getActionId()): RENAME writeD(petObjectId) writeS(petName); DISMISS writeD writeC(animation id)
	EXPECT_EQ(bytesOf(SM_PET(700001, std::string_view("Poppy"))), Bytes().header(101).H(10).D(700001).S("Poppy").data);
	EXPECT_EQ(bytesOf(SM_PET(700001, model::animations::ObjectDeleteAnimation::FADE_OUT)), Bytes().header(101).H(4).D(700001).C(1).data);
	// SPECIAL_FUNCTION: writeC(subType) then per subType: 2 doping (writeC(dopeAction) and its slots), 3 looting, 4 autosell
	EXPECT_EQ(bytesOf(SM_PET(0, 162000001, 3)), Bytes().header(101).H(13).C(2).C(0).D(162000001).D(3).data);
	EXPECT_EQ(bytesOf(SM_PET(1, 162000001, 3)), Bytes().header(101).H(13).C(2).C(1).D(3).data);
	EXPECT_EQ(bytesOf(SM_PET(2, 5, 4)), Bytes().header(101).H(13).C(2).C(2).D(4).D(5).data);
	EXPECT_EQ(bytesOf(SM_PET(3, 162000002, 0)), Bytes().header(101).H(13).C(2).C(3).D(162000002).data);
	EXPECT_EQ(bytesOf(SM_PET(model::gameobjects::PetSpecialFunction::AUTOLOOT, true)), Bytes().header(101).H(13).C(3).C(0).C(1).data);
	EXPECT_EQ(bytesOf(SM_PET(model::gameobjects::PetSpecialFunction::AUTOLOOT, false)), Bytes().header(101).H(13).C(3).C(0).C(0).data);
	EXPECT_EQ(bytesOf(SM_PET(model::gameobjects::PetSpecialFunction::AUTOLOOT, true, 300001)), Bytes().header(101).H(13).C(3).C(1).D(300001).data);
	EXPECT_EQ(bytesOf(SM_PET(model::gameobjects::PetSpecialFunction::AUTOLOOT, false, 300001)), Bytes().header(101).H(13).C(3).C(2).D(300001).data);
	EXPECT_EQ(bytesOf(SM_PET(model::gameobjects::PetSpecialFunction::AUTOSELL, true)), Bytes().header(101).H(13).C(4).C(0).C(1).data);
	// an action without a case writes only the action id
	EXPECT_EQ(bytesOf(SM_PET(PetAction::UNKNOWN)), Bytes().header(101).H(255).data);
}

TEST(ScalarPacketsTest, PositionAndMovementSnapshots) {
	EXPECT_EQ(bytesOf(SM_POSITION_SELF(1.5f, -2.25f, 100.0f, int8_t{-1})), Bytes().header(21).F(1.5f).F(-2.25f).F(100.0f).C(0xFF).data);
	EXPECT_EQ(bytesOf(SM_TARGET_SELECTED(nullptr)), Bytes().header(41).D(0).H(0).D(0).D(0).D(0).D(0).data);
	EXPECT_EQ(bytesOf(SM_TELEPORT_MAP(300001, 71)), Bytes().header(196).D(300001).H(71).data);
	EXPECT_EQ(bytesOf(SM_WINDSTREAM(12, 1)), Bytes().header(163).D(12).C(1).data);
	EXPECT_EQ(bytesOf(SM_WINDSTREAM_ANNOUNCE(1, 210050000, 3, 1)), Bytes().header(164).D(1).D(210050000).D(3).C(1).data);
	EXPECT_EQ(bytesOf(SM_USE_OBJECT(100001, 300001, 3000, 1)), Bytes().header(197).D(100001).D(300001).D(3000).C(1).data);
	EXPECT_EQ(bytesOf(SM_UNWRAP_ITEM(400001, 2)), Bytes().header(289).D(400001).C(2).data);
}

TEST(ScalarPacketsTest, QuestTexts) {
	// SM_QUESTIONNAIRE: writeD writeC writeC writeH(html.length() * 2) writeS(html) - the length counts UTF-16 units
	EXPECT_EQ(bytesOf(SM_QUESTIONNAIRE(9, int8_t{1}, int8_t{2}, "<p>\xE2\x82\xAC</p>")),
		Bytes().header(191).D(9).C(1).C(2).H(8 * 2).S("<p>\xE2\x82\xAC</p>").data);
	EXPECT_EQ(bytesOf(SM_QUEST_REPEAT({1000, 2000})), Bytes().header(290).H(2).D(1000).D(2000).data);
	// SM_QUEST_ACTION without quest data would read DataManager.QUEST_DATA (unpublished: NullPointerException, like Java)
}

TEST(ScalarPacketsTest, QuestionWindowParameters) {
	// writeD(code), three writeS (String.valueOf of the parameter, null for a missing one), writeD(0), writeC(range > 0), writeD(sender), writeD(range)
	EXPECT_EQ(bytesOf(SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_DUEL_DO_YOU_ACCEPT_REQUEST, 100001, 20, "Kiko")),
		Bytes().header(52).D(50028).S("Kiko").S("").S("").D(0).C(1).D(100001).D(20).data);
	EXPECT_EQ(bytesOf(SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_ASK_USE_ARTIFACT, 0, 0, int32_t{-5}, int64_t{10000000000LL}, "x")),
		Bytes().header(52).D(160016).S("-5").S("10000000000").S("x").D(0).C(0).D(0).D(0).data);
	EXPECT_THROW(SM_QUESTION_WINDOW(1, 0, 0, "a", "b", "c", "d"), commons::utils::IllegalArgumentException);
}

TEST(ScalarPacketsTest, RecalledByOther) {
	// writeC(casterName == null ? 1 : 0) writeS(casterName) writeH(skillId) writeH(seconds)
	EXPECT_EQ(bytesOf(SM_RECALLED_BY_OTHER()), Bytes().header(69).C(1).S("").H(0).H(0).data);
	EXPECT_EQ(bytesOf(SM_RECALLED_BY_OTHER(std::string_view("Caster"), 1606, 30)), Bytes().header(69).C(0).S("Caster").H(1606).H(30).data);
	EXPECT_EQ(bytesOf(SM_RECALLED_BY_OTHER(std::string_view(""), 1, 2)), Bytes().header(69).C(0).S("").H(1).H(2).data) << "empty is not null";
}

TEST(ScalarPacketsTest, RecipesAndBids) {
	EXPECT_EQ(bytesOf(SM_RECEIVE_BIDS(3)), Bytes().header(259).D(3).data);
	EXPECT_EQ(bytesOf(SM_RECIPE_DELETE(155000002)), Bytes().header(242).D(155000002).data);
	// SM_RECIPE_LIST: writeH(size), then per id of the HashSet writeD(id) writeC(0); ids 17 and 1 share no bucket: 1 (bucket 1), 17 (bucket 1)...
	// 1 & 15 = 1, 17 & 15 = 1 share bucket 1 (ascending insertion keeps 1 before 17), 2 & 15 = 2
	EXPECT_EQ(bytesOf(SM_RECIPE_LIST(std::unordered_set<int32_t>{17, 2, 1})), Bytes().header(207).H(3).D(1).C(0).D(17).C(0).D(2).C(0).data);
}

TEST(ScalarPacketsTest, RiftAnnounceWithoutRift) {
	// action 0: writeH(1 + values * 4) writeC(0) and the TreeMap values in key order; 1: writeH(9) writeC(1) writeD writeD; 4: writeH(5) writeC(4) writeD
	EXPECT_EQ(bytesOf(SM_RIFT_ANNOUNCE(std::map<int32_t, int32_t>{{2, 20}, {1, 10}})), Bytes().header(236).H(9).C(0).D(10).D(20).data);
	EXPECT_EQ(bytesOf(SM_RIFT_ANNOUNCE(true, false)), Bytes().header(236).H(9).C(1).D(1).D(0).data);
	EXPECT_EQ(bytesOf(SM_RIFT_ANNOUNCE(300001)), Bytes().header(236).H(5).C(4).D(300001).data);
}

TEST(ScalarPacketsTest, ShowBrand) {
	// writeH(size) and per entry writeD(1) writeD(iconId) writeD(targetObjectId); an empty map resets the brands 0..15
	EXPECT_EQ(bytesOf(SM_SHOW_BRAND(3, 100001)), Bytes().header(249).H(1).D(1).D(3).D(100001).data);
	Bytes reset;
	reset.header(249).H(16);
	for (int32_t brand = 0; brand < 16; brand++)
		reset.D(1).D(brand).D(0);
	EXPECT_EQ(bytesOf(SM_SHOW_BRAND(std::unordered_map<int32_t, int32_t>{})), reset.data);
	EXPECT_EQ(bytesOf(SM_SHOW_BRAND(std::unordered_map<int32_t, int32_t>{{5, 7}, {1, 8}})), Bytes().header(249).H(2).D(1).D(1).D(8).D(1).D(5).D(7).data);
}

TEST(ScalarPacketsTest, SkillsAndStats) {
	EXPECT_EQ(bytesOf(SM_SIEGE_LOCATION_STATE(1011, 1)), Bytes().header(210).D(1011).C(1).data);
	EXPECT_EQ(bytesOf(SM_SKILL_ACTIVATION(1001, true)), Bytes().header(46).H(1001).D(0).C(1).data);
	EXPECT_EQ(bytesOf(SM_SKILL_ACTIVATION(1001, false)), Bytes().header(46).H(1001).D(0).C(0).data);
	EXPECT_EQ(bytesOf(SM_SKILL_ACTIVATION(1002)), Bytes().header(46).H(1002).D(1).C(1).data);
	// SM_STATS_STATUS_UNK: writeD(points) writeC(1) writeC(lvl == 50 ? 1 : 2) writeD(lvl) writeD(lvl) writeD(lvl == 50 ? 1 : 0) writeC(0)
	EXPECT_EQ(bytesOf(SM_STATS_STATUS_UNK(50, 7)), Bytes().header(276).D(7).C(1).C(1).D(50).D(50).D(1).C(0).data);
	EXPECT_EQ(bytesOf(SM_STATS_STATUS_UNK(49, 7)), Bytes().header(276).D(7).C(1).C(2).D(49).D(49).D(0).C(0).data);
	EXPECT_EQ(bytesOf(SM_STATUPDATE_DP(4000)), Bytes().header(6).H(4000).data);
	EXPECT_EQ(bytesOf(SM_STATUPDATE_EXP(1, 2, std::numeric_limits<int64_t>::max(), -4, 5)),
		Bytes().header(8).Q(1).Q(2).Q(std::numeric_limits<int64_t>::max()).Q(-4).Q(5).data);
	EXPECT_EQ(bytesOf(SM_STATUPDATE_HP(800, 1000)), Bytes().header(3).D(800).D(1000).data);
	EXPECT_EQ(bytesOf(SM_STATUPDATE_MP(80, 100)), Bytes().header(4).D(80).D(100).data);
	EXPECT_EQ(bytesOf(SM_SUMMON_OWNER_REMOVE(500001)), Bytes().header(154).D(500001).data);
	EXPECT_EQ(bytesOf(SM_SUMMON_PANEL_REMOVE(0)), Bytes().header(73).H(0).C(0).data);
	EXPECT_EQ(bytesOf(SM_SUMMON_PANEL_REMOVE(1234)), Bytes().header(73).H(1234).C(1).data);
	EXPECT_EQ(bytesOf(SM_SUMMON_USESKILL(500001, 1234, 3, 600001)), Bytes().header(162).D(500001).H(1234).C(3).D(600001).data);
}

TEST(ScalarPacketsTest, TitleInfoWithoutPlayer) {
	// 1: writeH(titleId); 4: writeH(flag); 6 (action, bonusTitleId): writeH(bonusTitleId); other actions write only the action
	EXPECT_EQ(bytesOf(SM_TITLE_INFO(int32_t{77})), Bytes().header(176).C(1).H(77).data);
	EXPECT_EQ(bytesOf(SM_TITLE_INFO(true)), Bytes().header(176).C(4).H(1).data);
	EXPECT_EQ(bytesOf(SM_TITLE_INFO(false)), Bytes().header(176).C(4).H(0).data);
	EXPECT_EQ(bytesOf(SM_TITLE_INFO(6, 88)), Bytes().header(176).C(6).H(88).data);
	EXPECT_EQ(bytesOf(SM_TITLE_INFO(2, 88)), Bytes().header(176).C(2).data);
	// every enter world of a character without a bonus title sends this packet: PlayerEnterWorldService.java:240 calls
	// TitleList.setBonusTitle(pcd.getBonusTitleId()) for every id but 0, and players.bonus_title_id defaults to -1 (aion_gs.sql:931), so
	// TitleList.java:88 writes action 6 with the short -1 (0xFFFF), not 0
	EXPECT_EQ(bytesOf(SM_TITLE_INFO(6, -1)), Bytes().header(176).C(6).H(-1).data);
	EXPECT_EQ(bytesOf(SM_TITLE_INFO(int32_t{-1})), Bytes().header(176).C(1).H(-1).data) << "the display title of a new character";
}

TEST(ScalarPacketsTest, UpgradeArcadeWithoutProgress) {
	// 0: writeD(showIcon); 2: writeC(1); 6: writeD writeQ; 7: writeD; 8/9 (action, disableWindow): writeC(disableWindow) for 8 only; 10: empty lists
	EXPECT_EQ(bytesOf(SM_UPGRADE_ARCADE(true)), Bytes().header(298).C(0).D(1).data);
	EXPECT_EQ(bytesOf(SM_UPGRADE_ARCADE()), Bytes().header(298).C(2).C(1).data);
	EXPECT_EQ(bytesOf(SM_UPGRADE_ARCADE(186000237, int64_t{5})), Bytes().header(298).C(6).D(186000237).Q(5).data);
	EXPECT_EQ(bytesOf(SM_UPGRADE_ARCADE(int32_t{30})), Bytes().header(298).C(7).D(30).data);
	EXPECT_EQ(bytesOf(SM_UPGRADE_ARCADE(8, true)), Bytes().header(298).C(8).C(1).data);
	EXPECT_EQ(bytesOf(SM_UPGRADE_ARCADE(9, true)), Bytes().header(298).C(9).data);
	EXPECT_EQ(bytesOf(SM_UPGRADE_ARCADE(std::vector<const model::templates::event::upgradearcade::ArcadeRewards*>{})), Bytes().header(298).C(10).data);
}

TEST(ScalarPacketsTest, LootStatusWithoutLootEffect) {
	// only LOOT_ENABLE reads the drop registration; the other states write lootEffectId 0 (writeD(target) writeC(status id) writeD(effect))
	EXPECT_EQ(bytesOf(SM_LOOT_STATUS(300001, SM_LOOT_STATUS::Status::LOOT_DISABLE)), Bytes().header(205).D(300001).C(1).D(0).data);
	EXPECT_EQ(bytesOf(SM_LOOT_STATUS(300001, SM_LOOT_STATUS::Status::CLOSE_DROP_LIST)), Bytes().header(205).D(300001).C(3).D(0).data);
}

TEST(ScalarPacketsTest, PerRecipientPacketsWithoutConnectionThrow) {
	// Java dereferences con: a SHARED serialization (no connection) throws NullPointerException naming the packet (runtime-architecture.md §8.7)
	SM_PLAY_MOVIE movie(true, 0, 1000, 1, true);
	EXPECT_EQ(movie.recipients(), AionServerPacket::Recipients::PER_RECIPIENT);
	EXPECT_THROW(static_cast<void>(movie.serialize(nullptr)), runtime::NullPointerException);
}

} // namespace
} // namespace aion::gameserver::network::aion::serverpackets::testing
