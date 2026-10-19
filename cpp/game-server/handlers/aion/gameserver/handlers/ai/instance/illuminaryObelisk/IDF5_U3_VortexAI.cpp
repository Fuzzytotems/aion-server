#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/IDF5_U3_VortexAI.h"

#include <string>
#include <vector>

#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

AION_AI(IDF5_U3_VortexAI, "idf5_u3_vortex");

// Java IDF5_U3_VortexAI.java:33-43
void IDF5_U3_VortexAI::handleSpawned() {
	NoActionAI::handleSpawned();
	if (getPosition()->getMapId() == 301230000) // normal mode
		npcIds.addAll(std::vector<int32_t>{233857, 233880, 233881});
	else // hard mode
		npcIds.addAll(std::vector<int32_t>{234687, 234688, 234689});
	SYNCHRONIZED(tasks) {
		tasks.add(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { handlePhaseAttacks(); }, 120000, 120000));
	}
}

// Java IDF5_U3_VortexAI.java:45-104
void IDF5_U3_VortexAI::handlePhaseAttacks() {
	switch (getNpcId()) {
		case 702014: // east
			spawnWalker(npcIds.get(0), 252.3243f, 328.5881f, 325.0092f, int8_t{90}, 0, "idf5_u3_east_1");
			spawnWalker(npcIds.get(0), 255.3635f, 328.5584f, 325.0038f, int8_t{90}, 0, "idf5_u3_east_2");
			spawnWalker(npcIds.get(1), 256.6376f, 328.7015f, 325.0038f, int8_t{90}, 0, "idf5_u3_east_3");
			spawnWalker(npcIds.get(1), 258.5159f, 328.5792f, 325.0038f, int8_t{90}, 0, "idf5_u3_east_4");
			spawnWalker(npcIds.get(1), 256.9199f, 326.4982f, 325.0038f, int8_t{90}, 0, "idf5_u3_east_5");
			spawnWalker(npcIds.get(1), 253.8757f, 326.5010f, 325.0038f, int8_t{90}, 0, "idf5_u3_east_6");
			spawnWalker(npcIds.get(0), 252.3243f, 328.5881f, 325.0092f, int8_t{90}, 15000, "idf5_u3_east_1");
			spawnWalker(npcIds.get(0), 255.3635f, 328.5584f, 325.0038f, int8_t{90}, 15000, "idf5_u3_east_2");
			spawnWalker(npcIds.get(2), 256.6376f, 328.7015f, 325.0038f, int8_t{90}, 15000, "idf5_u3_east_3");
			spawnWalker(npcIds.get(2), 258.5159f, 328.5792f, 325.0038f, int8_t{90}, 15000, "idf5_u3_east_4");
			spawnWalker(npcIds.get(2), 256.9199f, 326.4982f, 325.0038f, int8_t{90}, 15000, "idf5_u3_east_5");
			spawnWalker(npcIds.get(2), 253.8757f, 326.5010f, 325.0038f, int8_t{90}, 15000, "idf5_u3_east_6");
			break;
		case 702015: // west
			spawnWalker(npcIds.get(0), 251.9594f, 183.4159f, 325.0038f, int8_t{30}, 0, "idf5_u3_west_1");
			spawnWalker(npcIds.get(0), 253.5314f, 183.5728f, 325.0038f, int8_t{30}, 0, "idf5_u3_west_2");
			spawnWalker(npcIds.get(1), 255.2491f, 183.4584f, 325.0038f, int8_t{30}, 0, "idf5_u3_west_3");
			spawnWalker(npcIds.get(1), 257.0595f, 183.5797f, 325.0045f, int8_t{30}, 0, "idf5_u3_west_4");
			spawnWalker(npcIds.get(1), 258.7057f, 183.6840f, 325.0038f, int8_t{30}, 0, "idf5_u3_west_5");
			spawnWalker(npcIds.get(1), 255.0448f, 185.5452f, 325.0038f, int8_t{30}, 0, "idf5_u3_west_6");
			spawnWalker(npcIds.get(0), 251.9594f, 183.4159f, 325.0038f, int8_t{30}, 15000, "idf5_u3_west_1");
			spawnWalker(npcIds.get(0), 253.5314f, 183.5728f, 325.0038f, int8_t{30}, 15000, "idf5_u3_west_2");
			spawnWalker(npcIds.get(2), 255.2491f, 183.4584f, 325.0038f, int8_t{30}, 15000, "idf5_u3_west_3");
			spawnWalker(npcIds.get(2), 257.0595f, 183.5797f, 325.0045f, int8_t{30}, 15000, "idf5_u3_west_4");
			spawnWalker(npcIds.get(2), 258.7057f, 183.6840f, 325.0038f, int8_t{30}, 15000, "idf5_u3_west_5");
			spawnWalker(npcIds.get(2), 255.0448f, 185.5452f, 325.0038f, int8_t{30}, 15000, "idf5_u3_west_6");
			break;
		case 702016: // south
			spawnWalker(npcIds.get(0), 326.3734f, 251.2209f, 291.8364f, int8_t{60}, 0, "idf5_u3_south_1");
			spawnWalker(npcIds.get(0), 326.3337f, 252.6159f, 291.8364f, int8_t{60}, 0, "idf5_u3_south_2");
			spawnWalker(npcIds.get(1), 326.3333f, 253.1857f, 291.8364f, int8_t{60}, 0, "idf5_u3_south_3");
			spawnWalker(npcIds.get(1), 326.4392f, 255.9983f, 291.8364f, int8_t{60}, 0, "idf5_u3_south_4");
			spawnWalker(npcIds.get(1), 326.4354f, 257.6836f, 291.8466f, int8_t{60}, 0, "idf5_u3_south_5");
			spawnWalker(npcIds.get(1), 324.7853f, 254.2962f, 291.8364f, int8_t{60}, 0, "idf5_u3_south_6");
			spawnWalker(npcIds.get(0), 326.3734f, 251.2209f, 291.8364f, int8_t{60}, 15000, "idf5_u3_south_1");
			spawnWalker(npcIds.get(0), 326.3337f, 252.6159f, 291.8364f, int8_t{60}, 15000, "idf5_u3_south_2");
			spawnWalker(npcIds.get(2), 326.3333f, 253.1857f, 291.8364f, int8_t{60}, 15000, "idf5_u3_south_3");
			spawnWalker(npcIds.get(2), 326.4392f, 255.9983f, 291.8364f, int8_t{60}, 15000, "idf5_u3_south_4");
			spawnWalker(npcIds.get(2), 326.4354f, 257.6836f, 291.8466f, int8_t{60}, 15000, "idf5_u3_south_5");
			spawnWalker(npcIds.get(2), 324.7853f, 254.2962f, 291.8364f, int8_t{60}, 15000, "idf5_u3_south_6");
			break;
		case 702017: // north
			spawnWalker(npcIds.get(0), 184.6565f, 256.3191f, 291.8364f, int8_t{0}, 0, "idf5_u3_north_1");
			spawnWalker(npcIds.get(0), 184.6415f, 253.7202f, 291.8364f, int8_t{0}, 0, "idf5_u3_north_2");
			spawnWalker(npcIds.get(1), 184.6134f, 253.0914f, 291.8364f, int8_t{0}, 0, "idf5_u3_north_3");
			spawnWalker(npcIds.get(1), 184.7428f, 251.3166f, 291.8842f, int8_t{0}, 0, "idf5_u3_north_4");
			spawnWalker(npcIds.get(1), 184.6134f, 253.0914f, 291.8364f, int8_t{0}, 0, "idf5_u3_north_5");
			spawnWalker(npcIds.get(1), 186.8694f, 254.6730f, 291.8364f, int8_t{0}, 0, "idf5_u3_north_6");
			spawnWalker(npcIds.get(0), 184.6565f, 256.3191f, 291.8364f, int8_t{0}, 15000, "idf5_u3_north_1");
			spawnWalker(npcIds.get(0), 184.6415f, 253.7202f, 291.8364f, int8_t{0}, 15000, "idf5_u3_north_2");
			spawnWalker(npcIds.get(2), 184.6134f, 253.0914f, 291.8364f, int8_t{0}, 15000, "idf5_u3_north_3");
			spawnWalker(npcIds.get(2), 184.7428f, 251.3166f, 291.8842f, int8_t{0}, 15000, "idf5_u3_north_4");
			spawnWalker(npcIds.get(2), 184.6134f, 253.0914f, 291.8364f, int8_t{0}, 15000, "idf5_u3_north_5");
			spawnWalker(npcIds.get(2), 186.8694f, 254.6730f, 291.8364f, int8_t{0}, 15000, "idf5_u3_north_6");
			break;
	}
}

// Java IDF5_U3_VortexAI.java:106-116 (Java's cast of the spawn to Npc: the walkers are npcs)
void IDF5_U3_VortexAI::spawnWalker(int32_t npcId, float x, float y, float z, int8_t h, int32_t delay, std::string_view walkerId) {
	addTask([this, npcId, x, y, z, h, walker = std::string(walkerId)] {
		runtime::Ref<Npc> npc(*runtime::cast<Npc>(spawn(npcId, x, y, z, h)));
		npc->getSpawn()->setWalkerId(walker);
		addTask([this, npc] {
			WalkManager::startWalking(dynamic_cast<NpcAI&>(npc->getAi()));
			npc->setState(CreatureState::ACTIVE, true);
			PacketSendUtility::broadcastToMap(getOwner(), network::aion::serverpackets::SM_EMOTION(getOwner(), EmotionType::RUN));
		}, 2500);
	}, delay);
}

// Java IDF5_U3_VortexAI.java:118-123
void IDF5_U3_VortexAI::addTask(std::function<void()> task, int32_t delayMs) {
	SYNCHRONIZED(tasks) {
		if (!tasks.isEmpty())
			tasks.add(ThreadPoolManager::getInstance().schedule({this}, [task = std::move(task)] { task(); }, delayMs));
	}
}

// Java IDF5_U3_VortexAI.java:125-132
void IDF5_U3_VortexAI::handleDespawned() {
	SYNCHRONIZED(tasks) {
		for (const runtime::FutureRef& t : tasks.snapshot()) {
			if (!t->isDone())
				t->cancel(true);
		}
		tasks.clear();
	}
	NoActionAI::handleDespawned();
}

} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk
