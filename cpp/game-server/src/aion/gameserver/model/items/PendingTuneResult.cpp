#include "aion/gameserver/model/items/PendingTuneResult.h"

namespace aion::gameserver::model::items {

PendingTuneResult::PendingTuneResult(int32_t optionalSocketsValue, int32_t enchantBonusValue, int32_t statBonusIdValue, bool attributeOnlyValue)
	: optionalSockets(optionalSocketsValue), enchantBonus(enchantBonusValue), statBonusId(statBonusIdValue), attributeOnly(attributeOnlyValue) {
}

PendingTuneResult::~PendingTuneResult() = default;

runtime::Ref<PendingTuneResult> PendingTuneResult::create(int32_t optionalSocketsValue, int32_t enchantBonusValue, int32_t statBonusIdValue,
	bool attributeOnlyValue) {
	return runtime::makeRef<PendingTuneResult>(optionalSocketsValue, enchantBonusValue, statBonusIdValue, attributeOnlyValue);
}

} // namespace aion::gameserver::model::items
