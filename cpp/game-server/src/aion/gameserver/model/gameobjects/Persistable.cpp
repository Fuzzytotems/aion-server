#include "aion/gameserver/model/gameobjects/Persistable.h"

namespace aion::gameserver::model::gameobjects {

const runtime::PinnedCallback<bool(Persistable&)> Persistable::NEW = Persistable::newPredicate(PersistentState::NEW);
const runtime::PinnedCallback<bool(Persistable&)> Persistable::CHANGED = Persistable::newPredicate(PersistentState::UPDATE_REQUIRED);
const runtime::PinnedCallback<bool(Persistable&)> Persistable::DELETED = Persistable::newPredicate(PersistentState::DELETED);

runtime::PinnedCallback<bool(Persistable&)> Persistable::newPredicate(PersistentState state) {
	/** Java: the lambda `persistable -> persistable != null && persistable.getPersistentState() == state` (Persistable.java:22) */
	struct PersistentStatePredicate : runtime::TaskStruct {
		PersistentState expected;

		bool operator()(Persistable& persistable) const { return persistable.getPersistentState() == expected; }
	};
	return runtime::PinnedCallback<bool(Persistable&)>(PersistentStatePredicate{{}, state});
}

} // namespace aion::gameserver::model::gameobjects
