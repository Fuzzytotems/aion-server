#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/templates/housing/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Rolandas
 */
class HousesDAO {
public:
	static std::vector<int32_t> getUsedIDs();
	static void storeHouse(model::house::House& house);
private:
	static void insertNewHouse(model::house::House& house);
	static void updateHouse(model::house::House& house);
public:
	static std::unordered_map<int32_t, runtime::Ref<model::house::House>> loadHouses(
		const std::vector<const model::templates::housing::HousingLand*>& lands, bool studios);
	static void deleteHouse(int32_t playerId);
};

} // namespace aion::gameserver::dao
