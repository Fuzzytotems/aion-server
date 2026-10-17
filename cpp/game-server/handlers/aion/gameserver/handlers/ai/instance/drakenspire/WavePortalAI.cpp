#include "aion/gameserver/handlers/ai/instance/drakenspire/WavePortalAI.h"

#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(WavePortalAI, "wave_portal");

// Java WavePortalAI.java:26-63
void WavePortalAI::handleSpawned() {
	NoActionAI::handleSpawned();
	int32_t staticId = getOwner().getSpawn()->getStaticId();
	tasks.add(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this, staticId] {
		switch (staticId) {
			case 84: // Top Left
				spawn(236204, 581.92f, 823.65f, 1609.64f, 15, "301390000_Wave_Top_Left_01");
				spawn(236204, 581.92f, 823.65f, 1609.64f, 15, "301390000_Wave_Top_Left_02");
				break;
			case 405: // Central
				spawn(commons::utils::Rnd::get(236216, 236220), 635.17f, 811.90f, 1598.50f, 30, "301390000_Wave_Commander_Left", 1000);
				spawn(commons::utils::Rnd::get(236216, 236220), 635.17f, 811.90f, 1598.50f, 30, "301390000_Wave_Commander_Middle", 0);
				spawn(commons::utils::Rnd::get(236216, 236220), 635.17f, 811.90f, 1598.50f, 30, "301390000_Wave_Commander_Right", 1000);
				break;
			case 548: // Bottom Left
				spawn(236204, 687.29f, 825.78f, 1609.66f, 45, "301390000_Wave_Bottom_Left_01");
				spawn(236204, 687.29f, 825.78f, 1609.66f, 45, "301390000_Wave_Bottom_Left_02");
				break;
			case 398: // Bottom Central
				spawn(236205, 704.21f, 877.60f, 1604.55f, 60, "301390000_Wave_Bottom_Central_01");
				spawn(236205, 704.21f, 877.60f, 1604.55f, 60, "301390000_Wave_Bottom_Central_02");
				break;
			case 401: // Top Central
				spawn(236205, 575.15f, 877.52f, 1600.89f, 0, "301390000_Wave_Top_Central_01");
				spawn(236205, 575.15f, 877.52f, 1600.89f, 0, "301390000_Wave_Top_Central_02");
				break;
			case 399: // Bottom Right
				spawn(236206, 690.59f, 932.85f, 1618.27f, 75, "301390000_Wave_Bottom_Right_01");
				spawn(236206, 690.59f, 932.85f, 1618.27f, 75, "301390000_Wave_Bottom_Right_02");
				break;
			case 407: // Top Right
				spawn(236206, 576.92f, 936.11f, 1620.33f, 104, "301390000_Wave_Top_Right_01");
				spawn(236206, 576.92f, 936.11f, 1620.33f, 104, "301390000_Wave_Top_Right_02");
				break;
		}
	}, 8000, staticId == 405 ? 40000 : 22000));
}

// Java WavePortalAI.java:65-67
void WavePortalAI::spawn(int32_t npcId, float x, float y, float z, int32_t heading, std::string_view walkerId) {
	spawn(npcId, x, y, z, heading, walkerId, 0);
}

// Java WavePortalAI.java:69-74
void WavePortalAI::spawn(int32_t npcId, float x, float y, float z, int32_t heading, std::string_view walkerId, int32_t delay) {
	tasks.add(ThreadPoolManager::getInstance().schedule({this}, [this, npcId, x, y, z, heading, id = std::string(walkerId)] {
		runtime::Ptr<Npc> npc = runtime::cast<Npc>(NoActionAI::spawn(npcId, x, y, z, static_cast<int8_t>(heading)));
		npc->getSpawn()->setWalkerId(std::string_view(id));
	}, delay));
}

// Java WavePortalAI.java:76-82
void WavePortalAI::handleDespawned() {
	for (const runtime::FutureRef& task : tasks)
		if (task != nullptr && !task->isCancelled())
			task->cancel(true);
	NoActionAI::handleDespawned();
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
