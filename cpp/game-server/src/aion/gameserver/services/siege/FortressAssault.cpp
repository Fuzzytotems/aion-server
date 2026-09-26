#include "aion/gameserver/services/siege/FortressAssault.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/Assaulter.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/templates/siegelocation/AssaultData.h"

namespace aion::gameserver::services::siege {

namespace {
/**
 * Java super(siege): the upcast of the erased Assault<FortressSiege> needs the complete FortressSiege (P5-12a has not written FortressSiege.h yet)
 */
[[noreturn]] Siege& baseOf(FortressSiege& siege) {
	static_cast<void>(siege);
	AION_UNPORTED();
}
} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("SIEGE_LOG");

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702) // the base initializer never returns until FortressSiege.h exists
#endif
FortressAssault::FortressAssault(FortressSiege& siege) : Assault(baseOf(siege)) {
	// Java: assaultData = siegeLocation.getTemplate().getAssaultData(); calculateDifficultySettings()
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

runtime::Ref<FortressAssault> FortressAssault::create(FortressSiege& siege) {
	return runtime::makeRef<FortressAssault>(siege);
}

void FortressAssault::handleAssault() {
	AION_UNPORTED();
}

void FortressAssault::onAssaultFinish(bool isCaptured) {
	AION_UNPORTED();
}

// lambda at FortressAssault.java:59 (fieldmap key siege.FortressAssault@L59:56)
void FortressAssault::scheduleSpawns() {
	AION_UNPORTED();
}

void FortressAssault::spawnWave() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::siege::Assaulter>> FortressAssault::computeWave() {
	AION_UNPORTED();
}

void FortressAssault::addAssaulters(const std::vector<runtime::Ptr<model::siege::Assaulter>>& output,
	const std::vector<runtime::Ptr<model::siege::Assaulter>>& input, float budget) {
	AION_UNPORTED();
}

void FortressAssault::announce(network::aion::serverpackets::SM_SYSTEM_MESSAGE& msg) {
	AION_UNPORTED();
}

void FortressAssault::calculateDifficultySettings() {
	AION_UNPORTED();
}

float FortressAssault::getFactionBalanceMultiplier() {
	AION_UNPORTED();
}

float FortressAssault::getInfluenceMultiplier() {
	AION_UNPORTED();
}

void FortressAssault::onDredgionCommanderKilled() {
	AION_UNPORTED();
}

FortressAssault::~FortressAssault() = default;

} // namespace aion::gameserver::services::siege
