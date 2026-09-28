#pragma once

#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/taskmanager/AbstractFIFOPeriodicTaskManager.h"
#include "aion/gameserver/world/zone/fwd.h"

// instantiated in AbstractPeriodicTaskManager.cpp, so this header needs no complete Creature (hub-headers.md §3.1)
extern template class aion::gameserver::taskmanager::AbstractFIFOPeriodicTaskManager<aion::gameserver::model::gameobjects::Creature>;

namespace aion::gameserver::world::zone {

/**
 * Revalidates the zones of the queued creatures every 500 ms (and the zone levels of players).
 * <p>
 * C++: RefCounted through AbstractPeriodicTaskManager; the singleton is a never-released Ref (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class ZoneUpdateService : public taskmanager::AbstractFIFOPeriodicTaskManager<model::gameobjects::Creature> {
	AION_MAKE_REF_FRIEND
private:
	ZoneUpdateService();
	~ZoneUpdateService() override;

protected:
	void callTask(model::gameobjects::Creature& creature) override;

	std::string getCalledMethodName() override;

public:
	static ZoneUpdateService& getInstance();
};

} // namespace aion::gameserver::world::zone
