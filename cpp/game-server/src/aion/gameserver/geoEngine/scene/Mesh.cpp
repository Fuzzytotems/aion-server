#include "aion/gameserver/geoEngine/scene/Mesh.h"

#include <cstddef>

#include "aion/gameserver/geoEngine/bounding/BoundingBox.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/collision/bih/BIHTree.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/scene/CollisionData.h"
#include "aion/gameserver/geoEngine/scene/mesh/IndexArray.h"

namespace aion::gameserver::geoEngine::scene {

Mesh::Mesh() : meshBound(bounding::BoundingBox::create()) { // Java field initializer: meshBound = new BoundingBox()
}

Mesh::~Mesh() = default;

runtime::Ref<Mesh> Mesh::create() {
	return runtime::makeRef<Mesh>();
}

int32_t Mesh::getTriangleCount() {
	return indices->size() / 3;
}

int32_t Mesh::getVertexCount() {
	return vertices->length();
}

void Mesh::getTriangle(int32_t index, math::Vector3f& v1, math::Vector3f& v2, math::Vector3f& v3) {
	runtime::Ptr<mesh::IndexArray> indexArray = indices.get();
	runtime::Ptr<runtime::Array<float>> vertexArray = vertices.get();
	index *= 3;
	int32_t vertexIndex = indexArray->get(index++) * 3;
	v1.x = vertexArray->get(vertexIndex++);
	v1.y = vertexArray->get(vertexIndex++);
	v1.z = vertexArray->get(vertexIndex);
	vertexIndex = indexArray->get(index++) * 3;
	v2.x = vertexArray->get(vertexIndex++);
	v2.y = vertexArray->get(vertexIndex++);
	v2.z = vertexArray->get(vertexIndex);
	vertexIndex = indexArray->get(index) * 3;
	v3.x = vertexArray->get(vertexIndex++);
	v3.y = vertexArray->get(vertexIndex++);
	v3.z = vertexArray->get(vertexIndex);
}

void Mesh::swapTriangles(int32_t i1, int32_t i2) {
	indices->swap(i1, i2);
}

void Mesh::createCollisionData() {
	// java-race: lazy build without synchronization (two threads may build and swap triangles of the same mesh concurrently); GeoWorldLoader
	// builds the trees of every mesh of the loaded maps before the maps are used, so no map mesh reaches the lazy path
	if (collisionTree.get() != nullptr) {
		return;
	}
	runtime::Ref<collision::bih::BIHTree> tree = collision::bih::BIHTree::create(*this);
	tree->construct();
	collisionTree.set(std::move(tree));
}

int32_t Mesh::collideWith(math::Ray& other, const math::Matrix4f& worldMatrix, bounding::BoundingVolume& worldBound,
	collision::CollisionResults& results) {
	if (collisionTree.get() == nullptr) {
		createCollisionData();
	}

	return collisionTree->collideWith(other, worldMatrix, worldBound, results);
}

void Mesh::setVertices(std::span<const float> value) {
	collisionTree.set(nullptr);
	runtime::Ref<runtime::Array<float>> array = runtime::Array<float>::make(static_cast<int32_t>(value.size()));
	for (size_t i = 0; i < value.size(); ++i)
		(*array)[static_cast<int32_t>(i)].set(value[i]);
	vertices.set(std::move(array));
}

void Mesh::setIndices(std::span<const int8_t> value) {
	collisionTree.set(nullptr);
	indices.set(mesh::IndexArray::from(value));
}

void Mesh::setIndices(std::span<const int16_t> value) {
	collisionTree.set(nullptr);
	indices.set(mesh::IndexArray::from(value));
}

void Mesh::updateBound() {
	runtime::Ptr<runtime::Array<float>> vertexArray = vertices.get();
	std::vector<float> points = vertexArray->snapshot(); // Java: FloatBuffer.wrap(vertices)
	meshBound->computeFromPoints(points);
}

void Mesh::setBound(runtime::Ptr<bounding::BoundingVolume> modelBound) {
	meshBound.set(modelBound);
}

} // namespace aion::gameserver::geoEngine::scene
