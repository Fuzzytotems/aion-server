#include "aion/gameserver/handlers/instance/abyss/RoahInstance.h"

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

AION_INSTANCE_HANDLER(RoahInstance, 300070000);

// Java RoahInstance.java:28-30
RoahInstance::RoahInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<RoahInstance> RoahInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<RoahInstance>(instance);
}

// Java RoahInstance.java:33-36 (the ring's template is immortal, as static data: FlyRingTemplate.h)
void RoahInstance::onInstanceCreate() {
	runtime::Ref<Point3D> center = Point3D::create(501.77, 409.53, 94.12);
	runtime::Ref<Point3D> p1 = Point3D::create(503.93, 409.65, 98.9);
	runtime::Ref<Point3D> p2 = Point3D::create(506.26, 409.7, 94.15);
	VisibleObject::create<FlyRing>(new FlyRingTemplate("ROAH_WING_1", mapId, *center, *p1, *p2, 10), instance->getInstanceId())->spawn();
}

// Java RoahInstance.java:39-48
bool RoahInstance::onPassFlyingRing(Player& player, std::string_view flyingRing) {
	if (flyingRing == "ROAH_WING_1") {
		if (startTime.compareAndSet(0, commons::utils::currentTimeMillis())) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_START_IDABRE());
			PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(0, 900));
			ThreadPoolManager::getInstance().schedule({this}, [this] { deleteAliveNpcs({700472, 700473, 700474, 701489, 701484}); }, 900000);
		}
	}
	return false;
}

// Java RoahInstance.java:51-64
void RoahInstance::onEnterInstance(Player& player) {
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

// Java RoahInstance.java:66-68
void RoahInstance::spawnGoldChest() {
	spawn(instanceRace.get() == Race::ELYOS ? 701484 : 701489, 504.44f, 460.57f, 86.88f, int8_t{60});
}

} // namespace aion::gameserver::handlers::instance::abyss
