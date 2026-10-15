#include "aion/gameserver/model/drop/Drop.h"

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::drop {

Drop::Drop(int32_t itemIdValue, int32_t minAmountValue, int32_t maxAmountValue, float chanceValue) {
	this->itemId = itemIdValue;
	this->minAmount = minAmountValue;
	this->maxAmount = maxAmountValue;
	this->chance = chanceValue;
	// Java: afterUnmarshal(null, null)
	if (std::optional<std::string> error = validate())
		throw runtime::IllegalArgumentException(*error);
}

std::optional<std::string> Drop::validate() {
	if (chance <= 0)
		return "chance (" + geoEngine::math::JavaFloat::toString(chance) + ") for drop " + std::to_string(itemId) + " must be greater than zero";
	if (minAmount <= 0)
		return "minAmount (" + std::to_string(minAmount) + ") for drop " + std::to_string(itemId) + " must be greater than zero";
	if (maxAmount == 0)
		maxAmount = minAmount;
	else if (maxAmount < minAmount)
		return "maxAmount (" + std::to_string(maxAmount) + ") for drop " + std::to_string(itemId) + " must be greater than minAmount ("
			+ std::to_string(minAmount) + ")";
	return std::nullopt;
}

void Drop::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	// Deviation: Java throws IllegalArgumentException, which fails the unmarshalling; LoadContext::fail throws StaticDataException with the
	// same message and the XML location (docs/deviations/P4-13.md)
	if (std::optional<std::string> error = validate())
		ctx.fail(*error);
}

std::string Drop::toString() const {
	return "Drop [itemId=" + std::to_string(itemId) + ", minAmount=" + std::to_string(minAmount) + ", maxAmount=" + std::to_string(maxAmount)
		+ ", chance=" + geoEngine::math::JavaFloat::toString(chance) + ", eachMember=" + (eachMember ? "true" : "false") + "]";
}

} // namespace aion::gameserver::model::drop
