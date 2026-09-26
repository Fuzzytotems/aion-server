#include "aion/gameserver/geoEngine/scene/Node.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/geoEngine/bounding/BoundingVolume.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntentionInfo.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/math/Ray.h"
#include "aion/gameserver/geoEngine/scene/CloneNotSupportedException.h"
#include "aion/gameserver/geoEngine/scene/Geometry.h"
#include "aion/gameserver/geoEngine/scene/Mesh.h"

namespace aion::gameserver::geoEngine::scene {

static const auto logger = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.geoEngine.scene.Node");

Node::Node() : children(runtime::RcArrayList<runtime::Ref<Spatial>>::create(AION_LOCK_CLASS(Node::children))) {
}

Node::Node(std::optional<std::string_view> nameValue)
	: Spatial(nameValue.value_or(std::string_view())),
	  children(runtime::RcArrayList<runtime::Ref<Spatial>>::create(AION_LOCK_CLASS(Node::children))) {
	collisionIntentions.set(collision::getId(collision::CollisionIntention::ALL));
}

Node::~Node() = default;

runtime::Ref<Node> Node::create() {
	return runtime::makeRef<Node>();
}

runtime::Ref<Node> Node::create(std::optional<std::string_view> nameValue) {
	return runtime::makeRef<Node>(nameValue);
}

int32_t Node::getQuantity() {
	return children->size();
}

int32_t Node::getTriangleCount() {
	int32_t total = 0;
	if (children.get() != nullptr) {
		for (runtime::Ptr<Spatial> child : *children.get()) {
			total += child->getTriangleCount();
		}
	}

	return total;
}

int32_t Node::getVertexCount() {
	int32_t total = 0;
	if (children.get() != nullptr) {
		for (runtime::Ptr<Spatial> child : *children.get()) {
			total += child->getVertexCount();
		}
	}

	return total;
}

int32_t Node::attachChild(runtime::Ptr<Spatial> child) {
	if (child == nullptr)
		throw runtime::NullPointerException("");

	if (child->getParent().get() != this && child.get() != this) {
		if (child->getParent() != nullptr) {
			child->getParent()->detachChild(child);
		}
		child->setParent(runtime::Ptr<Node>(*this));
		children->add(runtime::Ref<Spatial>(child));
	}

	return children->size();
}

int32_t Node::attachChildAt(runtime::Ptr<Spatial> child, int32_t index) {
	if (child == nullptr)
		throw runtime::NullPointerException("");

	if (child->getParent().get() != this && child.get() != this) {
		if (child->getParent() != nullptr) {
			child->getParent()->detachChild(child);
		}
		child->setParent(runtime::Ptr<Node>(*this));
		children->add(index, runtime::Ref<Spatial>(child));
	}

	return children->size();
}

int32_t Node::detachChild(runtime::Ptr<Spatial> child) {
	if (child == nullptr)
		throw runtime::NullPointerException("");

	if (child->getParent().get() == this) {
		int32_t index = children->indexOf(child);
		if (index != -1) {
			detachChildAt(index);
		}
		return index;
	}

	return -1;
}

int32_t Node::detachChildNamed(std::optional<std::string_view> childName) {
	if (!childName)
		throw runtime::NullPointerException("");

	for (int32_t x = 0, max = children->size(); x < max; x++) {
		runtime::Ptr<Spatial> child = children->get(x);
		if (*childName == child->getName()) {
			detachChildAt(x);
			return x;
		}
	}
	return -1;
}

runtime::Ptr<Spatial> Node::detachChildAt(int32_t index) {
	runtime::Ptr<Spatial> child = children->removeAt(index);
	if (child != nullptr) {
		child->setParent(nullptr);
	}
	return child;
}

void Node::detachAllChildren() {
	for (int32_t i = children->size() - 1; i >= 0; i--) {
		detachChildAt(i);
	}
	logger.info("All children removed.");
}

int32_t Node::getChildIndex(Spatial& sp) {
	return children->indexOf(runtime::Ptr<Spatial>(sp));
}

void Node::swapChildren(int32_t index1, int32_t index2) {
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<Spatial>>> list = children.get();
	runtime::Ref<Spatial> c2(list->get(index2));
	runtime::Ref<Spatial> c1(list->removeAt(index1));
	list->add(index1, c2);
	list->removeAt(index2);
	list->add(index2, c1);
}

runtime::Ptr<Spatial> Node::getChild(int32_t i) {
	return children->get(i);
}

runtime::Ptr<Spatial> Node::getChild(std::optional<std::string_view> value) {
	if (!value)
		return nullptr;

	for (runtime::Ptr<Spatial> child : *children.get()) {
		if (*value == child->getName()) {
			return child;
		} else if (runtime::Ptr<Node> node = runtime::as<Node>(child)) {
			runtime::Ptr<Spatial> out = node->getChild(value);
			if (out != nullptr) {
				return out;
			}
		}
	}
	return nullptr;
}

bool Node::hasChild(Spatial& spat) {
	if (children->contains(runtime::Ptr<Spatial>(spat)))
		return true;

	for (runtime::Ptr<Spatial> child : *children.get()) {
		runtime::Ptr<Node> node = runtime::as<Node>(child);
		if (node != nullptr && node->hasChild(spat))
			return true;
	}

	return false;
}

void Node::childChange(Geometry& geometry, int32_t index1, int32_t index2) {
	// just pass to parent
	runtime::Ptr<Node> p = getParent();
	if (p != nullptr) {
		p->childChange(geometry, index1, index2);
	}
}

int32_t Node::collideWith(math::Ray& other, collision::CollisionResults& results) {
	if ((getCollisionIntentions() & results.getIntentions()) == 0) {
		return 0;
	}

	runtime::Ptr<bounding::BoundingVolume> bound = worldBound.get();
	if (bound == nullptr || !bound->intersects(static_cast<const math::Ray&>(other))) {
		return 0;
	}

	int32_t total = 0;
	for (runtime::Ptr<Spatial> child : *children.get()) {
		if (runtime::as<Geometry>(child) != nullptr) {
			// not used materialIds do not have collision intention for materials set
			if ((child->getCollisionIntentions() & results.getIntentions()) == 0) {
				continue;
			}
		}
		total += child->collideWith(other, results);
		if (total > 0 && results.isOnlyFirst())
			break;
	}
	return total;
}

std::vector<runtime::Ptr<Spatial>> Node::descendantMatches(std::optional<std::string_view> nameRegex) {
	std::vector<runtime::Ptr<Spatial>> newList;
	if (getQuantity() < 1)
		return newList;
	for (runtime::Ptr<Spatial> child : *children.get()) {
		if (child->matches(nullptr, nameRegex))
			newList.push_back(child);
		if (runtime::Ptr<Node> node = runtime::as<Node>(child)) {
			std::vector<runtime::Ptr<Spatial>> descendants = node->descendantMatches(nameRegex);
			newList.insert(newList.end(), descendants.begin(), descendants.end());
		}
	}
	return newList;
}

void Node::setModelBound(runtime::Ptr<bounding::BoundingVolume> modelBound) {
	if (children.get() != nullptr) {
		for (runtime::Ptr<Spatial> child : *children.get()) {
			if (modelBound != nullptr)
				child->setModelBound(modelBound->clone(nullptr));
			else
				child->setModelBound(nullptr);
		}
	}
}

void Node::updateModelBound() {
	runtime::Ref<bounding::BoundingVolume> resultBound;
	if (children.get() != nullptr) {
		for (runtime::Ptr<Spatial> child : *children.get()) {
			child->updateModelBound();
			if (resultBound != nullptr) {
				// merge current world bound with child world bound
				resultBound->mergeLocal(child->getWorldBound());
			} else {
				// set world bound to first non-null child world bound
				if (child->getWorldBound() != nullptr) {
					resultBound = child->getWorldBound()->clone(worldBound.get());
				}
			}
		}
	}
	worldBound.set(std::move(resultBound));
}

void Node::setTransform(const math::Matrix3f& rotation, const math::Vector3f& loc, const math::Vector3f& scale) {
	if (children.get() != nullptr) {
		for (runtime::Ptr<Spatial> child : *children.get()) {
			child->setTransform(rotation, loc, scale);
		}
	}
}

runtime::Ref<Spatial> Node::clone() {
	runtime::Ref<Node> node = Node::create(std::string_view(name.get()));
	node->collisionIntentions.set(collisionIntentions.get());
	node->materialId.set(materialId.get());
	for (runtime::Ptr<Spatial> spatial : *children.get()) {
		if (runtime::Ptr<Geometry> geometry = runtime::as<Geometry>(spatial)) {
			runtime::Ref<Geometry> geom = Geometry::create(spatial->getName(), geometry->getMesh());
			node->attachChild(geom);
		} else if (runtime::Ptr<Node> child = runtime::as<Node>(spatial)) {
			node->attachChild(child->clone());
		} else {
			throw CloneNotSupportedException();
		}
	}
	return node;
}

int32_t Node::getMaterialId() {
	return materialId.get() & 0xFF;
}

} // namespace aion::gameserver::geoEngine::scene
