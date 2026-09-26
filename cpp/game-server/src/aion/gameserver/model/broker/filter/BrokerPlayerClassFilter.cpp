#include "aion/gameserver/model/broker/filter/BrokerPlayerClassFilter.h"

#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::broker::filter {

BrokerPlayerClassFilter::BrokerPlayerClassFilter(PlayerClass playerClassValue) : playerClass(playerClassValue) {
}

BrokerPlayerClassFilter::~BrokerPlayerClassFilter() = default;

runtime::Ref<BrokerPlayerClassFilter> BrokerPlayerClassFilter::create(PlayerClass playerClassValue) {
	return runtime::makeRef<BrokerPlayerClassFilter>(playerClassValue);
}

bool BrokerPlayerClassFilter::accept(const templates::item::ItemTemplate* template_) {
	if (template_ == nullptr)
		throw runtime::NullPointerException("template");
	return template_->isClassSpecific(playerClass);
}

} // namespace aion::gameserver::model::broker::filter
