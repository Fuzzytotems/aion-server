#include "aion/gameserver/model/items/GodStone.h"

#include <algorithm>

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/item/GodstoneInfo.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/JavaMath.h"

namespace aion::gameserver::model::items {

GodStone::GodStone(gameobjects::Item& parentItem, int32_t activatedCountValue, int32_t itemIdValue,
	const templates::item::GodstoneInfo* godstoneInfoValue, PersistentState state)
	: ItemStone(parentItem.getObjectId(), itemIdValue, 0, state), godstoneInfo(godstoneInfoValue), activatedCount(activatedCountValue) {
}

GodStone::~GodStone() = default;

runtime::Ref<GodStone> GodStone::create(gameobjects::Item& parentItem, int32_t activatedCountValue, int32_t itemIdValue,
	const templates::item::GodstoneInfo* godstoneInfoValue, PersistentState state) {
	return runtime::makeRef<GodStone>(parentItem, activatedCountValue, itemIdValue, godstoneInfoValue, state);
}

void GodStone::increaseActivatedCount() {
	activatedCount++; // java-race: unsynchronized increment, a concurrent activation may be lost as in Java
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

bool GodStone::tryActivate(bool isMainHandWeapon, gameobjects::Creature& target) {
	int64_t now = commons::utils::currentTimeMillis();
	if (configs::main::CustomConfig::GODSTONE_ACTIVATION_RATE.load() <= 0)
		return false;
	// Deviation (D6): Java reads and then sets the cooldown, so two concurrent hits of the weapon holder can both pass it and both roll; the
	// compare-and-set lets exactly one of them start the cooldown (docs/deviations/P4-13.md)
	for (;;) {
		int64_t expireTimeMillis = cooldownExpireTimeMillis.get();
		if (now < expireTimeMillis)
			return false;
		if (cooldownExpireTimeMillis.compareAndSet(expireTimeMillis,
				now + static_cast<int64_t>(configs::main::CustomConfig::GODSTONE_EVALUATION_COOLDOWN_MILLIS.load())))
			break;
	}

	if (godstoneInfo == nullptr)
		throw runtime::NullPointerException("godstoneInfo");
	int32_t procProbability = isMainHandWeapon ? godstoneInfo->getProbability() : godstoneInfo->getProbabilityLeft();
	procProbability -= target.getGameStats()->getStat(stats::container::StatEnum::PROC_REDUCE_RATE, 0)->getCurrent();
	if (procProbability > 0)
		procProbability =
			std::max(1, utils::JavaMath::round(static_cast<float>(procProbability) * configs::main::CustomConfig::GODSTONE_ACTIVATION_RATE.load()));

	return commons::utils::Rnd::get(1, 1000) <= procProbability;
}

} // namespace aion::gameserver::model::items
