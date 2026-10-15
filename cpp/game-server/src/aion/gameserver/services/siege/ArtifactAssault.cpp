#include "aion/gameserver/services/siege/ArtifactAssault.h"

#include <cmath>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/SiegeModType.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/siege/ArtifactSiege.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services::siege {

using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

// Java ArtifactAssault.java:24-26
ArtifactAssault::ArtifactAssault(ArtifactSiege& siege) : Assault(siege) {
}

runtime::Ref<ArtifactAssault> ArtifactAssault::create(ArtifactSiege& siege) {
	return runtime::makeRef<ArtifactAssault>(siege);
}

// Java ArtifactAssault.java:28-31
void ArtifactAssault::handleAssault() {
	spawnAssaulter();
}

// Java ArtifactAssault.java:33-37
void ArtifactAssault::onAssaultFinish(bool captured) {
	if (captured) {
		SM_SYSTEM_MESSAGE msg = SM_SYSTEM_MESSAGE::STR_ABYSS_DRAGON_BOSS_KILLED(getBossNpcL10n());
		siegeLocation->forEachPlayer([&msg](model::gameobjects::player::Player& p) { utils::PacketSendUtility::sendPacket(p, msg); });
	}
}

// Java ArtifactAssault.java:39-49
void ArtifactAssault::spawnAssaulter() {
	// Java: Math.toRadians(Rnd.nextFloat(180f)) (angdeg * DEGREES_TO_RADIANS)
	const double angleRadians = static_cast<double>(commons::utils::Rnd::nextFloat(180.0f)) * 0.017453292519943295;
	const float x1 = static_cast<float>(boss->getX() + std::cos(angleRadians));
	const float y1 = static_cast<float>(boss->getY() + std::sin(angleRadians));

	runtime::Ref<model::templates::spawns::siegespawns::SiegeSpawnTemplate> spawnTemplate = spawnengine::SpawnEngine::newSiegeSpawn(worldId,
		getAssaulterIdByBossLvl(), locationId, model::siege::SiegeRace::BALAUR, model::siege::SiegeModType::ASSAULT, x1, y1, boss->getZ(), 0);
	runtime::Ptr<model::gameobjects::Npc> assaulter =
		runtime::cast<model::gameobjects::Npc>(spawnengine::SpawnEngine::spawnObject(*spawnTemplate, 1));
	assaulter->getAggroList().addHate(*boss, 1000);
}

// Java ArtifactAssault.java:51-60
int32_t ArtifactAssault::getAssaulterIdByBossLvl() {
	switch (boss->getLevel()) {
		case 40:
			return 276719;
		case 50:
			return 277016;
		default:
			return 251463;
	}
}

ArtifactAssault::~ArtifactAssault() = default;

} // namespace aion::gameserver::services::siege
