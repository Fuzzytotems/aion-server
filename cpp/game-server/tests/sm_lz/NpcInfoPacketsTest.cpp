// SM_NPC_INFO and the packets that read an Npc's own state (SM_LOOKATOBJECT, SM_POSITION, SM_MOVE, SM_PLAYER_STATE, SM_SKILL_CANCEL,
// SM_MANTRA_EFFECT, SM_RESURRECT, SM_TRANSFORM): golden bytes written by hand from the Java writeImpls for real Npcs (stat container doubles).
// SM_NPC_INFO branches: overridden creature type, creator and master name (captured by the constructor), no equipment, override equipment in
// ItemSlot order with its mask, a FLAG template (0x13) against a new spawn (0x01), the spawn static id, visual state and target.
//
// Test doubles: the P5-01 stat reads (HP percentage, max HP, movement speed) and TownService go through detail::PacketLookups.

#include "SmLzTestSupport.h"

#include <memory>
#include <string>

#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/controllers/movement/MovementMask.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/dataholders/loadingutils/adapters/NpcEquipmentList.h"
#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/state/CreatureSeeState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LOOKATOBJECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MANTRA_EFFECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_NPC_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_POSITION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RESURRECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_CANCEL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TRANSFORM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TRANSFORM_IN_SUMMON.h"

namespace aion::gameserver::network::aion::serverpackets::testing {
namespace {

using runtime::Ref;

int32_t hpPercentage(model::gameobjects::Creature&) {
	return 77;
}
int32_t maxHp(model::gameobjects::Creature&) {
	return 54321;
}
float movementSpeed(model::gameobjects::Creature&) {
	return 7.5f;
}
int32_t townId(model::gameobjects::Creature&) {
	return 4;
}

const model::templates::npc::NpcTemplate* guardTemplate() {
	static const model::templates::npc::NpcTemplate* guard = npcTemplate(
		R"(<npc_template npc_id="203001" level="25" name_id="301001" title_id="350002" height="1.5" attack_speed="1800" tribe="GUARD" rating="NORMAL" rank="VETERAN">)"
		R"(<bound_radius front="1.25" side="0.75" upper="2.0"/></npc_template>)");
	return guard;
}

const model::templates::npc::NpcTemplate* flagTemplate() {
	static const model::templates::npc::NpcTemplate* flag =
		npcTemplate(R"(<npc_template npc_id="203002" level="1" name_id="301002" type="FLAG"/>)");
	return flag;
}

class NpcInfoPacketsTest : public PacketTest {
protected:
	void SetUp() override {
		PacketTest::SetUp();
		lookups.hpPercentage = &hpPercentage;
		lookups.maxHpCurrent = &maxHp;
		lookups.movementSpeedFloat = &movementSpeed;
		lookups.townIdByPosition = &townId;
		detail::setPacketLookupsForTests(&lookups);
	}

	detail::PacketLookupsForTests lookups;
};

TEST_F(NpcInfoPacketsTest, NpcWithoutEquipment) {
	PACKET_TEST_SCOPE;
	PlayerFixture viewer = makePlayer(100001, 9001, "Viewer");
	Ref<TestNpc> npc = createNpc(guardTemplate(), 0); // after the player: a new spawn for 1.5 s (Creature.isNewSpawn)
	npc->setPosition(world::WorldPosition::create(210010000, 100.5f, 200.25f, 300.0f, int8_t{60}));
	npc->setState(65);
	npc->overrideNpcType(model::CreatureType::FRIEND); // not spawned: no packet
	SM_NPC_INFO packet(*npc, *viewer.player);
	EXPECT_EQ(packet.recipients(), AionServerPacket::Recipients::SHARED);
	Bytes expected;
	expected.header(14).F(100.5f).F(200.25f).F(300.0f).D(npc->getObjectId()).D(203001).D(203001).C(38 /* FRIEND */).H(65).C(60);
	expected.D(301001).D(350002).H(0).C(0).D(0).D(0 /* creator */).S("" /* no master */).C(77).D(54321).C(25).D(0 /* no equipment */);
	expected.F(1.25f /* max(front, side) */).F(1.5f).F(7.5f).H(1800).H(1800).C(0x01 /* new spawn */);
	// NpcMoveController.getTargetX2(): the npc's own position while no move was started
	expected.F(100.5f).F(200.25f).F(300.0f).C(0 /* move type */).H(0 /* static id */).zeros(8).C(0 /* visible */).H(1 /* NORMAL */).C(0);
	expected.D(0 /* no target */).D(4 /* town */).D(0);
	EXPECT_BYTES(serialized(packet), expected.data);
}

TEST_F(NpcInfoPacketsTest, FlagNpcWithMasterGearTargetAndStaticId) {
	PACKET_TEST_SCOPE;
	Ref<TestNpc> npc = createNpc(flagTemplate(), 1234);
	npc->setPosition(world::WorldPosition::create(210010000, 1.0f, 2.0f, 3.0f, int8_t{0}));
	npc->setState(1);
	npc->setCreatorId(100002);
	npc->setMasterName("Owner");
	npc->setVisualState(model::gameobjects::state::CreatureVisualState::HIDE1);
	PlayerFixture viewer = makePlayer(100001, 9001, "Viewer");
	// equipment: a sword (main hand, then sub hand for the second sword) and a torso item; the TreeMap iterates in ItemSlot order
	xml::LoadContext context;
	const auto* sword = xml::bindString<model::templates::item::ItemTemplate>(context, R"(<item_template id="100000011" item_group="SWORD"/>)").release();
	const auto* robe = xml::bindString<model::templates::item::ItemTemplate>(context, R"(<item_template id="110000011" item_group="CL_TORSO"/>)").release();
	auto equipment = std::make_unique<dataholders::loadingutils::adapters::NpcEquipmentList>();
	equipment->items = {robe, sword, sword};
	npc->overrideEquipmentList(std::move(equipment));
	npc->overrideNpcType(model::CreatureType::AGGRESSIVE);
	SM_NPC_INFO packet(*npc, *viewer.player);
	// the constructor captured creator, master name and type: later changes are not sent
	npc->setMasterName("Other");
	npc->overrideNpcType(model::CreatureType::PEACE);
	// (no target: VisibleObject.setTarget reaches NpcGameStats.renewLastChangeTargetTime of P5-01 through the NpcController)
	npc->getMoveController()->movementMask.set(controllers::movement::MovementMask::NPC_RUN_FAST);

	int32_t hide1 = 1; // CreatureVisualState.HIDE1(1)
	Bytes expected;
	expected.header(14).F(1.0f).F(2.0f).F(3.0f).D(npc->getObjectId()).D(203002).D(203002).C(8 /* AGGRESSIVE */).H(1).C(0);
	expected.D(301002).D(0).H(0).C(0).D(0).D(100002).S("Owner").C(77).D(54321).C(1);
	expected.D(1 | 2 | 8); // MAIN_HAND | SUB_HAND | TORSO
	expected.D(100000011).D(0).D(0).H(0).H(0); // MAIN_HAND
	expected.D(100000011).D(0).D(0).H(0).H(0); // SUB_HAND
	expected.D(110000011).D(0).D(0).H(0).H(0); // TORSO
	// no bound_radius: VisibleObjectTemplate returns BoundRadius.DEFAULT (0, 0, 0); height 1 (the template default), attack speed 2000 (default)
	expected.F(0.0f /* BoundRadius.DEFAULT: max(front 0, side 0) */).F(1.0f).F(7.5f).H(2000).H(2000).C(0x13 /* flag */);
	expected.F(1.0f).F(2.0f).F(3.0f).C(controllers::movement::MovementMask::NPC_RUN_FAST).H(1234).zeros(8).C(hide1).H(1).C(0);
	expected.D(0 /* no target */).D(4).D(0);
	EXPECT_BYTES(serialized(packet), expected.data);
}

TEST_F(NpcInfoPacketsTest, LookAtObjectAndPosition) {
	PACKET_TEST_SCOPE;
	Ref<TestNpc> npc = createNpc(guardTemplate(), 0);
	npc->setPosition(world::WorldPosition::create(210010000, 5.5f, 6.5f, 7.5f, int8_t{-3}));
	int32_t npcId = npc->getObjectId();
	// SM_LOOKATOBJECT: the constructor reads the target and heading; writeD(objectId) writeD(targetObjectId) writeC(heading)
	// (no target: setTarget reaches the unported stat calculation of P5-01 through the Npc and Player controllers)
	EXPECT_EQ(serialized(SM_LOOKATOBJECT(*npc)), Bytes().header(40).D(npcId).D(0).C(-3).data);
	// SM_POSITION reads the object when it is serialized
	SM_POSITION position(*npc);
	npc->setPosition(world::WorldPosition::create(210010000, 8.0f, 9.0f, 10.0f, int8_t{12}));
	EXPECT_EQ(serialized(position), Bytes().header(204).D(npcId).F(8.0f).F(9.0f).F(10.0f).C(12).data);
}

TEST_F(NpcInfoPacketsTest, MoveOfAnNpc) {
	PACKET_TEST_SCOPE;
	Ref<TestNpc> npc = createNpc(guardTemplate(), 0);
	npc->setPosition(world::WorldPosition::create(210010000, 8.0f, 9.0f, 10.0f, int8_t{12}));
	int32_t npcId = npc->getObjectId();
	// SM_MOVE for an Npc (no PlayableMoveController): POSITION | MANUAL writes the target coordinates (a not started NpcMoveController
	// returns the npc's position), GLIDE a glide flag of 0, VEHICLE nothing
	npc->getMoveController()->movementMask.set(static_cast<int8_t>(controllers::movement::MovementMask::POSITION | 0x40));
	EXPECT_EQ(serialized(SM_MOVE(*npc)), Bytes().header(55).D(npcId).F(8.0f).F(9.0f).F(10.0f).C(12).C(0xC0).F(8.0f).F(9.0f).F(10.0f).data);
	EXPECT_EQ(serialized(SM_MOVE(*npc, int8_t{0x04})), Bytes().header(55).D(npcId).F(8.0f).F(9.0f).F(10.0f).C(12).C(0x04).C(0).data);
	EXPECT_EQ(serialized(SM_MOVE(*npc, int8_t{0x10})), Bytes().header(55).D(npcId).F(8.0f).F(9.0f).F(10.0f).C(12).C(0x10).data);
	EXPECT_EQ(serialized(SM_MOVE(*npc, int8_t{0x40})), Bytes().header(55).D(npcId).F(8.0f).F(9.0f).F(10.0f).C(12).C(0x40).data)
		<< "MANUAL without POSITION writes no coordinates";
}

TEST_F(NpcInfoPacketsTest, StateSkillCancelAndMantra) {
	PACKET_TEST_SCOPE;
	Ref<TestNpc> npc = createNpc(guardTemplate(), 0);
	int32_t npcId = npc->getObjectId();
	// SM_PLAYER_STATE: visual state, see state (the congenital see state of the NORMAL rating) and the BLINKING flag (CreatureVisualState(64))
	int32_t seeState = npc->getSeeState();
	npc->setVisualState(model::gameobjects::state::CreatureVisualState::BLINKING);
	EXPECT_EQ(serialized(SM_PLAYER_STATE(*npc)), Bytes().header(68).D(npcId).C(64).C(seeState).C(1).data);
	npc->unsetVisualState(model::gameobjects::state::CreatureVisualState::BLINKING);
	EXPECT_EQ(serialized(SM_PLAYER_STATE(*npc)), Bytes().header(68).D(npcId).C(0).C(seeState).C(0).data);
	EXPECT_EQ(serialized(SM_SKILL_CANCEL(*npc, 1801)), Bytes().header(42).D(npcId).H(1801).data);
	EXPECT_EQ(serialized(SM_MANTRA_EFFECT(*npc, 4)), Bytes().header(208).D(0).D(npcId).H(4).data);
	// SM_TRANSFORM of an untransformed npc: model id = template id, TransformType NONE, no restrictions, panel 0
	EXPECT_EQ(serialized(SM_TRANSFORM(*npc)),
		Bytes().header(58).D(npcId).D(203001).H(npc->getState()).F(0.25f).F(2.0f).C(0).D(0).C(0).C(0).C(0).C(0).C(0).C(0).D(0).data);
}

TEST_F(NpcInfoPacketsTest, ResurrectAndTransformInSummon) {
	PACKET_TEST_SCOPE;
	PlayerFixture player = makePlayer(100001, 9001, "Viewer");
	Ref<TestNpc> npc = createNpc(guardTemplate(), 0);
	// SM_RESURRECT: the creature name, writeH(skillId) writeD(0)
	EXPECT_EQ(serialized(SM_RESURRECT(*player.player, 2123)), Bytes().header(194).S("Viewer").H(2123).D(0).data);
	EXPECT_EQ(serialized(SM_RESURRECT(*player.player)), Bytes().header(194).S("Viewer").H(0).D(0).data);
	// SM_TRANSFORM_IN_SUMMON: writeD(summonObject) writeS(player name) writeD(player id)
	EXPECT_EQ(serialized(SM_TRANSFORM_IN_SUMMON(*player.player, *npc)), Bytes().header(156).D(npc->getObjectId()).S("Viewer").D(100001).data);
	EXPECT_EQ(serialized(SM_TRANSFORM_IN_SUMMON(*player.player, 42)), Bytes().header(156).D(42).S("Viewer").D(100001).data);
}

} // namespace
} // namespace aion::gameserver::network::aion::serverpackets::testing
