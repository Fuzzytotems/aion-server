#include "aion/gameserver/model/siege/ArtifactLocation.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/siege/ArtifactStatus.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/templates/siegelocation/ArtifactActivation.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/world/zone/SiegeZoneInstance.h"

namespace aion::gameserver::model::siege {

/** Java getTemplate().getActivation(): a NullPointerException without <artifact_activation> */
static const templates::siegelocation::ArtifactActivation& activationOf(ArtifactLocation& location) {
	const templates::siegelocation::ArtifactActivation* activation = location.getTemplate()->getActivation();
	if (activation == nullptr)
		throw runtime::NullPointerException("no artifact_activation for siege location " + std::to_string(location.getLocationId()));
	return *activation;
}

ArtifactLocation::ArtifactLocation(const templates::siegelocation::SiegeLocationTemplate* value)
	: SiegeLocation(value) {
	setVulnerable(true);
}

runtime::Ref<ArtifactLocation> ArtifactLocation::create(const templates::siegelocation::SiegeLocationTemplate* value) {
	return runtime::makeRef<ArtifactLocation>(value);
}

// Java ArtifactLocation.java:23-26
int32_t ArtifactLocation::getNextState() {
	return STATE_VULNERABLE;
}

// Java ArtifactLocation.java:32-35
void ArtifactLocation::setInitialDelay(int64_t capturedTime) {
	const int64_t cd = activationOf(*this).getCd();
	lastArtifactActivation.set(cd > 900000 ? capturedTime - cd + 900000 : capturedTime);
}

// Java ArtifactLocation.java:41-48
int32_t ArtifactLocation::getCoolDown() {
	const int64_t cd = activationOf(*this).getCd();
	const int64_t millisSinceLastActivation = commons::utils::currentTimeMillis() - lastArtifactActivation.get();
	if (millisSinceLastActivation > cd)
		return 0;
	else
		return static_cast<int32_t>((cd - millisSinceLastActivation) / 1000);
}

// Java ArtifactLocation.java:53-57
std::string ArtifactLocation::getL10n() {
	const templates::siegelocation::ArtifactActivation& activation = activationOf(*this);
	const skillengine::model::SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(activation.getSkillId());
	if (skillTemplate == nullptr)
		throw runtime::NullPointerException("no skill template " + std::to_string(activation.getSkillId())); // Java: skillTemplate.getL10n()
	return skillTemplate->getL10n();
}

// Java ArtifactLocation.java:59-61
bool ArtifactLocation::isStandAlone() {
	return !services::SiegeService::getInstance().getFortresses().containsKey(getLocationId());
}

// Java ArtifactLocation.java:63-65
runtime::Ptr<FortressLocation> ArtifactLocation::getOwningFortress() {
	return services::SiegeService::getInstance().getFortress(getLocationId());
}

// Java ArtifactLocation.java:70-72
ArtifactStatus ArtifactLocation::getStatus() {
	const std::optional<ArtifactStatus> value = status.get();
	return value ? *value : ArtifactStatus::IDLE;
}

ArtifactLocation::~ArtifactLocation() = default;

} // namespace aion::gameserver::model::siege
