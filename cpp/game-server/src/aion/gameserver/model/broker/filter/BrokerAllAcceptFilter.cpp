#include "aion/gameserver/model/broker/filter/BrokerAllAcceptFilter.h"

namespace aion::gameserver::model::broker::filter {

BrokerAllAcceptFilter::~BrokerAllAcceptFilter() = default;

runtime::Ref<BrokerAllAcceptFilter> BrokerAllAcceptFilter::create() {
	return runtime::makeRef<BrokerAllAcceptFilter>();
}

bool BrokerAllAcceptFilter::accept(const templates::item::ItemTemplate* /*template_*/) {
	return true;
}

} // namespace aion::gameserver::model::broker::filter
