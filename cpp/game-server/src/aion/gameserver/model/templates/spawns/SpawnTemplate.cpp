#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

#include <utility>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"

namespace aion::gameserver::model::templates::spawns {

SpawnTemplate::SpawnTemplate(SpawnGroup& spawnGroupValue, const SpawnSpotTemplate* spot)
	: OwnedPart(spawnGroupValue), randomWalk(0), walkerIdx(), anchor(), spawnGroup(spawnGroupValue), aiName(), state(0), aerialSpawn(false),
	  creatorId(0), temporarySpawn(nullptr) {
	// Java: x, y, z, h, staticId, randomWalk, walkerId, anchor, walkerIdx, aiName, state, aerialSpawn, temporarySpawn from the spot
	static_cast<void>(spot);
	AION_UNPORTED();
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
	AION_UNPORTED();
}

int32_t SpawnTemplate::getWorldId() {
	return spawnGroup.getWorldId();
}

runtime::Ptr<SpawnTemplate> SpawnTemplate::changeTemplate(int32_t instanceId) {
	AION_UNPORTED();
}

int32_t SpawnTemplate::getRespawnTime() {
	AION_UNPORTED();
}

void SpawnTemplate::resetPoolSpot(int32_t instanceId) {
	AION_UNPORTED();
}

const TemporarySpawn* SpawnTemplate::getTemporarySpawn() {
	AION_UNPORTED();
}

std::optional<spawnengine::SpawnHandlerType> SpawnTemplate::getHandlerType() {
	AION_UNPORTED();
}

bool SpawnTemplate::isNoRespawn() {
	AION_UNPORTED();
}

bool SpawnTemplate::hasPool() {
	AION_UNPORTED();
}

bool SpawnTemplate::isTemporarySpawn() {
	AION_UNPORTED();
}

bool SpawnTemplate::isEventSpawn() {
	AION_UNPORTED();
}

const event::EventTemplate* SpawnTemplate::getEventTemplate() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::templates::spawns
