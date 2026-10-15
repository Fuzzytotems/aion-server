#pragma once

#include "aion/gameserver/model/gameobjects/findGroup/fwd.h"

namespace aion::gameserver::model::gameobjects::findGroup {

class FindGroupEntry {
public:
	/** C++ only: Ref<FindGroupEntry> retains the implementing object (hub-headers.md §9.2). */
	virtual void retain() const noexcept = 0;

	virtual void release() const noexcept = 0;

	virtual ~FindGroupEntry() = default;
};

} // namespace aion::gameserver::model::gameobjects::findGroup
