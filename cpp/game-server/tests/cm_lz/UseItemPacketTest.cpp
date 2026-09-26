// CM_USE_ITEM (P5-16, m5b3-plan.md P-05/P-06, m5a-client-session.md F-2): C_USE_ITEM, what the client sends when the player uses an item of
// the cube - a potion, food, a scroll, a dye on an item or house object, a return scroll, an instance cooltime reset.
//
// Java: game-server/src/com/aionemu/gameserver/network/aion/clientpackets/CM_USE_ITEM.java:37-125.
//
// runImpl in Java order: stop the spawn protection, find the item (and the target item), cancel a cast in progress, notify the item-use
// observers, PlayerRestrictions.canUseItem, the actions (none: the quest engine may still accept the item), canAct for each action and act for
// those that can. The skilluse arm is driven end to end with Mercenary's Fruit Juice (item_templates.xml:821856, skill 10034 a statup, which
// M5b-2 ported): the cast pays one item and starts the item's cooldown. The three actions that take a parameter (DyeAction, MultiReturnAction,
// InstanceTimeClear) stay AION_UNPORTED (m5b3-plan.md D6), so the parameter each receives is not observable yet; neither is the house-object
// lookup of a target id found in no storage, which asks Player.getActiveHouse and so the database (HousingService); nor is the target item
// itself - the cube first, then the equipment (CM_USE_ITEM.java:67-70) - because the one ported action, SkillUseAction, does not read it and
// every action that does (enchant, dye, charge, ...) has an AION_UNPORTED canAct until M5c. All three are named here so their absence is not
// mistaken for coverage.

#include "../cm_ak/ItemPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/network/aion/clientpackets/CM_USE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Skill.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_USE_ITEM_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friend CM_USE_ITEM.h declares: the fields readImpl decoded, which Java keeps private */
struct CM_USE_ITEMTestAccess {
	static int32_t uniqueItemId(const CM_USE_ITEM& p) { return p.uniqueItemId; }
	static int32_t targetItemId(const CM_USE_ITEM& p) { return p.targetItemId; }
	static int32_t syncId(const CM_USE_ITEM& p) { return p.syncId; }
	static int32_t indexReturn(const CM_USE_ITEM& p) { return p.indexReturn; }
};

namespace testing::items {
namespace {

using Access = CM_USE_ITEMTestAccess;
using model::gameobjects::state::CreatureVisualState;
using network::test::LogCapture;
using serverpackets::SM_SYSTEM_MESSAGE;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";

/** the decoded opcode of ClientPacketInfo.gen.inc:48 (Java AionClientPacketFactory.java:65, State.IN_GAME) */
constexpr int32_t CM_USE_ITEM_OPCODE = 37;

/** Java CM_USE_ITEM.readImpl: D item, C type, then for type 2 D target item, 5 D sync id, 6 D return index */
std::vector<uint8_t> useBody(int32_t itemObjId, int32_t type, std::optional<int32_t> extra = std::nullopt) {
	PacketWriter writer;
	writer.D(itemObjId).C(type);
	if (extra)
		writer.D(*extra);
	return writer.data;
}

std::unique_ptr<CM_USE_ITEM> readPacket(const std::vector<uint8_t>& data, int32_t& unread) {
	std::vector<uint8_t> copy = data;
	auto packet = std::make_unique<CM_USE_ITEM>(CM_USE_ITEM_OPCODE, StateSet{AionConnection_State::IN_GAME});
	packet->setBuffer(commons::utils::ByteBuffer::wrap(copy));
	if (!packet->read())
		return nullptr;
	unread = packet->getRemainingBytes();
	return packet;
}

TEST(UseItemReadTest, TheTypeDecidesWhichFieldTheLastIntFills) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	struct Case {
		int32_t type;
		int32_t target, sync, index;
	};
	for (const Case c : {Case{2, 77, 0, 0}, Case{5, 0, 77, 0}, Case{6, 0, 0, 77}}) {
		SCOPED_TRACE("type " + std::to_string(c.type));
		std::unique_ptr<CM_USE_ITEM> p = readPacket(useBody(750001, c.type, 77), unread);
		ASSERT_NE(p, nullptr);
		EXPECT_EQ(Access::uniqueItemId(*p), 750001);
		EXPECT_EQ(Access::targetItemId(*p), c.target);
		EXPECT_EQ(Access::syncId(*p), c.sync);
		EXPECT_EQ(Access::indexReturn(*p), c.index);
		EXPECT_EQ(unread, 0);
	}
	// any other type reads nothing more (CM_USE_ITEM.java:41-51 has no default arm)
	for (int32_t type : {0, 1, 3, 4, 7}) {
		SCOPED_TRACE("type " + std::to_string(type));
		std::unique_ptr<CM_USE_ITEM> p = readPacket(useBody(750001, type, 77), unread);
		ASSERT_NE(p, nullptr);
		EXPECT_EQ(Access::targetItemId(*p) + Access::syncId(*p) + Access::indexReturn(*p), 0);
		EXPECT_EQ(unread, 4);
	}
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(UseItemReadTest, AShortBodyLogsTheMissingField) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	ASSERT_NE(readPacket(useBody(750001, 2), unread), nullptr);
	EXPECT_EQ(capture.count("Missing D"), 1) << capture.dump();
}

TEST(UseItemReadTest, TheMarkerRegistersTheClassUnderItsJavaOpcode) {
	EXPECT_NE(dynamic_cast<CM_USE_ITEM*>(CM_USE_ITEM_clientPacketFactory(CM_USE_ITEM_OPCODE, StateSet{AionConnection_State::IN_GAME}).get()), nullptr);
	int32_t found = 0;
#define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...)                                                                             \
	if (std::string_view(#Class) == "CM_USE_ITEM") {                                                                                                    \
		EXPECT_EQ(opcode, CM_USE_ITEM_OPCODE);                                                                                                          \
		++found;                                                                                                                                        \
	}
#include "aion/gameserver/network/aion/ClientPacketInfo.gen.inc"
#undef AION_CLIENT_PACKET_INFO
	EXPECT_EQ(found, 1);
}

/** An item-use observer that counts its aborts (ItemUseObserver.java: itemused and every other notification abort it) */
struct CountingItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	int32_t aborts = 0;

	static runtime::Ref<CountingItemUseObserver> create() { return runtime::makeRef<CountingItemUseObserver>(); }

	void abort() override { ++aborts; }

protected:
	CountingItemUseObserver() = default;
	~CountingItemUseObserver() override = default;
};

class UseItemRunTest : public ItemPacketTest {
protected:
	void SetUp() override {
		ItemPacketTest::SetUp();
		runtime::resetUnportedHitsForTests();
	}

	void use(int32_t itemObjId, int32_t type = 0, std::optional<int32_t> extra = std::nullopt) {
		Driver<CM_USE_ITEM> packet(CM_USE_ITEM_OPCODE);
		packet.readAndRun(useBody(itemObjId, type, extra), client->get());
	}

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return serializedFor(std::move(packet)); }
};

TEST_F(UseItemRunTest, TheSpawnProtectionEndsEvenForAnItemThatIsNotThere) {
	player().setVisualState(CreatureVisualState::BLINKING); // what startProtectionActiveTask sets (PlayerController.java)
	ASSERT_TRUE(player().isProtectionActive());

	use(750099);

	// CM_USE_ITEM.java:58-59 comes before the item lookup (:61-63)
	EXPECT_FALSE(player().isProtectionActive());
}

TEST_F(UseItemRunTest, AnItemThatIsNotInTheCubeDoesNothingElse) {
	runtime::Ref<CountingItemUseObserver> observer = CountingItemUseObserver::create();
	player().getObserveController()->attach(*observer);
	equipped(750002, TRAINING_SWORD, 1);

	use(750002); // equipped, so not in the inventory
	use(750099);

	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(observer->aborts, 0) << "CM_USE_ITEM.java:62-63 returns before the observers are notified";
	player().getObserveController()->removeObserver(*observer);
}

TEST_F(UseItemRunTest, TheItemUseObserversAreNotifiedBeforeTheRestrictions) {
	Item& junk = stored(750003, SPARKIE_CARAPACE_FRAGMENT, 3);
	runtime::Ref<CountingItemUseObserver> observer = CountingItemUseObserver::create();
	player().getObserveController()->attach(*observer);

	use(750003);

	// :80 notifyItemuseObservers, then :82 canUseItem - which refuses an item without actions (PlayerRestrictions.java:317-323)
	EXPECT_EQ(observer->aborts, 1);
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_ITEM_IS_NOT_USABLE())}));
	EXPECT_EQ(junk.getItemCount(), 3);
}

TEST_F(UseItemRunTest, ACastInProgressIsCancelledBeforeTheItemIsUsed) {
	stored(750004, SPARKIE_CARAPACE_FRAGMENT, 3);
	player().setCasting(skillengine::model::Skill::create(dataholders::DataManager::SKILL_DATA->getSkillTemplate(10034), player(), nullptr, 1));
	ASSERT_TRUE(player().isCasting());

	use(750004);

	// CM_USE_ITEM.java:76-77: "check use item multicast delay exploit cast (spam)"
	EXPECT_FALSE(player().isCasting());
}

TEST_F(UseItemRunTest, TheRestrictionsRefuseAnItemOnCooldown) {
	Item& juice = stored(750005, MERCENARYS_FRUIT_JUICE, 12);
	// usedelayid 21, usedelay 5000 ms (item_templates.xml:821860); the last argument is in seconds, as Player.startCooldown passes it
	player().addItemCoolDown(21, commons::utils::currentTimeMillis() + 5000, 5);

	use(750005);

	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_ITEM_CANT_USE_UNTIL_DELAY_TIME())}));
	EXPECT_EQ(juice.getItemCount(), 12);
}

TEST_F(UseItemRunTest, AFoodItemIsUsedThroughItsSkillAndThenCoolsDown) {
	Item& juice = stored(750006, MERCENARYS_FRUIT_JUICE, 12);

	use(750006);

	// :85-124: the one action, SkillUseAction, can act and acts: the item skill 10034 is cast (instant), its cost is one juice
	// (inventory.decreaseByObjectId(..., DEC_ITEM_USE)) and the item's use delay starts (Player.addItemCoolDown via Skill.startCooldown)
	EXPECT_EQ(juice.getItemCount(), 11);
	std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_FALSE(packetsOf(packets, SM_ITEM_USAGE_ANIMATION_OPCODE).empty());
	ASSERT_EQ(packetsOf(packets, SM_INVENTORY_UPDATE_ITEM_OPCODE).size(), 1u);
	EXPECT_EQ(PacketReader(bodyOf(packetsOf(packets, SM_INVENTORY_UPDATE_ITEM_OPCODE)[0])).D(), 750006);
	EXPECT_TRUE(player().hasCooldown(juice));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
	clearSent();

	use(750006);

	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_ITEM_CANT_USE_UNTIL_DELAY_TIME())})) << "the second use is inside the 5 s delay";
	EXPECT_EQ(juice.getItemCount(), 11);
}

TEST_F(UseItemRunTest, AQuestItemWithoutActionsNoQuestHandlerAcceptsIsNotUsable) {
	// canUseItem lets an item without actions through when the quest engine knows it (PlayerRestrictions.java:318-323); the packet then asks
	// the quest handlers (:89-90), and with none that answers SUCCESS it sends its own STR_ITEM_IS_NOT_USABLE (:92-95). The registration stays
	// in the engine for the rest of this process (ctest runs each case in its own); no other case of this executable uses the item
	questEngine::QuestEngine::getInstance().registerQuestItem(LESSER_ANCIENT_KINAH, 99999);
	Item& ancientKinah = stored(750007, LESSER_ANCIENT_KINAH, 2);

	use(750007);

	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_ITEM_IS_NOT_USABLE())}));
	EXPECT_EQ(ancientKinah.getItemCount(), 2);
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
