#include "aion/gameserver/handlers/instance/abyss/AsteriaInstance.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/flyring/FlyRing.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::instance::abyss {

AION_INSTANCE_HANDLER(AsteriaInstance, 300050000);

// Java AsteriaInstance.java:28-30
AsteriaInstance::AsteriaInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<AsteriaInstance> AsteriaInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<AsteriaInstance>(instance);
}

// Java AsteriaInstance.java:33-36 (the ring's template is immortal, as static data: FlyRingTemplate.h)
void AsteriaInstance::onInstanceCreate() {
	runtime::Ref<Point3D> center = Point3D::create(479.24, 572.57, 202.72);
	runtime::Ref<Point3D> p1 = Point3D::create(477.95, 567.64, 212.9);
	runtime::Ref<Point3D> p2 = Point3D::create(477.97, 563.35, 202.12);
	VisibleObject::create<FlyRing>(new FlyRingTemplate("ASTERIA_WING_1", mapId, *center, *p1, *p2, 10), instance->getInstanceId())->spawn();
}

// Java AsteriaInstance.java:39-48
bool AsteriaInstance::onPassFlyingRing(Player& player, std::string_view flyingRing) {
	if (flyingRing == "ASTERIA_WING_1") {
		if (startTime.compareAndSet(0, commons::utils::currentTimeMillis())) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_START_IDABRE());
			PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(0, 900));
			ThreadPoolManager::getInstance().schedule({this}, [this] { deleteAliveNpcs({700475, 700476, 700477, 701483, 701488}); }, 900000);
		}
	}
	return false;
}

// Java AsteriaInstance.java:51-64
void AsteriaInstance::onEnterInstance(Player& player) {
	int64_t start = startTime.get();
	if (start > 0) {
		int64_t time = commons::utils::currentTimeMillis() - start;
		if (time < 900000) {
			PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(0, 900 - static_cast<int32_t>(time) / 1000));
		}
	}
	if (!instanceRace.get()) {
		instanceRace = player.getRace();
		spawnGoldChest();
	}
}

// Java AsteriaInstance.java:66-68
void AsteriaInstance::spawnGoldChest() {
	spawn(instanceRace.get() == Race::ELYOS ? 701483 : 701488, 512.8f, 565.35f, 198.0f, int8_t{60});
}

} // namespace aion::gameserver::handlers::instance::abyss
