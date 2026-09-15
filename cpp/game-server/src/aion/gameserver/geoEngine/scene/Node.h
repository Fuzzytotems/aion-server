#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/geoEngine/bounding/fwd.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/geoEngine/math/fwd.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"

namespace aion::gameserver::geoEngine::scene {

/**
 * <code>Node</code> defines an internal node of a scene graph. The internal node maintains a collection of children and handles merging said
 * children into a single bound to allow for very fast culling of multiple nodes. Node allows for any number of children to be attached.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5): the base of GeoMap and DespawnableNode. RefCounted (fieldmap K4), created with
 * create(). C++ notes: the generic descendantMatches methods are member templates (§8.3; `Class<T>` is the template argument, the overload
 * without a class filter is the non-template one); getChildren() returns the live children list; `Node(String)` uses the CollisionIntention
 * companion (collision/CollisionIntentionInfo.h). The java.util.logging logger is the .cpp logger (same name, commons logging).
 *
 * @author Mark Powell
 * @author Gregg Patton
 * @author Joshua Slack
 */
class Node : public Spatial {
	AION_MAKE_REF_FRIEND
	// C++ only: Java package access (DespawnableNode.copyFrom reads the protected fields of another Node)
	friend class DespawnableNode;

protected:
	runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::Ref<Spatial>>>> children{};
	runtime::Field<int8_t> collisionIntentions{};
	runtime::Field<int8_t> materialId{};

	/**
	 * Do not use this constructor. Serialization purposes only.
	 */
	Node();

	/**
	 * Constructor instantiates a new <code>Node</code> with a default empty list for containing children.
	 *
	 * @param name the name of the scene element (Java: null for GeoMap)
	 */
	explicit Node(std::optional<std::string_view> name);

	~Node() override;

public:
	/** Java: new Node() (protected in Java; used by DespawnableNode) */
	static runtime::Ref<Node> create();

	/** Java: new Node(name) */
	static runtime::Ref<Node> create(std::optional<std::string_view> name);

	/**
	 * <code>getQuantity</code> returns the number of children this node maintains.
	 */
	int32_t getQuantity();

	/**
	 * <code>getTriangleCount</code> returns the number of triangles contained in all sub-branches of this node that contain geometry.
	 */
	int32_t getTriangleCount() override;

	/**
	 * <code>getVertexCount</code> returns the number of vertices contained in all sub-branches of this node that contain geometry.
	 */
	int32_t getVertexCount() override;

	/**
	 * <code>attachChild</code> attaches a child to this node. This node becomes the child's parent. The current number of children maintained
	 * is returned. If the child already had a parent it is detached from that former parent.
	 *
	 * @param child the child to attach to this node (Java throws NullPointerException for null)
	 */
	virtual int32_t attachChild(runtime::Ptr<Spatial> child);

	/**
	 * <code>attachChildAt</code> attaches a child to this node at an index.
	 */
	int32_t attachChildAt(runtime::Ptr<Spatial> child, int32_t index);

	/**
	 * <code>detachChild</code> removes a given child from the node's list. This child will no longe be maintained.
	 *
	 * @return the index the child was at. -1 if the child was not in the list.
	 */
	int32_t detachChild(runtime::Ptr<Spatial> child);

	/**
	 * <code>detachChild</code> removes a given child from the node's list. Only the first child with a matching name is removed.
	 */
	int32_t detachChildNamed(std::optional<std::string_view> childName);

	/**
	 * <code>detachChildAt</code> removes a child at a given index. That child is returned for saving purposes.
	 */
	runtime::Ptr<Spatial> detachChildAt(int32_t index);

	/**
	 * <code>detachAllChildren</code> removes all children attached to this node.
	 */
	void detachAllChildren();

	int32_t getChildIndex(Spatial& sp);

	/**
	 * More efficient than e.g detaching and attaching as no updates are needed.
	 */
	void swapChildren(int32_t index1, int32_t index2);

	/**
	 * <code>getChild</code> returns a child at a given index.
	 */
	runtime::Ptr<Spatial> getChild(int32_t i);

	/**
	 * <code>getChild</code> returns the first child found with exactly the given name (case sensitive.)
	 */
	runtime::Ptr<Spatial> getChild(std::optional<std::string_view> name);

	/**
	 * determines if the provided Spatial is contained in the children list of this node.
	 */
	bool hasChild(Spatial& spat);

	/**
	 * Returns all children to this node (Java: the live list).
	 */
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<Spatial>>> getChildren() const { return children.get(); }

	void childChange(Geometry& geometry, int32_t index1, int32_t index2);

	int32_t collideWith(math::Ray& other, collision::CollisionResults& results) override;

	/**
	 * Returns flat list of Spatials implementing the specified class AND with name matching the specified pattern (Java: <T extends Spatial>
	 * List<T> descendantMatches(Class<T> spatialSubclass, String nameRegex)).
	 */
	template <class T>
	std::vector<runtime::Ptr<T>> descendantMatches(std::optional<std::string_view> nameRegex) {
		std::vector<runtime::Ptr<T>> newList;
		if (getQuantity() < 1)
			return newList;
		for (runtime::Ptr<Spatial> child : *children.get()) {
			// Java: child.matches(spatialSubclass, nameRegex) with the class test done by the template argument
			runtime::Ptr<T> match = runtime::as<T>(child);
			if (match != nullptr && child->matches(nullptr, nameRegex))
				newList.push_back(match);
			if (runtime::Ptr<Node> node = runtime::as<Node>(child)) {
				std::vector<runtime::Ptr<T>> descendants = node->template descendantMatches<T>(nameRegex);
				newList.insert(newList.end(), descendants.begin(), descendants.end());
			}
		}
		return newList;
	}

	/**
	 * Convenience wrapper (Java: descendantMatches(Class<T> spatialSubclass)).
	 */
	template <class T>
	std::vector<runtime::Ptr<T>> descendantMatches() { return descendantMatches<T>(std::nullopt); }

	/**
	 * Convenience wrapper (Java: descendantMatches(String nameRegex), T = Spatial).
	 */
	std::vector<runtime::Ptr<Spatial>> descendantMatches(std::optional<std::string_view> nameRegex);

	void setModelBound(runtime::Ptr<bounding::BoundingVolume> modelBound) override;

	void updateModelBound() override;

	void setTransform(const math::Matrix3f& rotation, const math::Vector3f& loc, const math::Vector3f& scale) override;

	/** Java: Node clone() (covariant return) */
	runtime::Ref<Spatial> clone() override;

	int8_t getCollisionIntentions() override { return collisionIntentions.get(); }

	void setCollisionIntentions(int8_t value) override { collisionIntentions.set(value); }

	int32_t getMaterialId() override;

	void setMaterialId(int8_t value) override { materialId.set(value); }
};

} // namespace aion::gameserver::geoEngine::scene
