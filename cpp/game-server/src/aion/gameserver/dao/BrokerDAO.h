#pragma once

#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::dao {

class BrokerDAO {
public:
	static std::vector<runtime::Ref<model::gameobjects::BrokerItem>> loadBroker();
	static bool store(runtime::Ptr<model::gameobjects::BrokerItem> item);
private:
	static bool insertBrokerItem(model::gameobjects::BrokerItem& item);
	static bool deleteBrokerItem(model::gameobjects::BrokerItem& item);
	static bool updateBrokerItem(model::gameobjects::BrokerItem& item);
};

} // namespace aion::gameserver::dao
