#include "aion/gameserver/handlers/playercommands/Easter.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::handlers::playercommands {

AION_PLAYER_COMMAND(Easter);

// parity= private static final Logger log = LoggerFactory.getLogger(Easter.class); (in execute)
// parity= private static final int neededItem = 186000175; (Easter.h)

// Java Easter.java:24-34
const std::vector<Easter::Reward>& Easter::rewards() { // parity: C++ accessor of the static list
	static const auto* const rewards = new std::vector<Reward>{ // parity= private static final List<Reward> rewards = List.of(
		Reward{50, 186000147, 2}, // Mithril Medal // parity= new Reward(50, 186000147, 2),
		Reward{50, 186000055, 3}, // Major Ancient Goblet // parity= new Reward(50, 186000055, 3),
		Reward{75, 166020000, 5}, // Omega Enchantment Stone // parity= new Reward(75, 166020000, 5),
		Reward{75, 188053609, 3}, // [Event] Level 60 Composite Manastone Bundle // parity= new Reward(75, 188053609, 3),
		Reward{75, 166200013, 1}, // Enduring Mythic Weapon Tuning Scroll // parity= new Reward(75, 166200013, 1),
		Reward{75, 188053113, 3}, // Ahserion's Flight Ancient Manastone Bundle // parity= new Reward(75, 188053113, 3),
		Reward{100, 188053295, 1}, // Empyrean Plume Chest // parity= new Reward(100, 188053295, 1),
		Reward{100, 166030005, 5}, // Tempering Solution // parity= new Reward(100, 166030005, 5),
		Reward{300, 188053702, 1} // Vasharti's Equipment Box // parity= new Reward(300, 188053702, 1)
	}; // parity= );
	return *rewards; // parity: (the same)
}

// Java Easter.java:35-44
const std::vector<Easter::Reward>& Easter::randomRewards() { // parity: C++ accessor of the static list
	static const auto* const randomRewards = new std::vector<Reward>{ // parity= private static final List<Reward> randomRewards = List.of(
		Reward{25, 162002030, 10}, // [Event] Premium Restoration Serum // parity= new Reward(25, 162002030, 10),
		Reward{25, 186000237, 50}, // Ancient Coin // parity= new Reward(25, 186000237, 50),
		Reward{25, 162000137, 3}, // Sublime Life Serum // parity= new Reward(25, 162000137, 3),
		Reward{25, 162000139, 3}, // Sublime Mana Serum // parity= new Reward(25, 162000139, 3),
		Reward{25, 186000146, 5}, // Guestpetal // parity= new Reward(25, 186000146, 5),
		Reward{25, 188054198, 1}, // Greater Scroll Bundle // parity= new Reward(25, 188054198, 1),
		Reward{25, 164000126, 10}, // Major Strike Resist Scroll // parity= new Reward(25, 164000126, 10),
		Reward{25, 164000130, 10} // Major Spell Resist Scroll // parity= new Reward(25, 164000130, 10)
	}; // parity= );
	return *randomRewards; // parity: (the same)
}

// Java Easter.java:46-48
Easter::Easter() : PlayerCommand("easter", "Exchanges " + ChatUtil::item(186000175) + " for prizes.", buildSyntaxInfo()) {
}

// Java Easter.java:50-57
std::string Easter::buildSyntaxInfo() {
	std::string syntaxInfo = "Type in .easter <ID> to get your reward:";
	int32_t i = 1;
	syntaxInfo += "\n[" + std::to_string(i++) + "] - (" + std::to_string(randomRewards().front().requiredEggs) + " eggs) Random item"; // parity= syntaxInfo += "\n[" + i++ + "] - (" + randomRewards.getFirst().requiredEggs + " eggs) Random item";
	for (const Reward& r : rewards()) // parity= for (Reward r : rewards)
		syntaxInfo += "\n[" + std::to_string(i++) + "] - (" + std::to_string(r.requiredEggs) + " eggs) " + std::to_string(r.itemCount) + "x " + // parity= syntaxInfo += "\n[" + i++ + "] - (" + r.requiredEggs + " eggs) " + r.itemCount + "x " + ChatUtil.item(r.itemId);
			ChatUtil::item(r.itemId); // parity: (the same)
	return syntaxInfo;
}

// Java Easter.java:59-80
void Easter::execute(Player& player, std::span<const std::string> params) {
	if (params.size() == 0) { // parity= if (params.length == 0) {
		sendInfo(player);
		return;
	}
	int32_t rewardIndex = commons::utils::parseInt(params[0]) - 1; // parity= int rewardIndex = Integer.parseInt(params[0]) - 1;
	if (rewardIndex < 0 || rewardIndex >= static_cast<int32_t>(rewards().size()) + 1) { // parity= if (rewardIndex < 0 || rewardIndex >= rewards.size() + 1) {
		sendInfo(player, "Invalid reward ID.");
		return;
	}
	const Reward& reward = rewardIndex == 0 ? *commons::utils::Rnd::get(randomRewards()) : rewards()[rewardIndex - 1]; // parity= Reward reward = rewardIndex == 0 ? Rnd.get(randomRewards) : rewards.get(rewardIndex - 1);
	int32_t cost = reward.requiredEggs;
	if (player.getInventory().getItemCountByItemId(neededItem) < cost || !player.getInventory().decreaseByItemId(neededItem, cost)) {
		sendInfo(player, "You need " + std::to_string(cost) + " " + ChatUtil::item(neededItem) + " for this."); // parity= sendInfo(player, "You need " + cost + " " + ChatUtil.item(neededItem) + " for this.");
		return;
	}
	runtime::Ref<ItemService::ItemUpdatePredicate> predicate = ItemService::ItemUpdatePredicate::create( // parity: (the predicate of addItem, below)
		services::item::ItemPacketService::ItemAddType::DECOMPOSABLE, services::item::ItemPacketService::ItemUpdateType::INC_CASH_ITEM); // parity: (the same)
	int64_t notAddedCount = ItemService::addItem(player, reward.itemId, reward.itemCount, true, *predicate); // parity= long notAddedCount = ItemService.addItem(player, reward.itemId, reward.itemCount, true, new ItemUpdatePredicate(ItemAddType.DECOMPOSABLE, ItemUpdateType.INC_CASH_ITEM));
	if (notAddedCount > 0) {
		static const auto log = commons::logging::LoggerFactory::getLogger("playercommands.Easter"); // parity: private static final Logger log = LoggerFactory.getLogger(Easter.class);
		log.warn("[Easter Event] " + std::to_string(notAddedCount) + "/" + std::to_string(reward.itemCount) + " of " + std::to_string(reward.itemId) + // parity= log.warn("[Easter Event] " + notAddedCount + "/" + reward.itemCount + " of " + reward.itemId + " could not be added.");
			" could not be added."); // parity: (the same)
	}
}

// parity= private record Reward(int requiredEggs, int itemId, long itemCount) {} (Easter.h)

} // namespace aion::gameserver::handlers::playercommands
