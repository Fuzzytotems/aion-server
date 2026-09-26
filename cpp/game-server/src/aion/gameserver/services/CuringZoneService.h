#pragma once

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/curingzone/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 *
 * @author xTz
 */
class CuringZoneService : public runtime::Immortal {
private:
	runtime::ArrayList<runtime::Ref<model::curingzone::CuringObject>> curingObjects{AION_LOCK_CLASS(CuringZoneService::curingObjects)}; // Java: = new ArrayList<>()
	CuringZoneService();
	~CuringZoneService();
	void startTask();
public:
	static CuringZoneService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services
