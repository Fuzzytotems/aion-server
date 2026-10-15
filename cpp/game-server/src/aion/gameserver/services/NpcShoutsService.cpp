#include "aion/gameserver/services/NpcShoutsService.h"

#include <utility>
#include <vector>

#include <optional>
#include <string>

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcShoutData.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/npcshout/ShoutEventType.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldPosition.h"
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

// Java NpcShoutsService.java:121-125
void NpcShoutsService::NpcShoutTask::run() {
	if (npc->getPosition()->isMapRegionActive() && npc->getAi().ask(ai::poll::AIQuestion::CAN_SHOUT))
		NpcShoutsService::getInstance().shoutRandom(*npc, nullptr, shouts, 0); // Java: the enclosing instance, the singleton (npcShoutsService)
}

NpcShoutsService::NpcShoutsService() = default;

// Java NpcShoutsService.java:34-47
void NpcShoutsService::registerShoutTask(model::gameobjects::Npc& npc) {
	runtime::Ptr<model::templates::spawns::SpawnTemplate> spawn = npc.getSpawn();
	if (spawn == nullptr) // Java: npc.getSpawn().getWorldId() on null
		throw runtime::NullPointerException("Npc.getSpawn()");
	int32_t worldId = spawn->getWorldId();

	std::optional<std::vector<const model::templates::npcshout::NpcShout*>> shouts =
		dataholders::DataManager::NPC_SHOUT_DATA->getNpcShouts(worldId, npc.getNpcId(), model::templates::npcshout::ShoutEventType::IDLE);
	if (!shouts || shouts->empty())
		return;

	int32_t pollDelay = commons::utils::Rnd::get(180, 360) * 1000;
	for (const model::templates::npcshout::NpcShout* shout : *shouts) {
		if (shout->getPollDelay() != 0 && shout->getPollDelay() < pollDelay)
			pollDelay = shout->getPollDelay();
	}

	// Java schedules the Runnable itself; the Ref in the closure retains the task (and through it the npc) until the SHOUT task is cancelled
	// (CreatureController.cancelAllTasks), as QuestTasks.cpp's FollowingNpcCheckTask. The closure is pinned to the npc for the leak census.
	runtime::Ref<NpcShoutTask> task = NpcShoutTask::create(*this, npc, std::move(*shouts));
	npc.getController().addTask(model::TaskId::SHOUT,
		utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({&npc}, [task] { task->run(); }, 0, pollDelay));
}

// Java NpcShoutsService.java:49-51
void NpcShoutsService::removeShoutCooldown(model::gameobjects::Npc& npc) {
	shoutCooldowns.remove(npc.getObjectId());
}

// Java NpcShoutsService.java:53-56
bool NpcShoutsService::mayShout(model::gameobjects::Npc& npc) {
	std::optional<int64_t> cd = shoutCooldowns.get(npc.getObjectId());
	return !cd || commons::utils::currentTimeMillis() >= *cd;
}

// Java NpcShoutsService.java:58-62
void NpcShoutsService::shoutRandom(model::gameobjects::Npc& sender, runtime::Ptr<model::gameobjects::player::Player> target,
	const std::vector<const model::templates::npcshout::NpcShout*>& shouts, int32_t shoutCooldown) {
	if (shouts.empty())
		return;
	shout(runtime::Ptr<model::gameobjects::Npc>(sender), std::move(target), *commons::utils::Rnd::get(shouts), shoutCooldown);
}

// Java NpcShoutsService.java:64-94. The XML binding reads an absent pattern or param as the empty string (Java: null): `getPattern() != null`
// is `!getPattern().empty()`, and a null param is written as the empty string by SM_SYSTEM_MESSAGE.writeImpl (writeS(null)), the same bytes
void NpcShoutsService::shout(runtime::Ptr<model::gameobjects::Npc> sender, runtime::Ptr<model::gameobjects::player::Player> target,
	const model::templates::npcshout::NpcShout* shout, int32_t shoutCooldown) {
	if (sender == nullptr || shout == nullptr)
		return;

	if (!shout->getPattern().empty() && !sender->getAi().onPatternShout(shout->getWhen(), shout->getPattern(), shout->getSkillNo()))
		return;

	int32_t shoutRange = sender->getObjectTemplate()->getMinimumShoutRange();
	if (target != nullptr && !utils::PositionUtil::isInRange(*target, *sender, static_cast<float>(shoutRange)))
		return;

	std::string param = shout->getParam();
	if (runtime::Ptr<model::gameobjects::VisibleObject> senderTarget = sender->getTarget(); senderTarget != nullptr && "target" == param)
		param = senderTarget->getObjectTemplate()->getName();

	if (shoutCooldown > 0 && target != nullptr && "quest" == shout->getPattern())
		shoutCooldown = 0;

	network::aion::serverpackets::SM_SYSTEM_MESSAGE message(model::ChatType::NPC, sender, shout->getStringId(), std::vector<std::string>{param});

	if (target != nullptr) {
		utils::PacketSendUtility::sendPacket(*target, message);
	} else {
		model::gameobjects::Npc& npc = *sender;
		utils::PacketSendUtility::broadcastPacket(npc, message,
			[&npc, shoutRange](model::gameobjects::player::Player& player) { return utils::PositionUtil::isInRange(player, npc, static_cast<float>(shoutRange)); });
	}
	if (shoutCooldown <= 0)
		removeShoutCooldown(*sender);
	else
		shoutCooldowns.put(sender->getObjectId(), commons::utils::currentTimeMillis() + shoutCooldown * 1000 - 50); // 50ms offset to avoid tight cooldown conflicts
}

NpcShoutsService& NpcShoutsService::getInstance() {
	static NpcShoutsService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services
