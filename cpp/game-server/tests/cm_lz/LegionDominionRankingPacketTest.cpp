// M5i I-9 (LD-01, under P5-12b3's lease of P5-16's file): CM_LEGION_DOMINION_REQUEST_RANKING (C_REQUEST_LEGION_DOMINION_RANKIN,
// CM_LEGION_DOMINION_REQUEST_RANKING.java:24-36) on the item packet fixture of tests/cm_ak: the body is one D, and only the Stonespear ids 1-6
// are answered. An answered request (SM_LEGION_DOMINION_RANK of a loaded territory) needs the database; the weekly tests in
// tests/legionhouse/LegionServiceTest.cpp cover the ranking packet itself.

#include "../cm_ak/ItemPacketTestSupport.h"

#include <cstdint>
#include <vector>

#include "aion/gameserver/network/aion/clientpackets/CM_LEGION_DOMINION_REQUEST_RANKING.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

using network::test::PacketWriter;

// the decoded opcode of ClientPacketInfo.gen.inc (Java AionClientPacketFactory.java:57: packets[29], State.IN_GAME)
constexpr int32_t CM_LEGION_DOMINION_REQUEST_RANKING_OPCODE = 29;

class LegionDominionRankingPacketTest : public ItemPacketTest {
protected:
	void run(const std::vector<uint8_t>& body) {
		Driver<CM_LEGION_DOMINION_REQUEST_RANKING> packet(CM_LEGION_DOMINION_REQUEST_RANKING_OPCODE);
		packet.readAndRun(body, client->get());
	}
};

TEST_F(LegionDominionRankingPacketTest, OnlyTheSixStonespearIdsAreAnswered) {
	clearSent();
	for (const int32_t id : {0, 7, -1, 1000}) { // "idk sometimes it sends different bytes! TODO" (:31)
		SCOPED_TRACE("stonespearId " + std::to_string(id));
		run(PacketWriter().D(id).data);
		EXPECT_TRUE(sent().empty());
	}
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
