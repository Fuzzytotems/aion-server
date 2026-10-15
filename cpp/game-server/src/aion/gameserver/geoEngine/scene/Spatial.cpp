#include "aion/gameserver/geoEngine/scene/Spatial.h"

#include <regex>
#include <string>

#include "aion/gameserver/geoEngine/bounding/BoundingVolume.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntentionInfo.h"
#include "aion/gameserver/geoEngine/models/GeoMap.h"
#include "aion/gameserver/geoEngine/scene/CloneNotSupportedException.h"
#include "aion/gameserver/geoEngine/scene/DespawnableNode.h"
#include "aion/gameserver/geoEngine/scene/Geometry.h"
#include "aion/gameserver/geoEngine/scene/Node.h"
#include "aion/gameserver/utils/SimpleClassName.h"

namespace aion::gameserver::geoEngine::scene {

Spatial::Spatial() = default; // Java: this(null)

Spatial::Spatial(std::string_view nameValue) : name(std::string(nameValue)) {
}

Spatial::~Spatial() = default;

void Spatial::setName(std::optional<std::string_view> value) {
	if (value)
		name.set(std::string(*value));
}

void Spatial::setParent(runtime::Ptr<Node> value) {
	parent.set(value.get());
}

bool Spatial::removeFromParent() {
	runtime::Ptr<Node> p = getParent();
	if (p != nullptr) {
		p->detachChild(runtime::Ptr<Spatial>(*this));
		return true;
	}
	return false;
}

bool Spatial::hasAncestor(Node& ancestor) {
	runtime::Ptr<Node> p = getParent();
	if (p == nullptr) {
		return false;
	} else if (p.get() == &ancestor) { // Java: parent.equals(ancestor) (identity)
		return true;
	} else {
		return p->hasAncestor(ancestor);
	}
}

bool Spatial::matches(const std::type_info* spatialSubclass, std::optional<std::string_view> nameRegex) {
	if (spatialSubclass != nullptr) {
		// Java: spatialSubclass.isInstance(this). Deviation: a std::type_info cannot test subclasses, so the scene classes are tested with
		// dynamic_cast and any other class must match exactly
		const std::type_info& type = *spatialSubclass;
		bool instance;
		if (type == typeid(Spatial))
			instance = true;
		else if (type == typeid(Node))
			instance = dynamic_cast<Node*>(this) != nullptr;
		else if (type == typeid(Geometry))
			instance = dynamic_cast<Geometry*>(this) != nullptr;
		else if (type == typeid(DespawnableNode))
			instance = dynamic_cast<DespawnableNode*>(this) != nullptr;
		else if (type == typeid(models::GeoMap))
			instance = dynamic_cast<models::GeoMap*>(this) != nullptr;
		else
			instance = typeid(*this) == type;
		if (!instance)
			return false;
	}

	// Java: name == null || !name.matches(nameRegex). Deviation: a Java null name is the empty string, and the pattern is an ECMAScript
	// std::regex matched against the whole name (java.util.regex syntax differs in details)
	if (nameRegex && (name.get().empty() || !std::regex_match(name.get(), std::regex(std::string(*nameRegex)))))
		return false;

	return true;
}

std::string Spatial::toString() {
	return name.get() + " (" + utils::simpleClassName(typeid(*this)) + ") use " + collision::collisionIntentionToString(getCollisionIntentions());
}

runtime::Ref<Spatial> Spatial::clone() {
	// Java: (Spatial) super.clone(); every concrete scene class (Node, DespawnableNode, Geometry) overrides clone
	throw CloneNotSupportedException(utils::simpleClassName(typeid(*this)));
}

} // namespace aion::gameserver::geoEngine::scene
