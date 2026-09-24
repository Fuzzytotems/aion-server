#include "aion/gameserver/skillengine/properties/MaxCountProperty.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/skillengine/properties/TargetRangeAttribute.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::skillengine::properties {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;

bool MaxCountProperty::set(const Properties* properties, Properties::ValidationResult& result) {
	// Java: `TargetRangeAttribute value = properties.getTargetType()`; the only caller runs this step when target_type is set
	// (Properties.validateEffectedList), so `*` never reads an empty optional there
	TargetRangeAttribute value = *properties->getTargetType();
	int32_t maxCount = properties->getTargetMaxCount();
	if (maxCount == 0 || result.getTargets().size() <= maxCount)
		return true;

	switch (value) {
		case TargetRangeAttribute::AREA:
		case TargetRangeAttribute::PARTY:
		case TargetRangeAttribute::PARTY_WITHPET: {
			if (!result.getFirstTarget())
				return false;

			// Java: stream().sorted(Comparator.comparingDouble(distance to the first target)).limit(maxCount).collect(toSet()); sorted() is stable
			std::vector<Ptr<Creature>> sorted = result.getTargets().snapshot();
			Ptr<Creature> firstTarget = result.getFirstTarget();
			std::vector<double> distances;
			distances.reserve(sorted.size());
			std::vector<size_t> order(sorted.size());
			for (size_t i = 0; i < sorted.size(); ++i) {
				distances.push_back(utils::PositionUtil::getDistance(*firstTarget, *sorted[i]));
				order[i] = i;
			}
			std::stable_sort(order.begin(), order.end(), [&distances](size_t a, size_t b) { return distances[a] < distances[b]; });
			std::vector<Ptr<Creature>> nearestCreatures;
			for (size_t i = 0; i < order.size() && nearestCreatures.size() < static_cast<size_t>(maxCount); ++i)
				nearestCreatures.push_back(sorted[order[i]]);

			// rebuild effected list with correct number of creatures and their summons
			if (value == TargetRangeAttribute::PARTY_WITHPET) {
				std::vector<Ptr<Creature>> creatures = nearestCreatures; // Java: nearestCreatures.toArray()
				for (Ptr<Creature> creature : creatures) {
					Ptr<Player> player = runtime::as<Player>(creature);
					Ptr<Creature> summon = player ? Ptr<Creature>(player->getSummon()) : Ptr<Creature>();
					if (summon && result.getTargets().contains(summon))
						nearestCreatures.push_back(summon);
				}
			}
			result.getTargets().retainAll(nearestCreatures);
			break;
		}
		default:
			break;
	}
	return true;
}

} // namespace aion::gameserver::skillengine::properties
