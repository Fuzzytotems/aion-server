#include "aion/gameserver/geoEngine/scene/Node.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/geoEngine/bounding/BoundingVolume.h"

namespace aion::gameserver::geoEngine::scene {

static const auto logger = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.geoEngine.scene.Node");

Node::Node() : children(runtime::RcArrayList<runtime::Ref<Spatial>>::create(AION_LOCK_CLASS(Node::children))) {
}

Node::Node(std::optional<std::string_view> nameValue)
	: Spatial(nameValue.value_or(std::string_view())),
	  children(runtime::RcArrayList<runtime::Ref<Spatial>>::create(AION_LOCK_CLASS(Node::children))) {
	// Java: collisionIntentions = CollisionIntention.ALL.getId() (the enum companion does not exist yet)
	AION_UNPORTED();
}

Node::~Node() = default;

runtime::Ref<Node> Node::create() {
	return runtime::makeRef<Node>();
}

runtime::Ref<Node> Node::create(std::optional<std::string_view> nameValue) {
	return runtime::makeRef<Node>(nameValue);
}

int32_t Node::getQuantity() {
	AION_UNPORTED();
}

int32_t Node::getTriangleCount() {
	AION_UNPORTED();
}

int32_t Node::getVertexCount() {
	AION_UNPORTED();
}

int32_t Node::attachChild(runtime::Ptr<Spatial> child) {
	AION_UNPORTED();
}

int32_t Node::attachChildAt(runtime::Ptr<Spatial> child, int32_t index) {
	AION_UNPORTED();
}

int32_t Node::detachChild(runtime::Ptr<Spatial> child) {
	AION_UNPORTED();
}

int32_t Node::detachChildNamed(std::optional<std::string_view> childName) {
	AION_UNPORTED();
}

runtime::Ptr<Spatial> Node::detachChildAt(int32_t index) {
	AION_UNPORTED();
}

void Node::detachAllChildren() {
	AION_UNPORTED();
}

int32_t Node::getChildIndex(Spatial& sp) {
	AION_UNPORTED();
}

void Node::swapChildren(int32_t index1, int32_t index2) {
	AION_UNPORTED();
}

runtime::Ptr<Spatial> Node::getChild(int32_t i) {
	AION_UNPORTED();
}

runtime::Ptr<Spatial> Node::getChild(std::optional<std::string_view> value) {
	AION_UNPORTED();
}

bool Node::hasChild(Spatial& spat) {
	AION_UNPORTED();
}

void Node::childChange(Geometry& geometry, int32_t index1, int32_t index2) {
	AION_UNPORTED();
}

int32_t Node::collideWith(math::Ray& other, collision::CollisionResults& results) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<Spatial>> Node::descendantMatches(std::optional<std::string_view> nameRegex) {
	AION_UNPORTED();
}

void Node::setModelBound(runtime::Ptr<bounding::BoundingVolume> modelBound) {
	AION_UNPORTED();
}

void Node::updateModelBound() {
	AION_UNPORTED();
}

void Node::setTransform(const math::Matrix3f& rotation, const math::Vector3f& loc, const math::Vector3f& scale) {
	AION_UNPORTED();
}

runtime::Ref<Spatial> Node::clone() {
	AION_UNPORTED();
}

int32_t Node::getMaterialId() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::geoEngine::scene
