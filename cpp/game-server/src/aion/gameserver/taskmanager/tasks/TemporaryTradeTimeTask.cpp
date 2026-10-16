#include "aion/gameserver/taskmanager/tasks/TemporaryTradeTimeTask.h"

#include <cstdint>

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::taskmanager::tasks {

// Java TemporaryTradeTimeTask.java:22-24: super(1000)
TemporaryTradeTimeTask::TemporaryTradeTimeTask() : AbstractPeriodicTaskManager(1000, "TemporaryTradeTimeTask") {
}

TemporaryTradeTimeTask::~TemporaryTradeTimeTask() = default;

TemporaryTradeTimeTask& TemporaryTradeTimeTask::getInstance() {
	// Java: SingletonHolder; a never-released Ref (hub-headers.md §11.1)
	static const runtime::Ref<TemporaryTradeTimeTask>& instance = *new runtime::Ref<TemporaryTradeTimeTask>(runtime::makeRef<TemporaryTradeTimeTask>());
	return *instance;
}

// Java TemporaryTradeTimeTask.java:30-32
void TemporaryTradeTimeTask::addTask(model::gameobjects::Item& item, runtime::RcHashSet<int32_t>& players) {
	items.put(runtime::Ref<model::gameobjects::Item>(item), runtime::Ref<runtime::RcHashSet<int32_t>>(players));
}

// Java TemporaryTradeTimeTask.java:34-39
bool TemporaryTradeTimeTask::canTrade(model::gameobjects::Item& item, int32_t playerObjectId) {
	runtime::Ptr<runtime::RcHashSet<int32_t>> players = items.get(runtime::Ptr<model::gameobjects::Item>(item));
	if (!players)
		return false;
	return players->contains(playerObjectId);
}

// Java TemporaryTradeTimeTask.java:41-57
void TemporaryTradeTimeTask::run() {
	for (const auto& entry : items.entrySet()) {
		model::gameobjects::Item& item = *entry.key;
		// Java: System.currentTimeMillis() (class comment); (int) of the seconds keeps their low 32 bits, as Java's cast does
		int32_t now = static_cast<int32_t>(utils::ThreadPoolManager::clock().currentTimeMillis() / 1000);
		int32_t time = static_cast<int32_t>(static_cast<uint32_t>(item.getTemporaryExchangeTime()) - static_cast<uint32_t>(now)); // Java int -
		if (time <= 0) {
			for (int32_t playerId : entry.value->snapshot()) {
				runtime::Ptr<model::gameobjects::player::Player> player = world::World::getInstance().getPlayer(playerId);
				if (player)
					utils::PacketSendUtility::sendPacket(*player,
						network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_EXCHANGE_TIME_OVER(item.getL10n()));
			}
			item.setTemporaryExchangeTime(0);
			items.remove(entry.key); // Java: iter.remove() (ConcurrentHashMap's iterator removes the key)
		}
	}
}

} // namespace aion::gameserver::taskmanager::tasks
