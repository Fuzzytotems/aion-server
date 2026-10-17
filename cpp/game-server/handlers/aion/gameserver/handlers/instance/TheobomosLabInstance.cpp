#include "aion/gameserver/handlers/instance/TheobomosLabInstance.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(TheobomosLabInstance, 310110000);

// Java TheobomosLabInstance.java:23-25
TheobomosLabInstance::TheobomosLabInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<TheobomosLabInstance> TheobomosLabInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<TheobomosLabInstance>(instance);
}

// Java TheobomosLabInstance.java:27-44
void TheobomosLabInstance::onDie(Npc& npc) {
	if (isInstanceDestroyed.get())
		return;

	if (runtime::as<Player>(npc.getMaster()) != nullptr) // Java: npc.getMaster() instanceof Player
		return;

	switch (npc.getNpcId()) {
		case 214669: // Triroan
			spawn(730178, 571.15f, 490.6607f, 196.7324f, int8_t{60});
			break;
		case 280971:
		case 280972:
			if (guardDie(npc))
				removeBuff();
			break;
		default:
			break;
	}
}

// Java TheobomosLabInstance.java:51-56
void TheobomosLabInstance::onInstanceDestroy() {
	isDead1 = false;
	isDead2 = false;
	isInstanceDestroyed = true;
}

// Java TheobomosLabInstance.java:58-73
bool TheobomosLabInstance::guardDie(Npc& npc) {
	runtime::Ref<WorldPosition> p(npc.getPosition());
	int32_t npcId = npc.getNpcId();
	runtime::Ptr<Npc> orb = getNpc(280973);
	if (orb != nullptr && PositionUtil::getDistance(*orb, npc) <= 7) {
		switch (npcId) {
			case 280971:
				isDead1 = true;
				break;
			case 280972:
				isDead2 = true;
				break;
			default:
				break;
		}
		return true;
	} else {
		npc.getController().delete_();
		spawn(npcId, p->getX(), p->getY(), p->getZ(), int8_t{41});
		return false;
	}
}

// Java TheobomosLabInstance.java:75-82
void TheobomosLabInstance::removeBuff() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isInstanceDestroyed.get() && isDead1.get() && isDead2.get()) {
			getNpc(214668)->getEffectController()->removeEffect(18481);
			deleteAliveNpcs({280973});
		}
	}, 1000);
}

// Java TheobomosLabInstance.java:84-93
void TheobomosLabInstance::onInstanceCreate() {
	if (commons::utils::Rnd::nextBoolean()) {
		switch (commons::utils::Rnd::get(1, 3)) {
			case 1:
				spawn(798223, 256.26215f, 512.9742f, 187.79453f, int8_t{0});
				break;
			case 2:
				spawn(798223, 360.16098f, 526.9446f, 186.20251f, int8_t{105});
				break;
			case 3:
				spawn(798223, 476.77145f, 541.5697f, 187.79453f, int8_t{90});
				break;
			default:
				break;
		}
	}
}

} // namespace aion::gameserver::handlers::instance
