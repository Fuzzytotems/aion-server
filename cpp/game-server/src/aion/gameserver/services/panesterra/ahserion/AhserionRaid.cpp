#include "aion/gameserver/services/panesterra/ahserion/AhserionRaid.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraTeam.h"

namespace aion::gameserver::services::panesterra::ahserion {

AhserionRaid::AhserionRaid() {
	factions.add(PanesterraFaction::BELUS);
	factions.add(PanesterraFaction::ASPIDA);
	factions.add(PanesterraFaction::ATANATOS);
	factions.add(PanesterraFaction::DISILLON);
}

AhserionRaid::~AhserionRaid() = default;

AhserionRaid& AhserionRaid::getInstance() {
	// Java SingletonHolder. A per-run service stays RefCounted (fieldmap.toml per_run_services): one Ref that is never released.
	static const runtime::Ref<AhserionRaid>* const instance = new runtime::Ref<AhserionRaid>(runtime::makeRef<AhserionRaid>());
	return **instance;
}

void AhserionRaid::start() {
	AION_UNPORTED();
}

void AhserionRaid::stop() {
	AION_UNPORTED();
}

void AhserionRaid::cleanUp() {
	AION_UNPORTED();
}

// anonymous Runnable at AhserionRaid.java:78 (fieldmap key AhserionRaid$1); argument 1 of scheduleAtFixedRate(); storage: task in AhserionRaid
void AhserionRaid::startInstanceTimer() {
	AION_UNPORTED();
}

void AhserionRaid::checkForIllegalMovement() {
	AION_UNPORTED();
}

void AhserionRaid::spawnRaid() {
	AION_UNPORTED();
}

void AhserionRaid::spawnStage(int32_t stage, PanesterraFaction faction) {
	AION_UNPORTED();
}

void AhserionRaid::handleCorridorShieldDestruction(int32_t npcId) {
	AION_UNPORTED();
}

void AhserionRaid::sendConsolationReward(PanesterraTeam& eliminatedTeam) {
	AION_UNPORTED();
}

void AhserionRaid::handleBossKilled(model::gameobjects::Npc& ahserion, PanesterraFaction winnerFaction) {
	AION_UNPORTED();
}

void AhserionRaid::sendMsg(network::aion::serverpackets::SM_SYSTEM_MESSAGE& msg) {
	AION_UNPORTED();
}

void AhserionRaid::deleteNpcs(PanesterraFaction eliminatedFaction, int32_t flagToDelete) {
	AION_UNPORTED();
}

void AhserionRaid::forEachTeam(const std::function<void(PanesterraTeam&)>& consumer) {
	AION_UNPORTED();
}

void AhserionRaid::cancelProgressTask() {
	AION_UNPORTED();
}

bool AhserionRaid::isStarted() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::panesterra::ahserion
