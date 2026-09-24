// CM_MANASTONE (P5-16, m5b3-plan.md P-05/P-06, D8): C_ENCHANT_ITEM, what the 4.8 client sends to enchant an item (action 1), socket a manastone
// (2), remove a manastone at an npc (3), socket a godstone (4, there is no separate godstone packet: AionClientPacketFactory.java:119) or
// amplify an item (8).
//
// Java: game-server/src/com/aionemu/gameserver/network/aion/clientpackets/CM_MANASTONE.java:38-108.
//
// M5b-3 needs the godstone arm (m5b3-plan.md §2.6): it is driven end to end with the Fx Test Earth Godstone (item_templates.xml:848006) and the
// Training Sword (:375), through ItemSocketService.socketGodstone's 2 s task on the fixture's manual clock. The other arms reach bodies of M5c
// that stay AION_UNPORTED (EnchantItemAction.canAct and its five-argument act, StigmaService.chargeStigma, ItemSocketService.removeManastone,
// EnchantService.amplifyItem): the cases below pin the packet's own checks in front of them and that each arm reaches exactly its own
// unported body, which is the whole of what the packet decides. The remove arm is driven at a spawned npc of npc_templates.xml:37718-37722
// (in talk range and out of it), the stigma arm with two shipped stigmas (item_templates.xml:742368, :742371).
//
// NOT COVERED, and named so that nobody mistakes the absence for coverage: the arguments the arms hand to their unported bodies - the slot
// and `targetFusedSlot != 1` of removeManastone, the (target, stone) order of chargeStigma, the supplement and fused slot of act. An
// AION_UNPORTED body records its site only; they become observable when M5c ports the bodies.

#include "../cm_ak/ItemPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/items/GodStone.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/clientpackets/CM_MANASTONE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
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

TEST_F(ManastoneRunTest, TheEnchantArmsReachTheUnportedEnchantItemAction) {
	// with a stone and a target (equipped, then of the cube), actions 1 and 2 construct an EnchantItemAction and ask canAct (:79-80), which is
	// M5c's (m5b3-plan.md D8): AION_UNPORTED, and nothing of StigmaService or EnchantService is asked
	stored(760008, MANASTONE_HP_20, 1);
	equipped(760009, TRAINING_SWORD, 1);
	stored(760010, TRAINING_SWORD, 1);

	EXPECT_THROW(run(stoneBody(1, 0, 760009, 760008, 0)), runtime::UnportedException);
	EXPECT_THROW(run(stoneBody(2, 0, 760010, 760008, 0)), runtime::UnportedException);

	EXPECT_EQ(unportedHitsIn("EnchantItemAction.cpp"), 2u);
	EXPECT_EQ(runtime::unportedHitCount(), 2u);
	EXPECT_TRUE(sent().empty());
}

TEST_F(ManastoneRunTest, AmplificationReachesTheUnportedEnchantService) {
	EXPECT_THROW(run(stoneBody(8, 0, 760011, 760012, 760013)), runtime::UnportedException);

	EXPECT_EQ(unportedHitsIn("EnchantService.cpp"), 1u);
	EXPECT_EQ(runtime::unportedHitCount(), 1u);
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

	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "removeManastone is not reached";
}

TEST_F(ManastoneRunTest, TheRemoveArmReachesTheUnportedRemoveManastoneAtTheNamedNpcInTalkRange) {
	// CM_MANASTONE.java:91-93: the target is the packet's npc, an Npc, and in talk range - PositionUtil.isInTalkRange: talk_info distance
	// 5 + 1 plus both bound radii, and the npc stands 3 m away - so ItemSocketService.removeManastone (M5c, AION_UNPORTED) is asked
	model::gameobjects::Npc& npc = targetNpcAt(103.0f);

	EXPECT_THROW(run(removeBody(1, 760017, 2, npc.getObjectId())), runtime::UnportedException);

	EXPECT_EQ(unportedHitsIn("ItemSocketService.cpp"), 1u);
	EXPECT_EQ(runtime::unportedHitCount(), 1u);
	EXPECT_TRUE(sent().empty());
}

TEST_F(ManastoneRunTest, TheRemoveArmDoesNothingForAnotherNpcIdOrOutOfTalkRange) {
	// the npc in range, but the packet names another object: `visibleObject.getObjectId() == npcObjId` is false (:92)
	model::gameobjects::Npc& near = targetNpcAt(103.0f);
	EXPECT_NO_THROW(run(removeBody(1, 760017, 2, near.getObjectId() + 1)));

	// the named npc 30 m away: isInTalkRange is false (:92)
	model::gameobjects::Npc& far = targetNpcAt(130.0f);
	EXPECT_NO_THROW(run(removeBody(1, 760017, 2, far.getObjectId())));

	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "removeManastone is not reached";
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
	// :76 is `stone.isStigma() && targetItem.isStigma()`: a stigma stone on a sword and a manastone on a stigma both go to the EnchantItemAction
	stored(760020, SPITE_STRIKE_STIGMA, 1);
	stored(760021, TRAINING_SWORD, 1);
	stored(760022, MANASTONE_HP_20, 1);
	stored(760023, SURE_STRIKE_STIGMA, 1);

	EXPECT_THROW(run(stoneBody(2, 0, 760021, 760020, 0)), runtime::UnportedException) << "a stigma stone on a sword";
	EXPECT_THROW(run(stoneBody(2, 0, 760023, 760022, 0)), runtime::UnportedException) << "a manastone on a stigma";

	EXPECT_EQ(unportedHitsIn("EnchantItemAction.cpp"), 2u);
	EXPECT_EQ(unportedHitsIn("StigmaService.cpp"), 0u);
	EXPECT_EQ(runtime::unportedHitCount(), 2u);
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
