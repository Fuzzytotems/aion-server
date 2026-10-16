#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"

#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::siegelocation {

namespace {
const ArtifactActivation& activationOf(const ArtifactActivation* activation, int32_t id) {
	if (activation == nullptr)
		throw runtime::NullPointerException("SiegeLocationTemplate " + std::to_string(id) + " has no artifact activation");
	return *activation;
}
} // namespace

int32_t SiegeLocationTemplate::getRepeatCount() const {
	return activationOf(artifactActivation.get(), id).getRepeatCount();
}

int32_t SiegeLocationTemplate::getRepeatInterval() const {
	return activationOf(artifactActivation.get(), id).getRepeatInterval();
}

const std::vector<int32_t>& SiegeLocationTemplate::getFortressDependency() const {
	static const std::vector<int32_t> empty;
	return fortressDependency ? *fortressDependency : empty;
}

int32_t SiegeLocationTemplate::getKinahRewardByRewardLevel(int32_t rewardLevel) const {
	if (!kinahRewards || rewardLevel > static_cast<int32_t>(kinahRewards->size()) - 1)
		return 0;
	if (rewardLevel < 0) // Java: List.get throws for a negative level
		throw runtime::IndexOutOfBoundsException("Index " + std::to_string(rewardLevel) + " out of bounds for length " +
		                                         std::to_string(kinahRewards->size()));
	return (*kinahRewards)[static_cast<size_t>(rewardLevel)];
}

} // namespace aion::gameserver::model::templates::siegelocation
