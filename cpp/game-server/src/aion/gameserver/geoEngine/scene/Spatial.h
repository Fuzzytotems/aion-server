#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <typeinfo>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/geoEngine/bounding/fwd.h"
#include "aion/gameserver/geoEngine/collision/Collidable.h"
#include "aion/gameserver/geoEngine/math/fwd.h"
#include "aion/gameserver/geoEngine/scene/Spatial_CullHint.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"

namespace aion::gameserver::geoEngine::scene {

/**
 * <code>Spatial</code> defines the base class for scene graph nodes. It maintains a link to a parent, it's local transforms and the world's
 * transforms. All other nodes, such as <code>Node</code> and <code>Geometry</code> are subclasses of <code>Spatial</code>.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5): the scene graph base of GeoMap. RefCounted (fieldmap K4). C++ notes:
 * - `name` may be null in Java (`Node()`, `GeoMap(mapId)`); the C++ Field holds the empty string instead, setName takes std::optional (Java
 *   ignores null).
 * - Cloneable: clone() returns `Ref<Spatial>` (Node/DespawnableNode override it; Java's covariant return types are named in comments, §8.2).
 * - matches(Class, String): the class filter is a `const std::type_info*` (null = any class), the regex an optional string.
 *
 * @author Mark Powell
 * @author Joshua Slack
 * @author Rolandas - added materials
 */
class Spatial : public runtime::RefCounted, public collision::Collidable {
	AION_MAKE_REF_FRIEND
public:
	using CullHint = Spatial_CullHint;

protected:
	runtime::Field<runtime::Ref<bounding::BoundingVolume>> worldBound{};
	runtime::Field<std::string> name{};
	runtime::Field<runtime::Ref<Node>> parent{};

	/**
	 * Do not use this constructor. Serialization purposes only.
	 */
	Spatial();

	/**
	 * Constructor instantiates a new <code>Spatial</code> object setting the rotation, translation and scale value to defaults.
	 *
	 * @param name the name of the scene element. This is required for identification and comparison purposes. C++: a Java null name is "" (geo
	 *             nodes always carry a name; the `name != null` tests become `!name.empty()`).
	 */
	explicit Spatial(std::string_view name);

	~Spatial() override;

public:
	/**
	 * Sets the name of this spatial (Java ignores null).
	 */
	void setName(std::optional<std::string_view> name);

	/**
	 * Returns the name of this spatial (the empty string for Java's null).
	 */
	std::string getName() const { return name.get(); }

	/**
	 * <code>getParent</code> retrieve's this node's parent. If the parent is null this is the root node.
	 */
	runtime::Ptr<Node> getParent() const { return parent.get(); }

protected:
	/**
	 * Called by Node#attachChild(Spatial) and Node#detachChild(Spatial) - don't call directly. <code>setParent</code> sets the parent of this
	 * node.
	 */
	void setParent(runtime::Ptr<Node> parent);

public:
	/**
	 * <code>removeFromParent</code> removes this Spatial from it's parent.
	 *
	 * @return true if it has a parent and performed the remove.
	 */
	bool removeFromParent();

	/**
	 * determines if the provided Node is the parent, or parent's parent, etc. of this Spatial.
	 */
	bool hasAncestor(Node& ancestor);

	/**
	 * <code>updateModelBound</code> recalculates the bounding object for this Spatial.
	 */
	virtual void updateModelBound() = 0;

	/**
	 * <code>setModelBound</code> sets the bounding object for this Spatial (null in Node's recursion when no bound is given).
	 */
	virtual void setModelBound(runtime::Ptr<bounding::BoundingVolume> modelBound) = 0;

	virtual int32_t getVertexCount() = 0;

	virtual int32_t getTriangleCount() = 0;

	virtual void setCollisionIntentions(int8_t collisionIntentions) = 0;

	virtual void setMaterialId(int8_t materialId) = 0;

	virtual int8_t getCollisionIntentions() = 0;

	virtual int32_t getMaterialId() = 0;

	/**
	 * Note that we are <i>matching</i> the pattern, therefore the pattern must match the entire pattern (i.e. it behaves as if it is sandwiched
	 * between "^" and "$").
	 *
	 * @param spatialSubclass the class the spatial must be an instance of, null for any (Java: Class<? extends Spatial>)
	 * @param nameRegex the name pattern, nullopt for any
	 */
	bool matches(const std::type_info* spatialSubclass, std::optional<std::string_view> nameRegex);

	/**
	 * <code>getWorldBound</code> retrieves the world bound at this node level.
	 */
	runtime::Ptr<bounding::BoundingVolume> getWorldBound() const { return worldBound.get(); }

	/**
	 * Returns the Spatial's name followed by the class of the spatial
	 */
	std::string toString();

	virtual void setTransform(const math::Matrix3f& rotation, const math::Vector3f& loc, const math::Vector3f& scale) = 0;

	/** Java: Spatial clone() throws CloneNotSupportedException */
	virtual runtime::Ref<Spatial> clone();
};

} // namespace aion::gameserver::geoEngine::scene
