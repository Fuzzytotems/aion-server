// P5-15 client packets of M5j stage 2 CP3 (m5j-plan.md §18.3): CM_APPEARANCE's refusals and CM_CHAT_GROUP_INFO, read from their bytes and run
// on a party-fixture member (tests/team/P5-10b, by relative path). This executable has no database, so CM_APPEARANCE's rename past the
// name checks (PlayerService.isNameUsedOrReserved reads the players table) and LegionService.tryRename (lane B's legion PR) are not driven.
// Expectations from the Java files of the packets.

#include "../team/P5-10b/TeamTestSupport.h"

#include <regex>
#include <string>
#include <type_traits>
#include <vector>

#include "aion/gameserver/configs/main/NameConfig.h"
#include "aion/gameserver/network/aion/clientpackets/CM_APPEARANCE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CHAT_GROUP_INFO.h"
#include "aion/gameserver/network/aion/clientpackets/CM_TIME_CHECK_QUIT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHAT_WINDOW.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using network::test::PacketWriter;

/** AionClientPacketFactory packets[197] and [61] (ClientPacketInfo.gen.inc:171, :69) */
constexpr int32_t CM_APPEARANCE_OPCODE = 197;
constexpr int32_t CM_CHAT_GROUP_INFO_OPCODE = 61;

static_assert(std::is_base_of_v<CM_TIME_CHECK, CM_TIME_CHECK_QUIT>, "CM_TIME_CHECK_QUIT.java extends CM_TIME_CHECK");

class StageTwoPacketsTest : public TeamTest {
protected:
	void SetUp() override {
		TeamTest::SetUp();
		// NameConfig's defaults (NameConfig.java: gameserver.name.pattern "[a-zA-Z]{2,16}"): the test process loads no properties
		configs::main::NameConfig::CHAR_NAME_PATTERN.set(std::wregex(L"[a-zA-Z]{2,16}"));
		configs::main::NameConfig::FORBIDDEN_SEQUENCE_PATTERN.set(std::nullopt);
		configs::main::NameConfig::FORBIDDEN_WORDS.set({});
	}

	void TearDown() override {
		for (Member& m : members)
			world::World::getInstance().removeObject(m.player());
		TeamTest::TearDown();
	}

	template <class P>
	void run(Member& m, int32_t opcode, const std::vector<uint8_t>& body) {
		Driver<P> packet(opcode);
		packet.readAndRun(body, m.client->get());
	}
};

/** CM_APPEARANCE.java:37-50, :69-75, :100-107, :109-119: the same name, a name the pattern refuses, a legion rename without a legion, no item */
TEST_F(StageTwoPacketsTest, AppearanceRefusesBeforeTheTicket) {
	Member& a = addMember("Alpha");
	run<CM_APPEARANCE>(a, CM_APPEARANCE_OPCODE, PacketWriter().C(0).C(0).H(0).D(4711).S("alpha").data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_EDIT_CHAR_NAME_ERROR_SAME_YOUR_NAME()), 1) << "Util.convertName(alpha) is the old name";
	run<CM_APPEARANCE>(a, CM_APPEARANCE_OPCODE, PacketWriter().C(0).C(0).H(0).D(4711).S("Al9ha").data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_EDIT_CHAR_NAME_ERROR_WRONG_INPUT()), 1) << "NameRestrictionService.isValidName";
	run<CM_APPEARANCE>(a, CM_APPEARANCE_OPCODE, PacketWriter().C(1).C(0).H(0).D(4711).S("Legion").data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_EDIT_GUILD_NAME_ERROR_ONLY_MASTER_CAN_CHANGE_NAME()), 1) << "no legion";
	a.clearSent();
	run<CM_APPEARANCE>(a, CM_APPEARANCE_OPCODE, PacketWriter().C(2).C(0).H(0).D(4711).data);
	EXPECT_TRUE(a.sent().empty()) << "type 2 reads no name; no such item: nothing";
	run<CM_APPEARANCE>(a, CM_APPEARANCE_OPCODE, PacketWriter().C(3).C(0).H(0).D(4711).data);
	EXPECT_TRUE(a.sent().empty()) << "an unknown type does nothing";
}

/** CM_CHAT_GROUP_INFO.java:26-40: a name nobody has, then a player in the World */
TEST_F(StageTwoPacketsTest, ChatGroupInfoAnswersTheWindowOrNoSuchUser) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	world::World::getInstance().storeObject(b.player());
	run<CM_CHAT_GROUP_INFO>(a, CM_CHAT_GROUP_INFO_OPCODE, PacketWriter().S("Nobody").D(0).data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER("Nobody")), 1);
	run<CM_CHAT_GROUP_INFO>(a, CM_CHAT_GROUP_INFO_OPCODE, PacketWriter().S("Bravo").D(0).data);
	EXPECT_EQ(a.count(serverpackets::SM_CHAT_WINDOW(b.player(), true)), 1);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
