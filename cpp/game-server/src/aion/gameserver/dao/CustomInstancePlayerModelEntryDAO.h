#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/custom/instance/neuralnetwork/fwd.h"
#include "aion/gameserver/dao/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Jo, Estrayl
 */
class CustomInstancePlayerModelEntryDAO {
public:
	static std::vector<runtime::Ref<custom::instance::neuralnetwork::PlayerModelEntry>> loadPlayerModelEntries(int32_t playerId);
	static void insertNewRecords(const std::vector<runtime::Ptr<custom::instance::neuralnetwork::PlayerModelEntry>>& filteredEntries);
};

} // namespace aion::gameserver::dao
