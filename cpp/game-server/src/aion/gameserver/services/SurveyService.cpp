#include "aion/gameserver/services/SurveyService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/templates/survey/SurveyItem.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.SurveyService");

// Java implements Runnable (inner class: calls taskUpdate() of the enclosing instance). A task object of scheduleAtFixedRate with only
// immutable members (fieldmap K3): ported as a TaskStruct value (runtime-architecture.md §7.3, §14.2(f)).
class SurveyService::TaskUpdate {
public:
	const SurveyService* surveyService; // Java: the enclosing instance (this$0, the Immortal singleton)

	void run();
};

void SurveyService::TaskUpdate::run() {
	AION_UNPORTED();
}

SurveyService::SurveyService() {
	AION_UNPORTED();
}

bool SurveyService::isActive(model::gameobjects::player::Player& player, int32_t survId) {
	AION_UNPORTED();
}

void SurveyService::requestSurvey(model::gameobjects::player::Player& player, int32_t survId) {
	AION_UNPORTED();
}

void SurveyService::taskUpdate() {
	AION_UNPORTED();
}

void SurveyService::showAvailable(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

SurveyService& SurveyService::getInstance() {
	static SurveyService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services
