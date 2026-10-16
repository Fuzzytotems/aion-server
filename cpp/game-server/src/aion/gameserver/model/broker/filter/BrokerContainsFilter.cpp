#include "aion/gameserver/model/broker/filter/BrokerContainsFilter.h"

#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::broker::filter {

BrokerContainsFilter::BrokerContainsFilter(std::initializer_list<int32_t> masksValue) : masks(runtime::Array<int32_t>::of(masksValue)) {
}

BrokerContainsFilter::~BrokerContainsFilter() = default;

runtime::Ref<BrokerContainsFilter> BrokerContainsFilter::create(std::initializer_list<int32_t> masksValue) {
	return runtime::makeRef<BrokerContainsFilter>(masksValue);
}

bool BrokerContainsFilter::accept(const templates::item::ItemTemplate* template_) {
	if (template_ == nullptr)
		throw runtime::NullPointerException("template");
	int32_t mask = template_->getTemplateId() / 100000;
	for (int32_t i : *masks) {
		if (i == mask)
			return true;
	}
	return false;
}

} // namespace aion::gameserver::model::broker::filter
