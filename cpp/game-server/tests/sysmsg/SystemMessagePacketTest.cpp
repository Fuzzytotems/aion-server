// SM_SYSTEM_MESSAGE.writeImpl: golden bytes (hand-derived from the Java writeImpl) for five messages with int, long, byte and float parameters,
// a chat type with sender and special parameters, and the Java string forms of object parameters (toJavaString); the stand-ins the hand-written
// factories use (ChatType ids, ChatUtil.l10n, Race and AbyssRankEnum l10n ids, the %SubZone parameter), checked against the Java sources.

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/network/aion/SerializedBody.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/detail/SystemMessageL10n.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::network::aion::serverpackets {
namespace {

/** Little endian bytes as Java's BaseServerPacket writes them */
class Bytes {
public:
	Bytes& C(int32_t value) {
		data.push_back(static_cast<uint8_t>(value));
		return *this;
	}
	Bytes& H(int32_t value) { return C(value).C(value >> 8); }
	Bytes& D(int32_t value) { return H(value).H(value >> 16); }
	Bytes& S(std::string_view text) {
		for (char16_t c : commons::utils::StringUtils::toUtf16(text))
			H(c);
		return H(0);
	}
	/** the header of an SM_SYSTEM_MESSAGE (opcode 25): (25 + 207) ^ 0xDF = 0x37, 0x44, ~0x37 */
	Bytes& header() { return H(0x37).C(0x44).H(0xFFC8); }

	std::vector<uint8_t> data;
};

std::vector<uint8_t> serialized(SM_SYSTEM_MESSAGE packet) {
	return *packet.serialize(nullptr).bytes;
}

TEST(SystemMessagePacketTest, IntAndStringParameters) {
	// Java: writeC(chatType) writeC(0) writeD(senderObjId) writeD(msgId) writeC(params.length) writeS(param)... writeC(specialParams.length)
	EXPECT_EQ(serialized(SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_MY_ATTACK(-42, "Kromede")),
		(Bytes().header().C(25).C(0).D(0).D(1200000).C(2).S("-42").S("Kromede").C(0).data)); // GOLDEN_YELLOW is chat type 25
}

TEST(SystemMessagePacketTest, LongParameter) {
	EXPECT_EQ(serialized(SM_SYSTEM_MESSAGE::STR_MSG_GET_POLL_REWARD_ITEM_MULTI(std::numeric_limits<int64_t>::min(), "Kinah")),
		(Bytes().header().C(25).C(0).D(0).D(1300946).C(2).S("-9223372036854775808").S("Kinah").C(0).data));
}

TEST(SystemMessagePacketTest, ByteParameter) {
	EXPECT_EQ(serialized(SM_SYSTEM_MESSAGE::STR_HOUR(int8_t{-128})), (Bytes().header().C(25).C(0).D(0).D(1300533).C(1).S("-128").C(0).data));
}

TEST(SystemMessagePacketTest, FloatParameters) {
	// Java Float.toString: 1.5, 100.0, -0.25, 3.4028235E38
	EXPECT_EQ(serialized(SM_SYSTEM_MESSAGE::STR_CMD_LOCATION_DESC(210010000, 1.5f, 100.0f, -0.25f)),
		(Bytes().header().C(25).C(0).D(0).D(230038).C(4).S("210010000").S("1.5").S("100.0").S("-0.25").C(0).data));
	EXPECT_EQ(serialized(SM_SYSTEM_MESSAGE(1300000, std::numeric_limits<float>::max(), 0.001f)),
		(Bytes().header().C(25).C(0).D(0).D(1300000).C(2).S("3.4028235E38").S("0.001").C(0).data));
}

TEST(SystemMessagePacketTest, ChatTypeSenderlessAndSpecialParameters) {
	SM_SYSTEM_MESSAGE packet(model::ChatType::BRIGHT_YELLOW_CENTER, nullptr, 1400000, std::vector<std::string>{"a", ""}, {"[%target]"});
	EXPECT_EQ(packet.getId(), 1400000);
	EXPECT_EQ(serialized(std::move(packet)), (Bytes().header().C(36).C(0).D(0).D(1400000).C(2).S("a").S("").C(1).S("[%target]").data));
	EXPECT_EQ(serialized(SM_SYSTEM_MESSAGE(model::ChatType::NPC, nullptr, 1111, std::vector<std::string>{})),
		(Bytes().header().C(1).C(0).D(0).D(1111).C(0).C(0).data));
}

struct Named {
	std::string toString() { return "Named[7]"; }
};

TEST(SystemMessagePacketTest, JavaStringsOfOtherParameterTypes) {
	EXPECT_EQ(SM_SYSTEM_MESSAGE::toJavaString(true), "true");
	EXPECT_EQ(SM_SYSTEM_MESSAGE::toJavaString(false), "false");
	EXPECT_EQ(SM_SYSTEM_MESSAGE::toJavaString(int16_t{-32768}), "-32768");
	EXPECT_EQ(SM_SYSTEM_MESSAGE::toJavaString(u'é'), "\xC3\xA9");
	EXPECT_EQ(SM_SYSTEM_MESSAGE::toJavaString(model::ChatType::BRIGHT_YELLOW), "BRIGHT_YELLOW"); // Java Enum.toString
	Named named;
	EXPECT_EQ(SM_SYSTEM_MESSAGE::toJavaString(named), "Named[7]");
	EXPECT_EQ(SM_SYSTEM_MESSAGE(1, true, named, model::ChatType::NPC, int16_t{5}).getParams(),
		(std::vector<std::string>{"true", "Named[7]", "NPC", "5"}));
}

TEST(SystemMessageL10nTest, ChatTypeIdsOfAllConstants) {
	using enum model::ChatType;
	// ChatType.java constructor arguments
	const std::vector<std::pair<model::ChatType, int32_t>> expected{{NORMAL, 0}, {NPC, 1}, {SHOUT, 3}, {WHISPER, 4}, {GROUP, 5}, {ALLIANCE, 6},
		{GROUP_LEADER, 7}, {LEAGUE, 8}, {LEAGUE_ALERT, 9}, {LEGION, 10}, {CH1, 14}, {CH2, 15}, {CH3, 16}, {CH4, 17}, {CH5, 18}, {CH6, 19}, {CH7, 20},
		{CH8, 21}, {CH9, 22}, {CH10, 23}, {COMMAND, 24}, {GOLDEN_YELLOW, 25}, {GM_CHAT, 27}, {WHITE, 31}, {YELLOW, 32}, {BRIGHT_YELLOW, 33},
		{WHITE_CENTER, 34}, {YELLOW_CENTER, 35}, {BRIGHT_YELLOW_CENTER, 36}};
	ASSERT_EQ(expected.size(), xml::EnumTraits<model::ChatType>::names.size());
	for (const auto& [chatType, id] : expected)
		EXPECT_EQ(network::detail::chatTypeIdOf(chatType), id) << xml::EnumTraits<model::ChatType>::names[static_cast<size_t>(chatType)];
}

TEST(SystemMessageL10nTest, L10nStrings) {
	// ChatUtil.l10n: "$" + (char) (id * 2 + 1 & 0xFFFF) + (char) (id * 2 + 1 >>> 16); null for 0
	EXPECT_EQ(network::detail::l10n(0), "");
	EXPECT_EQ(commons::utils::StringUtils::toUtf16(network::detail::l10n(900240)), (std::u16string{u'$', char16_t(0x7921), char16_t(0x001B)})); // 1800481
	EXPECT_EQ(commons::utils::StringUtils::toUtf16(network::detail::l10n(901250)), (std::u16string{u'$', char16_t(0x8105), char16_t(0x001B)})); // 1802501
	// the shift keeps the low 32 bits: 0x7FFFFFFF << 1 | 1 = 0xFFFFFFFF
	EXPECT_EQ(commons::utils::StringUtils::toUtf16(network::detail::l10n(0x7FFFFFFF)), (std::u16string{u'$', char16_t(0xFFFF), char16_t(0xFFFF)}));
	// C++ only (docs/deviations/P4-06.md): a lone surrogate (27648 * 2 + 1 = 0xD801) becomes U+FFFD
	EXPECT_EQ(network::detail::l10n(27648), std::string("$\xEF\xBF\xBD") + std::string(1, '\0'));

	// Race.java: ELYOS(0, 900240), ASMODIANS(1, 900241); AbyssRankEnum.getRankL10n: (ELYOS ? 901215 : 901233) + ordinal
	EXPECT_EQ(network::detail::raceL10nIdOf(model::Race::ELYOS), 900240);
	EXPECT_EQ(network::detail::raceL10nIdOf(model::Race::ASMODIANS), 900241);
	EXPECT_EQ(network::detail::raceL10nIdOf(model::Race::NPC), 0);
	EXPECT_EQ(network::detail::rankL10nIdOf(model::Race::ELYOS, utils::stats::AbyssRankEnum::GRADE9_SOLDIER), 901215);
	EXPECT_EQ(network::detail::rankL10nIdOf(model::Race::ELYOS, utils::stats::AbyssRankEnum::SUPREME_COMMANDER), 901232);
	EXPECT_EQ(network::detail::rankL10nIdOf(model::Race::ASMODIANS, utils::stats::AbyssRankEnum::GRADE9_SOLDIER), 901233);
	EXPECT_EQ(network::detail::rankL10nIdOf(model::Race::ASMODIANS, utils::stats::AbyssRankEnum::SUPREME_COMMANDER), 901250);

	// "%SubZone:" + mapId + " " + x + " " + y + " " + z with Java Float.toString
	EXPECT_EQ(network::detail::subZoneOf(400010000, 1.5f, 100.0f, -0.25f), "%SubZone:400010000 1.5 100.0 -0.25");
}

TEST(SystemMessageL10nTest, RankerDieMessageBytes) {
	// the parameters STR_ABYSS_ORDER_RANKER_DIE(victim, zoneName) builds for an Asmodian supreme commander
	SM_SYSTEM_MESSAGE packet(1400023,
		std::vector<std::string>{network::detail::l10n(network::detail::raceL10nIdOf(model::Race::ASMODIANS)),
			network::detail::l10n(network::detail::rankL10nIdOf(model::Race::ASMODIANS, utils::stats::AbyssRankEnum::SUPREME_COMMANDER)), "Victim",
			network::detail::subZoneOf(220070000, 2.0f, 3.5f, 4.0f)});
	// 900241 * 2 + 1 = 1800483 = 0x1B7923; 901250 * 2 + 1 = 0x1B8105
	EXPECT_EQ(serialized(std::move(packet)), (Bytes()
												 .header()
												 .C(25)
												 .C(0)
												 .D(0)
												 .D(1400023)
												 .C(4)
												 .H('$')
												 .H(0x7923)
												 .H(0x1B)
												 .H(0)
												 .H('$')
												 .H(0x8105)
												 .H(0x1B)
												 .H(0)
												 .S("Victim")
												 .S("%SubZone:220070000 2.0 3.5 4.0")
												 .C(0)
												 .data));
}

} // namespace
} // namespace aion::gameserver::network::aion::serverpackets
