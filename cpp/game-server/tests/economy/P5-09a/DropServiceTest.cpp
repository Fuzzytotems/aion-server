// DropService, the solo path (P5-09, m5b3-plan.md L-02, L-05): scheduleFreeForAll, requestDropList, requestDropItem, resendDropList, the
// corpse's decay and deletion, and the lifetime of what a kill registers. The drops come from registerDrop on the fixture's shipped rows
// (DropTestSupport.h) at a forced drop rate (m5b3-plan.md D3); every expectation is DropService.java's, the cited line beside it. Items reach
// the cube through ItemService.addItem (the items lane's T-01); these cases read the inventory, not the storage packets (tests/itemsvc pins
// those).

#include "DropTestSupport.h"

#include <chrono>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "aion/gameserver/configs/detail/ConfigEnums.h"
#include "aion/gameserver/configs/main/DropConfig.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/LiveInstanceCounters.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/services/RespawnService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::economy::test {
namespace {

using namespace std::chrono_literals;
using model::drop::DropItem;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using model::gameobjects::state::CreatureState;
using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using runtime::Ref;
using services::drop::DropRegistrationService;
using services::drop::DropService;

// ServerPacketsOpcodes.java
constexpr int32_t SM_SYSTEM_MESSAGE_OPCODE = 25;
constexpr int32_t SM_EMOTION_OPCODE = 37;
constexpr int32_t SM_LOOT_STATUS_OPCODE = 205;
constexpr int32_t SM_LOOT_ITEMLIST_OPCODE = 206;
constexpr float FORCED_RATE = 1000000.0f;

/** SM_LOOT_STATUS (SM_LOOT_STATUS.java:39-43): LOOT_ENABLE 0, OPEN_DROP_LIST 2, CLOSE_DROP_LIST 3; the loot effect only on LOOT_ENABLE (:26-27) */
std::vector<uint8_t> lootStatus(int32_t target, int32_t status, int32_t lootEffectId = 0) {
	return network::test::PacketWriter().D(target).C(status).D(lootEffectId).data;
}

/** One entry of SM_LOOT_ITEMLIST as it is on the wire (SM_LOOT_ITEMLIST.java:46-56) */
struct ListedEntry {
	int32_t index;
	int32_t itemId;
	int32_t count;
	auto operator<=>(const ListedEntry&) const = default;
};

/**
 * Decodes SM_LOOT_ITEMLIST's body: target, then the entries (order is not Java's, m5b3-plan.md D10: returned sorted by index). The four bytes
 * of each entry after its count (optionalSocket, two zeros, showLootConfirmation) are written by SM_LOOT_ITEMLIST from the item template and
 * the looting team, nothing DropService chooses, and skipped (the M5b-3 gate's decoder, tests/scenario/decoders/ItemDecoders, reads them).
 */
std::set<ListedEntry> decodeLootItemList(const std::vector<uint8_t>& body, int32_t& target) {
	network::test::PacketReader reader(body);
	target = reader.D();
	int32_t n = reader.C();
	std::set<ListedEntry> entries;
	for (int32_t i = 0; i < n; i++) {
		ListedEntry entry{reader.C(), reader.D(), reader.D()};
		for (int32_t skipped = 0; skipped < 4; skipped++)
			reader.C();
		entries.insert(entry);
	}
	return entries;
}

std::set<ListedEntry> entriesAsListed(const std::vector<Ptr<DropItem>>& items) {
	std::set<ListedEntry> entries;
	for (const Ptr<DropItem>& item : items)
		entries.insert({item->getIndex(), item->getDropTemplate()->getItemId(), static_cast<int32_t>(item->getCount())});
	return entries;
}

class DropServiceTest : public DropTest {
protected:
	DropService& service = DropService::getInstance();
	DropRegistrationService& registration = DropRegistrationService::getInstance();

	/** The corpse as the world holds it: stored and spawned in its map instance (World.spawn: region, onAfterSpawn, known list) */
	Npc& spawnCorpse(int32_t npcId, int32_t mapId = POETA) {
		Npc& npc = spawnNpc(npcId, mapId);
		world::World::getInstance().storeObject(npc);
		world::World::getInstance().spawn(Ptr<model::gameobjects::VisibleObject>(npc));
		return npc;
	}

	/**
	 * Both know each other (KnownList.addPair, which pairs spawned objects only: the player is placed beside the corpse and marked spawned, as
	 * World.spawn does), and the see notifications' packets are dropped
	 */
	void know(Npc& npc, Player& player) {
		if (!player.isSpawned()) {
			player.setPosition(world::World::getInstance().createPosition(npc.getWorldId(), 101.0f, 100.0f, 50.0f, int8_t{0}, 1));
			player.getPosition()->setIsSpawned(true);
		}
		ASSERT_TRUE(DropKnownListPairing::pair(npc, player));
		takeSent(player);
	}
};

// ------------------------------------------------------------------------------------------------------------------- scheduleFreeForAll

// DropService.java:54-71: 240 s after the registration the DropNpc becomes free for all and the sighted players get LOOT_ENABLE - every one of
// them for an npc that is no Elyos or Asmodian (:65-66), such as the juvenile sparkie (BEAST)
TEST_F(DropServiceTest, TheFreeForAllOpensTheCorpseToEveryoneAfter240Seconds) {
	DROP_REQUIRE_DATABASE();
	setDropRate(0.0f);
	Player& killer = newPlayer(700001, "Killer");
	Player& elyos = newPlayer(700002, "Bystander");
	Player& asmodian = newPlayer(700003, "Stranger", model::Race::ASMODIANS);
	Npc& npc = spawnCorpse(JUVENILE_SPARKIE);
	know(npc, elyos);
	know(npc, asmodian);
	registration.registerDrop(npc, killer, killer.getLevel(), {});
	Ptr<model::gameobjects::DropNpc> dropNpc = registration.getDropRegistrationMap().get(npc.getObjectId());
	ASSERT_TRUE(dropNpc);
	takeSent(killer);

	executor->advance(239999ms);
	EXPECT_FALSE(dropNpc->isFreeForAll()) << "not before 240,000 ms";
	EXPECT_FALSE(dropNpc->isAllowedToLoot(elyos));
	EXPECT_TRUE(takeSent(elyos).empty());

	executor->advance(1ms);
	EXPECT_TRUE(dropNpc->isFreeForAll());
	EXPECT_TRUE(dropNpc->isAllowedToLoot(elyos)) << "DropNpc.isAllowedToLoot: free for all";
	for (Player* sighted : {&elyos, &asmodian}) {
		std::vector<SentPacket> sent = takeSent(*sighted);
		ASSERT_EQ(sent.size(), 1u) << sighted->getName();
		EXPECT_EQ(sent[0].opcode, SM_LOOT_STATUS_OPCODE);
		EXPECT_EQ(sent[0].body, lootStatus(npc.getObjectId(), 0)) << sighted->getName();
	}
}

// :63-64: an Elyos or Asmodian npc opens only to the other race ("fix for elyos/asmodians being able to loot elyos/asmodian npcs"); latri is
// ELYOS (the drop is registered all the same: an excluded npc only gets no rules)
TEST_F(DropServiceTest, TheFreeForAllOfAnElyosNpcIsAnnouncedToTheOtherRaceOnly) {
	DROP_REQUIRE_DATABASE();
	setDropRate(0.0f);
	Player& killer = newPlayer(700001, "Killer");
	Player& elyos = newPlayer(700002, "Bystander");
	Player& asmodian = newPlayer(700003, "Stranger", model::Race::ASMODIANS);
	Npc& npc = spawnCorpse(LATRI);
	know(npc, elyos);
	know(npc, asmodian);
	registration.registerDrop(npc, killer, killer.getLevel(), {});

	executor->advance(240000ms);

	EXPECT_TRUE(takeSent(elyos).empty()) << "same race as the npc";
	std::vector<SentPacket> sent = takeSent(asmodian);
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent[0].body, lootStatus(npc.getObjectId(), 0));
}

// ---------------------------------------------------------------------------------------------------------------------- requestDropList

// :99-102: a player who is no allowed looter gets STR_LOOT_NO_RIGHT and nothing else - no list, no state change
TEST_F(DropServiceTest, RequestDropListRefusesAPlayerWhoMayNotLoot) {
	DROP_REQUIRE_DATABASE();
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	Player& bystander = newPlayer(700002, "Bystander");
	Npc& npc = spawnCorpse(JUVENILE_SPARKIE);
	registration.registerDrop(npc, killer, killer.getLevel(), {});

	service.requestDropList(bystander, npc.getObjectId());

	std::vector<SentPacket> sent = takeSent(bystander);
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent[0].opcode, SM_SYSTEM_MESSAGE_OPCODE);
	EXPECT_EQ(sent[0].body, bodyFor(bystander, SM_SYSTEM_MESSAGE::STR_LOOT_NO_RIGHT()));
	EXPECT_FALSE(bystander.isLooting());
	EXPECT_FALSE(registration.getDropRegistrationMap().get(npc.getObjectId())->isBeingLooted());
}

// :104-112: while an online player loots the corpse, another allowed looter gets STR_LOOT_FAIL_ONLOOTING
TEST_F(DropServiceTest, RequestDropListRefusesWhileAnotherOnlinePlayerLoots) {
	DROP_REQUIRE_DATABASE();
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	Player& second = newPlayer(700002, "Second");
	Npc& npc = spawnCorpse(JUVENILE_SPARKIE);
	registration.registerDrop(npc, killer, killer.getLevel(), {});
	registration.getDropRegistrationMap().get(npc.getObjectId())->setAllowedLooter(second); // as the free for all would
	service.requestDropList(killer, npc.getObjectId());
	ASSERT_TRUE(killer.isLooting());

	service.requestDropList(second, npc.getObjectId());

	std::vector<SentPacket> sent = takeSent(second);
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent[0].body, bodyFor(second, SM_SYSTEM_MESSAGE::STR_LOOT_FAIL_ONLOOTING()));
	EXPECT_FALSE(second.isLooting());
	EXPECT_EQ(registration.getDropRegistrationMap().get(npc.getObjectId())->getLootingPlayer().get(), &killer);
}

// :114-135: the looter is set, the corpse's DECAY task is cancelled and its remaining delay kept (closeDropList reschedules it), then
// SM_LOOT_ITEMLIST with every entry, SM_LOOT_STATUS(OPEN_DROP_LIST), the LOOTING state and SM_EMOTION(START_LOOT) to the looter and his
// watchers. The sparkie's three entries at a forced rate (DropRegistrationServiceTest) keep the corpse 300 s (RespawnService.WITH_DROP_DECAY).
TEST_F(DropServiceTest, RequestDropListOpensTheListAndKeepsTheRestOfTheDecay) {
	DROP_REQUIRE_DATABASE();
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	Npc& npc = spawnCorpse(JUVENILE_SPARKIE);
	registration.registerDrop(npc, killer, killer.getLevel(), {});
	services::RespawnService::scheduleDecayTask(npc); // NpcController.onDie's order: registerDrop inside doReward, then the decay
	executor->advance(100000ms);
	takeSent(killer);

	service.requestDropList(killer, npc.getObjectId());

	Ptr<model::gameobjects::DropNpc> dropNpc = registration.getDropRegistrationMap().get(npc.getObjectId());
	EXPECT_EQ(dropNpc->getLootingPlayer().get(), &killer);
	EXPECT_FALSE(npc.getController().hasTask(model::TaskId::DECAY)) << "cancelTask(DECAY) (:117)";
	EXPECT_EQ(dropNpc->getRemaingDecayTime(), 200000) << "300,000 ms scheduled, 100,000 ms passed (:119-120)";
	EXPECT_TRUE(killer.isLooting());
	EXPECT_EQ(killer.getLootingNpcOid(), npc.getObjectId());
	std::vector<SentPacket> sent = takeSent(killer);
	ASSERT_EQ(sent.size(), 3u);
	EXPECT_EQ(sent[0].opcode, SM_LOOT_ITEMLIST_OPCODE);
	int32_t target = 0;
	EXPECT_EQ(decodeLootItemList(sent[0].body, target), entriesAsListed(entriesOf(npc)));
	EXPECT_EQ(target, npc.getObjectId());
	EXPECT_EQ(decodeLootItemList(sent[0].body, target).size(), 3u);
	EXPECT_EQ(sent[1].body, lootStatus(npc.getObjectId(), 2)) << "OPEN_DROP_LIST";
	EXPECT_EQ(sent[2].opcode, SM_EMOTION_OPCODE);
	EXPECT_EQ(sent[2].body, bodyFor(killer, SM_EMOTION(killer, model::EmotionType::START_LOOT, 0, npc.getObjectId())));
}

// :96-97: a looter who opens a corpse while the list of another is still open closes that one first (closeDropList :144-186): the first corpse
// loses its looter (:159), gets its DECAY task back with the time it had left (:168), and its allowed looters who see it get LOOT_ENABLE again
// (:184) - all before the second corpse's list opens. The looter sees both corpses (KnownList pairs).
TEST_F(DropServiceTest, OpeningASecondCorpseClosesTheListOfTheFirst) {
	DROP_REQUIRE_DATABASE();
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	Npc& first = spawnCorpse(JUVENILE_SPARKIE);
	Npc& second = spawnCorpse(STRIPED_KERUB);
	know(first, killer);
	know(second, killer);
	registration.registerDrop(first, killer, killer.getLevel(), {});
	services::RespawnService::scheduleDecayTask(first);
	registration.registerDrop(second, killer, killer.getLevel(), {});
	executor->advance(50000ms);
	service.requestDropList(killer, first.getObjectId());
	Ptr<model::gameobjects::DropNpc> firstDrop = registration.getDropRegistrationMap().get(first.getObjectId());
	ASSERT_TRUE(firstDrop);
	ASSERT_EQ(firstDrop->getLootingPlayer().get(), &killer);
	ASSERT_FALSE(first.getController().hasTask(model::TaskId::DECAY)) << "cancelled while its list is open";
	takeSent(killer);

	service.requestDropList(killer, second.getObjectId());

	EXPECT_FALSE(firstDrop->getLootingPlayer()) << "closeDropList: setLootingPlayer(null) (:159)";
	EXPECT_TRUE(first.getController().hasTask(model::TaskId::DECAY)) << "rescheduled with the 250,000 ms it had left";
	EXPECT_EQ(registration.getDropRegistrationMap().get(second.getObjectId())->getLootingPlayer().get(), &killer);
	EXPECT_EQ(killer.getLootingNpcOid(), second.getObjectId());
	std::vector<SentPacket> sent = takeSent(killer);
	ASSERT_GE(sent.size(), 3u);
	// SM_EMOTION (SM_EMOTION.java writeImpl): sender, emotion type, the sender's state when sent (ACTIVE again, :147-148), speed, target
	EXPECT_EQ(sent[0].opcode, SM_EMOTION_OPCODE) << "(:150)";
	network::test::PacketReader emotion(sent[0].body);
	EXPECT_EQ(emotion.D(), killer.getObjectId());
	EXPECT_EQ(emotion.C(), 41) << "END_LOOT (EmotionType.java:49)";
	emotion.H();
	emotion.D();
	EXPECT_EQ(emotion.D(), first.getObjectId()) << "for the first corpse";
	EXPECT_EQ(sent[1].body, lootStatus(first.getObjectId(), 0)) << "LOOT_ENABLE for the first corpse again (:184)";
	EXPECT_EQ(sent[2].opcode, SM_LOOT_ITEMLIST_OPCODE) << "then the second corpse's list";
	int32_t target = 0;
	decodeLootItemList(sent[2].body, target);
	EXPECT_EQ(target, second.getObjectId());
}

// ---------------------------------------------------------------------------------------------------------------------- requestDropItem

// :276-410 and resendDropList :423-440, the solo arm (:340-341): each loot moves the entry into the cube (ItemService.addItem) and removes it
// from the set once nothing of it is left (:392-396), then the looter gets the shorter list; the last one closes the list (CLOSE_DROP_LIST,
// the ACTIVE state, SM_EMOTION(END_LOOT)) and deletes the corpse at once (:436-438) - not 2 s, not 300 s later.
TEST_F(DropServiceTest, LootingEveryEntryFillsTheCubeAndDeletesTheCorpseAtOnce) {
	DROP_REQUIRE_DATABASE();
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	Npc& npc = spawnCorpse(JUVENILE_SPARKIE);
	int32_t npcObjId = npc.getObjectId();
	registration.registerDrop(npc, killer, killer.getLevel(), {});
	service.requestDropList(killer, npcObjId);
	std::vector<Ptr<DropItem>> entries = entriesOf(npc);
	ASSERT_EQ(entries.size(), 3u);
	takeSent(killer);

	for (int32_t index = 1; index <= 3; index++) {
		Ptr<DropItem> entry = entryAt(npc, index);
		ASSERT_TRUE(entry) << index;
		int32_t itemId = entry->getDropTemplate()->getItemId();
		int64_t count = entry->getCount();

		service.requestDropItem(killer, npcObjId, index);

		EXPECT_EQ(killer.getInventory().getItemCountByItemId(itemId), count) << "item " << itemId;
		EXPECT_FALSE(entryAt(npc, index)) << "a fully taken entry leaves the set (:392-396)";
		std::vector<SentPacket> sent = takeSent(killer);
		ASSERT_FALSE(sent.empty());
		if (index < 3) {
			EXPECT_EQ(sent.back().opcode, SM_LOOT_ITEMLIST_OPCODE) << "resendDropList: the shorter list last (:425-428)";
			int32_t target = 0;
			EXPECT_EQ(decodeLootItemList(sent.back().body, target).size(), static_cast<size_t>(3 - index));
		} else {
			ASSERT_GE(sent.size(), 2u);
			EXPECT_EQ(sent[sent.size() - 2].body, lootStatus(npcObjId, 3)) << "CLOSE_DROP_LIST (:431)";
			EXPECT_EQ(sent.back().body, bodyFor(killer, SM_EMOTION(killer, model::EmotionType::END_LOOT, 0, npcObjId))) << "(:434)";
		}
	}

	EXPECT_FALSE(killer.isInState(CreatureState::LOOTING)) << "(:432)";
	EXPECT_TRUE(killer.isInState(CreatureState::ACTIVE)) << "(:433)";
	EXPECT_EQ(killer.getLootingNpcOid(), npcObjId) << "resendDropList keeps it: closeDropList (the client's CM_START_LOOT close) clears it";
	EXPECT_FALSE(world::World::getInstance().findVisibleObject(npcObjId)) << "npc.getController().delete() at once (:436-438)";
	EXPECT_FALSE(registration.getDropRegistrationMap().get(npcObjId)) << "the despawn unregistered the drop (NpcController.onDespawn)";
	EXPECT_FALSE(registration.getCurrentDropMap().get(npcObjId));
}

// :331-339: kinah goes into the cube's kinah count (ItemService.addItem's kinah branch) - the striped kerub's first entry
TEST_F(DropServiceTest, LootedKinahGoesIntoTheKinahCount) {
	DROP_REQUIRE_DATABASE();
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	Npc& npc = spawnCorpse(STRIPED_KERUB);
	registration.registerDrop(npc, killer, killer.getLevel(), {});
	service.requestDropList(killer, npc.getObjectId());
	Ptr<DropItem> kinah = entryAt(npc, 1);
	ASSERT_TRUE(kinah);
	ASSERT_EQ(kinah->getDropTemplate()->getItemId(), KINAH);
	int64_t count = kinah->getCount();

	service.requestDropItem(killer, npc.getObjectId(), 1);

	EXPECT_EQ(killer.getInventory().getKinah(), count);
	EXPECT_FALSE(entryAt(npc, 1));
	EXPECT_EQ(entriesOf(npc).size(), 3u) << "the other entries stay";
}

// :341 hands a solo loot to ItemService.addItem, whose stackable path fills the stacks the cube already holds before it makes a new one
// (ItemService.addStackableItem): the sparkie's power shards (entry 2) looted into a cube that holds 5 Minor Power Shards leave one stack - the
// same item - of 5 + the entry's count (m5b3-plan.md L-05's stack merge), and the entry leaves the set.
TEST_F(DropServiceTest, LootingIntoAStackTheCubeHoldsMergesIntoIt) {
	DROP_REQUIRE_DATABASE();
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	ASSERT_EQ(services::item::ItemService::addItem(killer, MINOR_POWER_SHARD, 5), 0);
	std::vector<Ptr<model::gameobjects::Item>> before = killer.getInventory().getItemsByItemId(MINOR_POWER_SHARD);
	ASSERT_EQ(before.size(), 1u);
	Npc& npc = spawnCorpse(JUVENILE_SPARKIE);
	registration.registerDrop(npc, killer, killer.getLevel(), {});
	service.requestDropList(killer, npc.getObjectId());
	Ptr<DropItem> shards = entryAt(npc, 2);
	ASSERT_TRUE(shards);
	ASSERT_EQ(shards->getDropTemplate()->getItemId(), MINOR_POWER_SHARD);
	int64_t count = shards->getCount();

	service.requestDropItem(killer, npc.getObjectId(), 2);

	std::vector<Ptr<model::gameobjects::Item>> after = killer.getInventory().getItemsByItemId(MINOR_POWER_SHARD);
	ASSERT_EQ(after.size(), 1u) << "merged into the stack, no second stack";
	EXPECT_EQ(after[0].get(), before[0].get());
	EXPECT_EQ(after[0]->getItemCount(), 5 + count);
	EXPECT_FALSE(entryAt(npc, 2));
}

// :304-310: an item whose template has LIMIT_ONE (ItemMask.java:7) is refused to a looter who already holds one, in his cube or in his regular
// warehouse: STR_CAN_NOT_GET_LORE_ITEM with the item's name, nothing added, the entry stays and no new list is sent (the return comes before
// resendDropList). Namus's Diary (a Poeta quest item of rules_map_poeta.xml, mask 20545) stands in each corpse's drop as registerDrop's
// regDropItem adds it (:260-267; the rule's npcs are not in the fixture). A looter without one takes it.
TEST_F(DropServiceTest, ALimitOneItemIsRefusedToALooterWhoHoldsOne) {
	DROP_REQUIRE_DATABASE();
	setDropRate(0.0f);
	Player& killer = newPlayer(700001, "Killer");
	Player& keeper = newPlayer(700002, "Keeper");
	const model::templates::item::ItemTemplate* diary = dataholders::DataManager::ITEM_DATA->getItemTemplate(NAMUS_DIARY);
	ASSERT_TRUE(diary && diary->hasLimitOne());
	auto corpseWithADiary = [&](Player& looter) -> Npc& {
		Npc& npc = spawnCorpse(JUVENILE_SPARKIE);
		registration.registerDrop(npc, looter, looter.getLevel(), {});
		registration.getCurrentDropMap().get(npc.getObjectId())->add(registration.regDropItem(1, 0, npc.getObjectId(), NAMUS_DIARY, 1));
		service.requestDropList(looter, npc.getObjectId());
		takeSent(looter);
		return npc;
	};

	Npc& first = corpseWithADiary(killer);
	service.requestDropItem(killer, first.getObjectId(), 1);
	ASSERT_EQ(killer.getInventory().getItemCountByItemId(NAMUS_DIARY), 1) << "none held: looted";
	EXPECT_FALSE(entryAt(first, 1));
	takeSent(killer);

	Npc& second = corpseWithADiary(killer);
	service.requestDropItem(killer, second.getObjectId(), 1);
	std::vector<SentPacket> sent = takeSent(killer);
	ASSERT_EQ(sent.size(), 1u) << "the refusal alone";
	EXPECT_EQ(sent[0].body, bodyFor(killer, SM_SYSTEM_MESSAGE::STR_CAN_NOT_GET_LORE_ITEM(diary->getL10n()))) << "one in the cube";
	EXPECT_EQ(killer.getInventory().getItemCountByItemId(NAMUS_DIARY), 1);
	EXPECT_TRUE(entryAt(second, 1)) << "the entry stays";

	// the keeper's diary is in his regular warehouse (an inventory row as the DAO loads it: onLoadHandler, no packet)
	Ref<model::gameobjects::Item> stored = model::gameobjects::Item::create(900001, NAMUS_DIARY, 1, std::nullopt, 0, "", 0, 0, false, false, 0,
		model::items::storage::getId(model::items::storage::StorageType::REGULAR_WAREHOUSE), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0);
	keeper.getStorage(model::items::storage::getId(model::items::storage::StorageType::REGULAR_WAREHOUSE))->onLoadHandler(*stored);
	Npc& third = corpseWithADiary(keeper);
	service.requestDropItem(keeper, third.getObjectId(), 1);
	sent = takeSent(keeper);
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent[0].body, bodyFor(keeper, SM_SYSTEM_MESSAGE::STR_CAN_NOT_GET_LORE_ITEM(diary->getL10n()))) << "one in the warehouse";
	EXPECT_EQ(keeper.getInventory().getItemCountByItemId(NAMUS_DIARY), 0);
	EXPECT_TRUE(entryAt(third, 1));
}

// announceDrop (:496-503), called when a loot took anything (:397-398): with gameserver.drop.announce_quality set (the M5b-3 profile sets
// MYTHIC), a looted item of that quality or better is announced to every other player of the looter's race in his map - STR_FORCE_ITEM_WIN
// with the looter's name and the item's chat link - and nothing else is. The saendukal in Inggison drops Kinah (COMMON), Buff Food (COMMON) and
// the Omega Enchantment Stone (MYTHIC, item_templates.xml:838027) at a forced rate. The configured quality is the config's own enum
// (configs/detail/ConfigEnums.h), which the body maps onto ItemQuality by ordinal: a COMMON item under a RARE minimum, a MYTHIC one under MYTHIC
// and a COMMON one under COMMON pin that mapping from both sides.
TEST_F(DropServiceTest, ALootedItemOfTheAnnounceQualityIsAnnouncedToTheLootersRaceInHisMap) {
	DROP_REQUIRE_DATABASE();
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	Player& witness = newPlayer(700002, "Witness");
	Player& stranger = newPlayer(700003, "Stranger", model::Race::ASMODIANS);
	Npc& npc = spawnCorpse(SAENDUKAL, INGGISON);
	// the three players stand in the corpse's map instance (WorldMapInstance.addObject, World.spawn's step that fills the map's player list
	// broadcastToMap reads); the announce quality goes back to unset
	struct InTheMap {
		Ptr<world::WorldMapInstance> instance;
		std::vector<Player*> players;
		~InTheMap() {
			for (Player* player : players)
				instance->removeObject(*player);
			configs::main::DropConfig::MIN_ANNOUNCE_QUALITY.store(std::nullopt);
		}
	} inTheMap{npc.getPosition()->getWorldMapInstance(), {}};
	for (Player* player : {&killer, &witness, &stranger}) {
		player->setPosition(world::World::getInstance().createPosition(INGGISON, 101.0f, 100.0f, 50.0f, int8_t{0}, 1));
		inTheMap.instance->addObject(*player);
		inTheMap.players.push_back(player);
	}
	registration.registerDrop(npc, killer, killer.getLevel(), {});
	service.requestDropList(killer, npc.getObjectId());
	ASSERT_EQ(entriesOf(npc).size(), 3u);
	ASSERT_EQ(entryAt(npc, 3)->getDropTemplate()->getItemId(), OMEGA_ENCHANTMENT_STONE);
	for (Player* player : inTheMap.players)
		takeSent(*player);
	auto announced = [&](Player& receiver, int32_t itemId) {
		return bodyFor(receiver, SM_SYSTEM_MESSAGE::STR_FORCE_ITEM_WIN(killer.getName(), utils::ChatUtil::item(itemId)));
	};
	auto receivedAnnouncement = [&](Player& receiver, int32_t itemId) {
		std::vector<uint8_t> body = announced(receiver, itemId);
		for (const SentPacket& packet : takeSent(receiver))
			if (packet.body == body)
				return true;
		return false;
	};
	configs::main::DropConfig::MIN_ANNOUNCE_QUALITY.store(configs::detail::ItemQuality::RARE);
	service.requestDropItem(killer, npc.getObjectId(), 2); // Buff Food
	EXPECT_TRUE(takeSent(witness).empty()) << "COMMON is below RARE";
	EXPECT_TRUE(takeSent(stranger).empty());

	configs::main::DropConfig::MIN_ANNOUNCE_QUALITY.store(configs::detail::ItemQuality::MYTHIC);
	service.requestDropItem(killer, npc.getObjectId(), 3); // the Omega Enchantment Stone
	std::vector<SentPacket> sent = takeSent(witness);
	ASSERT_EQ(sent.size(), 1u) << "MYTHIC";
	EXPECT_EQ(sent[0].opcode, SM_SYSTEM_MESSAGE_OPCODE);
	EXPECT_EQ(sent[0].body, announced(witness, OMEGA_ENCHANTMENT_STONE));
	EXPECT_TRUE(takeSent(stranger).empty()) << "the other race";
	EXPECT_FALSE(receivedAnnouncement(killer, OMEGA_ENCHANTMENT_STONE)) << "not the looter himself";

	configs::main::DropConfig::MIN_ANNOUNCE_QUALITY.store(configs::detail::ItemQuality::COMMON);
	service.requestDropItem(killer, npc.getObjectId(), 1); // Kinah
	EXPECT_TRUE(receivedAnnouncement(witness, KINAH)) << "COMMON at a COMMON minimum";
	EXPECT_TRUE(takeSent(stranger).empty());
}

// :294-300: an index nobody listed, and a player who may not loot, change nothing (no packet, no item, the entry stays)
TEST_F(DropServiceTest, AnUnknownIndexOrAPlayerWhoMayNotLootChangesNothing) {
	DROP_REQUIRE_DATABASE();
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	Player& bystander = newPlayer(700002, "Bystander");
	Npc& npc = spawnCorpse(JUVENILE_SPARKIE);
	registration.registerDrop(npc, killer, killer.getLevel(), {});
	takeSent(killer);
	ASSERT_TRUE(entryAt(npc, 1));
	int32_t firstItemId = entryAt(npc, 1)->getDropTemplate()->getItemId();

	service.requestDropItem(killer, npc.getObjectId(), 9);
	service.requestDropItem(bystander, npc.getObjectId(), 1);

	EXPECT_EQ(entriesOf(npc).size(), 3u);
	EXPECT_TRUE(takeSent(killer).empty());
	EXPECT_TRUE(takeSent(bystander).empty());
	EXPECT_EQ(bystander.getInventory().getItemCountByItemId(firstItemId), 0);
}

// m5b3-plan.md risk 2 and L-05's lifetime case: kill, loot everything, the corpse goes - and nothing of the drop is left alive: the DropNpc (whose
// lootingPlayer would pin the looter) and every DropItem are reclaimed once the corpse despawned (DropService.unregisterDrop on the despawn,
// NpcController.onDespawn)
TEST_F(DropServiceTest, NothingOfALootedDropOutlivesItsCorpse) {
	DROP_REQUIRE_DATABASE();
	if (!runtime::LIVE_COUNTS_ENABLED)
		GTEST_SKIP() << "live instance counts exist in checked builds only";
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	{
		Npc& npc = spawnCorpse(STRIPED_KERUB);
		int32_t npcObjId = npc.getObjectId();
		registration.registerDrop(npc, killer, killer.getLevel(), {});
		service.requestDropList(killer, npcObjId);
		for (int32_t index = 1; index <= 4; index++)
			service.requestDropItem(killer, npcObjId, index);
		ASSERT_FALSE(world::World::getInstance().findVisibleObject(npcObjId));
	}
	npcs.clear(); // the fixture's own reference to the corpse
	scope.reset(); // a scope pins what it borrowed; the Reclaimer frees the retired objects once no scope can see them (SkillCastTest's pattern)
	runtime::Reclaimer::getInstance().drain();
	scope = std::make_unique<runtime::TaskScope>(AION_TASK_INFO(runtime::TaskKind::TEST));

	std::string live;
	for (const runtime::LiveCount& count : runtime::liveInstancesOf({"DropNpc", "RuntimeDropItem", "DropItem"}))
		live += " " + count.className + "=" + std::to_string(count.live);
	EXPECT_TRUE(live.empty()) << "still alive:" << live;
}

} // namespace
} // namespace aion::gameserver::economy::test
