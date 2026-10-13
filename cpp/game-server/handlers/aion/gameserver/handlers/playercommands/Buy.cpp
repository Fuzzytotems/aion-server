#include "aion/gameserver/handlers/playercommands/Buy.h"

#include <functional>

// C++: U+E06C (the coin icon of the client font) in four texts, as the Java literals

#include "aion/gameserver/custom/instance/CustomInstanceService.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::handlers::playercommands {

namespace {

/** Java: the anonymous RequestResponseHandler of buyItem: its acceptRequest body is the lambda (written where Java writes it) */
template <typename F>
struct AcceptHandler final : public RequestResponseHandler {
	AION_MAKE_REF_FRIEND

	const F accept;

	static runtime::Ref<AcceptHandler> create(F accept) { return runtime::makeRef<AcceptHandler>(std::move(accept)); }

	void acceptRequest(runtime::Ptr<Creature> requester, Player& responder) override { accept(requester, responder); }

	explicit AcceptHandler(F accept) : RequestResponseHandler(nullptr), accept(std::move(accept)) {}
	~AcceptHandler() override = default;
};

} // namespace

AION_PLAYER_COMMAND(Buy);

// parity= private static final Map<Integer, Map<String, Integer>> rewards = new LinkedHashMap<>(); (Buy.h)

// Java Buy.java:31-37
Buy::Buy()
	: PlayerCommand("buy", "Exchanges your " + ChatUtil::item(custom::instance::CustomInstanceService::REWARD_COIN_ID) + " for various rewards.",
		  " - Shows all rewards.\n"
		  "<item link|ID> - Buys the respective item.\n") {
	static_cast<void>(rewards()); // parity= initRewards();
}

// Java Buy.java:39-62 (initRewards) and 136-141 (addReward)
const std::vector<Buy::Reward>& Buy::rewards() { // parity: C++ accessor of the static map, filled once (Buy.h)
	static const auto* const rewards = new std::vector<Reward>{ // parity= private void initRewards() {
		Reward{166000194, 40, 5}, // Delta Enchantment Stone // parity= addReward(166000194, 5, 40);
		Reward{166000195, 50, 5}, // Epsilon Enchantment Stone // parity= addReward(166000195, 5, 50);
		Reward{188051516, 100, 1}, // Smart Greater Scroll Bundle // parity= addReward(188051516, 1, 100);
		Reward{162000107, 100, 30}, // Saam King's Herb // parity= addReward(162000107, 30, 100);
		Reward{162000124, 100, 50}, // Superior Recovery Serum // parity= addReward(162000124, 50, 100);
		Reward{188051868, 50, 1}, // [Event] Strongest Inquin Form Candy Pouch // parity= addReward(188051868, 1, 50);
		Reward{188053610, 150, 3}, // [Event] Level 70 Composite Manastone Bundle // parity= addReward(188053610, 3, 150);
		Reward{188053618, 200, 1}, // Honorable Elim's Idian Bundle // parity= addReward(188053618, 1, 200);
		Reward{166500002, 50, 2}, // Amplification Stone // parity= addReward(166500002, 2, 50);
		Reward{166030005, 80, 2}, // Tempering Solution // parity= addReward(166030005, 2, 80);
		Reward{188053295, 200, 1}, // Empyrean Plume Chest // parity= addReward(188053295, 1, 200);
		Reward{188950015, 400, 1}, // Special Courier Pass (Eternal/Lv. 61-65) // parity= addReward(188950015, 1, 400);
		Reward{188950019, 500, 1}, // Special Courier Pass (Mythic/Lv. 61-65) // parity= addReward(188950019, 1, 500);
		Reward{165020015, 1000, 1}, // Armor Wrapping Scroll (Eternal/Lv. 65 and lower) // parity= addReward(165020015, 1, 1000);
		Reward{165020014, 1150, 1}, // Weapon Wrapping Scroll (Eternal/Lv. 65 and lower) // parity= addReward(165020014, 1, 1150);
		Reward{165020021, 1350, 1}, // Noble Armor Wrapping Scroll (Mythic/Lv. 65 and lower) // parity= addReward(165020021, 1, 1350);
		Reward{165020020, 1500, 1}, // Noble Weapon Wrapping Scroll (Mythic/Lv. 65 and lower) // parity= addReward(165020020, 1, 1500);
		Reward{190020175, 1500, 1}, // Tahabata Egg // parity= addReward(190020175, 1, 1500);
		Reward{187060103, 1300, 1}, // Prestige Wings // parity= addReward(187060103, 1, 1300);
		Reward{169610342, 1000, 1}, // [Title] Forgotten Conqueror // parity= addReward(169610342, 1, 1000);
		Reward{190100051, 1200, 1}, // Flying Pagati // parity= addReward(190100051, 1, 1200);
		Reward{188053109, 2500, 1} // Ahserion's Equipment Chest // parity= addReward(188053109, 1, 2500);
	}; // parity: }
	return *rewards; // parity: (the same)
}


// Java Buy.java:64-72
void Buy::execute(Player& player, std::span<const std::string> params) {
	if (params.size() == 0) // parity= if (params.length == 0)
		showRewards(player);
	else if (params.size() == 1) // parity= else if (params.length == 1)
		buyItem(player, params[0]);
	else
		sendInfo(player);
}

// Java Buy.java:74-96
void Buy::showRewards(Player& player) {
	sendInfo(player, "Cost:");
	for (const Reward& idMap : rewards()) { // parity= for (Entry<Integer, Map<String, Integer>> idMap : rewards.entrySet()) {
		std::string itemString;
		std::string cost = std::to_string(idMap.cost); // parity= String cost = String.valueOf(idMap.getValue().get("cost"));
		int32_t amount = idMap.amount; // parity= int amount = idMap.getValue().get("amount");
		int32_t digitsToPad = 4 - static_cast<int32_t>(cost.length()); // parity= int digitsToPad = 4 - cost.length();
		if (digitsToPad > 0)
			cost = std::string(static_cast<size_t>(digitsToPad * 2), ' ') + cost; // parity= cost = " ".repeat(digitsToPad * 2) + cost;

		if (amount == 1)
			itemString = cost + "\uE06C   "; // parity= itemString = cost + "   ";
		else
			itemString = cost + "\uE06C   " + std::to_string(amount) + "x "; // parity= itemString = cost + "   " + amount + "x ";

		itemString += ChatUtil::item(idMap.itemId); // parity= itemString += ChatUtil.item(idMap.getKey());
		sendInfo(player, itemString);
	}
	sendInfo(player, "To buy:");
	sendInfo(player, "1. Right-click an item from the list to your Memo Pad.");
	sendInfo(player, "2. Type " + ChatUtil::color(getAliasWithPrefix(), utils::JavaColor::WHITE) + " in your chat field (with space)."); // parity= sendInfo(player, "2. Type " + ChatUtil.color(getAliasWithPrefix(), Color.WHITE) + " in your chat field (with space).");
	sendInfo(player, "3. (Ctrl + Right-click) the item from your Memo Pad.");
}

// Java Buy.java:98-134
void Buy::buyItem(Player& player, const std::string& itemLink) {
	using custom::instance::CustomInstanceService; // parity: C++ using-declaration
	// redeem item
	int32_t itemId = ChatUtil::getItemId(itemLink);
	if (dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId) == nullptr) { // parity= if (DataManager.ITEM_DATA.getItemTemplate(itemId) == null) {
		sendInfo(player, "\"" + itemLink + "\" is not a valid item (must be an item link or ID).");
		return;
	}

	const Reward* reward = findReward(itemId); // parity: rewards.get(itemId)
	if (reward == nullptr) { // parity= if (!rewards.containsKey(itemId)) {
		sendInfo(player, ChatUtil::item(itemId) + " is not contained in the rewards.");
		return;
	}
	int32_t cost = reward->cost; // parity= int cost = rewards.get(itemId).get("cost");
	int64_t rewardCoins = 0;
	for (runtime::Ptr<model::gameobjects::Item> i : player.getInventory().getItemsByItemId(CustomInstanceService::REWARD_COIN_ID)) // parity= for (Item i : player.getInventory().getItemsByItemId(REWARD_COIN_ID))
		rewardCoins += i->getItemCount();

	if (cost > rewardCoins) {
		sendInfo(player, "You need " + std::to_string(cost) + "\uE06C to buy " + ChatUtil::item(itemId) + "."); // parity= sendInfo(player, "You need " + cost + " to buy " + ChatUtil.item(itemId) + ".");
		return;
	}

	runtime::Ref<Player> buyer(player); // parity: the anonymous class's capture of player
	auto handler = AcceptHandler<std::function<void(runtime::Ptr<Creature>, Player&)>>::create( // parity= RequestResponseHandler<Creature> handler = new RequestResponseHandler<Creature>(null) {
		[this, buyer, itemId, cost](runtime::Ptr<Creature> requester, Player& responder) { // parity= public void acceptRequest(Creature requester, Player responder) {
			static_cast<void>(requester); // parity: (unused, as Java)
			static_cast<void>(responder); // parity: (unused, as Java)
			if (buyer->getInventory().decreaseByItemId(CustomInstanceService::REWARD_COIN_ID, cost)) { // parity= if (player.getInventory().decreaseByItemId(REWARD_COIN_ID, cost)) {
				sendInfo(*buyer, "You have spent " + std::to_string(cost) + "\uE06C."); // parity= sendInfo(player, "You have spent " + cost + ".");
				runtime::Ref<ItemService::ItemUpdatePredicate> predicate = ItemService::ItemUpdatePredicate::create( // parity: (the predicate of addItem, below)
					services::item::ItemPacketService::ItemAddType::DECOMPOSABLE, services::item::ItemPacketService::ItemUpdateType::INC_CASH_ITEM); // parity: (the same)
				ItemService::addItem(*buyer, itemId, findReward(itemId)->amount, true, *predicate); // parity= ItemService.addItem(player, itemId, rewards.get(itemId).get("amount"), true, new ItemUpdatePredicate(ItemAddType.DECOMPOSABLE, ItemUpdateType.INC_CASH_ITEM));
			}
		}); // parity: };
	if (player.getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_AIONJEWEL_SHOP_BUY_CONFIRM, handler))
		PacketSendUtility::sendPacket(player, SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_AIONJEWEL_SHOP_BUY_CONFIRM, 0, 0, cost, // parity= PacketSendUtility.sendPacket(player, new SM_QUESTION_WINDOW(SM_QUESTION_WINDOW.STR_AIONJEWEL_SHOP_BUY_CONFIRM, 0, 0, cost,
			dataholders::DataManager::ITEM_DATA->getItemTemplate(CustomInstanceService::REWARD_COIN_ID)->getL10n())); // parity= DataManager.ITEM_DATA.getItemTemplate(REWARD_COIN_ID).getL10n()));
}


// parity= private void addReward(int itemID, int amount, int cost) { Map<String, Integer> val = new LinkedHashMap<>(); val.put("cost", cost); val.put("amount", amount); rewards.put(itemID, val); }
const Buy::Reward* Buy::findReward(int32_t itemId) { // parity: Java rewards.get(itemId) and containsKey
	for (const Reward& reward : rewards()) // parity: (the same)
		if (reward.itemId == itemId) // parity: (the same)
			return &reward; // parity: (the same)
	return nullptr; // parity: (the same)
}

} // namespace aion::gameserver::handlers::playercommands
