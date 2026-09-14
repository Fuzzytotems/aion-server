#include "aion/gameserver/services/instance/InstanceScaler.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::services::instance {

runtime::HashMap<runtime::Ref<world::WorldMapInstance>, runtime::Ref<InstanceScaler::Scaling>> InstanceScaler::scalings{
	AION_LOCK_CLASS(InstanceScaler::scalings)};

InstanceScaler::InstanceScaler() = default;

InstanceScaler::~InstanceScaler() = default;

InstanceScaler& InstanceScaler::getInstance() {
	static InstanceScaler instance; // Java: INSTANCE
	return instance;
}

runtime::Ref<InstanceScaler::Scaling> InstanceScaler::Scaling::create() {
	return runtime::makeRef<Scaling>();
}

bool InstanceScaler::Scaling::update(world::WorldMapInstance& instance) {
	AION_UNPORTED();
}

bool InstanceScaler::Scaling::isLowLevelInstanceWithHighLevelPlayers(world::WorldMapInstance& instance, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players) {
	AION_UNPORTED();
}

int32_t InstanceScaler::Scaling::getInstanceEnterMinLevel(world::WorldMapInstance& instance, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players) {
	AION_UNPORTED();
}

std::vector<runtime::Ref<InstanceScaler::InstanceScalerStatFunction>> InstanceScaler::Scaling::createStatFunctions(world::WorldMapInstance& instance, int32_t value) {
	AION_UNPORTED();
}

InstanceScaler::Scaling::~Scaling() = default;

InstanceScaler::InstanceScalerStatFunction::InstanceScalerStatFunction(model::stats::container::StatEnum statValue, float rateValue) : rate(rateValue) {
	this->stat = statValue;
}

runtime::Ref<InstanceScaler::InstanceScalerStatFunction> InstanceScaler::InstanceScalerStatFunction::create(model::stats::container::StatEnum statValue, float rateValue) {
	return runtime::makeRef<InstanceScaler::InstanceScalerStatFunction>(statValue, rateValue);
}

void InstanceScaler::InstanceScalerStatFunction::apply(model::stats::calc::Stat2& statValue, const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

int32_t InstanceScaler::InstanceScalerStatFunction::getPriority() {
	AION_UNPORTED();
}

InstanceScaler::InstanceScalerStatFunction::~InstanceScalerStatFunction() = default;

void InstanceScaler::onEnterInstance(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void InstanceScaler::onBeforeSpawn(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void InstanceScaler::rescale(world::WorldMapInstance& instance, InstanceScaler::Scaling& scaling) {
	AION_UNPORTED();
}

bool InstanceScaler::canScale(world::WorldMapInstance& instance) {
	AION_UNPORTED();
}

bool InstanceScaler::shouldScale(model::gameobjects::Npc& npc, world::WorldMapInstance& instance) {
	AION_UNPORTED();
}

void InstanceScaler::scaleNpc(model::gameobjects::Npc& npc, InstanceScaler::Scaling& scaling) {
	AION_UNPORTED();
}

float InstanceScaler::calculateMultiplier(world::WorldMapInstance& instance, float floor, float scaleFactor, int32_t playerCount) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::instance
