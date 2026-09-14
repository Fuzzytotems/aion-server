#include "aion/gameserver/geoEngine/scene/Spatial.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/geoEngine/bounding/BoundingVolume.h"
#include "aion/gameserver/geoEngine/scene/Node.h"

namespace aion::gameserver::geoEngine::scene {

Spatial::Spatial() = default; // Java: this(null)

Spatial::Spatial(std::string_view nameValue) : name(std::string(nameValue)) {
}

Spatial::~Spatial() = default;

void Spatial::setName(std::optional<std::string_view> value) {
	AION_UNPORTED();
}

void Spatial::setParent(runtime::Ptr<Node> value) {
	parent.set(value);
}

bool Spatial::removeFromParent() {
	AION_UNPORTED();
}

bool Spatial::hasAncestor(Node& ancestor) {
	AION_UNPORTED();
}

bool Spatial::matches(const std::type_info* spatialSubclass, std::optional<std::string_view> nameRegex) {
	AION_UNPORTED();
}

std::string Spatial::toString() {
	AION_UNPORTED();
}

runtime::Ref<Spatial> Spatial::clone() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::geoEngine::scene
