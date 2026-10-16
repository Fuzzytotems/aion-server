#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/model/Race.h"

namespace aion::gameserver::geoEngine::collision {

/**
 * Properties of a collision check that let geometry be ignored: siege shields of the own race and the static object with the given id.
 * <p>
 * K3 immutable (fieldmap), RefCounted because collision results and callers hold it as `Ptr<IgnoreProperties>` (CollisionResults.h,
 * GeoMap.h). C++ notes:
 * - `race` is `std::optional<Race>`: Java's ANY_RACE and of(staticId) store null (a fieldmap.toml decision, header request geo-2).
 * - The four named constants are `static const Ref<IgnoreProperties>&` bound in the .cpp to never-released Refs (hub-headers.md §11.1), so the
 *   identity comparisons of DespawnableNode (`ignoreProperties == IgnoreProperties.ANY_RACE`) keep working.
 */
class IgnoreProperties : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	// fieldmap.toml: static final RefCounted constants bound to never-released Refs (hub-headers.md §11.1; header request geo-2)
	static const runtime::Ref<IgnoreProperties>& ELYOS;
	static const runtime::Ref<IgnoreProperties>& ASMODIANS;
	static const runtime::Ref<IgnoreProperties>& BALAUR;
	static const runtime::Ref<IgnoreProperties>& ANY_RACE;

private:
	// fieldmap.toml: Java stores null for ANY_RACE and of(staticId); an optional keeps it (header request geo-2)
	const std::optional<model::Race> race;
	const int32_t staticId;

	IgnoreProperties(std::optional<model::Race> race, int32_t staticId);

protected:
	~IgnoreProperties() override;

public:
	/** Java: of(Race race, int staticId) - the shared constant for a player race without static id, otherwise a new object */
	static runtime::Ref<IgnoreProperties> of(std::optional<model::Race> race, int32_t staticId);

	static runtime::Ref<IgnoreProperties> of(model::Race race);

	/** Java: of(int staticId) (race null) */
	static runtime::Ref<IgnoreProperties> of(int32_t staticId);

	/** Java: Race getRace() (null: std::nullopt) */
	std::optional<model::Race> getRace() const { return race; }

	int32_t getStaticId() const { return staticId; }

	std::string toString() const;
};

} // namespace aion::gameserver::geoEngine::collision
