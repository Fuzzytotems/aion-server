#include "aion/gameserver/model/broker/filter/BrokerPlayerClassExtraFilter.h"

#include "aion/gameserver/model/templates/item/ItemTemplate.h"

namespace aion::gameserver::model::broker::filter {

BrokerPlayerClassExtraFilter::BrokerPlayerClassExtraFilter(int32_t maskValue, PlayerClass playerClass)
	: BrokerPlayerClassFilter(playerClass), mask(maskValue) {
}

BrokerPlayerClassExtraFilter::~BrokerPlayerClassExtraFilter() = default;

runtime::Ref<BrokerPlayerClassExtraFilter> BrokerPlayerClassExtraFilter::create(int32_t maskValue, PlayerClass playerClass) {
	return runtime::makeRef<BrokerPlayerClassExtraFilter>(maskValue, playerClass);
}

bool BrokerPlayerClassExtraFilter::accept(const templates::item::ItemTemplate* template_) {
	return BrokerPlayerClassFilter::accept(template_) && mask == template_->getTemplateId() / 100000;
}

} // namespace aion::gameserver::model::broker::filter
