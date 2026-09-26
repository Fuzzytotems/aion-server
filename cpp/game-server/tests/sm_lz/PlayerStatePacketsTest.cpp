// Golden bytes of P4-17 server packets that read a Player's own state or small model objects (SM_PLAYER_STANCE, SM_RENAME, SM_RIDE_ROBOT,
// SM_UPDATE_NOTE, SM_TARGET_UPDATE, SM_PLAYER_REGION, SM_SHOW_NPC_ON_MAP, SM_RECIPE_COOLDOWN, SM_MACRO_LIST, the legion emblem packets,
// SM_SHIELD_EFFECT), written by hand from the Java writeImpls with the opcodes of ServerPacketsOpcodes.java.

#include "SmLzTestSupport.h"

#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
#include "aion/gameserver/model/gameobjects/player/Macros.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/team/legion/LegionEmblem.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_SEND_EMBLEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_EMBLEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LOGIN_QUEUE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MACRO_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_REGION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STANCE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECIPE_COOLDOWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RENAME.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RIDE_ROBOT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SHIELD_EFFECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SHOW_NPC_ON_MAP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TARGET_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UPDATE_NOTE.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::network::aion::serverpackets::testing {
namespace {

using runtime::Ref;

/** Java String.hashCode of an ASCII string: s[0]*31^(n-1) + ... + s[n-1], int overflow (independent of the production helper) */
int32_t javaStringHashCode(std::string_view ascii) {
	uint32_t h = 0;
	for (char c : ascii)
		h = 31u * h + static_cast<uint8_t>(c);
	return static_cast<int32_t>(h);
}

class PlayerStatePacketsTest : public PacketTest {};

TEST_F(PlayerStatePacketsTest, StanceRenameRobotNoteAndTarget) {
	PACKET_TEST_SCOPE;
	PlayerFixture player = makePlayer(100001, 9001, "Viewer");
	// SM_PLAYER_STANCE: writeD(objectId) writeC(state)
	EXPECT_BYTES(serialized(SM_PLAYER_STANCE(*player.player, 1)), Bytes().header(31).D(100001).C(1).data);
	// SM_RENAME(Player, oldName): writeD(isLegion 0) writeD(0) writeD(id) writeS(oldName) writeS(the player's current name)
	EXPECT_BYTES(serialized(SM_RENAME(*player.player, "Before")), Bytes().header(88).D(0).D(0).D(100001).S("Before").S("Viewer").data);
	// SM_RIDE_ROBOT: the player's robot id is read by the constructor
	player.player->setRobotId(7);
	SM_RIDE_ROBOT ride(*player.player);
	player.player->setRobotId(0);
	EXPECT_BYTES(serialized(ride), Bytes().header(92).D(100001).D(7).data);
	EXPECT_BYTES(serialized(SM_RIDE_ROBOT(*player.player, 9)), Bytes().header(92).D(100001).D(9).data);
	// SM_UPDATE_NOTE: the note is read by the constructor
	player.commonData->setNote("hello");
	SM_UPDATE_NOTE note(*player.player);
	player.commonData->setNote("changed");
	EXPECT_BYTES(serialized(note), Bytes().header(104).D(100001).S("hello").data);
	// SM_TARGET_UPDATE without a target: writeD(objectId) writeD(0)
	EXPECT_BYTES(serialized(SM_TARGET_UPDATE(*player.player)), Bytes().header(81).D(100001).D(0).data);
}

TEST_F(PlayerStatePacketsTest, RegionAndNpcOnMap) {
	PACKET_TEST_SCOPE;
	PlayerFixture player = makePlayer(100001, 9001, "Viewer");
	player.player->setPosition(world::WorldPosition::create(210010000, 3.0f, 4.0f, 0.0f, int8_t{7}));
	// SM_PLAYER_REGION: writeD(id) 3x writeC(0) writeD(subZone.name().hashCode())
	const world::zone::ZoneName* zone = world::zone::ZoneName::createOrGet("SUB_TEST_ZONE_210010000");
	EXPECT_BYTES(serialized(SM_PLAYER_REGION(*player.player, zone)),
		Bytes().header(217).D(100001).C(0).C(0).C(0).D(javaStringHashCode("SUB_TEST_ZONE_210010000")).data);
	// Java: subZone.name() on null throws the NullPointerException
	EXPECT_THROW(serialized(SM_PLAYER_REGION(*player.player, nullptr)), runtime::NullPointerException);
	// SM_SHOW_NPC_ON_MAP: another map writes the world id as instance; the player's own map (not an instance, no region: instance id 1)
	// writes worldid + instanceId - 1
	EXPECT_BYTES(serialized(SM_SHOW_NPC_ON_MAP(*player.player, 798000, 220020000, 1.5f, 2.5f, 3.5f)),
		Bytes().header(89).D(798000).D(220020000).D(220020000).F(1.5f).F(2.5f).F(3.5f).data);
	EXPECT_BYTES(serialized(SM_SHOW_NPC_ON_MAP(*player.player, 798001, 210010000, 4.0f, 5.0f, 6.0f)),
		Bytes().header(89).D(798001).D(210010000).D(210010000).F(4.0f).F(5.0f).F(6.0f).data);
}

TEST_F(PlayerStatePacketsTest, RecipeCooldownsInHashMapOrder) {
	PACKET_TEST_SCOPE;
	PlayerFixture player = makePlayer(100001, 9001, "Viewer");
	// empty cooldowns: writeC(mode) writeH(0)
	EXPECT_BYTES(serialized(SM_RECIPE_COOLDOWN(*player.player, 0)), Bytes().header(165).C(0).H(0).data);
	int64_t now = commons::utils::currentTimeMillis();
	player.player->getCraftCooldowns()->put(1500, now + 100'500);
	player.player->getCraftCooldowns()->put(7, now + 50'500);
	player.player->getCraftCooldowns()->put(9, now - 1); // an expired reuse time is removed by put
	// Java HashMap<Integer, Integer> of 16 buckets: 7 (bucket 7) before 1500 (0x5DC, bucket 12); the remaining seconds are truncated
	SM_RECIPE_COOLDOWN packet(*player.player, 1);
	std::vector<uint8_t> bytes = serialized(packet);
	EXPECT_BYTES(bytes, Bytes().header(165).C(1).H(2).D(7).D(50).D(1500).D(100).data);
}

TEST_F(PlayerStatePacketsTest, MacroListWithClearFlag) {
	PACKET_TEST_SCOPE;
	using model::gameobjects::player::Macros;
	std::vector<runtime::Ptr<Macros::Macro>> macros{Macros::Macro::create(1, "<m a=\"1\"/>"), Macros::Macro::create(12, "")};
	// writeD(playerObjectId) writeC(clearList) writeH(-size) then per macro writeC(id) writeS(xml)
	EXPECT_BYTES(serialized(SM_MACRO_LIST(100001, macros, true)),
		Bytes().header(231).D(100001).C(1).H(-2).C(1).S("<m a=\"1\"/>").C(12).S("").data);
	EXPECT_BYTES(serialized(SM_MACRO_LIST(100001, {}, false)), Bytes().header(231).D(100001).C(0).H(0).data);
	// the dynamic body part size of one macro: 1 + UTF-16 length * 2 + 2
	Ref<Macros::Macro> macro = Macros::Macro::create(3, "abc");
	EXPECT_EQ(SM_MACRO_LIST::DYNAMIC_BODY_PART_SIZE_CALCULATOR(*macro), 9);
}

TEST_F(PlayerStatePacketsTest, LegionEmblemsShieldsAndLoginQueue) {
	PACKET_TEST_SCOPE;
	// a new LegionEmblem: id and colors 0, LegionEmblemType.DEFAULT (0x00)
	Ref<model::team::legion::LegionEmblem> emblem = model::team::legion::LegionEmblem::create();
	EXPECT_BYTES(serialized(SM_LEGION_SEND_EMBLEM(5, *emblem, 1024, "Guild")),
		Bytes().header(213).D(5).C(0).C(0).D(1024).C(0).C(0).C(0).C(0).S("Guild").C(1).data);
	EXPECT_BYTES(serialized(SM_LEGION_UPDATE_EMBLEM(5, *emblem)), Bytes().header(215).D(5).C(0).C(0).C(0).C(0).C(0).C(0).data);
	// SM_SHIELD_EFFECT of no locations: writeH(0)
	EXPECT_BYTES(serialized(SM_SHIELD_EFFECT(std::vector<runtime::Ptr<model::siege::SiegeLocation>>{})), Bytes().header(218).H(0).data);
	// SM_LOGIN_QUEUE's only constructor is private (like Java's): not constructible outside the class
	EXPECT_FALSE(std::is_default_constructible_v<SM_LOGIN_QUEUE>);
}

} // namespace
} // namespace aion::gameserver::network::aion::serverpackets::testing
