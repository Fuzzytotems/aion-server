#include "aion/gameserver/services/NpcShoutsService.h"

#include <utility>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/npcshout/NpcShout.h"

namespace aion::gameserver::services {

// Java implements Runnable (a task: the port turns it into a TaskStruct scheduled with its pins)
class NpcShoutsService::NpcShoutTask {
public:
	runtime::Ref<model::gameobjects::Npc> npc{};
	std::vector<const model::templates::npcshout::NpcShout*> shouts{};
	const NpcShoutsService* npcShoutsService{}; // Java: the enclosing instance (inner class)

	NpcShoutTask(const NpcShoutsService& outer, model::gameobjects::Npc& npc, std::vector<const model::templates::npcshout::NpcShout*> shouts);
	void run();
};

NpcShoutsService::NpcShoutTask::NpcShoutTask(const NpcShoutsService& outer, model::gameobjects::Npc& npcValue,
	std::vector<const model::templates::npcshout::NpcShout*> shoutsValue)
	: npc(npcValue), shouts(std::move(shoutsValue)), npcShoutsService(&outer) {
}

void NpcShoutsService::NpcShoutTask::run() {
	AION_UNPORTED();
}

NpcShoutsService::NpcShoutsService() = default;

void NpcShoutsService::registerShoutTask(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void NpcShoutsService::removeShoutCooldown(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

bool NpcShoutsService::mayShout(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void NpcShoutsService::shoutRandom(model::gameobjects::Npc& sender, runtime::Ptr<model::gameobjects::player::Player> target,
	const std::vector<const model::templates::npcshout::NpcShout*>& shouts, int32_t shoutCooldown) {
	AION_UNPORTED();
}

void NpcShoutsService::shout(runtime::Ptr<model::gameobjects::Npc> sender, runtime::Ptr<model::gameobjects::player::Player> target,
	const model::templates::npcshout::NpcShout* shout, int32_t shoutCooldown) {
	AION_UNPORTED();
}

NpcShoutsService& NpcShoutsService::getInstance() {
	static NpcShoutsService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services
