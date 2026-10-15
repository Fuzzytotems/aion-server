#include "aion/gameserver/geoEngine/collision/IgnoreProperties.h"

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"

namespace aion::gameserver::geoEngine::collision {

// Java: public static final IgnoreProperties ELYOS = new IgnoreProperties(Race.ELYOS, 0) (and the other constants)
const runtime::Ref<IgnoreProperties>& IgnoreProperties::ELYOS = *new runtime::Ref<IgnoreProperties>(
	runtime::makeRef<IgnoreProperties>(model::Race::ELYOS, 0));
const runtime::Ref<IgnoreProperties>& IgnoreProperties::ASMODIANS = *new runtime::Ref<IgnoreProperties>(
	runtime::makeRef<IgnoreProperties>(model::Race::ASMODIANS, 0));
const runtime::Ref<IgnoreProperties>& IgnoreProperties::BALAUR = *new runtime::Ref<IgnoreProperties>(
	runtime::makeRef<IgnoreProperties>(model::Race::DRAKAN, 0));
const runtime::Ref<IgnoreProperties>& IgnoreProperties::ANY_RACE = *new runtime::Ref<IgnoreProperties>(
	runtime::makeRef<IgnoreProperties>(std::nullopt, 0));

IgnoreProperties::IgnoreProperties(std::optional<model::Race> raceValue, int32_t staticIdValue) : race(raceValue), staticId(staticIdValue) {
}

IgnoreProperties::~IgnoreProperties() = default;

runtime::Ref<IgnoreProperties> IgnoreProperties::of(std::optional<model::Race> raceValue, int32_t staticIdValue) {
	if (staticIdValue == 0) {
		if (raceValue == model::Race::ELYOS)
			return ELYOS;
		if (raceValue == model::Race::ASMODIANS)
			return ASMODIANS;
		if (raceValue == model::Race::DRAKAN)
			return BALAUR;
	}
	return runtime::makeRef<IgnoreProperties>(raceValue, staticIdValue);
}

runtime::Ref<IgnoreProperties> IgnoreProperties::of(model::Race raceValue) {
	return of(std::optional<model::Race>(raceValue), 0);
}

runtime::Ref<IgnoreProperties> IgnoreProperties::of(int32_t staticIdValue) {
	return of(std::optional<model::Race>(), staticIdValue);
}

std::string IgnoreProperties::toString() const {
	return "[IgnoreProperties] Race: " + (race ? std::string(xml::enumName(*race)) : std::string("null")) + " staticId: " + std::to_string(staticId);
}

} // namespace aion::gameserver::geoEngine::collision
