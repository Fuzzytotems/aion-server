#include "aion/gameserver/handlers/instance/abyss/SulfurTreeNestInstance.h"

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

AION_INSTANCE_HANDLER(SulfurTreeNestInstance, 300060000);

// Java SulfurTreeNestInstance.java:28-30
SulfurTreeNestInstance::SulfurTreeNestInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<SulfurTreeNestInstance> SulfurTreeNestInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<SulfurTreeNestInstance>(instance);
}

// Java SulfurTreeNestInstance.java:33-36 (the ring's template is immortal, as static data: FlyRingTemplate.h)
void SulfurTreeNestInstance::onInstanceCreate() {
	runtime::Ref<Point3D> center = Point3D::create(462.9394, 380.34888, 168.97256);
	runtime::Ref<Point3D> p1 = Point3D::create(462.9394, 380.34888, 174.97256);
	runtime::Ref<Point3D> p2 = Point3D::create(468.9229, 380.7933, 168.97256);
	VisibleObject::create<FlyRing>(new FlyRingTemplate("SULFUR_1", mapId, *center, *p1, *p2, 6), instance->getInstanceId())->spawn();
}

// Java SulfurTreeNestInstance.java:39-48
bool SulfurTreeNestInstance::onPassFlyingRing(Player& player, std::string_view flyingRing) {
	if (flyingRing == "SULFUR_1") {
		if (startTime.compareAndSet(0, commons::utils::currentTimeMillis())) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_START_IDABRE());
			PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(0, 900));
			ThreadPoolManager::getInstance().schedule({this}, [this] { deleteAliveNpcs({214804, 700463, 700462, 700464, 701485, 701480}); }, 900000);
		}
	}
	return false;
}

// Java SulfurTreeNestInstance.java:51-64
void SulfurTreeNestInstance::onEnterInstance(Player& player) {
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

// Java SulfurTreeNestInstance.java:66-68
void SulfurTreeNestInstance::spawnGoldChest() {
	spawn(instanceRace.get() == Race::ELYOS ? 701480 : 701485, 482.87f, 474.07f, 163.16f, int8_t{90});
}

} // namespace aion::gameserver::handlers::instance::abyss
