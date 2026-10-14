#include "aion/gameserver/services/BaseService.h"

#include <cstdint>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/BaseData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/base/Base.h"
#include "aion/gameserver/model/base/BaseException.h"
#include "aion/gameserver/model/base/BaseLocation.h"
#include "aion/gameserver/model/base/BaseOccupier.h"
#include "aion/gameserver/model/base/BaseOccupierInfo.h"
#include "aion/gameserver/model/base/BaseType.h"
#include "aion/gameserver/model/base/CasualBase.h"
#include "aion/gameserver/model/base/PanesterraArtifact.h"
#include "aion/gameserver/model/base/PanesterraBase.h"
#include "aion/gameserver/model/base/PanesterraBaseLocation.h"
#include "aion/gameserver/model/base/PanesterraFactionCamp.h"
#include "aion/gameserver/model/base/SiegeBase.h"
#include "aion/gameserver/model/base/SiegeBaseLocation.h"
#include "aion/gameserver/model/base/StainedBase.h"
#include "aion/gameserver/model/base/StainedBaseLocation.h"
#include "aion/gameserver/model/templates/base/BaseTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/panesterra/PanesterraService.h"

namespace aion::gameserver::services {

using model::base::BaseOccupier;
using model::base::BaseType;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.BaseService");

// Java BaseService.java:30-42
BaseService::BaseService() {
	log.info("Initializing bases...");
	for (const model::templates::base::BaseTemplate& template_ : dataholders::DataManager::BASE_DATA->getAllBaseTemplates()) {
		runtime::Ref<model::base::BaseLocation> loc;
		switch (template_.getType().value()) { // Java: the switch on a null type throws NullPointerException
			case BaseType::CASUAL:
				loc = model::base::BaseLocation::create(&template_);
				break;
			case BaseType::SIEGE:
				loc = model::base::SiegeBaseLocation::create(&template_);
				break;
			case BaseType::STAINED:
				loc = model::base::StainedBaseLocation::create(&template_);
				break;
			case BaseType::PANESTERRA:
			case BaseType::PANESTERRA_ARTIFACT:
			case BaseType::PANESTERRA_FACTION_CAMP:
				loc = model::base::PanesterraBaseLocation::create(&template_);
				break;
		}
		allBaseLocations.put(template_.getId(), loc);
	}
}

BaseService::~BaseService() = default;

BaseService& BaseService::getInstance() {
	static BaseService instance; // Java SingletonHolder
	return instance;
}

// Java BaseService.java:47-54
void BaseService::initBases() {
	for (const runtime::Ptr<model::base::BaseLocation>& loc : allBaseLocations.values()) {
		switch (loc->getType()) {
			case BaseType::CASUAL:
			case BaseType::STAINED:
			case BaseType::PANESTERRA:
			case BaseType::PANESTERRA_FACTION_CAMP:
				start(loc->getId());
				break;
			default:
				break;
		}
	}
}

// Java BaseService.java:58-72
void BaseService::start(int32_t id) {
	runtime::Ref<model::base::Base> base;
	SYNCHRONIZED(*this) {
		if (activeBases.containsKey(id))
			return;
		base = newBase(id);
		activeBases.put(id, base);
	}
	try {
		base->start();
	} catch (const model::base::BaseException& e) {
		log.error("Base could not be started! ID:" + std::to_string(id), e);
	} catch (const runtime::NullPointerException& e) {
		log.error("Base could not be started! ID:" + std::to_string(id), e);
	}
}

// Java BaseService.java:79-95
void BaseService::stop(int32_t id) {
	if (!isActive(id))
		return;
	runtime::Ptr<model::base::Base> base;
	SYNCHRONIZED(*this) {
		base = activeBases.remove(id);
	}
	if (!base || base->isStopped())
		return;
	try {
		base->stop();
	} catch (const model::base::BaseException& e) {
		log.error("Base could not be stopped! ID:" + std::to_string(id), e);
	}
}

// Java BaseService.java:100-110
runtime::Ref<model::base::Base> BaseService::newBase(int32_t id) {
	runtime::Ptr<model::base::BaseLocation> loc = allBaseLocations.get(id);
	switch (loc->getType()) {
		case BaseType::CASUAL:
			return model::base::CasualBase::create(*loc);
		case BaseType::STAINED:
			return model::base::StainedBase::create(*runtime::cast<model::base::StainedBaseLocation>(loc));
		case BaseType::SIEGE:
			return model::base::SiegeBase::create(*runtime::cast<model::base::SiegeBaseLocation>(loc));
		case BaseType::PANESTERRA:
			return model::base::PanesterraBase::create(*runtime::cast<model::base::PanesterraBaseLocation>(loc));
		case BaseType::PANESTERRA_ARTIFACT:
			return model::base::PanesterraArtifact::create(*runtime::cast<model::base::PanesterraBaseLocation>(loc));
		case BaseType::PANESTERRA_FACTION_CAMP:
			return model::base::PanesterraFactionCamp::create(*runtime::cast<model::base::PanesterraBaseLocation>(loc));
	}
	throw runtime::IllegalStateException("unknown base type"); // unreachable: the switch covers every BaseType
}

// Java BaseService.java:112-125
void BaseService::capture(int32_t id, BaseOccupier newOccupier) {
	if (!isActive(id))
		return;
	stop(id);
	runtime::Ptr<model::base::BaseLocation> loc = allBaseLocations.get(id);
	if (loc->getType() == BaseType::PANESTERRA_FACTION_CAMP) {
		const std::optional<panesterra::ahserion::PanesterraFaction> faction = getPanesterraFaction(loc->getOccupier());
		if (!faction) // Java: handleTeamElimination(null) - a camp's occupier always has a faction (PEACE or a temple)
			throw runtime::NullPointerException("PanesterraFaction of " + std::string(xml::enumName(loc->getOccupier())));
		panesterra::PanesterraService::getInstance().handleTeamElimination(*faction);
	}
	loc->setOccupier(newOccupier); // Java: if (newOccupier != null) - every C++ caller passes an occupier
	start(id);
	if (runtime::Ptr<model::base::StainedBaseLocation> coloredLoc = runtime::as<model::base::StainedBaseLocation>(loc))
		handleStainedFeatures(coloredLoc->getColor(), newOccupier);
}

// Java BaseService.java:132-142
void BaseService::handleStainedFeatures(model::base::BaseColorType colorType, BaseOccupier newOccupier) {
	std::vector<runtime::Ptr<model::base::StainedBase>> spec;
	for (const runtime::Ptr<model::base::Base>& base : activeBases.values())
		if (runtime::Ptr<model::base::StainedBase> sb = runtime::as<model::base::StainedBase>(base); sb && sb->getColor() == colorType)
			spec.push_back(sb);
	int64_t equalBases = 0;
	for (const runtime::Ptr<model::base::StainedBase>& base : spec)
		if (base->getOccupier() == newOccupier)
			equalBases++;
	if (newOccupier == BaseOccupier::BALAUR || equalBases < 3) {
		for (const runtime::Ptr<model::base::StainedBase>& base : spec)
			base->deactivateEnhancedSpawns();
	} else {
		for (const runtime::Ptr<model::base::StainedBase>& base : spec)
			base->scheduleEnhancedSpawns();
	}
}

// Java BaseService.java:144-146
std::vector<runtime::Ptr<model::base::BaseLocation>> BaseService::getBaseLocations() {
	return allBaseLocations.values();
}

// Java BaseService.java:148-150
runtime::Ptr<model::base::Base> BaseService::getActiveBase(int32_t id) {
	return activeBases.get(id);
}

// Java BaseService.java:152-154
runtime::Ptr<model::base::BaseLocation> BaseService::getBaseLocation(int32_t id) {
	return allBaseLocations.get(id);
}

// Java BaseService.java:156-158
bool BaseService::isActive(int32_t id) {
	return activeBases.containsKey(id);
}

} // namespace aion::gameserver::services
