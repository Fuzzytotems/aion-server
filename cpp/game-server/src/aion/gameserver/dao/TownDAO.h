#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/town/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author ViAl
 */
class TownDAO {
public:
	static std::unordered_map<int32_t, runtime::Ref<model::town::Town>> load(model::Race race);
	static void store(model::town::Town& town);
private:
	static void insertTown(model::town::Town& town);
	static void updateTown(model::town::Town& town);
};

} // namespace aion::gameserver::dao
