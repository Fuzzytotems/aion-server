#include "aion/gameserver/handlers/playercommands/Decompose.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/DecomposableItemsData.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/DecomposeAction.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::handlers::playercommands {

/**
 * Java: the anonymous Runnable of Decompose.startTask (Decompose.java:54-91, its fields and run) together with the anonymous ItemUseObserver of
 * its initializer (Decompose.java:60-72), see the header. The scheduled task holds it (Ref) and the player's ObserveController holds it as
 * observer until cancelTask removes it (the logout breaker's ObserveController::clearWithoutNotify drops it). The command is a registry
 * singleton that outlives it.
 */
struct Decompose_Task final : ItemUseObserver {
	AION_MAKE_REF_FRIEND

	Decompose& command;                      // the enclosing instance (cancelTask)
	const runtime::Ref<Player> player;       // captured param Player player
	const int32_t itemId;                    // captured param int itemId
	const DecomposeAction& decomposeAction;  // captured param DecomposeAction decomposeAction (static data)
	runtime::Field<int64_t> remainingCount;  // long remainingCount = count
	runtime::Field<int64_t> totalCount;      // long totalCount = 0

	/** the Runnable's construction: its fields (the constructor) and its initializer's addObserver */
	static runtime::Ref<Decompose_Task> create(Decompose& command, Player& player, int32_t itemId, int64_t count, const DecomposeAction& decomposeAction);

	void itemused(Item& item) override;

	void abort() override;

	void run();

protected:
	Decompose_Task(Decompose& commandValue, Player& playerValue, int32_t itemIdValue, int64_t count, const DecomposeAction& decomposeActionValue);
	~Decompose_Task() override = default;
};

AION_PLAYER_COMMAND(Decompose);

Decompose::Decompose()
	: PlayerCommand("decompose", "Opens decomposable items.",
		  "<item> [count] - Decomposes the specified item (default: all, optional: number of items to decompose).\n") {
}

// Java Decompose.java:30-52
void Decompose::execute(Player& player, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(player);
		return;
	}

	runtime::Ptr<Item> item = player.getInventory().getFirstItemByItemId(ChatUtil::getItemId(params[0]));
	if (item == nullptr) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_NO_TARGET_ITEM());
		return;
	}
	int64_t count = params.size() == 1 ? std::numeric_limits<int64_t>::max() : commons::utils::parseLong(params[1]); // parity= long count = params.length == 1 ? Long.MAX_VALUE : Long.parseLong(params[1]);
	const ItemActions* itemActions = item->getItemTemplate()->getActions();
	const DecomposeAction* decomposeAction = nullptr; // parity= DecomposeAction decomposeAction = itemActions == null ? null
	if (itemActions != nullptr) { // parity: (the same statement)
		for (const std::unique_ptr<AbstractItemAction>& a : itemActions->getItemActions()) { // parity= : itemActions.getItemActions().stream().filter(a -> a instanceof DecomposeAction).map(DecomposeAction.class::cast).findAny().orElse(null);
			if ((decomposeAction = dynamic_cast<const DecomposeAction*>(a.get())) != nullptr) // parity: (the same statement; findAny of a sequential stream is the first)
				break; // parity: (the same statement)
		}
	}
	if (decomposeAction == nullptr || DataManager::DECOMPOSABLE_ITEMS_DATA->getSelectableItems(item->getItemId()) != std::nullopt) { // exclude selectable decomposables
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_IT_CAN_NOT_BE_DECOMPOSED(item->getItemTemplate()->getL10n()));
		return;
	}
	if (!decomposeAction->canAct(player, item, item))
		return;
	startTask(player, item->getItemId(), count, *decomposeAction);
}

// Java Decompose.java:54-91: the Runnable (Decompose_Task, see the header) is created before scheduleAtFixedRate schedules it
void Decompose::startTask(Player& player, int32_t itemId, int64_t count, const DecomposeAction& decomposeAction) {
	runtime::Ref<Decompose_Task> task = Decompose_Task::create(*this, player, itemId, count, decomposeAction); // parity: the new Runnable() of the next line, compared at Decompose_Task's members below
	player.getController().addTask(TaskId::SKILL_USE, ThreadPoolManager::getInstance().scheduleAtFixedRate({&player, task.get()}, [task] { task->run(); }, // parity= player.getController().addTask(TaskId.SKILL_USE, ThreadPoolManager.getInstance().scheduleAtFixedRate(new Runnable() {
		10, DataManager::ITEM_DATA->getItemTemplate(itemId)->getCastingDelay() + 100)); // parity: compared at the closing brace of Decompose_Task::run
}

// Java Decompose.java:55-60: the Runnable's fields and the start of its initializer
Decompose_Task::Decompose_Task(Decompose& commandValue, Player& playerValue, int32_t itemIdValue, int64_t count, const DecomposeAction& decomposeActionValue) // parity: the captures
	: command(commandValue), player(runtime::Ref<Player>(playerValue)), itemId(itemIdValue), decomposeAction(decomposeActionValue), remainingCount(count), // parity: long remainingCount = count; and the captures
	  totalCount(0) { // parity= long totalCount = 0; ItemUseObserver observer; { observer = new ItemUseObserver() {
}

// Java Decompose.java:62-66: use observer to abort task on move, attack, die, item use, etc.
void Decompose_Task::itemused(Item& item) {
	if (item.getItemId() != itemId)
		abort();
}

// Java Decompose.java:68-71
void Decompose_Task::abort() {
	command.cancelTask(*player, *this, "Decomposing aborted: Processed " + std::to_string(std::max(int64_t{0}, totalCount.get() - 1)) + "x " + ChatUtil::item(itemId) + "."); // parity= cancelTask(player, observer, "Decomposing aborted: Processed " + Math.max(0, totalCount - 1) + "x " + ChatUtil.item(itemId) + ".");
}

// Java Decompose.java:73-74: the end of the initializer
runtime::Ref<Decompose_Task> Decompose_Task::create(Decompose& command, Player& player, int32_t itemId, int64_t count, const DecomposeAction& decomposeAction) { // parity: the Runnable's construction
	runtime::Ref<Decompose_Task> runnable = runtime::makeRef<Decompose_Task>(command, player, itemId, count, decomposeAction); // parity: (the same)
	player.getObserveController()->addObserver(*runnable); // parity= player.getObserveController().addObserver(observer);
	return runnable; // parity: (the same)
}

// Java Decompose.java:76-90
void Decompose_Task::run() { // parity: the Runnable's run, which the comparison drops as the lambda's

	runtime::Ptr<Item> item = player->getInventory().getFirstItemByItemId(itemId);
	remainingCount.set(std::min(player->getInventory().getItemCountByItemId(itemId), remainingCount.get())); // parity= remainingCount = Math.min(player.getInventory().getItemCountByItemId(itemId), remainingCount);
	if (item == nullptr || remainingCount.get() <= 0) { // parity= if (item == null || remainingCount <= 0) {
		command.cancelTask(*player, *this, "Decomposing finished: Processed " + std::to_string(totalCount.get()) + "x " + ChatUtil::item(itemId) + "."); // parity= cancelTask(player, observer, "Decomposing finished: Processed " + totalCount + "x " + ChatUtil.item(itemId) + ".");
		return;
	}
	if (!decomposeAction.canAct(*player, item, item)) {
		command.cancelTask(*player, *this, "Decomposing aborted: Processed " + std::to_string(totalCount.get()) + "x " + ChatUtil::item(itemId) + "."); // parity= cancelTask(player, observer, "Decomposing aborted: Processed " + totalCount + "x " + ChatUtil.item(itemId) + ".");
		return;
	}
	player->getObserveController()->notifyItemuseObservers(*item); // cancel hide
	decomposeAction.act(*player, item, item);
	remainingCount.set(remainingCount.get() - 1); // parity= remainingCount--;
	totalCount.set(totalCount.get() + 1); // parity= totalCount++;
} // parity= }, 10, DataManager.ITEM_DATA.getItemTemplate(itemId).getCastingDelay() + 100));

// Java Decompose.java:93-97
void Decompose::cancelTask(Player& player, ItemUseObserver& observer, std::string_view message) {
	player.getController().cancelTask(TaskId::SKILL_USE);
	player.getObserveController()->removeObserver(observer);
	sendInfo(player, message);
}

} // namespace aion::gameserver::handlers::playercommands
