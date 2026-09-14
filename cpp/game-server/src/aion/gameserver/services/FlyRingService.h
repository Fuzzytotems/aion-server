#pragma once

#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 *
 * @author xavier
 */
class FlyRingService : public runtime::Immortal {
public:
	static FlyRingService& getInstance(); // Java singleton
private:
	FlyRingService();
	~FlyRingService();
};

} // namespace aion::gameserver::services
