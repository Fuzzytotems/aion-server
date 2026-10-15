#include "aion/gameserver/handlers/instance/InfernalDanuarReliquaryInstance.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(InfernalDanuarReliquaryInstance, 301360000);

// Java InfernalDanuarReliquaryInstance.java:14-16
InfernalDanuarReliquaryInstance::InfernalDanuarReliquaryInstance(WorldMapInstance& instance) : DanuarReliquaryInstance(instance) {
}

runtime::Ref<InfernalDanuarReliquaryInstance> InfernalDanuarReliquaryInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<InfernalDanuarReliquaryInstance>(instance);
}

// Java InfernalDanuarReliquaryInstance.java:18-21
int32_t InfernalDanuarReliquaryInstance::getExitId() {
	return 730843;
}

// Java InfernalDanuarReliquaryInstance.java:23-26
int32_t InfernalDanuarReliquaryInstance::getTreasureBoxId() {
	return 701795;
}

// Java InfernalDanuarReliquaryInstance.java:28-31
int32_t InfernalDanuarReliquaryInstance::getEnragedModorId() {
	return 234691;
}

// Java InfernalDanuarReliquaryInstance.java:33-36
int32_t InfernalDanuarReliquaryInstance::getCursedModorId() {
	return 234690;
}

// Java InfernalDanuarReliquaryInstance.java:38-41
int32_t InfernalDanuarReliquaryInstance::getRealCloneId() {
	return 855244;
}

// Java InfernalDanuarReliquaryInstance.java:43-46
int32_t InfernalDanuarReliquaryInstance::getFakeCloneId() {
	return 855245;
}

// Java InfernalDanuarReliquaryInstance.java:48-72
void InfernalDanuarReliquaryInstance::onInstanceEnd(bool successful) {
	cancelWipeTask();
	if (!successful) {
		runtime::Ptr<Npc> modor = getNpc(getEnragedModorId());
		if (modor == nullptr) {
			modor = getNpc(getCursedModorId());
		}
		if (modor != nullptr) {
			PacketSendUtility::broadcastMessage(modor, 1500739);
		}
		instance->forEachNpc([](Npc& npc) { npc.getController().delete_(); });
	} else {
		runtime::Ptr<Npc> modor = getNpc(getEnragedModorId());
		if (modor != nullptr) {
			PacketSendUtility::broadcastMessage(modor, 343629);
		}
		const int32_t enragedModorId = getEnragedModorId();
		instance->forEachNpc([enragedModorId](Npc& npc) {
			if (npc.getNpcId() != enragedModorId) {
				npc.getController().delete_();
			}
		});
	}
	spawn(getExitId(), 255.66669f, 263.78525f, 241.7986f, int8_t{86}); // Spawn exit portal
}

} // namespace aion::gameserver::handlers::instance
