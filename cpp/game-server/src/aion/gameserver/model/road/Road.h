#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/road/fwd.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/model/templates/road/fwd.h"

namespace aion::gameserver::model::road {

/**
 * This class handles roads (teleport ways) between maps (e.g. road from Verteron to Eltnen)
 * Each road is a one-way portal, so for a map-connection in both directions you need two roads.
 * <p>
 * A visible object: `VisibleObject::create<Road>(template, instanceId)` (§10.1). The constructor is declared but not defined yet: it creates the
 * RoadController (the VisibleObject controller part) and binds it, and controllers/RoadController.h has no declaration header yet (P4-11b).
 * The id is never released (runtime-architecture.md §6 group (c)).
 *
 * @author SheppeR
 */
class Road : public gameobjects::VisibleObject {
	AION_MAKE_REF_FRIEND
private:
	// Java field `template` (a C++ keyword: template_)
	const templates::road::RoadTemplate* template_;
	const runtime::Ref<geometry::Plane3D> plane;

protected:
	Road(CreateKey key, const templates::road::RoadTemplate* template_, std::optional<int32_t> instanceId);
	~Road() override;

public:
	bool isCrossed(const geoEngine::math::Vector3f& oldPosition, const geoEngine::math::Vector3f& newPosition);

	const templates::road::RoadTemplate* getTemplate() const { return template_; }

	std::string getName() override;

	void spawn();
};

} // namespace aion::gameserver::model::road
