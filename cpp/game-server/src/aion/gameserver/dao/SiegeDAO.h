#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Sarynth
 */
class SiegeDAO {
public:
	static bool loadSiegeLocations(const std::unordered_map<int32_t, runtime::Ptr<model::siege::SiegeLocation>>& locations);
	static bool updateSiegeLocation(model::siege::SiegeLocation& siegeLocation);
private:
	static bool insertSiegeLocation(model::siege::SiegeLocation& siegeLocation);
};

} // namespace aion::gameserver::dao
