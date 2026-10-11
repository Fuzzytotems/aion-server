#include "aion/gameserver/handlers/playercommands/Symphony.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::handlers::playercommands {

AION_PLAYER_COMMAND(Symphony);

// parity= private static final Logger log = LoggerFactory.getLogger(Symphony.class); (in execute)
// parity= private static final int REQUIRED_ITEM_ID = 182007170; (Symphony.h)
// parity= private static final int[][] REWARDS = { { 3, 186000236, 10 }, { 5, 186000399, 10 }, { 15, 166000195, 5 }, { 40, 188052388, 1 }, { 50, 188053695, 2 }, { 50, 188053610, 3 }, { 60, 188053321, 1 }, { 65, 188053903, 1 }, { 70, 166020003, 10 }, { 70, 166500005, 10 }, { 70, 166030007, 10 }, { 100, 188950015, 2 }, { 150, 188053099, 1 }, { 200, 188054238, 1 }, { 250, 187000090, 1 }, }; (Symphony.h)

// Java Symphony.java:21-38
const std::array<std::array<int32_t, 3>, 15>& Symphony::rewards() { // parity: C++ accessor of the static array
	static constexpr std::array<std::array<int32_t, 3>, 15> REWARDS{{ // parity: (the Java array, above)
		// COLLECTION_COUNT, REWARD_ID, REWARD_COUNT
		{3, 186000236, 10}, // Blood Mark // parity: (the same)
		{5, 186000399, 10}, // Honorable Conqueror's Mark // parity: (the same)
		{15, 166000195, 5}, // Epsilon Enchantment Stone // parity: (the same)
		{40, 188052388, 1}, // Modor's Equipment Box // parity: (the same)
		{50, 188053695, 2}, // High Grade Crafting Material Box of Conquest // parity: (the same)
		{50, 188053610, 3}, // [Event] Level 70 Composite Manastone Bundle // parity: (the same)
		{60, 188053321, 1}, // [Event] Empyrean Plume Chest // parity: (the same)
		{65, 188053903, 1}, // Honorable Equipment of Conquest Box // parity: (the same)
		{70, 166020003, 10}, // [Event] Omega Enchantment Stone // parity: (the same)
		{70, 166500005, 10}, // [Event] Amplification Stone // parity: (the same)
		{70, 166030007, 10}, // [Event] Tempering Solution // parity: (the same)
		{100, 188950015, 2}, // Special Courier Pass (Eternal/Lv. 61-65) // parity: (the same)
		{150, 188053099, 1}, // Pure Modor's Equipment Crux Box // parity: (the same)
		{200, 188054238, 1}, // Iron Wall Armor Box // parity: (the same)
		{250, 187000090, 1}, // Tiamat's Spectral Wings // parity: (the same)
	}}; // parity: (the same)
	return REWARDS; // parity: (the same)
}

// Java Symphony.java:41-43
Symphony::Symphony() : PlayerCommand("symphony", "Exchanges " + ChatUtil::item(REQUIRED_ITEM_ID) + " for prizes.", buildSyntaxInfo()) {
}

// Java Symphony.java:45-66
void Symphony::execute(Player& player, std::span<const std::string> params) {
	const std::array<std::array<int32_t, 3>, 15>& REWARDS = rewards(); // parity: (the static array)
	if (params.size() == 0) { // parity= if (params.length == 0) {
		sendInfo(player);
		return;
	}
	int32_t rewardIndex = commons::utils::parseInt(params[0]) - 1; // parity= int rewardIndex = Integer.parseInt(params[0]) - 1;
	if (rewardIndex < 0 || rewardIndex >= static_cast<int32_t>(REWARDS.size())) { // parity= if (rewardIndex < 0 || rewardIndex >= REWARDS.length) {
		sendInfo(player, "Invalid prize.");
		return;
	}
	int32_t cost = REWARDS[rewardIndex][0];
	if (player.getInventory().getItemCountByItemId(REQUIRED_ITEM_ID) < cost || !player.getInventory().decreaseByItemId(REQUIRED_ITEM_ID, cost)) {
		sendInfo(player, "You need " + std::to_string(cost) + " " + ChatUtil::item(REQUIRED_ITEM_ID) + " to buy this."); // parity= sendInfo(player, "You need %d %s to buy this.".formatted(cost, ChatUtil.item(REQUIRED_ITEM_ID)));
		return;
	}
	int32_t itemId = REWARDS[rewardIndex][1];
	int32_t itemCount = REWARDS[rewardIndex][2];
	runtime::Ref<ItemService::ItemUpdatePredicate> predicate = ItemService::ItemUpdatePredicate::create( // parity: (the predicate of addItem, below)
		services::item::ItemPacketService::ItemAddType::DECOMPOSABLE, services::item::ItemPacketService::ItemUpdateType::INC_CASH_ITEM); // parity: (the same)
	int64_t notAddedCount = ItemService::addItem(player, itemId, itemCount, true, *predicate); // parity= long notAddedCount = ItemService.addItem(player, itemId, itemCount, true, new ItemUpdatePredicate(ItemAddType.DECOMPOSABLE, ItemUpdateType.INC_CASH_ITEM));
	if (notAddedCount > 0) {
		static const auto log = commons::logging::LoggerFactory::getLogger("playercommands.Symphony"); // parity: private static final Logger log = LoggerFactory.getLogger(Symphony.class);
		log.warn("[Legendary Symphony Event] " + std::to_string(notAddedCount) + "x " + std::to_string(itemId) + " could not be added to " + // parity= log.warn("[Legendary Symphony Event] {}x {} could not be added to {}'s inventory.", notAddedCount, itemId, player.getName());
			player.getName() + "'s inventory."); // parity: (the same)
	}
}

// Java Symphony.java:70-80
std::string Symphony::buildSyntaxInfo() {
	const std::array<std::array<int32_t, 3>, 15>& REWARDS = rewards(); // parity: (the static array)
	std::string sb = "Type .symphony <id> to get your reward:\n"; // parity= StringBuilder sb = new StringBuilder("Type .symphony <id> to get your reward:\n");

	for (size_t i = 0; i < REWARDS.size(); i++) { // parity= for (int i = 0; i < REWARDS.length; i++) {
		const std::array<int32_t, 3>& reward = REWARDS[i]; // parity= int[] reward = REWARDS[i];
		sb += std::to_string(i + 1) + " - (" + std::to_string(reward[0]) + " copies): " + std::to_string(reward[2]) + "x " + // parity= sb.append(i + 1).append(" - (").append(reward[0]).append(" copies): ").append(reward[2]).append("x ").append(ChatUtil.item(reward[1])).append("\n");
			ChatUtil::item(reward[1]) + "\n"; // parity: (the same)
	}

	return sb; // parity= return sb.toString();
}

} // namespace aion::gameserver::handlers::playercommands
