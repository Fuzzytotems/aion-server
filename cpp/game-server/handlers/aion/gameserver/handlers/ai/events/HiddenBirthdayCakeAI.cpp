#include "aion/gameserver/handlers/ai/events/HiddenBirthdayCakeAI.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::ai::events {

AION_AI(HiddenBirthdayCakeAI, "hidden_cake");

const commons::logging::Logger HiddenBirthdayCakeAI::log = commons::logging::LoggerFactory::getLogger("EVENT_LOG");

runtime::AtomicInteger& HiddenBirthdayCakeAI::collectedCakes() {
	static auto* const value = new runtime::AtomicInteger(AION_LOCK_CLASS(HiddenBirthdayCakeAI::collectedCakes));
	return *value;
}

runtime::AtomicLong& HiddenBirthdayCakeAI::lastLogTime() {
	static auto* const value = new runtime::AtomicLong(AION_LOCK_CLASS(HiddenBirthdayCakeAI::lastLogTime), commons::utils::currentTimeMillis());
	return *value;
}

runtime::AtomicInteger& HiddenBirthdayCakeAI::lastCakeCount() {
	static auto* const value = new runtime::AtomicInteger(AION_LOCK_CLASS(HiddenBirthdayCakeAI::lastCakeCount));
	return *value;
}

// Java HiddenBirthdayCakeAI.java:39-47
void HiddenBirthdayCakeAI::logCollectedCakes(int32_t cakes) {
	int32_t deviation = cakes - lastCakeCount().get();
	int64_t currentTime = commons::utils::currentTimeMillis();
	if (currentTime - lastLogTime().get() >= 3600 * 1000) { // Only log once every hour
		log.info("[EVENT] Total cakes collected: {}; Cakes collected during the last hour: {}.", cakes, deviation);
		lastCakeCount().set(cakes);
		lastLogTime().set(currentTime);
	}
}

// Java HiddenBirthdayCakeAI.java:49-54
void HiddenBirthdayCakeAI::handleDialogStart(Player& player) {
	if (player.isInPlayerMode(model::actions::PlayerMode::RIDE))
		player.unsetPlayerMode(model::actions::PlayerMode::RIDE);
	ChestAI::handleDialogStart(player);
}

// Java HiddenBirthdayCakeAI.java:56-70
void HiddenBirthdayCakeAI::handleUseItemFinish(Player& player) {
	if (getOwner().isInState(CreatureState::DEAD))
		return;
	logCollectedCakes(collectedCakes().incrementAndGet());
	if (commons::utils::Rnd::chance() < JEST_SPAWN_CHANCE) {
		const runtime::Array<int32_t>& ids = *JEST_SPAWN_IDS; // Java Rnd.get(int[]): the only element, or a random one
		int32_t jestId = ids.length() == 1 ? ids[0].get() : ids[commons::utils::Rnd::nextInt(ids.length())].get();
		runtime::Ptr<Npc> npc = runtime::cast<Npc>(spawn(jestId, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}));
		runtime::Ref<Npc> jester(*npc);
		npc->getController().addTask(TaskId::DESPAWN, ThreadPoolManager::getInstance().schedule({npc.get()},
			[jester] { jester->getController().deleteIfAliveOrCancelRespawn(); }, 2 * 60 * 1000));
		PacketSendUtility::sendPacket(player,
			SM_SYSTEM_MESSAGE::STR_MSG_TOYPET_FEED_FOOD_NOT_LOVEFLAVOR(npc->getObjectTemplate()->getL10n(), getObjectTemplate()->getL10n()));
	}
	ChestAI::handleUseItemFinish(player);
}

} // namespace aion::gameserver::handlers::ai::events
