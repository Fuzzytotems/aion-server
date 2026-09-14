#include "aion/gameserver/model/siege/ArtifactLocation.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/world/zone/SiegeZoneInstance.h"

namespace aion::gameserver::model::siege {

ArtifactLocation::ArtifactLocation(const templates::siegelocation::SiegeLocationTemplate* value)
	: SiegeLocation(value) {
	setVulnerable(true);
}

runtime::Ref<ArtifactLocation> ArtifactLocation::create(const templates::siegelocation::SiegeLocationTemplate* value) {
	return runtime::makeRef<ArtifactLocation>(value);
}

int32_t ArtifactLocation::getNextState() {
	AION_UNPORTED();
}

void ArtifactLocation::setInitialDelay(int64_t capturedTime) {
	AION_UNPORTED();
}

int32_t ArtifactLocation::getCoolDown() {
	AION_UNPORTED();
}

std::string ArtifactLocation::getL10n() {
	AION_UNPORTED();
}

bool ArtifactLocation::isStandAlone() {
	AION_UNPORTED();
}

runtime::Ptr<FortressLocation> ArtifactLocation::getOwningFortress() {
	AION_UNPORTED();
}

ArtifactStatus ArtifactLocation::getStatus() {
	AION_UNPORTED();
}

ArtifactLocation::~ArtifactLocation() = default;

} // namespace aion::gameserver::model::siege
