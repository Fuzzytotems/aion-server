// CM_START_LOOT and CM_LOOT_ITEM (P5-16, m5b3-plan.md P-05/P-06): C_LOOT, what the client sends to open (action 0) or close (action 1) the loot
// window of a corpse, and C_LOOT_ITEM, what it sends to take one entry of it.
//
// Java: game-server/src/com/aionemu/gameserver/network/aion/clientpackets/CM_START_LOOT.java:34-54 and CM_LOOT_ITEM.java:22-34.
//
// The run tests register a corpse's drop the way DropRegistrationService.registerDrop leaves it (a DropNpc that lets the player loot, and the
// set of DropItems with their indexes) and drive the packets into DropService. DropService's own arms (refusals, team loot, the decay task) are
// the loot lane's tests (tests/economy); these cases pin what the packets add: the reads (readUC for the index), the switch on the action, the
// null-player return of CM_LOOT_ITEM and the warning for an unknown action. The corpse is registered without an Npc in the World, so
// DropService's decay and delete arms do nothing here.

#include "../cm_ak/ItemPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/network/aion/clientpackets/CM_LOOT_ITEM.h"
#include "aion/gameserver/network/aion/clientpackets/CM_START_LOOT.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_START_LOOT_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_LOOT_ITEM_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friends CM_START_LOOT.h and CM_LOOT_ITEM.h declare: the fields readImpl decoded, which Java keeps private */
struct CM_START_LOOTTestAccess {
	static int32_t targetObjectId(const CM_START_LOOT& p) { return p.targetObjectId; }
	static int32_t action(const CM_START_LOOT& p) { return p.action; }
};

struct CM_LOOT_ITEMTestAccess {
	static int32_t targetObjectId(const CM_LOOT_ITEM& p) { return p.targetObjectId; }
	static int32_t index(const CM_LOOT_ITEM& p) { return p.index; }
};

namespace testing::items {
namespace {

using model::gameobjects::state::CreatureState;
using network::test::LogCapture;
using services::drop::DropRegistrationService;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";
const char* CM_START_LOOT_LOGGER = "com.aionemu.gameserver.network.aion.clientpackets.CM_START_LOOT";

/** the decoded opcodes of ClientPacketInfo.gen.inc:137-138 (Java AionClientPacketFactory.java:182-183, State.IN_GAME) */
constexpr int32_t CM_START_LOOT_OPCODE = 154;
constexpr int32_t CM_LOOT_ITEM_OPCODE = 155;

constexpr int32_t CORPSE = 730001;

/** Java CM_START_LOOT.readImpl: D target, C action */
std::vector<uint8_t> startLootBody(int32_t target, int32_t action) {
	return PacketWriter().D(target).C(action).data;
}

/** Java CM_LOOT_ITEM.readImpl: D target, C index (read unsigned) */
std::vector<uint8_t> lootItemBody(int32_t target, int32_t index) {
	return PacketWriter().D(target).C(index).data;
}

template <class P>
std::unique_ptr<P> readPacket(const std::vector<uint8_t>& data, int32_t opcode, int32_t& unread) {
	std::vector<uint8_t> copy = data;
	auto packet = std::make_unique<P>(opcode, StateSet{AionConnection_State::IN_GAME});
	packet->setBuffer(commons::utils::ByteBuffer::wrap(copy));
	if (!packet->read())
		return nullptr;
	unread = packet->getRemainingBytes();
	return packet;
}

TEST(LootPacketsReadTest, StartLootReadsTheTargetAndASignedAction) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	std::unique_ptr<CM_START_LOOT> p = readPacket<CM_START_LOOT>(startLootBody(CORPSE, 1), CM_START_LOOT_OPCODE, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_START_LOOTTestAccess::targetObjectId(*p), CORPSE);
	EXPECT_EQ(CM_START_LOOTTestAccess::action(*p), 1);
	EXPECT_EQ(unread, 0);
	std::unique_ptr<CM_START_LOOT> negative = readPacket<CM_START_LOOT>(startLootBody(CORPSE, 0x80), CM_START_LOOT_OPCODE, unread);
	ASSERT_NE(negative, nullptr);
	EXPECT_EQ(CM_START_LOOTTestAccess::action(*negative), -128) << "Java byte action = readC()";
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(LootPacketsReadTest, LootItemReadsTheIndexUnsigned) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	std::unique_ptr<CM_LOOT_ITEM> p = readPacket<CM_LOOT_ITEM>(lootItemBody(CORPSE, 0xC8), CM_LOOT_ITEM_OPCODE, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_LOOT_ITEMTestAccess::targetObjectId(*p), CORPSE);
	EXPECT_EQ(CM_LOOT_ITEMTestAccess::index(*p), 200) << "readUC: 0xC8 is 200, not -56";
	EXPECT_EQ(unread, 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(LootPacketsReadTest, ShortBodiesLogTheMissingField) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	ASSERT_NE(readPacket<CM_START_LOOT>(PacketWriter().D(CORPSE).data, CM_START_LOOT_OPCODE, unread), nullptr);
	EXPECT_EQ(capture.count("Missing C"), 1) << capture.dump();
	ASSERT_NE(readPacket<CM_LOOT_ITEM>(PacketWriter().H(1).data, CM_LOOT_ITEM_OPCODE, unread), nullptr);
	EXPECT_TRUE(capture.contains("Missing D")) << capture.dump();
}

TEST(LootPacketsReadTest, TheMarkersRegisterBothClassesUnderTheirJavaOpcodes) {
	EXPECT_NE(dynamic_cast<CM_START_LOOT*>(CM_START_LOOT_clientPacketFactory(CM_START_LOOT_OPCODE, StateSet{AionConnection_State::IN_GAME}).get()), nullptr);
	EXPECT_NE(dynamic_cast<CM_LOOT_ITEM*>(CM_LOOT_ITEM_clientPacketFactory(CM_LOOT_ITEM_OPCODE, StateSet{AionConnection_State::IN_GAME}).get()), nullptr);
	int32_t found = 0;
#define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...)                                                                             \
	if (std::string_view(#Class) == "CM_START_LOOT") {                                                                                                  \
		EXPECT_EQ(opcode, CM_START_LOOT_OPCODE);                                                                                                        \
		++found;                                                                                                                                        \
	} else if (std::string_view(#Class) == "CM_LOOT_ITEM") {                                                                                            \
		EXPECT_EQ(opcode, CM_LOOT_ITEM_OPCODE);                                                                                                         \
		++found;                                                                                                                                        \
	}
#include "aion/gameserver/network/aion/ClientPacketInfo.gen.inc"
#undef AION_CLIENT_PACKET_INFO
	EXPECT_EQ(found, 2);
}

/** SM_LOOT_STATUS as Java writes it (SM_LOOT_STATUS.java:29-33): D target, C status id (OPEN_DROP_LIST 2, CLOSE_DROP_LIST 3), D lootEffectId 0 */
std::vector<uint8_t> lootStatus(int32_t target, int32_t statusId) {
	return javaPacket(SM_LOOT_STATUS_OPCODE, PacketWriter().D(target).C(statusId).D(0));
}

class LootPacketsRunTest : public ItemPacketTest {
protected:
	void SetUp() override {
		ItemPacketTest::SetUp();
		runtime::resetUnportedHitsForTests();
	}

	void TearDown() override {
		// the registration service is an Immortal: the corpse's entries must not outlive the case (they hold the looting player)
		DropRegistrationService::getInstance().getCurrentDropMap().remove(CORPSE);
		DropRegistrationService::getInstance().getDropRegistrationMap().remove(CORPSE);
		dropNpc = nullptr;
		ItemPacketTest::TearDown();
	}

	/** What registerDrop leaves for a solo kill: a DropNpc the player may loot and the corpse's drop set, one entry per (index, item, count) */
	void registerCorpse(std::initializer_list<std::tuple<int32_t, int32_t, int64_t>> entries) {
		dropNpc = model::gameobjects::DropNpc::create(CORPSE);
		dropNpc->setAllowedLooter(player());
		// the set as registerDrop creates it (DropRegistrationService.cpp: RcHashSet::create with the currentDropMap#dropItems lock class)
		runtime::Ref<runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>> dropItems =
			runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>::create(AION_LOCK_CLASS(DropRegistrationService::currentDropMap#dropItems));
		for (const auto& [index, itemId, count] : entries) {
			runtime::Ref<model::drop::DropItem> dropItem = model::drop::DropItem::create(model::drop::Drop(itemId, 1, 1, 100.0f));
			dropItem->setIndex(index);
			dropItem->setCount(count);
			dropItems->add(dropItem);
		}
		DropRegistrationService::getInstance().getDropRegistrationMap().put(CORPSE, dropNpc);
		DropRegistrationService::getInstance().getCurrentDropMap().put(CORPSE, dropItems);
	}

	void startLoot(int32_t target, int32_t action) {
		Driver<CM_START_LOOT> packet(CM_START_LOOT_OPCODE);
		packet.readAndRun(startLootBody(target, action), client->get());
	}

	void lootItem(int32_t target, int32_t index) {
		Driver<CM_LOOT_ITEM> packet(CM_LOOT_ITEM_OPCODE);
		packet.readAndRun(lootItemBody(target, index), client->get());
	}

	size_t entriesLeft() { return DropRegistrationService::getInstance().getCurrentDropMap().get(CORPSE)->size(); }

	runtime::Ref<model::gameobjects::DropNpc> dropNpc;
};

TEST_F(LootPacketsRunTest, ActionZeroOpensTheLootWindow) {
	registerCorpse({{1, SPARKIE_CARAPACE_FRAGMENT, 3}});

	startLoot(CORPSE, 0);

	// CM_START_LOOT.java:45-46 -> DropService.requestDropList: the list, the status OPEN_DROP_LIST, the looting emotion (DropService.java:126-135)
	std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(opcodesOf(packets), (std::vector<int32_t>{SM_LOOT_ITEMLIST_OPCODE, SM_LOOT_STATUS_OPCODE, SM_EMOTION_OPCODE}));
	ASSERT_EQ(packets.size(), 3u);
	EXPECT_EQ(PacketReader(bodyOf(packets[0])).D(), CORPSE) << "SM_LOOT_ITEMLIST names the corpse of the packet";
	EXPECT_EQ(packets[1], lootStatus(CORPSE, 2));
	EXPECT_EQ(dropNpc->getLootingPlayer().get(), &player());
	EXPECT_TRUE(player().isInState(CreatureState::LOOTING));
	EXPECT_EQ(player().getLootingNpcOid(), CORPSE);
}

TEST_F(LootPacketsRunTest, ActionOneClosesIt) {
	registerCorpse({{1, SPARKIE_CARAPACE_FRAGMENT, 3}});
	startLoot(CORPSE, 0);
	clearSent();

	startLoot(CORPSE, 1);

	// CM_START_LOOT.java:48-49 -> DropService.closeDropList: END_LOOT, the looter released (the entry stays for a later opening)
	EXPECT_EQ(opcodesOf(sent()), (std::vector<int32_t>{SM_EMOTION_OPCODE}));
	EXPECT_FALSE(dropNpc->getLootingPlayer());
	EXPECT_FALSE(player().isInState(CreatureState::LOOTING));
	EXPECT_EQ(player().getLootingNpcOid(), 0);
	EXPECT_EQ(entriesLeft(), 1u);
}

TEST_F(LootPacketsRunTest, AnUnknownActionIsLoggedAndIgnored) {
	registerCorpse({{1, SPARKIE_CARAPACE_FRAGMENT, 3}});
	LogCapture capture({CM_START_LOOT_LOGGER});

	startLoot(CORPSE, 2);
	startLoot(CORPSE, 0xFF);

	// CM_START_LOOT.java:51-52, `player + " sent unknown loot action type " + action` with the signed byte
	EXPECT_EQ(capture.count(player().toString() + " sent unknown loot action type 2"), 1) << capture.dump();
	EXPECT_EQ(capture.count(player().toString() + " sent unknown loot action type -1"), 1) << capture.dump();
	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(dropNpc->getLootingPlayer());
}

TEST_F(LootPacketsRunTest, TakingTheLastEntryMovesItIntoTheCubeAndClosesTheWindow) {
	registerCorpse({{200, SPARKIE_CARAPACE_FRAGMENT, 3}});
	startLoot(CORPSE, 0);
	clearSent();

	lootItem(CORPSE, 200); // the index byte 0xC8: readUC, so entry 200 is found

	// CM_LOOT_ITEM.java:33 -> DropService.requestDropItem(player, corpse, 200): the stack is added to the cube (a new one: SM_INVENTORY_ADD_ITEM,
	// SM_CUBE_UPDATE), the entry leaves the set, and resendDropList closes the empty window (CLOSE_DROP_LIST, END_LOOT)
	ASSERT_TRUE(player().getInventory().getFirstItemByItemId(SPARKIE_CARAPACE_FRAGMENT));
	EXPECT_EQ(player().getInventory().getFirstItemByItemId(SPARKIE_CARAPACE_FRAGMENT)->getItemCount(), 3);
	EXPECT_EQ(entriesLeft(), 0u);
	std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(opcodesOf(packets),
		(std::vector<int32_t>{SM_INVENTORY_ADD_ITEM_OPCODE, SM_CUBE_UPDATE_OPCODE, SM_LOOT_STATUS_OPCODE, SM_EMOTION_OPCODE}));
	ASSERT_EQ(packets.size(), 4u);
	EXPECT_EQ(packets[2], lootStatus(CORPSE, 3));
	EXPECT_FALSE(player().isInState(CreatureState::LOOTING));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(LootPacketsRunTest, AnIndexThatIsNotInTheSetTakesNothing) {
	registerCorpse({{1, SPARKIE_CARAPACE_FRAGMENT, 3}});

	lootItem(CORPSE, 2);
	lootItem(CORPSE + 1, 1); // no corpse of that id

	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(entriesLeft(), 1u);
	EXPECT_FALSE(player().getInventory().getFirstItemByItemId(SPARKIE_CARAPACE_FRAGMENT));
}

TEST_F(LootPacketsRunTest, WithoutAnActivePlayerTheLootRequestIsDropped) {
	// CM_LOOT_ITEM.java:30-32: a connection that has no player (not in the world, or already left) returns before DropService
	registerCorpse({{1, SPARKIE_CARAPACE_FRAGMENT, 3}});
	TestClient stranger; // a connection without enterWorld: getActivePlayer() is null
	Driver<CM_LOOT_ITEM> packet(CM_LOOT_ITEM_OPCODE);

	EXPECT_NO_THROW(packet.readAndRun(lootItemBody(CORPSE, 1), stranger.get()));

	EXPECT_EQ(entriesLeft(), 1u);
	EXPECT_TRUE(stranger->sentBytes().empty());
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
