#include "aion/gameserver/model/broker/filter/BrokerMinMaxFilter.h"

#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::broker::filter {

BrokerMinMaxFilter::BrokerMinMaxFilter(int32_t minValue, int32_t maxValue) : min(minValue), max(maxValue) {
}

BrokerMinMaxFilter::~BrokerMinMaxFilter() = default;

runtime::Ref<BrokerMinMaxFilter> BrokerMinMaxFilter::create(int32_t minValue, int32_t maxValue) {
	return runtime::makeRef<BrokerMinMaxFilter>(minValue, maxValue);
}

bool BrokerMinMaxFilter::accept(const templates::item::ItemTemplate* template_) {
	if (template_ == nullptr)
		throw runtime::NullPointerException("template");
	int32_t templateMask = template_->getTemplateId() / 100000;
	return templateMask >= min && templateMask <= max;
}

} // namespace aion::gameserver::model::broker::filter
