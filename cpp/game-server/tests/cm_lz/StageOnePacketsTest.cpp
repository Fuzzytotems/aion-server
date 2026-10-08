// P5-16 client packets of M5j stage 1 CP5 (m5j-plan.md §18.1, S-06): CM_MARK_FRIENDLIST, CM_MACRO_CREATE, CM_MACRO_DELETE, CM_OPEN_STATICDOOR,
// CM_QUESTIONNAIRE and CM_MEGAPHONE's first checks, read from their bytes and run on a party-fixture member (tests/team/P5-10b, by relative
// path). The macro DAO has no database here: PlayerService.addMacro's in-memory list is what the cases read (the DAO arms are
// PlayerServiceTest's). Expectations from the Java files of the packets.

#include "../team/P5-10b/TeamTestSupport.h"

#include <string>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/Macros.h"
#include "aion/gameserver/network/aion/clientpackets/CM_MACRO_CREATE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_MACRO_DELETE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_MARK_FRIENDLIST.h"
#include "aion/gameserver/network/aion/clientpackets/CM_MEGAPHONE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_OPEN_STATICDOOR.h"
#include "aion/gameserver/network/aion/clientpackets/CM_QUESTIONNAIRE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MACRO_RESULT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MARK_FRIENDLIST.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using network::test::PacketWriter;

class StageOnePacketsLzTest : public TeamTest {
protected:
	template <class P>
	void run(Member& m, int32_t opcode, const std::vector<uint8_t>& body) {
		Driver<P> packet(opcode);
		packet.readAndRun(body, m.client->get());
	}
};

/** CM_MARK_FRIENDLIST.java:27-30 */
TEST_F(StageOnePacketsLzTest, TheFriendListMarkIsAnswered) {
	Member& a = addMember("Alpha");
	run<CM_MARK_FRIENDLIST>(a, 110, {});
	EXPECT_EQ(a.count(serverpackets::SM_MARK_FRIENDLIST()), 1);
}

/** CM_MACRO_CREATE.java:40-51, CM_MACRO_DELETE.java:37-47: the position (an unsigned byte) and the XML; the result packets */
TEST_F(StageOnePacketsLzTest, AMacroIsCreatedAndDeleted) {
	Member& a = addMember("Alpha");
	a.player().setMacros(model::gameobjects::player::Macros::create());
	run<CM_MACRO_CREATE>(a, 175, PacketWriter().C(5).S("<macro>hi</macro>").data);
	EXPECT_EQ(a.count(serverpackets::SM_MACRO_RESULT(serverpackets::SM_MACRO_RESULT::SM_MACRO_CREATED)), 1);
	std::vector<runtime::Ptr<model::gameobjects::player::Macros::Macro>> all = a.player().getMacros()->getAll();
	ASSERT_EQ(all.size(), 1u);
	EXPECT_EQ(all[0]->id(), 5);
	EXPECT_EQ(all[0]->xml(), "<macro>hi</macro>");

	a.clearSent();
	run<CM_MACRO_DELETE>(a, 176, PacketWriter().C(5).data);
	EXPECT_EQ(a.count(serverpackets::SM_MACRO_RESULT(serverpackets::SM_MACRO_RESULT::SM_MACRO_DELETED)), 1);
	EXPECT_TRUE(a.player().getMacros()->getAll().empty());
}

/** CM_OPEN_STATICDOOR.java:31-35: StaticDoorService.openStaticDoor with the door id read (no door here: the warning) */
TEST_F(StageOnePacketsLzTest, ADoorClickReachesTheDoorService) {
	Member& a = addMember("Alpha");
	network::test::LogCapture capture({"com.aionemu.gameserver.services.StaticDoorService"});
	run<CM_OPEN_STATICDOOR>(a, 23, PacketWriter().D(145).data);
	EXPECT_EQ(capture.count("Door (ID: 145) is missing near"), 1) << capture.dump();
}

/** CM_QUESTIONNAIRE.java:42-48: an object id of 0 does nothing */
TEST_F(StageOnePacketsLzTest, AnEmptyQuestionnaireAnswerDoesNothing) {
	Member& a = addMember("Alpha");
	run<CM_QUESTIONNAIRE>(a, 145, PacketWriter().D(0).H(2).D(100).D(200).S("").data);
	EXPECT_TRUE(a.sent().empty());
}

/** CM_MEGAPHONE.java:36-42: a megaphone item the player does not have is ignored */
TEST_F(StageOnePacketsLzTest, AMissingMegaphoneIsIgnored) {
	Member& a = addMember("Alpha");
	run<CM_MEGAPHONE>(a, 237, PacketWriter().S("hello").D(999).data);
	EXPECT_TRUE(a.sent().empty());
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
