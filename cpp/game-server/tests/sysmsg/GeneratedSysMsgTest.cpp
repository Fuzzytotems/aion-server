#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

// the real packet (chunk P4-06); the generated definitions SM_SYSTEM_MESSAGE.gen0..7.cpp are compiled into aion_gs_sysmsg
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::network::aion::serverpackets {
namespace {

using Params = std::vector<std::string>;

TEST(GeneratedSysMsgTest, PlainFactoriesPassTheirParametersInOrder) {
	// Java: STR_MSG_COMBAT_MY_ATTACK(int num1, String value0) { return new SM_SYSTEM_MESSAGE(1200000, num1, value0); }
	auto msg = SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_MY_ATTACK(-42, "Kromede");
	EXPECT_EQ(msg.getId(), 1200000);
	EXPECT_EQ(msg.getParams(), (Params{"-42", "Kromede"}));

	// Java: STR_MSG_GET_POLL_REWARD_ITEM_MULTI(long num1, String value0)
	msg = SM_SYSTEM_MESSAGE::STR_MSG_GET_POLL_REWARD_ITEM_MULTI(std::numeric_limits<int64_t>::min(), "x");
	EXPECT_EQ(msg.getId(), 1300946);
	EXPECT_EQ(msg.getParams(), (Params{"-9223372036854775808", "x"}));

	// Java: STR_HOUR(byte num0) - Byte.toString is signed
	msg = SM_SYSTEM_MESSAGE::STR_HOUR(int8_t{-128});
	EXPECT_EQ(msg.getId(), 1300533);
	EXPECT_EQ(msg.getParams(), (Params{"-128"}));

	// Java: STR_CMD_LOCATION_DESC(int worldId, float x, float y, float z)
	msg = SM_SYSTEM_MESSAGE::STR_CMD_LOCATION_DESC(210010000, 1.5f, 100.0f, -0.25f);
	EXPECT_EQ(msg.getId(), 230038);
	EXPECT_EQ(msg.getParams(), (Params{"210010000", "1.5", "100.0", "-0.25"}));
}

TEST(GeneratedSysMsgTest, OverloadsAndFactoriesWithoutParameters) {
	EXPECT_EQ(SM_SYSTEM_MESSAGE::STR_LIST_USER(7).getParams(), (Params{"7"}));
	EXPECT_EQ(SM_SYSTEM_MESSAGE::STR_LIST_USER("seven").getParams(), (Params{"seven"}));
	EXPECT_EQ(SM_SYSTEM_MESSAGE::STR_LIST_USER(7).getId(), 1300641);

	const auto withoutParams = SM_SYSTEM_MESSAGE::STR_MSG_DELETE_CASH_SOCIALACTION_BY_TIMEOUT();
	EXPECT_EQ(withoutParams.getId(), 1390245);
	EXPECT_TRUE(withoutParams.getParams().empty());
	EXPECT_EQ(SM_SYSTEM_MESSAGE::STR_MSG_DELETE_CASH_SOCIALACTION_BY_TIMEOUT("wave").getParams(), (Params{"wave"}));
}

TEST(GeneratedSysMsgTest, NonPlainBodies) {
	// reordered: STR_CANNOT_USE_ITEM_TOO_LOW_LEVEL_MUST_BE_THIS_LEVEL(String value1, int value0) -> (1300372, value0, value1)
	auto msg = SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_TOO_LOW_LEVEL_MUST_BE_THIS_LEVEL("sword", 30);
	EXPECT_EQ(msg.getId(), 1300372);
	EXPECT_EQ(msg.getParams(), (Params{"30", "sword"}));

	// concatenation: STR_MAIL_CASHITEM_BUY(int itemId) -> (1300956, "[item:" + itemId + "]")
	msg = SM_SYSTEM_MESSAGE::STR_MAIL_CASHITEM_BUY(100000001);
	EXPECT_EQ(msg.getId(), 1300956);
	EXPECT_EQ(msg.getParams(), (Params{"[item:100000001]"}));

	// STR_CRAFT_RECIPE_LEARN(int value0, String name) -> (1330061, "[recipe_ex:" + value0 + ";" + name + "]")
	msg = SM_SYSTEM_MESSAGE::STR_CRAFT_RECIPE_LEARN(155000001, "Bread");
	EXPECT_EQ(msg.getParams(), (Params{"[recipe_ex:155000001;Bread]"}));

	// numeric operand first: STR_MSG_CASH_ITEM_TIME_LEFT(String value0, int minutes) -> (1400481, value0, minutes + "min")
	msg = SM_SYSTEM_MESSAGE::STR_MSG_CASH_ITEM_TIME_LEFT("hat", 15);
	EXPECT_EQ(msg.getParams(), (Params{"hat", "15min"}));

	// literal argument: STR_GUILD_NOTICE(String value0, long i) -> (1400019, value0, i, 2)
	msg = SM_SYSTEM_MESSAGE::STR_GUILD_NOTICE("Legion", 5);
	EXPECT_EQ(msg.getId(), 1400019);
	EXPECT_EQ(msg.getParams(), (Params{"Legion", "5", "2"}));

	// STR_FIELDABYSS_DARKBOSS_KILLED(String playerName, String faction) -> (1400323, faction, playerName)
	msg = SM_SYSTEM_MESSAGE::STR_FIELDABYSS_DARKBOSS_KILLED("Hero", "Elyos");
	EXPECT_EQ(msg.getParams(), (Params{"Elyos", "Hero"}));

	// Java returns AionServerPacket: STR_MSG_MERCHANT_PET_GET_SELL_ITEM(String name)
	SM_SYSTEM_MESSAGE pet = SM_SYSTEM_MESSAGE::STR_MSG_MERCHANT_PET_GET_SELL_ITEM("Pet");
	EXPECT_EQ(pet.getId(), 1402570);
}

TEST(GeneratedSysMsgTest, RenamedReservedIdentifiers) {
	// Java: _STR_MSG_Heal_TO_ME (leading underscore + capital is reserved in C++), STR_RESURRECT_DIALOG__SKILL (double underscore)
	EXPECT_EQ(SM_SYSTEM_MESSAGE::STR_MSG_Heal_TO_ME_(12).getId(), 1390216);
	EXPECT_EQ(SM_SYSTEM_MESSAGE::STR_RESURRECT_DIALOG_SKILL_().getId(), 1300741);
	EXPECT_EQ(SM_SYSTEM_MESSAGE::STR_RESURRECT_DIALOG_5MIN_("x").getParams(), (Params{"x"}));
}

} // namespace
} // namespace aion::gameserver::network::aion::serverpackets
