#include "aion/gameserver/services/siege/ArtifactAssault.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"

namespace aion::gameserver::services::siege {

namespace {
/**
 * Java super(siege): the upcast of the erased Assault<ArtifactSiege> needs the complete ArtifactSiege (P5-12a has not written ArtifactSiege.h yet)
 */
[[noreturn]] Siege& baseOf(ArtifactSiege& siege) {
	static_cast<void>(siege);
	AION_UNPORTED();
}
} // namespace

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702) // the base initializer never returns until ArtifactSiege.h exists
#endif
ArtifactAssault::ArtifactAssault(ArtifactSiege& siege) : Assault(baseOf(siege)) {
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

runtime::Ref<ArtifactAssault> ArtifactAssault::create(ArtifactSiege& siege) {
	return runtime::makeRef<ArtifactAssault>(siege);
}

void ArtifactAssault::handleAssault() {
	AION_UNPORTED();
}

void ArtifactAssault::onAssaultFinish(bool captured) {
	AION_UNPORTED();
}

void ArtifactAssault::spawnAssaulter() {
	AION_UNPORTED();
}

int32_t ArtifactAssault::getAssaulterIdByBossLvl() {
	AION_UNPORTED();
}

ArtifactAssault::~ArtifactAssault() = default;

} // namespace aion::gameserver::services::siege
