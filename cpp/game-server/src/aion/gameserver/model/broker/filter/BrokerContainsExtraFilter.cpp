#include "aion/gameserver/model/broker/filter/BrokerContainsExtraFilter.h"

#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::broker::filter {

BrokerContainsExtraFilter::BrokerContainsExtraFilter(std::initializer_list<int32_t> masksValue)
	: masks(runtime::Array<int32_t>::of(masksValue)) {
}

BrokerContainsExtraFilter::~BrokerContainsExtraFilter() = default;

runtime::Ref<BrokerContainsExtraFilter> BrokerContainsExtraFilter::create(std::initializer_list<int32_t> masksValue) {
	return runtime::makeRef<BrokerContainsExtraFilter>(masksValue);
}

bool BrokerContainsExtraFilter::accept(const templates::item::ItemTemplate* template_) {
	if (template_ == nullptr)
		throw runtime::NullPointerException("template");
	int32_t mask = template_->getTemplateId() / 10000;
	for (int32_t i : *masks) {
		if (i == mask)
			return true;
	}
	return false;
}

} // namespace aion::gameserver::model::broker::filter
