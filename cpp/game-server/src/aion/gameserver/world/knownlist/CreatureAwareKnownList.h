#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/fwd.h"

namespace aion::gameserver::world::knownlist {

/**
 * A known list that only knows creatures.
 * <p>
 * C++: a KnownList part (`setKnownlist(std::make_unique<CreatureAwareKnownList>(*this))`).
 *
 * @author ATracer
 */
class CreatureAwareKnownList : public KnownList {
public:
	explicit CreatureAwareKnownList(model::gameobjects::VisibleObject& owner);
	~CreatureAwareKnownList() override;

protected:
	bool isAwareOf(runtime::Ptr<model::gameobjects::VisibleObject> newObject) override final;
};

} // namespace aion::gameserver::world::knownlist
