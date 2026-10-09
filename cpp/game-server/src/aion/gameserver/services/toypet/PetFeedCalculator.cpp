#include "aion/gameserver/services/toypet/PetFeedCalculator.h"

#include <algorithm>
#include <set>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/PetFeedData.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/pet/PetFeedResult.h"
#include "aion/gameserver/model/templates/pet/PetFlavour.h"
#include "aion/gameserver/model/templates/pet/PetRewards.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/toypet/PetFeedProgress.h"
#include "aion/gameserver/services/toypet/PetHungryLevelInfo.h"
#include "aion/gameserver/utils/JavaMath.h"

namespace aion::gameserver::services::toypet {

namespace {

/** the body of calculate() (PetFeedCalculator.java:63-82), shared by the static initializer and calculate() itself */
void fillPointValues() {
	runtime::Ref<runtime::Array<int8_t>> levels = PetFeedCalculator::itemLevels.get();
	runtime::Ref<runtime::Array<int16_t>> counts = PetFeedCalculator::fullCounts.get();
	runtime::Ref<runtime::Array<runtime::Ref<runtime::Array<int32_t>>>> points = PetFeedCalculator::pointValues.get();
	for (int32_t l = 0; l < levels->length(); l++) {
		const int8_t levelByte = (*levels)[l].get();
		const int16_t level = static_cast<int16_t>(levelByte & 0xFF);
		if (level < 10)
			continue;
		int32_t countIndex = 0;
		for (int32_t c = 0; c < counts->length(); c++) {
			const int16_t countByte = (*counts)[c].get();
			const int16_t count = static_cast<int16_t>(countByte & 0xFF);
			int32_t finalLevel = level;
			if (finalLevel % 5 == 0)
				finalLevel--;
			const int32_t pointLevel = (*levels)[finalLevel / 5].get();
			const int32_t feedPoints = std::max(0, pointLevel - 5) / 5 * 8;
			runtime::Ref<runtime::Array<int32_t>> row = (*points)[finalLevel / 5].get();
			(*row)[countIndex++] = PetFeedCalculator::getPoints(feedPoints, count);
		}
	}
}

/**
 * Java's static initializer (PetFeedCalculator.java:38-59), run when the class is first used: the sorted distinct full counts of the
 * flavours, the twelve item level steps and the point table (calculate()). A magic static, so it runs once, after the static data loaded.
 */
void ensureInitialized() {
	static const bool initialized = [] {
		std::set<int16_t> counts; // Java TreeSet<Short>: natural (signed) order
		for (const model::templates::pet::PetFlavour* flavour : dataholders::DataManager::PET_FEED_DATA->getPetFlavours()) {
			if (flavour->getFullCount() > 0)
				counts.insert(static_cast<int16_t>(flavour->getFullCount() & 0xFFFF));
		}
		runtime::Ref<runtime::Array<int16_t>> full = runtime::Array<int16_t>::make(static_cast<int32_t>(counts.size()));
		int32_t i = 0;
		for (int16_t count : counts)
			(*full)[i++] = count;
		PetFeedCalculator::fullCounts = full;
		runtime::Ref<runtime::Array<int8_t>> levels = runtime::Array<int8_t>::make(PetFeedCalculator::ITEM_MAX_LEVEL.get() / 5);
		(*levels)[0] = static_cast<int8_t>(5);
		for (int32_t j = 1; j < levels->length(); j++)
			(*levels)[j] = static_cast<int8_t>((*levels)[j - 1].get() + 5);
		PetFeedCalculator::itemLevels = levels;
		runtime::Ref<runtime::Array<runtime::Ref<runtime::Array<int32_t>>>> points =
			runtime::Array<runtime::Ref<runtime::Array<int32_t>>>::make(levels->length());
		for (int32_t row = 0; row < levels->length(); row++) // Java new int[a][b]: every row allocated
			(*points)[row] = runtime::Array<int32_t>::make(full->length());
		PetFeedCalculator::pointValues = points;
		fillPointValues(); // calculate()
		return true;
	}();
	(void)initialized;
}

} // namespace

// Java PetFeedCalculator.java:63-82: the class is initialized first (its static block runs calculate() once itself)
void PetFeedCalculator::calculate() {
	ensureInitialized();
	fillPointValues();
}

// Java PetFeedCalculator.java:90-112: int * float comparisons in float, `* 1.05` in double
int32_t PetFeedCalculator::getPoints(int32_t feedPoints, int32_t maxFeedCount) {
	int32_t points = 0;
	int32_t state = 0;
	int32_t consumed = 0;
	while (consumed < maxFeedCount) {
		bool needSwitch = false;
		const int32_t oldPoints = points;
		if ((state == 0 && static_cast<float>(consumed) > static_cast<float>(maxFeedCount) * 0.5f) ||
			(state == 1 && static_cast<float>(consumed) > static_cast<float>(maxFeedCount) * 0.8f) ||
			(state == 2 && static_cast<double>(consumed) > static_cast<double>(maxFeedCount) * 1.05)) {
			needSwitch = true;
		}
		points += feedPoints;
		if (needSwitch) {
			state++;
			if ((state == 1 && static_cast<float>(consumed) <= 0.487f * static_cast<float>(maxFeedCount)) ||
				(state == 2 && static_cast<float>(consumed) <= 0.78f * static_cast<float>(maxFeedCount))) {
				state--;
				points = oldPoints;
			}
		}
		consumed++;
	}
	return points;
}

// Java PetFeedCalculator.java:114-154
void PetFeedCalculator::updatePetFeedProgress(PetFeedProgress& progress, int32_t itemLevel, int32_t maxFeedCount) {
	ensureInitialized();
	const PetHungryLevel currHungryLevel = progress.getHungryLevel();
	if (progress.isLovedFeeded()) { // loved food
		if (progress.getLovedFoodRemaining() == 0)
			return;
		progress.setHungryLevel(PetHungryLevel::FULL);
		progress.incrementCount(true);
		return;
	}

	const int32_t oldPoints = progress.getTotalPoints();
	bool needSwitch = false;
	const float regular = static_cast<float>(progress.getRegularCount());
	if ((currHungryLevel == PetHungryLevel::HUNGRY && regular > static_cast<float>(maxFeedCount) * 0.5f) ||
		(currHungryLevel == PetHungryLevel::CONTENT && regular > static_cast<float>(maxFeedCount) * 0.8f) ||
		(currHungryLevel == PetHungryLevel::SEMIFULL &&
			static_cast<double>(progress.getRegularCount()) > static_cast<double>(maxFeedCount) * 1.05)) {
		// forcefully switch level
		needSwitch = true;
	} else {
		int32_t finalLevel = itemLevel;
		if (finalLevel % 5 == 0)
			finalLevel--;
		const int8_t pointLevel = (*itemLevels.get())[finalLevel / 5].get();
		const int8_t pointsEarned = static_cast<int8_t>(std::max(0, pointLevel - 5) / 5 * 8);
		const int32_t feedProgress = progress.getTotalPoints() + pointsEarned;
		progress.setTotalPoints(feedProgress);
	}

	if (needSwitch) {
		// just a prevention to not switch level
		const PetHungryLevel nextLevel = getNextValue(progress.getHungryLevel());
		if ((nextLevel == PetHungryLevel::CONTENT && static_cast<float>(progress.getRegularCount()) <= 0.487f * static_cast<float>(maxFeedCount)) ||
			(nextLevel == PetHungryLevel::SEMIFULL && static_cast<float>(progress.getRegularCount()) <= 0.78f * static_cast<float>(maxFeedCount))) {
			progress.setTotalPoints(oldPoints);
		} else {
			progress.setHungryLevel(nextLevel);
		}
	}
	progress.incrementCount(false);
}

// Java PetFeedCalculator.java:156-200
const model::templates::pet::PetFeedResult* PetFeedCalculator::getReward(int32_t fullCount, const model::templates::pet::PetRewards* rewardGroup,
	PetFeedProgress& progress, int32_t playerLevel) {
	ensureInitialized();
	if (rewardGroup == nullptr && progress.getHungryLevel() == PetHungryLevel::FULL) // Java: rewardGroup.getResults() on null
		throw runtime::NullPointerException("rewardGroup");
	if (progress.getHungryLevel() != PetHungryLevel::FULL || rewardGroup->getResults().empty())
		return nullptr;

	// binary search works because fullCounts is sorted
	runtime::Ref<runtime::Array<int16_t>> counts = fullCounts.get();
	std::vector<int16_t> sorted;
	for (int32_t i = 0; i < counts->length(); i++)
		sorted.push_back((*counts)[i].get());
	const auto found = std::lower_bound(sorted.begin(), sorted.end(), static_cast<int16_t>(fullCount));
	if (found == sorted.end() || *found != static_cast<int16_t>(fullCount)) // Arrays.binarySearch < 0
		return nullptr;
	const int32_t pointsIndex = static_cast<int32_t>(found - sorted.begin());

	const std::vector<model::templates::pet::PetFeedResult>& results = rewardGroup->getResults();
	if (progress.isLovedFeeded()) { // for cash feed
		if (results.size() == 1)
			return &results[0];
		std::vector<const model::templates::pet::PetFeedResult*> validRewards;
		int32_t maxLevel = 0;
		for (const model::templates::pet::PetFeedResult& result : results) {
			const model::templates::item::ItemTemplate* item = dataholders::DataManager::ITEM_DATA->getItemTemplate(result.getItem());
			if (item == nullptr) // Java: getItemTemplate(...).getLevel() on null
				throw runtime::NullPointerException("ItemData.getItemTemplate(" + std::to_string(result.getItem()) + ")");
			const int32_t resultLevel = item->getLevel();
			if (resultLevel > playerLevel)
				continue;
			if (resultLevel > maxLevel) {
				maxLevel = resultLevel;
				validRewards.clear();
			}
			validRewards.push_back(&result);
		}
		const model::templates::pet::PetFeedResult* const* chosen = commons::utils::Rnd::get(validRewards);
		return chosen == nullptr ? nullptr : *chosen; // Java Rnd.get(emptyList) answers null
	}

	int32_t rewardIndex = 0;
	const int32_t totalRewards = static_cast<int32_t>(results.size());
	runtime::Ref<runtime::Array<runtime::Ref<runtime::Array<int32_t>>>> points = pointValues.get();
	for (int32_t row = 1; row < points->length(); row++) {
		runtime::Ref<runtime::Array<int32_t>> rowPoints = (*points)[row].get();
		if ((*rowPoints)[pointsIndex].get() <= progress.getTotalPoints()) {
			rewardIndex = utils::JavaMath::round(static_cast<float>(totalRewards) / static_cast<float>(points->length() - 1) * static_cast<float>(row)) - 1;
		}
	}
	// Fix rounding discrepancy
	if (rewardIndex < 0)
		rewardIndex = 0;
	else if (rewardIndex > totalRewards - 1)
		rewardIndex = totalRewards - 1;
	return &results[static_cast<size_t>(rewardIndex)];
}

} // namespace aion::gameserver::services::toypet
