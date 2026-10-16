#include "aion/gameserver/taskmanager/tasks/ExpireTimerTask.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/Expirable.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::taskmanager::tasks {

ExpireTimerTask::ExpireTimerTask() : AbstractPeriodicTaskManager(1000, "ExpireTimerTask") {
}

ExpireTimerTask::~ExpireTimerTask() = default;

ExpireTimerTask& ExpireTimerTask::getInstance() {
	// Java: SingletonHolder; a never-released Ref (hub-headers.md §11.1)
	static const runtime::Ref<ExpireTimerTask>& instance = *new runtime::Ref<ExpireTimerTask>(runtime::makeRef<ExpireTimerTask>());
	return *instance;
}

void ExpireTimerTask::registerExpirable(model::Expirable& expirable, model::gameobjects::player::Player& player) {
	// Java: registerExpirables(Collections.singletonList(expirable), player)
	if (expirable.getExpireTime() > 0)
		expirables.put(runtime::Ref<model::Expirable>(expirable), runtime::Ref<model::gameobjects::player::Player>(player));
}

void ExpireTimerTask::registerExpirables(const std::vector<runtime::Ptr<model::Expirable>>& expirablesValue, model::gameobjects::player::Player& player) {
	for (const runtime::Ptr<model::Expirable>& expirable : expirablesValue)
		registerExpirable(*expirable, player);
}

void ExpireTimerTask::unregisterExpirables(model::gameobjects::player::Player& player) {
	expirables.removeIf([&player](const auto&, const auto& owner) { return player.equals(*owner); });
}

void ExpireTimerTask::run() {
	int32_t timeNow = static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000);
	for (const auto& entry : expirables.entrySet()) {
		model::Expirable& expirable = *entry.key;
		model::gameobjects::player::Player& player = *entry.value;
		int32_t remainingSeconds = expirable.getExpireTime() - timeNow;
		if (remainingSeconds < 0 && expirable.canExpireNow()) {
			expirable.onExpire(player);
			expirables.remove(entry.key); // Java: i.remove() (ConcurrentHashMap's iterator removes the key)
		} else {
			switch (remainingSeconds) {
				case 1800:
				case 900:
				case 600:
				case 300:
				case 60:
					expirable.onBeforeExpire(player, remainingSeconds / 60);
					break;
				default:
					break;
			}
		}
	}
}

} // namespace aion::gameserver::taskmanager::tasks
