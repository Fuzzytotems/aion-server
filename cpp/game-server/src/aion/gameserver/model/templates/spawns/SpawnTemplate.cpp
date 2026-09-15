#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

#include <utility>

#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnSpotTemplate.h"

namespace aion::gameserver::model::templates::spawns {

SpawnTemplate::SpawnTemplate(SpawnGroup& spawnGroupValue, const SpawnSpotTemplate* spot)
	: OwnedPart(spawnGroupValue), x(spot->getX()), y(spot->getY()), z(spot->getZ()), h(spot->getHeading()), staticId(spot->getStaticId()),
	  randomWalk(spot->getRandomWalk()), walkerId(spot->getWalkerId()), walkerIdx(spot->getWalkerIndex()), anchor(spot->getAnchor()),
	  spawnGroup(spawnGroupValue), aiName(spot->getAi()), state(spot->getState()), aerialSpawn(spot->isAerialSpawn()), creatorId(0),
	  temporarySpawn(spot->getTemporarySpawn()) {
	// Java order: x, y, z, h, staticId, randomWalk, walkerId, anchor, walkerIdx, aiName, state, aerialSpawn, temporarySpawn (the C++ member order
	// differs only for the const members, which have no side effects); Java null strings are the empty strings of the spot
}

SpawnTemplate::SpawnTemplate(SpawnGroup& spawnGroupValue, float xValue, float yValue, float zValue, int8_t heading, int32_t randWalk,
	std::optional<std::string_view> walkerIdValue, int32_t staticIdValue)
	: SpawnTemplate(spawnGroupValue, xValue, yValue, zValue, heading, randWalk, walkerIdValue, staticIdValue, 0, std::nullopt) {
}

SpawnTemplate::SpawnTemplate(SpawnGroup& spawnGroupValue, float xValue, float yValue, float zValue, int8_t heading, int32_t randWalk,
	std::optional<std::string_view> walkerIdValue, int32_t staticIdValue, int32_t creatorIdValue, std::optional<std::string_view> aiNameValue)
	: OwnedPart(spawnGroupValue), x(xValue), y(yValue), z(zValue), h(heading), staticId(staticIdValue), randomWalk(randWalk),
	  walkerId(walkerIdValue ? std::string(*walkerIdValue) : std::string()), walkerIdx(), anchor(), spawnGroup(spawnGroupValue),
	  aiName(aiNameValue ? std::string(*aiNameValue) : std::string()), state(0), aerialSpawn(false), creatorId(creatorIdValue), temporarySpawn(nullptr) {
}

SpawnTemplate::~SpawnTemplate() = default;

runtime::Ref<SpawnTemplate> SpawnTemplate::create(SpawnGroup& spawnGroupValue, float xValue, float yValue, float zValue, int8_t heading,
	int32_t randWalk, std::optional<std::string_view> walkerIdValue, int32_t staticIdValue) {
	// Java: addTemplate() is the last statement of the constructor
	return runtime::Ref<SpawnTemplate>(
		addTemplate(std::make_unique<SpawnTemplate>(spawnGroupValue, xValue, yValue, zValue, heading, randWalk, walkerIdValue, staticIdValue)));
}

runtime::Ref<SpawnTemplate> SpawnTemplate::create(SpawnGroup& spawnGroupValue, float xValue, float yValue, float zValue, int8_t heading,
	int32_t randWalk, std::optional<std::string_view> walkerIdValue, int32_t staticIdValue, int32_t creatorIdValue,
	std::optional<std::string_view> aiNameValue) {
	// Java: addTemplate() is the last statement of the constructor
	return runtime::Ref<SpawnTemplate>(addTemplate(std::make_unique<SpawnTemplate>(spawnGroupValue, xValue, yValue, zValue, heading, randWalk,
		walkerIdValue, staticIdValue, creatorIdValue, aiNameValue)));
}

std::optional<std::string> SpawnTemplate::getWalkerId() {
	const std::string& id = walkerId.get();
	if (id.empty())
		return std::nullopt;
	return id;
}

void SpawnTemplate::setWalkerId(std::optional<std::string_view> value) {
	walkerId.set(value ? std::string(*value) : std::string());
}

std::optional<std::string> SpawnTemplate::getAiName() {
	if (aiName.empty())
		return std::nullopt;
	return aiName;
}

SpawnTemplate& SpawnTemplate::addTemplate(std::unique_ptr<SpawnTemplate> spawnTemplate) {
	SpawnGroup& group = spawnTemplate->spawnGroup;
	return group.addSpawnTemplate(std::move(spawnTemplate));
}

int32_t SpawnTemplate::getNpcId() {
	return spawnGroup.getNpcId();
}

int32_t SpawnTemplate::getWorldId() {
	return spawnGroup.getWorldId();
}

runtime::Ptr<SpawnTemplate> SpawnTemplate::changeTemplate(int32_t instanceId) {
	return spawnGroup.reserveRandomFreePoolSpot(instanceId);
}

int32_t SpawnTemplate::getRespawnTime() {
	return spawnGroup.getRespawnTime();
}

void SpawnTemplate::resetPoolSpot(int32_t instanceId) {
	spawnGroup.resetPoolSpot(instanceId, *this);
}

const TemporarySpawn* SpawnTemplate::getTemporarySpawn() {
	return temporarySpawn != nullptr ? temporarySpawn : spawnGroup.getTemporarySpawn();
}

std::optional<spawnengine::SpawnHandlerType> SpawnTemplate::getHandlerType() {
	return spawnGroup.getHandlerType();
}

bool SpawnTemplate::isNoRespawn() {
	return spawnGroup.getRespawnTime() == 0;
}

bool SpawnTemplate::hasPool() {
	return spawnGroup.hasPool();
}

bool SpawnTemplate::isTemporarySpawn() {
	return spawnGroup.isTemporarySpawn();
}

bool SpawnTemplate::isEventSpawn() {
	return getEventTemplate() != nullptr;
}

const event::EventTemplate* SpawnTemplate::getEventTemplate() {
	return spawnGroup.getEventTemplate();
}

} // namespace aion::gameserver::model::templates::spawns
