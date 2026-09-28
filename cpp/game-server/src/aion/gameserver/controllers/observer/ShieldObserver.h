#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/model/templates/shield/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Wakizashi, Source
 */
class ShieldObserver : public ActionObserver {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<model::siege::FortressLocation> location;
	const runtime::Ref<model::gameobjects::Creature> creature;
	const model::templates::shield::ShieldTemplate* shield;
	const runtime::Ref<model::geometry::Point3D> oldPosition;

protected:
	ShieldObserver(model::siege::FortressLocation& location, const model::templates::shield::ShieldTemplate* shield,
		model::gameobjects::Creature& creature);

public:
	static runtime::Ref<ShieldObserver> create(model::siege::FortressLocation& value, const model::templates::shield::ShieldTemplate* shieldValue,
		model::gameobjects::Creature& creatureValue);

	void moved() override;

protected:
	~ShieldObserver() override;
};

} // namespace aion::gameserver::controllers::observer
