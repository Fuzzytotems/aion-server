#include "aion/gameserver/geoEngine/scene/DespawnableNode.h"

#include <optional>
#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/geoEngine/GeoCallbacks.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/collision/IgnoreProperties.h"
#include "aion/gameserver/geoEngine/scene/CloneNotSupportedException.h"
#include "aion/gameserver/geoEngine/scene/Geometry.h"
#include "aion/gameserver/model/RaceInfo.h"

namespace aion::gameserver::geoEngine::scene {

namespace {

/** Java: SiegeRace.getRaceId() (the SiegeRace companion does not exist yet): ELYOS and ASMODIANS use their Race ids, BALAUR is 2 */
int32_t getSiegeRaceId(model::siege::SiegeRace race) {
	switch (race) {
		case model::siege::SiegeRace::ELYOS:
			return model::getRaceId(model::Race::ELYOS);
		case model::siege::SiegeRace::ASMODIANS:
			return model::getRaceId(model::Race::ASMODIANS);
		case model::siege::SiegeRace::BALAUR:
			break;
	}
	return 2;
}

} // namespace

DespawnableNode::DespawnableNode() = default;

DespawnableNode::~DespawnableNode() = default;

runtime::Ref<DespawnableNode> DespawnableNode::create() {
	return runtime::makeRef<DespawnableNode>();
}

void DespawnableNode::setActive(int32_t instanceId, bool active) {
	SYNCHRONIZED(instances) {
		if (instanceId < 0) // Java: BitSet.set
			throw runtime::IndexOutOfBoundsException("bitIndex < 0: " + std::to_string(instanceId));
		if (active)
			instances.add(instanceId);
		else
			instances.remove(instanceId);
	}
}

bool DespawnableNode::isActive(int32_t instanceId) {
	bool active = false;
	SYNCHRONIZED(instances) {
		if (instanceId < 0) // Java: BitSet.get
			throw runtime::IndexOutOfBoundsException("bitIndex < 0: " + std::to_string(instanceId));
		active = instances.contains(instanceId);
	}
	return active;
}

void DespawnableNode::copyFrom(Node& node) {
	name.set(node.name.get());
	collisionIntentions.set(node.collisionIntentions.get());
	materialId.set(node.materialId.get());
	for (runtime::Ptr<Spatial> spatial : *node.getChildren()) {
		if (runtime::Ptr<Geometry> geometry = runtime::as<Geometry>(spatial)) {
			runtime::Ref<Geometry> geom = Geometry::create(spatial->getName(), geometry->getMesh());
			attachChild(geom);
		} else if (runtime::Ptr<Node> child = runtime::as<Node>(spatial)) {
			attachChild(child->clone());
		} else {
			throw CloneNotSupportedException();
		}
	}
}

int32_t DespawnableNode::collideWith(math::Ray& other, collision::CollisionResults& results) {
	DespawnableType nodeType = type.get();
	if (nodeType == DespawnableType::EVENT) {
		if (GeoCallbacks::getEventThemeId() != id.get())
			return 0;
	} else if (nodeType == DespawnableType::SHIELD) {
		runtime::Ptr<collision::IgnoreProperties> ignoreProperties = results.getIgnoreProperties();
		if (ignoreProperties == collision::IgnoreProperties::ANY_RACE)
			return 0;
		std::optional<GeoCallbacks::SiegeShieldState> loc = GeoCallbacks::getSiegeShieldState(id.get());
		if (loc) {
			if (!loc->underShield)
				return 0;
			if (ignoreProperties != nullptr) {
				if (loc->race != model::siege::SiegeRace::BALAUR) {
					std::optional<model::Race> race = ignoreProperties->getRace();
					if (!race) // Java: ignoreProperties.getRace().getRaceId() with a null race
						throw runtime::NullPointerException("IgnoreProperties.getRace() is null");
					if (model::getRaceId(*race) == getSiegeRaceId(loc->race))
						return 0;
				}
				if (loc->race == model::siege::SiegeRace::BALAUR && ignoreProperties->getRace() == collision::IgnoreProperties::BALAUR->getRace())
					return 0;
			}
		}
	} else if (nodeType != DespawnableType::HOUSE && !isActive(results.getInstanceId())) {
		return 0;
	} else if (results.getIgnoreProperties() != nullptr) {
		if (results.getIgnoreProperties()->getStaticId() > 0 && results.getIgnoreProperties()->getStaticId() == id.get()) {
			return 0;
		}
	}
	return Node::collideWith(other, results);
}

runtime::Ref<Spatial> DespawnableNode::clone() {
	runtime::Ref<DespawnableNode> node = DespawnableNode::create();
	node->type.set(type.get());
	node->id.set(id.get());
	node->levelBitMask.set(levelBitMask.get());
	node->instances.addAll(instances.snapshot()); // Java: node.instances.or(instances)
	node->name.set(name.get());
	node->collisionIntentions.set(collisionIntentions.get());
	node->materialId.set(materialId.get());
	for (runtime::Ptr<Spatial> spatial : *getChildren()) {
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

} // namespace aion::gameserver::geoEngine::scene
