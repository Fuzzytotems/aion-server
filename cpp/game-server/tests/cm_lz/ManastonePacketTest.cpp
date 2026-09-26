// CM_MANASTONE (P5-16, m5b3-plan.md P-05/P-06, D8): C_ENCHANT_ITEM, what the 4.8 client sends to enchant an item (action 1), socket a manastone
// (2), remove a manastone at an npc (3), socket a godstone (4, there is no separate godstone packet: AionClientPacketFactory.java:119) or
// amplify an item (8).
//
// Java: game-server/src/com/aionemu/gameserver/network/aion/clientpackets/CM_MANASTONE.java:38-108.
//
// M5b-3 needs the godstone arm (m5b3-plan.md §2.6): it is driven end to end with the Fx Test Earth Godstone (item_templates.xml:848006) and the
// Training Sword (:375), through ItemSocketService.socketGodstone's 2 s task on the fixture's manual clock. M5c stage 0 ported the bodies the
// other arms reach (m5c-plan.md E-01..E-03: EnchantItemAction's canAct and five-argument act, EnchantService.socketManastone and
// amplifyItem, ItemSocketService.removeManastone), so the cases below pin the packet's own checks and what each arm hands its body: the
// enchant arms socket the stone into the packet's target, into its own slots for the fused slot 1 and into the fused weapon's for any other
// value, and end at a supplement that is no 1661xxxxx item; the remove arm hands on the slot and `targetFusedSlot != 1`; amplification hands
// on the target, the supplement as the material and the stone as the tool. The bodies themselves are tested in tests/itemsvc
// (EnchantServiceTest, EnhanceActionsTest). The remove arm is driven at a spawned npc of npc_templates.xml:37718-37722 (in talk range and out
// of it), the stigma arm with two shipped stigmas (item_templates.xml:742368, :742371); StigmaService.chargeStigma stays AION_UNPORTED (M5e,
// m5c-plan.md §3a).
//
// NOT COVERED, and named so that nobody mistakes the absence for coverage: the (target, stone) order of chargeStigma (its body is unported),
// a real supplement (the fixture has no 1661xxxxx row; EnchantServiceTest covers supplements) and removeManastone's successful removal, which
// writes item_stones (EnchantServiceTest.ARemovedStoneIsDeletedFromTheDatabase covers it against the test database).

#include "../cm_ak/ItemPacketTestSupport.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/PricesConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/items/GodStone.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/clientpackets/CM_MANASTONE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/item/ItemSocketService.h"
#include "aion/gameserver/services/trade/PricesService.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_MANASTONE_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friend CM_MANASTONE.h declares: the fields readImpl decoded, which Java keeps private */
struct CM_MANASTONETestAccess {
	static int32_t npcObjId(const CM_MANASTONE& p) { return p.npcObjId; }
	static int32_t slotNum(const CM_MANASTONE& p) { return p.slotNum; }
	static int32_t actionType(const CM_MANASTONE& p) { return p.actionType; }
	static int32_t targetFusedSlot(const CM_MANASTONE& p) { return p.targetFusedSlot; }
	static int32_t stoneUniqueId(const CM_MANASTONE& p) { return p.stoneUniqueId; }
	static int32_t targetItemUniqueId(const CM_MANASTONE& p) { return p.targetItemUniqueId; }
	static int32_t supplementUniqueId(const CM_MANASTONE& p) { return p.supplementUniqueId; }
};

namespace testing::items {
namespace {

using Access = CM_MANASTONETestAccess;
using network::test::LogCapture;
using serverpackets::SM_SYSTEM_MESSAGE;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";

/** the decoded opcode of ClientPacketInfo.gen.inc:81 (Java AionClientPacketFactory.java:102, State.IN_GAME) */
constexpr int32_t CM_MANASTONE_OPCODE = 74;

/** Java readImpl for actions 1, 2, 4, 8: C action, C fused slot, D target, D stone, D supplement */
std::vector<uint8_t> stoneBody(int32_t action, int32_t fusedSlot, int32_t target, int32_t stone, int32_t supplement) {
	return PacketWriter().C(action).C(fusedSlot).D(target).D(stone).D(supplement).data;
}

/** Java readImpl for action 3: C action, C fused slot, D target, C slot, C (skipped), H (skipped), D npc */
std::vector<uint8_t> removeBody(int32_t fusedSlot, int32_t target, int32_t slot, int32_t npc) {
	return PacketWriter().C(3).C(fusedSlot).D(target).C(slot).C(0x55).H(0x6666).D(npc).data;
}

std::unique_ptr<CM_MANASTONE> readPacket(const std::vector<uint8_t>& data, int32_t& unread) {
	std::vector<uint8_t> copy = data;
	auto packet = std::make_unique<CM_MANASTONE>(CM_MANASTONE_OPCODE, StateSet{AionConnection_State::IN_GAME});
	packet->setBuffer(commons::utils::ByteBuffer::wrap(copy));
	if (!packet->read())
		return nullptr;
	unread = packet->getRemainingBytes();
	return packet;
}

TEST(ManastoneReadTest, TheStoneActionsReadTheStoneAndTheSupplement) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	for (int32_t action : {1, 2, 4, 8}) {
		SCOPED_TRACE("action " + std::to_string(action));
		int32_t unread = -1;
		std::unique_ptr<CM_MANASTONE> p = readPacket(stoneBody(action, 0xF1, 0x01020304, 0x11121314, 0x21222324), unread);
		ASSERT_NE(p, nullptr);
		EXPECT_EQ(Access::actionType(*p), action);
		EXPECT_EQ(Access::targetFusedSlot(*p), 0xF1) << "readUC";
		EXPECT_EQ(Access::targetItemUniqueId(*p), 0x01020304);
		EXPECT_EQ(Access::stoneUniqueId(*p), 0x11121314);
		EXPECT_EQ(Access::supplementUniqueId(*p), 0x21222324);
		EXPECT_EQ(Access::slotNum(*p) + Access::npcObjId(*p), 0);
		EXPECT_EQ(unread, 0);
	}
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(ManastoneReadTest, TheRemoveActionReadsTheSlotAndTheNpc) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	std::unique_ptr<CM_MANASTONE> p = readPacket(removeBody(1, 0x01020304, 0xC3, 0x31323334), unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(Access::actionType(*p), 3);
	EXPECT_EQ(Access::targetFusedSlot(*p), 1);
	EXPECT_EQ(Access::targetItemUniqueId(*p), 0x01020304);
	EXPECT_EQ(Access::slotNum(*p), 0xC3) << "readUC";
	EXPECT_EQ(Access::npcObjId(*p), 0x31323334) << "after a skipped byte and a skipped short";
	EXPECT_EQ(Access::stoneUniqueId(*p) + Access::supplementUniqueId(*p), 0);
	EXPECT_EQ(unread, 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(ManastoneReadTest, AnyOtherActionReadsOnlyTheHeader) {
	int32_t unread = -1;
	std::unique_ptr<CM_MANASTONE> p = readPacket(stoneBody(5, 0, 7, 8, 9), unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(Access::targetItemUniqueId(*p), 7);
	EXPECT_EQ(Access::stoneUniqueId(*p), 0);
	EXPECT_EQ(unread, 8);
}

TEST(ManastoneReadTest, TheMarkerRegistersTheClassUnderItsJavaOpcode) {
	EXPECT_NE(dynamic_cast<CM_MANASTONE*>(CM_MANASTONE_clientPacketFactory(CM_MANASTONE_OPCODE, StateSet{AionConnection_State::IN_GAME}).get()), nullptr);
	int32_t found = 0;
#define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...)                                                                             \
	if (std::string_view(#Class) == "CM_MANASTONE") {                                                                                                   \
		EXPECT_EQ(opcode, CM_MANASTONE_OPCODE);                                                                                                         \
		++found;                                                                                                                                        \
	}
#include "aion/gameserver/network/aion/ClientPacketInfo.gen.inc"
#undef AION_CLIENT_PACKET_INFO
	EXPECT_EQ(found, 1);
}

/** The number of hits of the AION_UNPORTED sites in `file` since the last reset */
uint64_t unportedHitsIn(std::string_view file) {
	uint64_t hits = 0;
	for (const runtime::UnportedHit& hit : runtime::unportedHits()) {
		if (hit.file.find(file) != std::string::npos)
			hits += hit.hits;
	}
	return hits;
}

/**
 * gameserver.rates.manastone_chances for one case, restored at its end. m5c-plan.md D6 gives the gate 200: socketManastone's chance has no
 * cap (EnchantService.java:344-395), so every socketing then succeeds and the case does not depend on Rnd
 */
class ManastoneChancesScope {
public:
	explicit ManastoneChancesScope(float chance) : previous(configs::main::RatesConfig::MANASTONE_CHANCES.get()) {
		configs::main::RatesConfig::MANASTONE_CHANCES.set(std::vector<float>{chance, chance});
	}
	~ManastoneChancesScope() { configs::main::RatesConfig::MANASTONE_CHANCES.set(*previous); }
	ManastoneChancesScope(const ManastoneChancesScope&) = delete;
	ManastoneChancesScope& operator=(const ManastoneChancesScope&) = delete;

private:
	const std::shared_ptr<const std::vector<float>> previous;
};

/** Sets an atomic configuration value for the scope and restores the previous value (the executable loads no properties file) */
template <class T>
class AtomicConfigScope {
public:
	AtomicConfigScope(std::atomic<T>& configValue, T value) : config(configValue), previous(configValue.load()) { config.store(value); }
	~AtomicConfigScope() { config.store(previous); }
	AtomicConfigScope(const AtomicConfigScope&) = delete;
	AtomicConfigScope& operator=(const AtomicConfigScope&) = delete;

private:
	std::atomic<T>& config;
	const T previous;
};

/** (slot, stone item id) of an item's own stones, or of its fusion stones, in slot order */
std::vector<std::pair<int32_t, int32_t>> stonesOf(Item& item, bool fusion = false) {
	std::vector<std::pair<int32_t, int32_t>> stones;
	for (const runtime::Ptr<model::items::ManaStone>& stone : (fusion ? item.getFusionStones() : item.getItemStones())->snapshot())
		stones.emplace_back(stone->getSlot(), stone->getItemId());
	return stones;
}

using Stones = std::vector<std::pair<int32_t, int32_t>>;

/** Whether `packets` holds `packet` byte for byte */
bool containsPacket(const std::vector<std::vector<uint8_t>>& packets, const std::vector<uint8_t>& packet) {
	for (const std::vector<uint8_t>& candidate : packets) {
		if (candidate == packet)
			return true;
	}
	return false;
}

/** the npc of the remove arm's cases */
constexpr int32_t TESTNPC_02 = 207016;

/** npc_templates.xml:37718-37722, verbatim: an Elyos GENERAL npc whose talk distance is 5 (talk_info), without equipment */
constexpr std::string_view TESTNPC_02_XML =
	R"(<npc_template npc_id="207016" level="1" name="testnpc 02" name_id="355747" height="2" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="useitem" srange="20" sangle="300" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="139" />
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" is_dialog="true" can_talk_invisible="false" />
	</npc_template>)";

/**
 * The template of the row, bound through NpcData as the server loads npc_templates.xml (NpcData.init fills the stats the row leaves out). Npc
 * templates are immortal static data: the holder is kept for the process, as DataManager keeps its own, so an npc the Reclaimer frees late
 * never outlives its template.
 */
const model::templates::npc::NpcTemplate* testNpc02Template() {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		return xml::bindString<dataholders::NpcData>(context, "<npc_templates>" + std::string(TESTNPC_02_XML) + "</npc_templates>").release();
	}();
	return holder->getNpcTemplate(TESTNPC_02);
}

/** A spawn template at the coordinates the case chooses */
class ManastoneSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	ManastoneSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

class ManastoneRunTest : public ItemPacketTest {
protected:
	void SetUp() override {
		ItemPacketTest::SetUp();
		runtime::resetUnportedHitsForTests();
		// the row names its AI ("useitem"); whether this executable links that handler or not, the warn mode gives the npc an AI
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
	}

	void TearDown() override {
		if (f.player)
			f.player->setTarget(nullptr);
		npcs.clear(); // before the map instance their positions name
		spawnGroups.clear();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		ItemPacketTest::TearDown();
	}

	void run(const std::vector<uint8_t>& body) {
		Driver<CM_MANASTONE> packet(CM_MANASTONE_OPCODE);
		packet.readAndRun(body, client->get());
	}

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return serializedFor(std::move(packet)); }

	/**
	 * The npc of TESTNPC_02_XML spawned `x` on the x axis in the player's map instance, level with him at (x, 100, 50) (Java
	 * VisibleObjectSpawner.spawnNpc: the known list and the EffectController), and the player's target (clearing what the selection sent)
	 */
	model::gameobjects::Npc& targetNpcAt(float x) {
		runtime::Ref<model::templates::spawns::SpawnGroup> group =
			model::templates::spawns::SpawnGroup::create(210010000, TESTNPC_02, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<ManastoneSpawnTemplate>(*group, x, 100.0f, 50.0f));
		runtime::Ref<model::gameobjects::Npc> npc = model::gameobjects::VisibleObject::create<model::gameobjects::Npc>(
			std::make_unique<controllers::NpcController>(), spawn, testNpc02Template());
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(x, 100.0f, 50.0f)));
		npc->getPosition()->setIsSpawned(true);
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		player().setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*npc));
		clearSent(); // PlayerController.onTargetChanged sent its own packets
		return *npc;
	}

	std::shared_ptr<const std::string> savedMissingAiHandlers;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<model::gameobjects::Npc>> npcs;
};

TEST_F(ManastoneRunTest, ActionFourSocketsTheGodstoneIntoTheWeaponOfTheCube) {
	Item& sword = stored(760001, TRAINING_SWORD, 1);
	Item& stone = stored(760002, FX_TEST_EARTH_GODSTONE, 1);

	run(stoneBody(4, 0, 760001, 760002, 0));

	// CM_MANASTONE.java:95-102 -> ItemSocketService.socketGodstone(player, weapon, stone): the socketing takes 2 s (ItemSocketService.java:193)
	EXPECT_FALSE(sword.hasGodStone());
	EXPECT_EQ(packetsOf(sent(), SM_ITEM_USAGE_ANIMATION_OPCODE).size(), 1u);
	executor->advance(std::chrono::milliseconds(2000));

	ASSERT_TRUE(sword.hasGodStone()) << "the packet's target is the weapon, its stone the godstone";
	EXPECT_EQ(sword.getGodStoneId(), FX_TEST_EARTH_GODSTONE);
	EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(760002)) << "the stone is used up";
	static_cast<void>(stone);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(ManastoneRunTest, ActionFourRefusesAnEquippedWeaponWithItsOwnMessage) {
	equipped(760003, TRAINING_SWORD, 1);
	stored(760004, FX_TEST_EARTH_GODSTONE, 1);

	run(stoneBody(4, 0, 760003, 760004, 0));

	// CM_MANASTONE.java:96-100: only the cube is searched for the weapon; an equipped one gets its own message
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_PROC_CANNOT_GIVE_PROC_TO_EQUIPPED_ITEM())}));
}

TEST_F(ManastoneRunTest, ActionFourWithoutAWeaponSaysThereIsNoTarget) {
	stored(760005, FX_TEST_EARTH_GODSTONE, 1);

	run(stoneBody(4, 0, 760099, 760005, 0));

	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_PROC_NO_TARGET_ITEM())}));
}

TEST_F(ManastoneRunTest, TheEnchantArmsNeedTheStoneAndATarget) {
	stored(760006, MANASTONE_HP_20, 1);

	run(stoneBody(1, 0, 760007, 760099, 0)); // no stone: :68-69 returns silently
	EXPECT_TRUE(sent().empty());

	run(stoneBody(1, 0, 760099, 760006, 0)); // the stone, but no target in the equipment or the cube: each action its own message (:71-74)
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_NO_TARGET_ITEM())}));
	clearSent();
	run(stoneBody(2, 0, 760099, 760006, 0));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_NO_TARGET_ITEM())}));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(ManastoneRunTest, TheEnchantArmsSocketTheStoneIntoTheTargetThroughTheEnchantItemAction) {
	// With a stone and a target (equipped, then of the cube), actions 1 and 2 ask Java's attribute-less EnchantItemAction (:79-80). Its canAct
	// passes for a manastone (167xxxxxx) on a weapon (below 120xxxxxx, EnchantItemAction.java:75-77), whichever of the two actions carries it.
	// A supplement of the cube that is no 1661xxxxx item ends the arm before act (:81-85). act waits 2 s for a manastone
	// (EnchantItemAction.java:91, 114), and with the fused slot 1 socketManastone and socketManastoneAct use the item's own slots
	// (EnchantService.java:302-303, 320-322, 329-331, 412): the stone lands in the packet's target, slot 0, and one of the stack is used each time
	ManastoneChancesScope chances(200.0f);
	Item& stone = stored(760008, MANASTONE_HP_20, 2);
	Item& worn = equipped(760009, TRAINING_SWORD, 1);
	Item& carried = stored(760010, TRAINING_SWORD, 1);

	run(stoneBody(1, 1, 760009, 760008, 760010)); // the carried sword as the supplement: 100000094 / 100000 is no 1661
	EXPECT_TRUE(sent().empty()) << "no animation, no message: the arm returned before act";

	run(stoneBody(1, 1, 760009, 760008, 0));
	EXPECT_EQ(packetsOf(sent(), SM_ITEM_USAGE_ANIMATION_OPCODE).size(), 1u);
	EXPECT_TRUE(stonesOf(worn).empty()) << "the task sockets, 2 s later";
	executor->advance(std::chrono::milliseconds(2000));
	EXPECT_EQ(stonesOf(worn), (Stones{{0, MANASTONE_HP_20}}));
	EXPECT_TRUE(stonesOf(carried).empty());
	EXPECT_EQ(stone.getItemCount(), 1);
	EXPECT_TRUE(containsPacket(sent(), message(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_SUCCEED(worn.getL10n()))));

	clearSent();
	run(stoneBody(2, 1, 760010, 760008, 0));
	executor->advance(std::chrono::milliseconds(2000));
	EXPECT_EQ(stonesOf(carried), (Stones{{0, MANASTONE_HP_20}}));
	EXPECT_EQ(stonesOf(worn), (Stones{{0, MANASTONE_HP_20}})) << "unchanged";
	EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(760008)) << "the last stone of the stack is used up";
	EXPECT_TRUE(containsPacket(sent(), message(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_SUCCEED(carried.getL10n()))));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(ManastoneRunTest, AFusedSlotOtherThanOneSocketsTheFusedWeapon) {
	// CM_MANASTONE.java:86 hands targetFusedSlot to act as its targetWeapon. For any value but 1, socketManastone takes the fused weapon's
	// level, stones and sockets (EnchantService.java:304-306, 323-326, 336-337), and socketManastoneAct puts the stone among the fusion stones
	// (`targetWeapon != 1`, :412). Here the sword carries a second Training Sword as its fused weapon (Item.setFusionedItem, the state an arms fusion leaves). On an
	// item without a fused weapon the same arm is Java's NullPointerException on item.getFusionedItemTemplate() (:306)
	ManastoneChancesScope chances(200.0f);
	stored(760024, MANASTONE_HP_20, 1);
	Item& sword = stored(760025, TRAINING_SWORD, 1);
	sword.setFusionedItem(dataholders::DataManager::ITEM_DATA->getItemTemplate(TRAINING_SWORD), 0, 0);

	run(stoneBody(2, 0, 760025, 760024, 0));
	executor->advance(std::chrono::milliseconds(2000));

	EXPECT_EQ(stonesOf(sword, true), (Stones{{0, MANASTONE_HP_20}}));
	EXPECT_TRUE(stonesOf(sword).empty()) << "not the item's own slots";
	EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(760024));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(ManastoneRunTest, AmplificationHandsTheTargetTheMaterialAndTheToolToAmplifyItem) {
	// CM_MANASTONE.java:104-106: amplifyItem(player, target, the supplement as the material, the stone as the tool). Without the three items:
	// STR_MSG_EXCEED_NO_TARGET_ITEM (EnchantService.java:557-563). Set Test Sword 01 can exceed its enchant and has no max_enchant, so at +0 it is
	// at its maximum (:568-575); with a second one as the material (:576) and any item as the tool it is amplified and both are used up (:580-582)
	run(stoneBody(8, 0, 760011, 760012, 760013));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_NO_TARGET_ITEM())}));
	clearSent();

	Item& target = stored(760011, SET_TEST_SWORD_01, 1);
	stored(760012, MANASTONE_HP_20, 1);	  // the tool
	stored(760013, SET_TEST_SWORD_01, 1); // the material

	run(stoneBody(8, 0, 760011, 760012, 760013));

	EXPECT_TRUE(target.isAmplified());
	EXPECT_TRUE(storage(StorageType::CUBE).getItemByObjId(760011)) << "the target stays";
	EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(760012)) << "the tool is used up";
	EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(760013)) << "the material is used up";
	EXPECT_TRUE(containsPacket(sent(), message(SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_SUCCEED(target.getL10n()))));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(ManastoneRunTest, TheRemoveArmNeedsTheNamedNpcAsTheTarget) {
	// CM_MANASTONE.java:91-93: getTarget().getObjectId() with no target is Java's NullPointerException; a target that is not the npc of the
	// packet (here the player himself) does nothing
	EXPECT_THROW(run(removeBody(1, 760014, 2, 760015)), runtime::NullPointerException);

	player().setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(player()));
	clearSent(); // PlayerController.onTargetChanged sent its own packets
	EXPECT_NO_THROW(run(removeBody(1, 760014, 2, 760015)));
	// the player is no Npc even when the ids match: `visibleObject instanceof Npc npc` is false
	EXPECT_NO_THROW(run(removeBody(1, 760014, 2, player().getObjectId())));

	EXPECT_TRUE(sent().empty()) << "removeManastone is not reached: it would answer each of these with a message";
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(ManastoneRunTest, TheRemoveArmHandsTheSlotAndTheSocketKindToRemoveManastoneAtTheNamedNpcInTalkRange) {
	// CM_MANASTONE.java:91-93: the target is the packet's npc, an Npc, and in talk range - PositionUtil.isInTalkRange: talk_info distance
	// 5 + 1 plus both bound radii, and the npc stands 3 m away - so ItemSocketService.removeManastone(player, target, slot, `targetFusedSlot !=
	// 1`) is asked (ItemSocketService.java:102-138). The superior test sword (two sockets) carries one stone, in slot 1, as an item_stones row
	// loads it. Each call ends at its own refusal, in the order of the body: no such item; no stone in slot 0; no fusion stone for the fused slot
	// 0; and for slot 1 the stone is found and its price (PricesService.getPriceForService(650)) cannot be paid with no kinah, so it stays.
	// The shipped default prices, modifier and taxes (prices.properties:8, :12, :16) give that price; the executable loads no profile
	AtomicConfigScope<int32_t> prices(configs::main::PricesConfig::DEFAULT_PRICES, 100);
	AtomicConfigScope<int32_t> modifier(configs::main::PricesConfig::DEFAULT_MODIFIER, 100);
	AtomicConfigScope<int32_t> taxes(configs::main::PricesConfig::DEFAULT_TAXES, 100);
	ASSERT_GT(services::trade::PricesService::getPriceForService(650, player().getRace()), 0);
	model::gameobjects::Npc& npc = targetNpcAt(103.0f);
	Item& sword = stored(760017, SOUL_BOUND_TEST_SWORD, 1);
	ASSERT_TRUE(services::item::ItemSocketService::addManaStone(runtime::Ptr<Item>(sword), MANASTONE_HP_20, 1, false));

	run(removeBody(1, 760099, 1, npc.getObjectId()));
	run(removeBody(1, 760017, 0, npc.getObjectId()));
	run(removeBody(0, 760017, 1, npc.getObjectId()));
	run(removeBody(1, 760017, 1, npc.getObjectId()));

	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_REMOVE_ITEM_OPTION_NO_TARGET_ITEM()),
						  message(SM_SYSTEM_MESSAGE::STR_REMOVE_ITEM_OPTION_INVALID_OPTION_SLOT_NUMBER(sword.getL10n())),
						  message(SM_SYSTEM_MESSAGE::STR_REMOVE_ITEM_OPTION_NO_OPTION_TO_REMOVE(sword.getL10n())),
						  message(SM_SYSTEM_MESSAGE::STR_REMOVE_ITEM_OPTION_NOT_ENOUGH_GOLD(sword.getL10n()))}));
	EXPECT_EQ(stonesOf(sword), (Stones{{1, MANASTONE_HP_20}}));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(ManastoneRunTest, TheRemoveArmDoesNothingForAnotherNpcIdOrOutOfTalkRange) {
	// the npc in range, but the packet names another object: `visibleObject.getObjectId() == npcObjId` is false (:92)
	model::gameobjects::Npc& near = targetNpcAt(103.0f);
	EXPECT_NO_THROW(run(removeBody(1, 760017, 2, near.getObjectId() + 1)));

	// the named npc 30 m away: isInTalkRange is false (:92)
	model::gameobjects::Npc& far = targetNpcAt(130.0f);
	EXPECT_NO_THROW(run(removeBody(1, 760017, 2, far.getObjectId())));

	EXPECT_TRUE(sent().empty()) << "removeManastone is not reached: it would answer the missing item 760017 with a message";
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(ManastoneRunTest, AStigmaOnAStigmaIsChargedByTheUnportedChargeStigma) {
	// CM_MANASTONE.java:76-77: when the stone and the target are both stigmas, actions 1 and 2 charge the stigma (StigmaService.chargeStigma,
	// M5c, AION_UNPORTED) and make no EnchantItemAction
	stored(760018, SURE_STRIKE_STIGMA, 1);
	stored(760019, SPITE_STRIKE_STIGMA, 1);

	EXPECT_THROW(run(stoneBody(1, 0, 760018, 760019, 0)), runtime::UnportedException);
	EXPECT_THROW(run(stoneBody(2, 0, 760018, 760019, 0)), runtime::UnportedException);

	EXPECT_EQ(unportedHitsIn("StigmaService.cpp"), 2u);
	EXPECT_EQ(unportedHitsIn("EnchantItemAction.cpp"), 0u);
	EXPECT_EQ(runtime::unportedHitCount(), 2u);
	EXPECT_TRUE(sent().empty());
}

TEST_F(ManastoneRunTest, AStigmaIsChargedOnlyWhenBothItemsAreStigmas) {
	// :76 is `stone.isStigma() && targetItem.isStigma()`: a stigma stone on a sword and a manastone on a stigma both go to the EnchantItemAction,
	// whose canAct refuses both without a message (EnchantItemAction.java:75-77: a stigma, 140xxxxxx, is neither a 166/167 stone nor a target
	// below 120xxxxxx), so nothing happens and StigmaService's unported chargeStigma is not asked
	stored(760020, SPITE_STRIKE_STIGMA, 1);
	stored(760021, TRAINING_SWORD, 1);
	stored(760022, MANASTONE_HP_20, 1);
	stored(760023, SURE_STRIKE_STIGMA, 1);

	EXPECT_NO_THROW(run(stoneBody(2, 1, 760021, 760020, 0))) << "a stigma stone on a sword";
	EXPECT_NO_THROW(run(stoneBody(2, 1, 760023, 760022, 0))) << "a manastone on a stigma";

	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(unportedHitsIn("StigmaService.cpp"), 0u);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
	EXPECT_EQ(storage(StorageType::CUBE).getItemByObjId(760022)->getItemCount(), 1) << "the manastone is not used";
}

TEST_F(ManastoneRunTest, AnUnknownActionDoesNothing) {
	stored(760016, TRAINING_SWORD, 1);

	run(stoneBody(5, 0, 760016, 0, 0));

	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
