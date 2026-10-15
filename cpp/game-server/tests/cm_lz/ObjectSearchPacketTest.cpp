// M5d D-05 (m5d-plan.md §7, §18.3, P5-16, optional): CM_OBJECT_SEARCH (C_FIND_NPC_POS), the client's "where is this npc" - the npc shown
// on the map from a quest's journal or the npc search.
//
// Java: CM_OBJECT_SEARCH.java:29-46 - D npc id; SpawnsData.getNearestSpawnByNpcId(player, npcId, player's map) (SpawnsData.java: the nearest
// spot of the npc's spawns on the player's map; else the first spot of the first spawn on a map of the player's race, else on any other map),
// answered with SM_SHOW_NPC_ON_MAP (npc, map, the instance id - the player's own channel on his map, the map id elsewhere - and the spot), or
// STR_FIND_POS_UNKNOWN_NAME when the npc spawns nowhere.
// The run cases drive runImpl against the item packet fixture (ItemPacketTestSupport.h: a level-1 Elyos warrior at (100, 100, 50) in Poeta's
// instance 1 and a real AionConnection whose send queue the cases read) on the world holders of P4-10's test set (Poeta, ELYSEA; Reshanta,
// ABYSS; tests/world/WorldTestSupport.h). The spawn rows are the shipped data's, verbatim (spawns/Npcs/210010000_Poeta.xml and
// 400010000_Reshanta.xml, the lines beside each). The expected nearest spot is the data's: of the striped kerub's 18 spots the one nearest to
// (100, 100, 50) is (1055.42, 962.302, 133.088), the fourth, at 1,289.7 m (the first, (1038.58, 989.604, 129.484), is 1,295.6 m away).
// Expected packets are Java's bytes (writeOP + the writeImpl fields: SM_SHOW_NPC_ON_MAP.java; ServerPacketsOpcodes.java:107).

#include "../cm_ak/ItemPacketTestSupport.h"
#include "../world/WorldTestSupport.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/dataholders/SpawnsData.bind.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/network/aion/clientpackets/CM_OBJECT_SEARCH.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_OBJECT_SEARCH_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friend CM_OBJECT_SEARCH.h declares: the field readImpl decoded, which Java keeps private */
struct CM_OBJECT_SEARCHTestAccess {
	static int32_t npcId(const CM_OBJECT_SEARCH& p) { return p.npcId; }
};

namespace testing::items {
namespace {

using network::test::LogCapture;
using serverpackets::SM_SYSTEM_MESSAGE;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";

/** the decoded opcode of ClientPacketInfo.gen.inc:28 (Java AionClientPacketFactory: C_FIND_NPC_POS, State.IN_GAME) */
constexpr int32_t CM_OBJECT_SEARCH_OPCODE = 11;
/** ServerPacketsOpcodes.java:107 */
constexpr int32_t SM_SHOW_NPC_ON_MAP_OPCODE = 89;

constexpr int32_t POETA = 210010000;
constexpr int32_t RESHANTA = 400010000;
constexpr int32_t STRIPED_KERUB = 210133;
constexpr int32_t MIRES = 203057;
constexpr int32_t ABEND = 278129;

/** spawns/Npcs/210010000_Poeta.xml and 400010000_Reshanta.xml, verbatim rows */
constexpr std::string_view SPAWNS_XML = R"xml(<spawns>
	<spawn_map map_id="210010000">
		<!-- :1223-1243, Striped Kerub -->
		<spawn npc_id="210133" respawn_time="15">
			<spot x="1038.58" y="989.604" z="129.484" h="93"/>
			<spot x="1050.87" y="996.053" z="131.596" h="111"/>
			<spot x="1052.9" y="1070.34" z="117.854" h="37"/>
			<spot x="1055.42" y="962.302" z="133.088" h="69"/>
			<spot x="1063" y="1079.48" z="117.494" h="7"/>
			<spot x="1064.42" y="1064.27" z="123.261" h="29"/>
			<spot x="1072.1" y="1073.21" z="122.555" h="97"/>
			<spot x="1073.99" y="1057.03" z="125.867" h="30"/>
			<spot x="1097.29" y="977.352" z="130.113" h="118"/>
			<spot x="1098.26" y="1000.43" z="125.6" h="93"/>
			<spot x="1102.11" y="999.931" z="126.5" h="10"/>
			<spot x="1111.75" y="998.109" z="127.321" h="115"/>
			<spot x="1124.27" y="1018.68" z="127.584" h="115"/>
			<spot x="1147.76" y="997.14" z="135.315" h="62" walker_id="F912DD8BEF6BA40288166F8A9136151966300044"/>
			<spot x="1153.9" y="995.87" z="137" h="93"/>
			<spot x="1191.35" y="1136.9" z="139.6" h="71"/>
			<spot x="1199.99" y="1127.68" z="142.085" h="30"/>
			<spot x="1210.93" y="1133.86" z="144.74124" h="106" walker_id="6C27EAF1FDB090595CE9BD14912F6BFF8F18C22E"/>
		</spawn>
		<!-- :877-880, Mires -->
		<spawn npc_id="203057" respawn_time="295">
			<spot x="1141" y="1032" z="128.875" h="3"/>
		</spawn>
	</spawn_map>
	<spawn_map map_id="400010000">
		<!-- :4-7, Abend -->
		<spawn npc_id="278129" respawn_time="295">
			<spot x="687.21" y="2841.99" z="1595.24" h="108"/>
		</spawn>
	</spawn_map>
</spawns>)xml";

/** C_FIND_NPC_POS: D npc (CM_OBJECT_SEARCH.java:29-32) */
std::vector<uint8_t> searchBody(int32_t npcId) {
	return PacketWriter().D(npcId).data;
}

/** SM_SHOW_NPC_ON_MAP.writeImpl: D npc, D map, D instance id, F x, F y, F z */
std::vector<uint8_t> npcOnMap(int32_t npcId, int32_t worldId, int32_t instanceId, float x, float y, float z) {
	return javaPacket(SM_SHOW_NPC_ON_MAP_OPCODE, PacketWriter().D(npcId).D(worldId).D(instanceId).F(x).F(y).F(z));
}

std::unique_ptr<CM_OBJECT_SEARCH> readPacket(const std::vector<uint8_t>& data, int32_t& unread) {
	std::vector<uint8_t> copy = data;
	auto packet = std::make_unique<CM_OBJECT_SEARCH>(CM_OBJECT_SEARCH_OPCODE, StateSet{AionConnection_State::IN_GAME});
	packet->setBuffer(commons::utils::ByteBuffer::wrap(copy));
	if (!packet->read())
		return nullptr;
	unread = packet->getRemainingBytes();
	return packet;
}

TEST(ObjectSearchReadTest, TheBodyIsTheNpcId) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	std::unique_ptr<CM_OBJECT_SEARCH> p = readPacket(searchBody(0x0A0B0C0D), unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_OBJECT_SEARCHTestAccess::npcId(*p), 0x0A0B0C0D);
	EXPECT_EQ(unread, 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(ObjectSearchReadTest, TheMarkerRegistersTheClassUnderItsJavaOpcode) {
	EXPECT_NE(dynamic_cast<CM_OBJECT_SEARCH*>(
				  CM_OBJECT_SEARCH_clientPacketFactory(CM_OBJECT_SEARCH_OPCODE, StateSet{AionConnection_State::IN_GAME}).get()),
		nullptr);
	using enum AionConnection_State;
	int32_t found = 0;
#define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...)                                                                             \
	if (std::string_view(#Class) == "CM_OBJECT_SEARCH") {                                                                                               \
		EXPECT_EQ(opcode, CM_OBJECT_SEARCH_OPCODE);                                                                                                     \
		EXPECT_EQ((StateSet{__VA_ARGS__}), (StateSet{IN_GAME}));                                                                                       \
		++found;                                                                                                                                        \
	}
#include "aion/gameserver/network/aion/ClientPacketInfo.gen.inc"
#undef AION_CLIENT_PACKET_INFO
	EXPECT_EQ(found, 1);
}

class ObjectSearchRunTest : public ItemPacketTest {
protected:
	void SetUp() override {
		// the world holders are published once per process, P4-10's test set first (the fixture of ClassChangeServiceTest)
		ASSERT_TRUE(world::test::publishTestStaticData()) << "this process published the real static data";
		ItemPacketTest::SetUp();
		xml::LoadContext context;
		dataholders::DataManager::SPAWNS_DATA.publish(xml::bindString<dataholders::SpawnsData>(context, SPAWNS_XML));
	}

	void TearDown() override {
		ItemPacketTest::TearDown();
		dataholders::DataManager::SPAWNS_DATA.resetForTests();
	}

	void search(int32_t npcId) {
		Driver<CM_OBJECT_SEARCH> packet(CM_OBJECT_SEARCH_OPCODE);
		packet.readAndRun(searchBody(npcId), client->get());
	}
};

TEST_F(ObjectSearchRunTest, AnNpcOfThePlayersMapIsShownAtItsNearestSpot) {
	search(STRIPED_KERUB);
	search(MIRES);

	// SpawnsData.getNearestSpawn: the spot nearest to the player (the fourth of the kerub's), not the first; SM_SHOW_NPC_ON_MAP.java: on the
	// player's own map outside an instance the instance id is map id + channel (instance 1 -> + 0)
	EXPECT_EQ(sent(), exactly({npcOnMap(STRIPED_KERUB, POETA, POETA, 1055.42f, 962.302f, 133.088f), npcOnMap(MIRES, POETA, POETA, 1141.0f, 1032.0f, 128.875f)}));
}

TEST_F(ObjectSearchRunTest, AnNpcOfAnotherMapIsShownAtItsFirstSpotThere) {
	search(ABEND);

	// no Abend on Poeta, none on another map of the Elyos (P4-10's set has no other ELYSEA map), so the other maps: Reshanta; there the
	// first spot of the first spawn (getNearestSpawn with worldId != the player's map), and the map id as the instance id
	EXPECT_EQ(sent(), exactly({npcOnMap(ABEND, RESHANTA, RESHANTA, 687.21f, 2841.99f, 1595.24f)}));
}

TEST_F(ObjectSearchRunTest, AnNpcThatSpawnsNowhereIsAnUnknownName) {
	search(798007); // minalinerk spawns in Sanctum, which is not in this data

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_FIND_POS_UNKNOWN_NAME())}));
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
