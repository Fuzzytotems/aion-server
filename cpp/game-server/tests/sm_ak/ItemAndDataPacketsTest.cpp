// P4-16 golden bytes of the server packets that embed data written by other writers (the item info blobs and skill entries of P4-15, compared
// with the bytes those writers produce for the same objects) or that read published static data holders (DataManager.INSTANCE_COOLTIME_DATA,
// DataManager.AUTO_GROUP, bound from XML text for the test). The packet's own fields, masks and slots are written by hand from Java.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/dataholders/AutoGroupData.bind.h"
#include "aion/gameserver/dataholders/AutoGroupData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.bind.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/broker/BrokerRace.h"
#include "aion/gameserver/model/gameobjects/BrokerItem.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob.h"
#include "aion/gameserver/network/aion/serverpackets/SM_AUTO_GROUP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BROKER_SERVICE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EXCHANGE_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GM_SHOW_PLAYER_SKILLS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/skillinfo/SkillEntryWriter.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemAddType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "SmAkTestSupport.h"

namespace aion::gameserver::network::aion::serverpackets::test {
namespace {

using runtime::Ptr;
using runtime::Ref;
using services::item::ItemPacketService_ItemAddType;
using services::item::ItemPacketService_ItemUpdateType;

#define SKIP_IF_UNPORTED(statement)                                                                                                                  \
	try {                                                                                                                                              \
		statement;                                                                                                                                       \
	} catch (const runtime::UnportedException& unported) {                                                                                             \
		GTEST_SKIP() << unported.what();                                                                                                                 \
	}

/** Java ChatUtil.l10n(id) */
std::u16string l10nOf(int32_t id) {
	const uint32_t encoded = static_cast<uint32_t>(id) << 1 | 1;
	return std::u16string{u'$', static_cast<char16_t>(encoded & 0xFFFF), static_cast<char16_t>(encoded >> 16)};
}

/** The bytes a P4-15 writer produces into a buffer of its own */
template <class Write>
std::vector<uint8_t> writtenBy(Write&& write) {
	commons::utils::ByteBuffer buffer = commons::utils::ByteBuffer::allocate(8192);
	write(buffer);
	return std::vector<uint8_t>(buffer.data(), buffer.data() + buffer.position());
}

class ItemPacketTest : public PacketTest {
protected:
	void SetUp() override {
		PacketTest::SetUp();
		// ItemInfoBlob reads the item restriction cleanup data (an empty holder: no restrictions)
		dataholders::DataManager::ITEM_CLEAN_UP.publish(std::make_unique<dataholders::ItemRestrictionCleanupData>());
	}

	void TearDown() override {
		dataholders::DataManager::ITEM_CLEAN_UP.resetForTests();
		PacketTest::TearDown();
	}

	static const model::templates::item::ItemTemplate* itemTemplate() {
		static const model::templates::item::ItemTemplate* bound = [] {
			xml::LoadContext context;
			return xml::bindString<model::templates::item::ItemTemplate>(context, R"(<item_template id="160000001" desc="1200" max_stack_count="100"/>)")
				.release();
		}();
		return bound;
	}
};

TEST_F(ItemPacketTest, InventoryPacketsFrameTheFullItemBlob) {
	PlayerFixture f = makePlayer(100201, 9201, "Owner");
	f.commonData->setNpcExpands(1);
	f.commonData->setQuestExpands(2);
	f.commonData->setItemExpands(3);
	Ref<model::gameobjects::Item> item = model::gameobjects::Item::create(5001, itemTemplate(), 7, false, 12);
	std::vector<uint8_t> blob;
	SKIP_IF_UNPORTED(blob = writtenBy([&](commons::utils::ByteBuffer& buf) { iteminfo::ItemInfoBlob::getFullBlob(f.player, *item)->writeMe(buf); }));
	const int32_t cloth = itemTemplate()->isCloth() ? 1 : 0;

	// SM_INVENTORY_INFO: first packet flag, the three expands, the entries (null items removed like Java's removeAll(singletonList(null)))
	EXPECT_EQ(dataOf(SM_INVENTORY_INFO(true, std::vector<Ptr<model::gameobjects::Item>>{item, nullptr}, *f.player)),
		Bytes().C(1).C(1).C(2).C(3).H(1).D(5001).D(160000001).S(l10nOf(1200)).append(blob).H(12).C(cloth).data);

	// SM_INVENTORY_ADD_ITEM: ITEM_COLLECT with one item in a slot becomes PARTIAL_WITH_SLOT (0x07); the slot as a short
	EXPECT_EQ(dataOf(SM_INVENTORY_ADD_ITEM(std::vector<Ptr<model::gameobjects::Item>>{item}, *f.player, ItemPacketService_ItemAddType::ITEM_COLLECT)),
		Bytes().H(0x07).H(1).D(5001).D(160000001).S(l10nOf(1200)).append(blob).H(12).C(cloth).data);
	EXPECT_EQ(dataOf(SM_INVENTORY_ADD_ITEM(std::vector<Ptr<model::gameobjects::Item>>{item}, *f.player, ItemPacketService_ItemAddType::MAIL)),
		Bytes().H(0x36).H(1).D(5001).D(160000001).S(l10nOf(1200)).append(blob).H(12).C(cloth).data);
	// the first available slot (65535) keeps ITEM_COLLECT (0x19), and (int) (slot & 0xFFFF) is written as a short
	Ref<model::gameobjects::Item> unslotted = model::gameobjects::Item::create(5002, itemTemplate(), 1, false, 65535);
	std::vector<uint8_t> unslottedBlob =
		writtenBy([&](commons::utils::ByteBuffer& buf) { iteminfo::ItemInfoBlob::getFullBlob(f.player, *unslotted)->writeMe(buf); });
	EXPECT_EQ(dataOf(SM_INVENTORY_ADD_ITEM(std::vector<Ptr<model::gameobjects::Item>>{unslotted}, *f.player, ItemPacketService_ItemAddType::ITEM_COLLECT)),
		Bytes().H(0x19).H(1).D(5002).D(160000001).S(l10nOf(1200)).append(unslottedBlob).H(0xFFFF).C(cloth).data);

	// SM_EXCHANGE_ADD_ITEM: action, template id, object id, l10n, blob
	EXPECT_EQ(dataOf(SM_EXCHANGE_ADD_ITEM(1, *item, *f.player)), Bytes().C(1).D(160000001).D(5001).S(l10nOf(1200)).append(blob).data);

	// SM_INVENTORY_UPDATE_ITEM: the default DEC_ITEM_USE writes the full blob and its mask 0x16
	EXPECT_EQ(dataOf(SM_INVENTORY_UPDATE_ITEM(*f.player, *item)), Bytes().D(5001).S(l10nOf(1200)).append(blob).H(0x16).data);
	EXPECT_EQ(dataOf(SM_INVENTORY_UPDATE_ITEM(*f.player, *item, ItemPacketService_ItemUpdateType::STATS_CHANGE)),
		Bytes().D(5001).S(l10nOf(1200)).append(blob).H(0).data);
	// EQUIP_UNEQUIP is internal (not sendable): only the EQUIPPED_SLOT entry and no mask
	std::vector<uint8_t> equippedSlot = writtenBy([&](commons::utils::ByteBuffer& buf) {
		Ref<iteminfo::ItemInfoBlob> slotBlob = iteminfo::ItemInfoBlob::create(f.player, *item);
		slotBlob->addBlobEntry(iteminfo::ItemInfoBlob::ItemBlobType::EQUIPPED_SLOT);
		slotBlob->writeMe(buf);
	});
	EXPECT_EQ(dataOf(SM_INVENTORY_UPDATE_ITEM(*f.player, *item, ItemPacketService_ItemUpdateType::EQUIP_UNEQUIP)),
		Bytes().D(5001).S(l10nOf(1200)).append(equippedSlot).data);
}

TEST_F(PacketTest, PlayerSkillsAreWrittenBySkillEntryWriter) {
	Ref<model::skill::PlayerSkillEntry> first =
		model::skill::PlayerSkillEntry::create(1001, 3, 0, model::gameobjects::Persistable::PersistentState::NOACTION);
	Ref<model::skill::PlayerSkillEntry> second =
		model::skill::PlayerSkillEntry::create(30002, 120, 0, model::gameobjects::Persistable::PersistentState::NOACTION);
	std::vector<uint8_t> entries;
	SKIP_IF_UNPORTED(entries = writtenBy([&](commons::utils::ByteBuffer& buf) {
		skillinfo::SkillEntryWriter::writeSkillEntry(*first, buf);
		skillinfo::SkillEntryWriter::writeSkillEntry(*second, buf);
	}));
	EXPECT_EQ(dataOf(SM_GM_SHOW_PLAYER_SKILLS(std::vector<Ptr<model::skill::PlayerSkillEntry>>{first, second})), Bytes().H(2).append(entries).data);
	EXPECT_EQ(dataOf(SM_GM_SHOW_PLAYER_SKILLS(std::vector<Ptr<model::skill::PlayerSkillEntry>>{})), Bytes().H(0).data);
}

TEST_F(PacketTest, SettledBrokerItemsOfSoldEntries) {
	const commons::database::Timestamp settled(std::chrono::milliseconds(1700000040000LL));
	Ref<model::gameobjects::BrokerItem> sold = model::gameobjects::BrokerItem::create(nullptr, 160000001, 9001, 5, "Maker", 250, 100201,
		model::broker::BrokerRace::ELYOS, true, true, settled, settled, false);
	Ref<model::gameobjects::BrokerItem> unsold = model::gameobjects::BrokerItem::create(nullptr, 160000002, 9002, 2, "", 100, 100201,
		model::broker::BrokerRace::ELYOS, false, true, settled, settled, false);
	// SETTLED_ITEMS(5): kinah, total count, page, 0, size; per item id, sold ? price * count : 0, count twice, settle minutes, 138 zero bytes of
	// the missing item's enchant info, creator
	EXPECT_EQ(dataOf(SM_BROKER_SERVICE(std::vector<Ptr<model::gameobjects::BrokerItem>>{sold, unsold}, 2, 0, 1250)),
		Bytes()
			.C(5)
			.Q(1250)
			.D(2)
			.H(0)
			.C(0)
			.H(2)
			.D(160000001)
			.Q(1250)
			.Q(5)
			.Q(5)
			.D(28333334)
			.zeros(138)
			.S("Maker")
			.D(160000002)
			.Q(0)
			.Q(2)
			.Q(2)
			.D(28333334)
			.zeros(138)
			.S("")
			.data);
	// Java: item.getItemCreator() == null ? 2 : length * 2 + 2, plus 32 and EnchantInfoBlobEntry.SIZE
	EXPECT_EQ(SM_BROKER_SERVICE::SETTLED_ITEMS_DYNAMIC_BODY_PART_SIZE_CALCULATOR(*sold), 32 + 138 + 5 * 2 + 2);
	EXPECT_EQ(SM_BROKER_SERVICE::SETTLED_ITEMS_DYNAMIC_BODY_PART_SIZE_CALCULATOR(*unsold), 32 + 138 + 2);
}

class HolderPacketTest : public PacketTest {
protected:
	void TearDown() override {
		dataholders::DataManager::INSTANCE_COOLTIME_DATA.resetForTests();
		dataholders::DataManager::AUTO_GROUP.resetForTests();
		PacketTest::TearDown();
	}
};

TEST_F(HolderPacketTest, InstanceInfoOfAPlayerWithoutCooldowns) {
	xml::LoadContext context;
	dataholders::DataManager::INSTANCE_COOLTIME_DATA.publish(xml::bindString<dataholders::InstanceCooltimeData>(context,
		R"(<instance_cooltimes>)"
		R"(<instance_cooltime id="11" worldId="300100000" race="PC_ALL" sync_id="1"><maxcount>3</maxcount></instance_cooltime>)"
		R"(<instance_cooltime id="12" worldId="300200000" race="ASMODIANS" sync_id="2"><maxcount>1</maxcount></instance_cooltime>)"
		R"(</instance_cooltimes>)"));
	PlayerFixture f = makePlayer(100202, 9202, "Explorer"); // an Elyos player: ASMODIANS is the opposite race
	TestConnection con;
	ASSERT_TRUE(con->setActivePlayer(Ptr<model::gameobjects::player::Player>(f.player)));

	// one instance: the cooldown id of it; no cooldown: 0 seconds and entry offset 0; hide flag 1 unless the race is the opposite race
	EXPECT_EQ(dataOf(SM_INSTANCE_INFO(int8_t{2}, *f.player, {300100000}), con.get()),
		Bytes().C(2).D(11).C(0).H(1).D(100202).H(1).D(11).D(0).D(0).D(3).D(0).C(1).S("Explorer").data);
	// every instance (the holder's world order): cooldown id 0
	EXPECT_EQ(dataOf(SM_INSTANCE_INFO(int8_t{0}, *f.player), con.get()),
		Bytes().C(0).D(0).C(0).H(1).D(100202).H(2).D(11).D(0).D(0).D(3).D(0).C(1).D(12).D(0).D(0).D(1).D(0).C(0).S("Explorer").data);
	// a world without cooltime: Java NullPointerException
	SM_INSTANCE_INFO unknown(int8_t{2}, *f.player, {999});
	EXPECT_THROW(unknown.serialize(con.get()), runtime::NullPointerException);
}

TEST_F(HolderPacketTest, AutoGroupReadsTheTemplateOfTheMaskId) {
	xml::LoadContext context;
	dataholders::DataManager::AUTO_GROUP.publish(xml::bindString<dataholders::AutoGroupData>(context,
		R"(<auto_groups>)"
		R"(<auto_group id="1" instanceId="300110000" name_id="401193" title_id="401197" min_lvl="46" max_lvl="50"/>)"
		R"(<auto_group id="2" instanceId="300210000" name_id="401675" title_id="401677" min_lvl="51" max_lvl="55"/>)"
		R"(</auto_groups>)"));
	// window 0/7: message (the template's name id), title, 0
	EXPECT_EQ(dataOf(SM_AUTO_GROUP(2)), Bytes().D(2).C(0).D(300210000).D(401675).D(401677).D(0).C(0).S("").data);
	EXPECT_EQ(dataOf(SM_AUTO_GROUP(1, 3, 4, "Leader")), Bytes().D(1).C(3).D(300110000).D(0).D(0).D(4).C(0).S("Leader").data);
	EXPECT_EQ(dataOf(SM_AUTO_GROUP(1, 5)), Bytes().D(1).C(5).D(300110000).D(0).D(0).D(0).C(0).S("").data);
	// the entry icon (6): close ? 0 : 1
	EXPECT_EQ(dataOf(SM_AUTO_GROUP(1, SM_AUTO_GROUP::WND_ENTRY_ICON, true)), Bytes().D(1).C(6).D(300110000).D(401193).D(401197).D(0).C(0).S("").data);
	EXPECT_EQ(dataOf(SM_AUTO_GROUP(1, SM_AUTO_GROUP::WND_ENTRY_ICON, false)), Bytes().D(1).C(6).D(300110000).D(401193).D(401197).D(1).C(0).S("").data);
	// an unknown window writes no block
	EXPECT_EQ(dataOf(SM_AUTO_GROUP(1, 9)), Bytes().D(1).C(9).D(300110000).C(0).S("").data);
	// Java's getAGTByMaskId loop reaches TERATH_DREDGION (mask id 3) without a template before it finds no match: NullPointerException
	EXPECT_THROW(SM_AUTO_GROUP(999), runtime::NullPointerException);
}

} // namespace
} // namespace aion::gameserver::network::aion::serverpackets::test
