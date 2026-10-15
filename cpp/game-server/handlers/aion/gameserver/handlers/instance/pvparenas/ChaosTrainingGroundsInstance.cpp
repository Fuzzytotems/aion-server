#include "aion/gameserver/handlers/instance/pvparenas/ChaosTrainingGroundsInstance.h"

#include "aion/gameserver/model/instance/instancescore/PvPArenaScore.h"
#include "aion/gameserver/network/aion/instanceinfo/ArenaScoreWriter.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/flyring/FlyRing.h"
#include "aion/gameserver/model/gameobjects/Gatherable.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.h"

namespace aion::gameserver::handlers::instance::pvparenas {

AION_INSTANCE_HANDLER(ChaosTrainingGroundsInstance, 300420000);

// Java ChaosTrainingGroundsInstance.java:23-25
ChaosTrainingGroundsInstance::ChaosTrainingGroundsInstance(WorldMapInstance& instance) : PvPArenaInstance(instance) {
}

runtime::Ref<ChaosTrainingGroundsInstance> ChaosTrainingGroundsInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<ChaosTrainingGroundsInstance>(instance);
}

// Java ChaosTrainingGroundsInstance.java:27-32
void ChaosTrainingGroundsInstance::onInstanceCreate() {
	pointsPerKill = 1000;
	pointsPerDeath = -125;
	PvPArenaInstance::onInstanceCreate();
}

// Java ChaosTrainingGroundsInstance.java:34-42
int32_t ChaosTrainingGroundsInstance::getBoostMoraleEffectDuration(int32_t rank) {
	switch (rank) {
		case 0:
		case 1:
			return 14000;
		case 4:
		case 5:
			return 16000;
		case 6:
		case 7:
			return 17000;
		default:
			return 15000;
	}
}

// Java ChaosTrainingGroundsInstance.java:44-51
float ChaosTrainingGroundsInstance::getRunnerUpScoreMod(int32_t victimRank) {
	switch (victimRank) {
		case 0:
			return 3.0f;
		case 1:
			return 2.0f;
		default:
			return 1.0f;
	}
}

// Java ChaosTrainingGroundsInstance.java:53-57
void ChaosTrainingGroundsInstance::sendPacket(runtime::Ptr<Player> player, std::optional<InstanceScoreType> scoreType) {
	instance->forEachPlayer([this](Player& p) {
		runtime::Ref<ArenaScoreWriter> writer = ArenaScoreWriter::create(*instanceScore.get(), p.getObjectId(), false);
		PacketSendUtility::sendPacket(p, SM_INSTANCE_SCORE(instance->getMapId(), *writer));
	});
}

// Java ChaosTrainingGroundsInstance.java:59-64
void ChaosTrainingGroundsInstance::onGather(Player& player, Gatherable& gatherable) {
	if (!instanceScore.get()->isStartProgress())
		return;
	updatePoints(getPlayerSpecificReward(player), player, gatherable, 1250);
}

// Java ChaosTrainingGroundsInstance.java:66-80
void ChaosTrainingGroundsInstance::spawnRings() {
	spawnFlyRing("PVP_ARENA_1", *Point3D::create(674.66974f, 1792.8499f, 149.77501f), *Point3D::create(674.66974f, 1792.8499f, 155.77501f),
		*Point3D::create(678.83636f, 1788.5325f, 149.77501f));
	spawnFlyRing("PVP_ARENA_2", *Point3D::create(688.30615f, 1769.7937f, 149.88556f), *Point3D::create(688.30615f, 1769.7937f, 155.88556f),
		*Point3D::create(689.42096f, 1763.8982f, 149.88556f));
	spawnFlyRing("PVP_ARENA_3", *Point3D::create(664.2252f, 1761.671f, 170.95732f), *Point3D::create(664.2252f, 1761.671f, 176.95732f),
		*Point3D::create(669.2843f, 1764.8967f, 170.95732f));
	spawnFlyRing("PVP_ARENA_VOID_1", *Point3D::create(690.28625f, 1753.8561f, 192.07726f), *Point3D::create(690.28625f, 1753.8561f, 198.07726f),
		*Point3D::create(689.4365f, 1747.9165f, 192.07726f));
	spawnFlyRing("PVP_ARENA_VOID_2", *Point3D::create(690.1935f, 1797.0029f, 203.79236f), *Point3D::create(690.1935f, 1797.0029f, 209.79236f),
		*Point3D::create(692.8295f, 1802.3928f, 203.79236f));
	spawnFlyRing("PVP_ARENA_VOID_3", *Point3D::create(659.2784f, 1766.0273f, 207.25465f), *Point3D::create(659.2784f, 1766.0273f, 213.25465f),
		*Point3D::create(665.2619f, 1766.4718f, 207.25465f));
}

// Java ChaosTrainingGroundsInstance.java:82-84 (the template is allocated once and never freed, like static data: FlyRingTemplate.h)
void ChaosTrainingGroundsInstance::spawnFlyRing(std::string_view name, Point3D& center, Point3D& p1, Point3D& p2) {
	VisibleObject::create<FlyRing>(new FlyRingTemplate(name, mapId, center, p1, p2, 6), instance->getInstanceId())->spawn();
}

// Java ChaosTrainingGroundsInstance.java:86-111
bool ChaosTrainingGroundsInstance::onPassFlyingRing(Player& player, std::string_view flyingRing) {
	runtime::Ptr<PvPArenaPlayerReward> playerReward = getPlayerSpecificReward(player);
	if (playerReward == nullptr || !instanceScore.get()->isStartProgress()) {
		return false;
	}
	runtime::Ptr<Npc> npc = nullptr;
	if (flyingRing == "PVP_ARENA_1")
		npc = getNpc(674.841f, 1793.065f, 150.964f); // Flame Loop
	else if (flyingRing == "PVP_ARENA_2")
		npc = getNpc(688.410f, 1769.611f, 150.964f); // Flame Loop
	else if (flyingRing == "PVP_ARENA_3")
		npc = getNpc(664.160f, 1761.933f, 171.504f); // Flame Loop
	else if (flyingRing == "PVP_ARENA_VOID_1")
		npc = getNpc(693.061f, 1752.479f, 186.750f); // Aether Vortex
	else if (flyingRing == "PVP_ARENA_VOID_2")
		npc = getNpc(688.061f, 1798.229f, 198.500f); // Aether Vortex
	else if (flyingRing == "PVP_ARENA_VOID_3")
		npc = getNpc(659.311f, 1768.979f, 201.500f); // Aether Vortex
	if (npc != nullptr && npc->isSpawned()) {
		if (npc->getNpcId() == 701184 || npc->getNpcId() == 701198 || npc->getNpcId() == 701212) { // Flame Loop
			npc->getController().deleteAndScheduleRespawn();
			updatePoints(playerReward, player, *npc, 250);
		} else if (npc->getNpcId() == 701183 || npc->getNpcId() == 701197 || npc->getNpcId() == 701211) { // Aether Vortex
			useSkill(*npc, player, 20059, 1);
			npc->getController().deleteAndScheduleRespawn();
		}
	}
	return false;
}

} // namespace aion::gameserver::handlers::instance::pvparenas
