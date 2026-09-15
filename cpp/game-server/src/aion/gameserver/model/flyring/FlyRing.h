#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/flyring/fwd.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/model/templates/flyring/fwd.h"

namespace aion::gameserver::model::flyring {

/**
 * A fly ring of a map: crossing its plane within the radius counts as flying through it.
 * <p>
 * A visible object: `VisibleObject::create<FlyRing>(template, instanceId)` (§10.1). The constructor is declared but not defined yet: it creates the
 * FlyRingController (the VisibleObject controller part) and binds it, and controllers/FlyRingController.h has no declaration header yet (P4-11b).
 * The id is never released (runtime-architecture.md §6 group (c)).
 *
 * @author xavier
 */
class FlyRing : public gameobjects::VisibleObject {
	AION_MAKE_REF_FRIEND
private:
	// Java field `template` (a C++ keyword: template_)
	const templates::flyring::FlyRingTemplate* template_;
	const runtime::Ref<geometry::Plane3D> plane;

protected:
	FlyRing(CreateKey key, const templates::flyring::FlyRingTemplate* template_, int32_t instanceId);
	~FlyRing() override;

public:
	bool isCrossed(const geoEngine::math::Vector3f& oldPosition, const geoEngine::math::Vector3f& newPosition);

	const templates::flyring::FlyRingTemplate* getTemplate() const { return template_; }

	std::string getName() override;

	void spawn();
};

} // namespace aion::gameserver::model::flyring
