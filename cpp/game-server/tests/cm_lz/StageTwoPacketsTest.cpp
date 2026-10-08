// P5-16 client packets of M5j stage 2 CP3 (m5j-plan.md §18.3): CM_UPGRADE_ARCADE's switch and its log for an unknown action, read from its
// bytes and run on a party-fixture member (tests/team/P5-10b, by relative path). The arcade service's arms are UpgradeArcadeService's own tests'
// (M5b-3 leftovers CP3). Expectations from CM_UPGRADE_ARCADE.java.

#include "../team/P5-10b/TeamTestSupport.h"

#include <vector>

#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/network/aion/clientpackets/CM_UPGRADE_ARCADE.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using network::test::PacketWriter;

/** AionClientPacketFactory packets[246] (ClientPacketInfo.gen.inc:202) */
constexpr int32_t CM_UPGRADE_ARCADE_OPCODE = 246;

class StageTwoPacketsLzTest : public TeamTest {
protected:
	void run(Member& m, const std::vector<uint8_t>& body) {
		Driver<CM_UPGRADE_ARCADE> packet(CM_UPGRADE_ARCADE_OPCODE);
		packet.readAndRun(body, m.client->get());
	}
};

/** CM_UPGRADE_ARCADE.java:29-57: nothing while gameserver.event.arcade.enable is off; an unknown action logs */
TEST_F(StageTwoPacketsLzTest, TheArcadeIsOffOrWarnsOfAnUnknownAction) {
	Member& a = addMember("Alpha");
	network::test::LogCapture capture({"com.aionemu.gameserver.network.aion.clientpackets.CM_UPGRADE_ARCADE"});
	{
		ConfigScope<bool> off(configs::main::EventsConfig::ENABLE_EVENT_ARCADE, false);
		run(a, PacketWriter().C(9).D(0).data);
		EXPECT_EQ(capture.count("Unhandled arcade action"), 0) << "the event is off";
	}
	ConfigScope<bool> on(configs::main::EventsConfig::ENABLE_EVENT_ARCADE, true);
	run(a, PacketWriter().C(9).D(0).data);
	EXPECT_EQ(capture.count("Unhandled arcade action 9"), 1) << capture.dump();
	EXPECT_TRUE(a.sent().empty());
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
