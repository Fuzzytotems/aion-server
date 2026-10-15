#include "aion/gameserver/handlers/instance/abyss/RightWingChamberInstance.h"

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

AION_INSTANCE_HANDLER(RightWingChamberInstance, 300090000);

// Java RightWingChamberInstance.java:28-30
RightWingChamberInstance::RightWingChamberInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<RightWingChamberInstance> RightWingChamberInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<RightWingChamberInstance>(instance);
}

// Java RightWingChamberInstance.java:33-36 (the ring's template is immortal, as static data: FlyRingTemplate.h)
void RightWingChamberInstance::onInstanceCreate() {
	runtime::Ref<Point3D> center = Point3D::create(271.87686, 361.04962, 107.83435);
	runtime::Ref<Point3D> p1 = Point3D::create(262.87686, 361.04962, 113.83435);
	runtime::Ref<Point3D> p2 = Point3D::create(256.22054, 358.58627, 107.83435);
	VisibleObject::create<FlyRing>(new FlyRingTemplate("RIGHT_WING_1", mapId, *center, *p1, *p2, 8), instance->getInstanceId())->spawn();
}

// Java RightWingChamberInstance.java:39-48
bool RightWingChamberInstance::onPassFlyingRing(Player& player, std::string_view flyingRing) {
	if (flyingRing == "RIGHT_WING_1") {
		if (startTime.compareAndSet(0, commons::utils::currentTimeMillis())) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_START_IDABRE());
			PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(0, 900));
			ThreadPoolManager::getInstance().schedule({this}, [this] { deleteAliveNpcs({700471, 701482, 701487}); }, 900000);
		}
	}
	return false;
}

// Java RightWingChamberInstance.java:51-64
void RightWingChamberInstance::onEnterInstance(Player& player) {
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

// Java RightWingChamberInstance.java:66-68
void RightWingChamberInstance::spawnGoldChest() {
	spawn(instanceRace.get() == Race::ELYOS ? 701482 : 701487, 261.69f, 206.11f, 102.33f, int8_t{30});
}

} // namespace aion::gameserver::handlers::instance::abyss
