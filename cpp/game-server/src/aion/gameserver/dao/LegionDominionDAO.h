#pragma once

#include <cstdint>
#include <map>
#include <string_view>
#include <unordered_map>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/legionDominion/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Yeats
 */
class LegionDominionDAO {
public:
	static bool loadOrCreateLegionDominionLocations(const std::unordered_map<int32_t,
		runtime::Ptr<model::legionDominion::LegionDominionLocation>>& locations);
	static void updateLegionDominionLocation(model::legionDominion::LegionDominionLocation& loc);
	/** C++: Java TreeMap */
	static std::map<int32_t, runtime::Ref<model::legionDominion::LegionDominionParticipantInfo>> loadParticipants(
		model::legionDominion::LegionDominionLocation& loc);
	static void storeNewInfo(int32_t id, model::legionDominion::LegionDominionParticipantInfo& info);
	static void updateInfo(model::legionDominion::LegionDominionParticipantInfo& info);
	static void delete_(model::legionDominion::LegionDominionParticipantInfo& info);
};

} // namespace aion::gameserver::dao
