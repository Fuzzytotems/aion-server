#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/model/gameobjects/siege/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/services/siege/fwd.h"

namespace aion::gameserver::services::siege {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 * Java generic Assault<SiegeType>: erased to a non-template class, type variables spelled as their bounds (hub-headers.md §8.1).
 *
 * @author Luzien, Estrayl
 */
class Assault : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicBoolean isStarted{AION_LOCK_CLASS(Assault::isStarted)}; // Java: = new AtomicBoolean()

protected:
	const runtime::Ref<model::siege::SiegeLocation> siegeLocation;
	const runtime::Ref<model::gameobjects::siege::SiegeNpc> boss;
	const int32_t locationId;
	const int32_t worldId;
	runtime::Field<runtime::FutureRef> dredgionTask{};
	runtime::Field<runtime::FutureRef> spawnTask{};

private:
protected:
	explicit Assault(Siege& siege);

public:
	int32_t getWorldId() const { return this->worldId; }

	void startAssault(int32_t delay);

	void finishAssault(bool captured);

protected:
	virtual void onAssaultFinish(bool captured) = 0;

	virtual void handleAssault() = 0;

	void spawnAssaulter(model::siege::Assaulter& a, model::gameobjects::siege::SiegeNpc& target);

	std::string getBossNpcL10n();

	~Assault() override;
};

} // namespace aion::gameserver::services::siege
