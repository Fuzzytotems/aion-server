// The item info blob pieces that need no Item object (entry ids, sizes, the ItemSlot and StatEnum stand-ins, the stat bonus entry), the score
// writer helpers that read no score, BannedMacEntry and the connection flood filter of GameConnectionFactoryImpl.

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatRateFunction.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"
#include "aion/gameserver/network/BannedMacEntry.h"
#include "aion/gameserver/network/aion/GameConnectionFactoryImpl.h"
#include "aion/gameserver/network/aion/iteminfo/EnchantInfoBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/ItemBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob_ItemBlobTypeInfo.h"
#include "aion/gameserver/network/detail/InstanceInfoWriting.h"
#include "aion/gameserver/network/detail/ItemData.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "FakeGameClient.h"
#include "support/GameServerTestServer.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::test {
namespace {

using aion::iteminfo::ItemInfoBlob_ItemBlobType;
using model::items::ItemSlot;

TEST(ItemInfoTest, EntryIdsAndSizes) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	EXPECT_EQ(getEntryId(ItemInfoBlob_ItemBlobType::GENERAL_INFO), 0x00);
	EXPECT_EQ(getEntryId(ItemInfoBlob_ItemBlobType::PREMIUM_OPTION), 0x10);
	EXPECT_EQ(getEntryId(ItemInfoBlob_ItemBlobType::STAT_BONUSES), 0x0A);
	EXPECT_EQ(getEntryId(ItemInfoBlob_ItemBlobType::CONDITIONING_INFO), 0x0F);

	struct Expected {
		ItemInfoBlob_ItemBlobType type;
		int32_t size;
	};
	// Java getSize() of each entry that does not depend on the item
	const std::vector<Expected> sizes{{ItemInfoBlob_ItemBlobType::SLOTS_WEAPON, 16}, {ItemInfoBlob_ItemBlobType::SLOTS_ARMOR, 20},
		{ItemInfoBlob_ItemBlobType::SLOTS_SHIELD, 20}, {ItemInfoBlob_ItemBlobType::SLOTS_ACCESSORY, 16}, {ItemInfoBlob_ItemBlobType::SLOTS_ARROW, 8},
		{ItemInfoBlob_ItemBlobType::EQUIPPED_SLOT, 8}, {ItemInfoBlob_ItemBlobType::STIGMA_INFO, 306}, {ItemInfoBlob_ItemBlobType::STIGMA_SHARD, 4},
		{ItemInfoBlob_ItemBlobType::PREMIUM_OPTION, 3}, {ItemInfoBlob_ItemBlobType::POLISH_INFO, 4}, {ItemInfoBlob_ItemBlobType::WRAP_INFO, 1},
		{ItemInfoBlob_ItemBlobType::PLUME_INFO, 32}, {ItemInfoBlob_ItemBlobType::STAT_BONUSES, 7}, {ItemInfoBlob_ItemBlobType::ENCHANT_INFO, 138},
		{ItemInfoBlob_ItemBlobType::SLOTS_WING, 16}, {ItemInfoBlob_ItemBlobType::COMPOSITE_ITEM, 6 * 4 + 6},
		{ItemInfoBlob_ItemBlobType::CONDITIONING_INFO, 4}};
	for (const Expected& expected : sizes)
		EXPECT_EQ(aion::iteminfo::newBlobEntry(expected.type)->getSize(), expected.size) << static_cast<int>(expected.type);
	EXPECT_EQ(aion::iteminfo::EnchantInfoBlobEntry::SIZE, 138);

	// an entry that does not read its item: [entry id][int 0]
	commons::utils::ByteBuffer buf = commons::utils::ByteBuffer::allocate(16);
	aion::iteminfo::newBlobEntry(ItemInfoBlob_ItemBlobType::STIGMA_SHARD)->writeMe(buf);
	buf.flip();
	EXPECT_EQ(std::vector<uint8_t>(buf.remainingSpan().begin(), buf.remainingSpan().end()), (PacketWriter().C(0x08).D(0).data));
}

TEST(ItemInfoTest, ItemSlotsOfMasks) {
	// Java ItemSlot.getSlotsFor: the non-combo slots contained in the mask, in ordinal order
	EXPECT_EQ(detail::slotsFor(detail::slotIdMaskOf(ItemSlot::MAIN_OR_SUB)), (std::vector<ItemSlot>{ItemSlot::MAIN_HAND, ItemSlot::SUB_HAND}));
	EXPECT_EQ(detail::slotsFor(1LL << 3), (std::vector<ItemSlot>{ItemSlot::TORSO}));
	EXPECT_EQ(detail::slotFor(detail::slotIdMaskOf(ItemSlot::RING_RIGHT_OR_LEFT)), ItemSlot::RING_LEFT);
	EXPECT_EQ(detail::slotIdMaskOf(ItemSlot::ALL_STIGMA), 0xFC0000000LL);
	EXPECT_EQ(detail::slotsFor(detail::slotIdMaskOf(ItemSlot::VISIBLE)).size(), 15u);
	EXPECT_THROW(detail::slotsFor(0), runtime::IllegalArgumentException);
	EXPECT_THROW(detail::slotFor(1LL << 20), runtime::ArrayIndexOutOfBoundsException); // no slot uses bit 20
	EXPECT_TRUE(detail::isAccessoryArmorGroup(model::templates::item::enums::ItemGroup::POWER_SHARDS));
	EXPECT_FALSE(detail::isAccessoryArmorGroup(model::templates::item::enums::ItemGroup::TORSO));
}

std::vector<uint8_t> bytesOf(commons::utils::ByteBuffer& buf) {
	buf.flip();
	return std::vector<uint8_t>(buf.remainingSpan().begin(), buf.remainingSpan().end());
}

TEST(ItemInfoTest, StatEnumStandInFollowsTheJavaConstructors) {
	using model::stats::container::StatEnum;
	// StatEnum.java: MAXDP(22), ATTACK_SPEED(29, -1), ALLSPEED (no arguments: 0, 1), MAGICAL_ACCURACY(105), MAGIC_SKILL_BOOST_RESIST(126),
	// ABNORMAL_RESISTANCE_ALL(1), BLOCK_PENETRATION (the last constant)
	EXPECT_EQ(detail::itemStoneMaskOf(StatEnum::MAXDP), 22);
	EXPECT_EQ(detail::signOf(StatEnum::MAXDP), 1);
	EXPECT_EQ(detail::itemStoneMaskOf(StatEnum::ATTACK_SPEED), 29);
	EXPECT_EQ(detail::signOf(StatEnum::ATTACK_SPEED), -1);
	EXPECT_EQ(detail::itemStoneMaskOf(StatEnum::ALLSPEED), 0);
	EXPECT_EQ(detail::signOf(StatEnum::ALLSPEED), 1);
	EXPECT_EQ(detail::itemStoneMaskOf(StatEnum::MAGICAL_ACCURACY), 105);
	EXPECT_EQ(detail::itemStoneMaskOf(StatEnum::MAGIC_SKILL_BOOST_RESIST), 126);
	EXPECT_EQ(detail::itemStoneMaskOf(StatEnum::ABNORMAL_RESISTANCE_ALL), 1);
	EXPECT_EQ(detail::itemStoneMaskOf(StatEnum::PVP_DEFEND_RATIO), 107);
	EXPECT_EQ(detail::itemStoneMaskOf(StatEnum::STUN_RESISTANCE_PENETRATION), 81);
	EXPECT_EQ(detail::itemStoneMaskOf(StatEnum::BLOCK_PENETRATION), 0);
	int32_t negativeSigns = 0;
	for (const detail::StatEnumData& data : detail::STAT_ENUM_DATA)
		negativeSigns += data.sign < 0 ? 1 : 0;
	EXPECT_EQ(negativeSigns, 1) << "only ATTACK_SPEED has sign -1";
}

TEST(ItemInfoTest, StatBonusEntry) {
	using model::stats::calc::functions::IStatFunction;
	using model::stats::calc::functions::RcStatFunction;
	using model::stats::calc::functions::StatAddFunction;
	using model::stats::calc::functions::StatRateFunction;
	using model::stats::container::StatEnum;
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<aion::iteminfo::ItemBlobEntry> entry = aion::iteminfo::newBlobEntry(ItemInfoBlob_ItemBlobType::STAT_BONUSES);

	// Java: writeC(entryId 0x0A) writeH(stoneMask) writeD(value * sign) writeC(modifier instanceof StatRateFunction ? 1 : 0)
	runtime::Ref<IStatFunction> rate = RcStatFunction<StatRateFunction>::create(StatEnum::ATTACK_SPEED, 5, true);
	entry->modifier.set(rate);
	commons::utils::ByteBuffer buf = commons::utils::ByteBuffer::allocate(16);
	entry->writeMe(buf);
	EXPECT_EQ(bytesOf(buf), (PacketWriter().C(0x0A).H(29).D(-5).C(1).data));

	runtime::Ref<IStatFunction> add = RcStatFunction<StatAddFunction>::create(StatEnum::PHYSICAL_ATTACK, 100, true);
	entry->modifier.set(add);
	buf.clear();
	entry->writeMe(buf);
	EXPECT_EQ(bytesOf(buf), (PacketWriter().C(0x0A).H(25).D(100).C(0).data));

	runtime::Ref<IStatFunction> overflowing = RcStatFunction<StatAddFunction>::create(StatEnum::ATTACK_SPEED, std::numeric_limits<int32_t>::min(), true);
	entry->modifier.set(overflowing);
	buf.clear();
	entry->writeMe(buf);
	EXPECT_EQ(bytesOf(buf), (PacketWriter().C(0x0A).H(29).D(std::numeric_limits<int32_t>::min()).C(0).data)) << "Java int multiplication wraps";

	entry->modifier.set(nullptr);
	buf.clear();
	EXPECT_THROW(entry->writeMe(buf), runtime::NullPointerException);
}

struct FakeScorePlayer {
	bool dead;
	int32_t objectId;
	bool isDead() const { return dead; }
	int32_t getObjectId() const { return objectId; }
};

TEST(InstanceScoreWriterHelpersTest, SimpleRewardBuffInfoAndEmptyData) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	commons::utils::ByteBuffer buf = commons::utils::ByteBuffer::allocate(512);
	// Java ArenaScoreWriter/HarmonyScoreWriter.writeSimpleReward: writeD(id) writeD((int) count), or two zero ints
	runtime::Ref<model::templates::rewards::RewardItem> reward = model::templates::rewards::RewardItem::create(186000030, 0x100000005LL);
	detail::writeSimpleReward(buf, reward.get());
	detail::writeSimpleReward(buf, nullptr);
	EXPECT_EQ(bytesOf(buf), (PacketWriter().D(186000030).D(5).D(0).D(0).data));

	// Java PvpInstanceScoreWriter.writePlayerBuffInfo: [0][dead ? 60 : 0][objectId] per player, then 12 zero bytes per missing player (of 24)
	buf.clear();
	FakeScorePlayer alive{false, 11};
	FakeScorePlayer dead{true, 12};
	detail::writePlayerBuffInfo(buf, std::vector<const FakeScorePlayer*>{&alive, &dead}, 24);
	std::vector<uint8_t> expected = PacketWriter().D(0).D(0).D(11).D(0).D(60).D(12).data;
	expected.resize(expected.size() + 12 * 22, 0);
	EXPECT_EQ(bytesOf(buf), expected);

	// Java: new byte[dataSize * missingPlayerCount] throws NegativeArraySizeException for more players than the table has
	buf.clear();
	EXPECT_THROW(detail::writeEmptyData(buf, 69, -1), runtime::IllegalArgumentException);
	std::vector<const FakeScorePlayer*> tooMany(25, &alive);
	buf.clear();
	EXPECT_THROW(detail::writePlayerBuffInfo(buf, tooMany, 24), runtime::IllegalArgumentException);
	buf.clear();
	detail::writeEmptyData(buf, 69, 0);
	EXPECT_EQ(buf.position(), 0);
}

TEST(BannedMacEntryTest, ActivityDependsOnTheEndTime) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	const int64_t now = commons::utils::currentTimeMillis();
	runtime::Ref<BannedMacEntry> entry = BannedMacEntry::create("AA-BB-CC-DD-EE-FF", now + 60000);
	EXPECT_EQ(entry->getMac(), "AA-BB-CC-DD-EE-FF");
	EXPECT_TRUE(entry->isActive());
	EXPECT_TRUE(entry->isActiveTill(now + 59999));
	EXPECT_FALSE(entry->isActiveTill(now + 60000));
	entry->updateTime(now - 1);
	EXPECT_FALSE(entry->isActive());
	entry->setDetails("details");
	EXPECT_EQ(entry->getDetails(), "details");

	runtime::Ref<BannedMacEntry> withoutEnd = BannedMacEntry::create("11-22-33-44-55-66", std::nullopt, "");
	EXPECT_FALSE(withoutEnd->getTime().has_value());
	EXPECT_FALSE(withoutEnd->isActive()); // Java: timeEnd != null && ...
	EXPECT_FALSE(withoutEnd->isActiveTill(0));
}

TEST(GameConnectionFactoryImplTest, FloodingHostsAreRejected) {
	using configs::network::NetworkConfig;
	LogCapture logs({"com.aionemu.gameserver.network.aion.GameConnectionFactoryImpl"});
	configureNetworkForTests();
	NetworkConfig::ENABLE_FLOOD_CONNECTIONS = true;
	NetworkConfig::Flood_Tick = 3600000;
	NetworkConfig::Flood_SWARN = 1;
	NetworkConfig::Flood_SReject = 2;
	NetworkConfig::Flood_STick = 1;
	NetworkConfig::Flood_LWARN = 100;
	NetworkConfig::Flood_LReject = 200;
	NetworkConfig::Flood_LTick = 2;
	struct Restore {
		~Restore() { configs::network::NetworkConfig::ENABLE_FLOOD_CONNECTIONS = false; }
	} restore;

	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<aion::GameConnectionFactoryImpl> factory = aion::GameConnectionFactoryImpl::create();
	commons::network::NioServer server(1, {commons::network::ServerCfg{{"127.0.0.1", 0}, "Aion game clients", factory->toConnectionFactory()}});
	server.connect();
	const uint16_t port = server.getBoundAddresses().at(0).port;
	{
		FakeGameClient first(port); // 1: accepted
		first.readKey();
		FakeGameClient second(port); // 2: over the warn limit
		second.readKey();
		EXPECT_TRUE(logs.waitFor("Connection over warn limit from 127.0.0.1")) << logs.dump();
		TestSocket third(port); // 3: over the reject limit: the socket is closed without a packet
		EXPECT_TRUE(third.waitClosed());
		EXPECT_TRUE(logs.waitFor("Rejected connection from 127.0.0.1")) << logs.dump();
	}
	server.shutdown(std::chrono::seconds(2));
}

} // namespace
} // namespace aion::gameserver::network::test
