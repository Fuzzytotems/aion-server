#include "aion/gameserver/handlers/ai/instance/idgelDome/FlameVentAI.h"

#include "aion/gameserver/controllers/NpcController.h"

namespace aion::gameserver::handlers::ai::instance::idgelDome {

AION_AI(FlameVentAI, "flame_vent");

void FlameVentAI::despawnIn60s(Npc& npc) {
	runtime::Ref<Npc> ref(npc);
	npc.getController().addTask(TaskId::DESPAWN, ThreadPoolManager::getInstance().schedule({&npc}, [ref] { ref->getController().delete_(); }, 60000));
}

// Java FlameVentAI.java:23-51
void FlameVentAI::handleUseItemFinish(Player& player) {
	switch (getNpcId()) {
		case 802548: {
			runtime::Ptr<Npc> repelling1 = runtime::cast<Npc>(spawn(855010, 234.4599f, 194.21619f, 79.589996f, int8_t{119}));
			runtime::Ptr<Npc> fx1 = runtime::cast<Npc>(spawn(702405, 232.19067f, 185.85762f, 80.199997f, int8_t{75}));
			runtime::Ptr<Npc> fx2 = runtime::cast<Npc>(spawn(702405, 238.54938f, 200.84813f, 80.0f, int8_t{15}));
			despawnIn60s(*repelling1);
			despawnIn60s(*fx1);
			despawnIn60s(*fx2);
			getOwner().getController().die();
			break;
		}
		case 802549: {
			runtime::Ptr<Npc> repelling2 = runtime::cast<Npc>(spawn(855010, 294.62436f, 324.11783f, 79.790443f, int8_t{119}));
			runtime::Ptr<Npc> fx3 = runtime::cast<Npc>(spawn(702405, 290.67102f, 317.26324f, 80.099998f, int8_t{75}));
			runtime::Ptr<Npc> fx4 = runtime::cast<Npc>(spawn(702405, 297.08356f, 332.35382f, 80.099998f, int8_t{15}));
			despawnIn60s(*repelling2);
			despawnIn60s(*fx3);
			despawnIn60s(*fx4);
			getOwner().getController().die();
			break;
		}
	}
}

} // namespace aion::gameserver::handlers::ai::instance::idgelDome
