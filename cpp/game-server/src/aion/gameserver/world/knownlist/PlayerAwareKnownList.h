#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/fwd.h"

namespace aion::gameserver::world::knownlist {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A KnownList part (House: `setKnownlist(std::make_unique<PlayerAwareKnownList>(*this))`).
 *
 * @author ATracer
 */
class PlayerAwareKnownList : public KnownList {
public:
	explicit PlayerAwareKnownList(model::gameobjects::VisibleObject& owner);
	~PlayerAwareKnownList() override;

protected:
	bool isAwareOf(runtime::Ptr<model::gameobjects::VisibleObject> newObject) override final;
};

} // namespace aion::gameserver::world::knownlist
