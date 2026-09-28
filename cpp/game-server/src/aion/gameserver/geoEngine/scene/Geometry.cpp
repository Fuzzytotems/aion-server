#include "aion/gameserver/geoEngine/scene/Geometry.h"

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/geoEngine/bounding/BoundingVolume.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/math/Matrix3f.h"
#include "aion/gameserver/geoEngine/math/Ray.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/scene/Mesh.h"
#include "aion/gameserver/geoEngine/scene/Node.h"

namespace aion::gameserver::geoEngine::scene {

Geometry::Geometry() = default;

Geometry::Geometry(std::string_view nameValue, runtime::Ptr<Mesh> meshValue) : Spatial(nameValue) {
	if (meshValue == nullptr)
		throw runtime::NullPointerException("");

	mesh.set(meshValue);
}

Geometry::~Geometry() = default;

runtime::Ref<Geometry> Geometry::create(std::string_view nameValue, runtime::Ptr<Mesh> meshValue) {
	return runtime::makeRef<Geometry>(nameValue, meshValue);
}

int32_t Geometry::getVertexCount() {
	return mesh->getVertexCount();
}

int32_t Geometry::getTriangleCount() {
	return mesh->getTriangleCount();
}

void Geometry::setMesh(runtime::Ptr<Mesh> value) {
	mesh.set(value);
}

runtime::Ptr<bounding::BoundingVolume> Geometry::getModelBound() {
	return mesh->getBound();
}

void Geometry::updateModelBound() {
	mesh->updateBound();
	worldBound.set(getModelBound()->transform(cachedWorldMat.get(), worldBound.get()));
}

void Geometry::setModelBound(runtime::Ptr<bounding::BoundingVolume> modelBound) {
	mesh->setBound(modelBound);
}

int32_t Geometry::collideWith(math::Ray& other, collision::CollisionResults& results) {
	runtime::Ptr<bounding::BoundingVolume> bound = worldBound.get();
	if (!bound->intersects(static_cast<const math::Ray&>(other)))
		return 0;
	// NOTE: BIHTree in mesh already checks collision with the mesh's bound
	int32_t prevSize = results.size();
	int32_t added = mesh->collideWith(other, cachedWorldMat.get(), *bound, results);
	int32_t newSize = results.size();
	for (int32_t i = prevSize; i < newSize; i++)
		results.setGeometryDirect(i, runtime::Ptr<Geometry>(*this)); // Java: results.getCollisionDirect(i).setGeometry(this)
	return added;
}

void Geometry::setTransform(const math::Matrix3f& rotation, const math::Vector3f& loc, const math::Vector3f& scale) {
	math::Matrix4f matrix = cachedWorldMat.get();
	matrix.loadIdentity();
	matrix.setRotationMatrix(rotation);
	matrix.scale(scale);
	matrix.setTranslation(loc);
	cachedWorldMat.set(matrix);
}

int8_t Geometry::getCollisionIntentions() {
	return mesh->getCollisionIntentions();
}

void Geometry::setCollisionIntentions(int8_t value) {
	mesh->setCollisionIntentions(value);
}

int32_t Geometry::getMaterialId() {
	return mesh->getMaterialId();
}

void Geometry::setMaterialId(int8_t value) {
	mesh->setMaterialId(value);
}

runtime::Ref<Spatial> Geometry::clone() {
	// Java: (Geometry) super.clone() copies every field (name, parent, worldBound, mesh), then the copy gets its own matrix
	runtime::Ref<Geometry> geometry = runtime::makeRef<Geometry>();
	geometry->name.set(name.get());
	geometry->parent.set(parent.get());
	geometry->worldBound.set(worldBound.get());
	geometry->mesh.set(mesh.get());
	geometry->cachedWorldMat.set(cachedWorldMat.get());
	return geometry;
}

} // namespace aion::gameserver::geoEngine::scene
