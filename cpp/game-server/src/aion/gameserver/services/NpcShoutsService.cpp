#include "aion/gameserver/services/NpcShoutsService.h"

#include <utility>
#include <vector>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/npcshout/NpcShout.h"

namespace aion::gameserver::services {

// Java implements Runnable. Scheduled at a fixed rate and kept as the Npc's SHOUT task, it reads npc and shouts on the pool thread in every
// run, so it is K4 (fieldmap.toml [kinds]): RefCounted, created with create(), retaining the Npc. The cycle Npc controller tasks -> Future ->
// task -> Npc is cut by CreatureController.cancelAllTasks (onDelete, design §5.1 Tasks).
class NpcShoutsService::NpcShoutTask final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<model::gameobjects::Npc> npc;
	const std::vector<const model::templates::npcshout::NpcShout*> shouts;
	const NpcShoutsService* npcShoutsService; // Java: the enclosing instance (inner class, an Immortal singleton)

protected:
	NpcShoutTask(const NpcShoutsService& outer, model::gameobjects::Npc& npc, std::vector<const model::templates::npcshout::NpcShout*> shouts);
	~NpcShoutTask() override;

public:
	/** Java: new NpcShoutTask(npc, shouts) */
	static runtime::Ref<NpcShoutTask> create(const NpcShoutsService& outer, model::gameobjects::Npc& npc,
		std::vector<const model::templates::npcshout::NpcShout*> shouts);
	void run();
};

NpcShoutsService::NpcShoutTask::NpcShoutTask(const NpcShoutsService& outer, model::gameobjects::Npc& npcValue,
	std::vector<const model::templates::npcshout::NpcShout*> shoutsValue)
	: npc(npcValue), shouts(std::move(shoutsValue)), npcShoutsService(&outer) {
}

NpcShoutsService::NpcShoutTask::~NpcShoutTask() = default;

runtime::Ref<NpcShoutsService::NpcShoutTask> NpcShoutsService::NpcShoutTask::create(const NpcShoutsService& outer, model::gameobjects::Npc& npcValue,
	std::vector<const model::templates::npcshout::NpcShout*> shoutsValue) {
	return runtime::makeRef<NpcShoutTask>(outer, npcValue, std::move(shoutsValue));
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
