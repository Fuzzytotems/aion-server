#include "aion/gameserver/handlers/instance/abyss/LeftWingChamberInstance.h"

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

AION_INSTANCE_HANDLER(LeftWingChamberInstance, 300080000);

// Java LeftWingChamberInstance.java:29-31
LeftWingChamberInstance::LeftWingChamberInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<LeftWingChamberInstance> LeftWingChamberInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<LeftWingChamberInstance>(instance);
}

// Java LeftWingChamberInstance.java:34-40
void LeftWingChamberInstance::onInstanceCreate() {
	spawnRings();
	if (commons::utils::Rnd::nextBoolean()) // spawn Treasurer Nabatma (pool=1)
		spawn(215424, 502.1326f, 502.89673f, 352.94437f, int8_t{20});
	else
		spawn(215424, 508.71814f, 660.93994f, 352.94638f, int8_t{60});
}

// Java LeftWingChamberInstance.java:42-46 (the ring's template is immortal, as static data: FlyRingTemplate.h)
void LeftWingChamberInstance::spawnRings() {
	runtime::Ref<Point3D> center = Point3D::create(576.2102, 585.4146, 353.90677);
	runtime::Ref<Point3D> p1 = Point3D::create(576.2102, 585.4146, 359.90677);
	runtime::Ref<Point3D> p2 = Point3D::create(575.18384, 596.36664, 353.90677);
	runtime::Ref<FlyRing> f1 = VisibleObject::create<FlyRing>(new FlyRingTemplate("LEFT_WING_1", mapId, *center, *p1, *p2, 10),
		instance->getInstanceId());
	f1->spawn();
}

// Java LeftWingChamberInstance.java:49-57
bool LeftWingChamberInstance::onPassFlyingRing(Player& player, std::string_view flyingRing) {
	if (flyingRing == "LEFT_WING_1") {
		if (startTime.compareAndSet(0, commons::utils::currentTimeMillis())) {
			PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(0, 900));
			ThreadPoolManager::getInstance().schedule({this}, [this] { deleteAliveNpcs({700466, 701481, 701486}); }, 900000);
		}
	}
	return false;
}

// Java LeftWingChamberInstance.java:60-74
void LeftWingChamberInstance::onEnterInstance(Player& player) {
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

// Java LeftWingChamberInstance.java:76-78
void LeftWingChamberInstance::spawnGoldChest() {
	spawn(instanceRace.get() == Race::ELYOS ? 701481 : 701486, 496.87f, 664.07f, 352.94f, int8_t{90});
}

// Java LeftWingChamberInstance.java:81-84
void LeftWingChamberInstance::handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) {
	if (npc.getNpcId() == 700455)
		TeleportService::moveToInstanceExit(*player, mapId, player->getRace());
}

} // namespace aion::gameserver::handlers::instance::abyss
