#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/commons/database/Connection.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/house/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Rolandas
 */
class PlayerRegisteredItemsDAO {
public:
	static std::vector<int32_t> getUsedIDs();
	static void loadRegistry(model::house::HouseRegistry& registry);
private:
	static runtime::Ref<model::gameobjects::HouseObject> constructObject(model::house::HouseRegistry& registry, commons::database::ResultSet& rset);
	static runtime::Ref<model::gameobjects::HouseDecoration> createDecoration(model::house::HouseRegistry& registry,
		commons::database::ResultSet& rset);
public:
	static bool store(model::house::HouseRegistry& registry, int32_t playerId);
private:
	static bool storeObjects(commons::database::Connection& con, const std::vector<runtime::Ptr<model::gameobjects::HouseObject>>& objects,
		int32_t playerId, bool isNew);
	static bool storeDecors(commons::database::Connection& con, const std::vector<runtime::Ptr<model::gameobjects::HouseDecoration>>& decors,
		int32_t playerId, bool isNew);
	static bool deleteObjects(commons::database::Connection& con, const std::vector<runtime::Ptr<model::gameobjects::HouseObject>>& objects);
	static bool deleteDecors(commons::database::Connection& con, const std::vector<runtime::Ptr<model::gameobjects::HouseDecoration>>& decors);
public:
	static bool deletePlayerItems(int32_t playerId);
	static void resetRegistry(int32_t playerId);
};

} // namespace aion::gameserver::dao
