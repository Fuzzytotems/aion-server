#pragma once

#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author SheppeR
 */
class RoadService : public runtime::Immortal {
public:
	static RoadService& getInstance(); // Java singleton
private:
	RoadService();
};

} // namespace aion::gameserver::services
