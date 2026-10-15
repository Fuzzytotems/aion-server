#include "aion/gameserver/handlers/instance/pvparenas/DisciplineTrainingGroundsInstance.h"

#include "aion/gameserver/model/instance/instancescore/PvPArenaScore.h"
#include "aion/gameserver/network/aion/instanceinfo/ArenaScoreWriter.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"

namespace aion::gameserver::handlers::instance::pvparenas {

AION_INSTANCE_HANDLER(DisciplineTrainingGroundsInstance, 300430000);

// Java DisciplineTrainingGroundsInstance.java:17-19
DisciplineTrainingGroundsInstance::DisciplineTrainingGroundsInstance(WorldMapInstance& instance) : PvPArenaInstance(instance) {
}

runtime::Ref<DisciplineTrainingGroundsInstance> DisciplineTrainingGroundsInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<DisciplineTrainingGroundsInstance>(instance);
}

// Java DisciplineTrainingGroundsInstance.java:21-25
void DisciplineTrainingGroundsInstance::setScoreCaps() {
	instanceScore.get()->setLowerScoreCap(10000);
	instanceScore.get()->setUpperScoreCap(50000);
	instanceScore.get()->setMaxScoreGap(1500);
}

// Java DisciplineTrainingGroundsInstance.java:27-32
void DisciplineTrainingGroundsInstance::onInstanceCreate() {
	pointsPerKill = 200;
	pointsPerDeath = -100;
	PvPArenaInstance::onInstanceCreate();
}

// Java DisciplineTrainingGroundsInstance.java:34-41
int32_t DisciplineTrainingGroundsInstance::getBoostMoraleEffectDuration(int32_t rank) {
	switch (rank) {
		case 0:
			return 14000;
		case 1:
			return 16000;
		default:
			return 15000;
	}
}

// Java DisciplineTrainingGroundsInstance.java:43-47
void DisciplineTrainingGroundsInstance::sendPacket(runtime::Ptr<Player> player, std::optional<InstanceScoreType> scoreType) {
	instance->forEachPlayer([this](Player& p) {
		runtime::Ref<ArenaScoreWriter> writer = ArenaScoreWriter::create(*instanceScore.get(), p.getObjectId(), true);
		PacketSendUtility::sendPacket(p, SM_INSTANCE_SCORE(instance->getMapId(), *writer));
	});
}

} // namespace aion::gameserver::handlers::instance::pvparenas
