#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/legionDominion/fwd.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 *
 * @author Yeats
 */
class LegionDominionService : public runtime::Immortal {
private:
	runtime::TreeMap<int32_t, runtime::Ref<model::legionDominion::LegionDominionLocation>> legionDominionLocations{AION_LOCK_CLASS(LegionDominionService::legionDominionLocations)}; // Java: = new TreeMap<>()
	LegionDominionService();
	~LegionDominionService();
public:
	static LegionDominionService& getInstance(); // Java singleton
	void initLocations();
	std::vector<runtime::Ptr<model::legionDominion::LegionDominionLocation>> getLegionDominions();
	runtime::Ptr<model::legionDominion::LegionDominionLocation> getLegionDominionLoc(int32_t locId);
	bool join(int32_t legionId, int32_t locId);
	void onFinishInstance(runtime::Ptr<model::team::legion::Legion> legion, int32_t points, int64_t time);
	void startWeeklyCalculation();
private:
	void updateLegionOccupation(runtime::Ptr<model::team::legion::Legion> legion, model::legionDominion::LegionDominionLocation& location, bool shouldOccupy);
public:
	bool isInCalculationTime();
	bool openInvasionRift(int32_t territoryId);
};

} // namespace aion::gameserver::services
