#pragma once

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/model/summons/fwd.h"

namespace aion::gameserver::model::summons {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Summon.pendingRelease`), created with create(). The release
 * task is a `FutureRef` (null until setTask).
 */
class SummonRelease : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const UnsummonType unsummonType;
	runtime::Field<runtime::FutureRef> task{};
	runtime::Field<bool> started{};

protected:
	explicit SummonRelease(UnsummonType unsummonType);
	~SummonRelease() override;

public:
	/** Java: new SummonRelease(unsummonType) */
	static runtime::Ref<SummonRelease> create(UnsummonType unsummonType);

	UnsummonType getUnsummonType() const { return unsummonType; }

	void setTask(runtime::FutureRef task);

	void markStarted() { started.set(true); }

	bool hasStarted() const { return started.get(); }

	bool isCancelableByMaster();

	bool cancel();
};

} // namespace aion::gameserver::model::summons
