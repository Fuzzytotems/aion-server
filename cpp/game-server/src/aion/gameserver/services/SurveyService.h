#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/survey/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author KID
 */
class SurveyService : public runtime::Immortal {
public:
	/** Java: public inner class TaskUpdate implements Runnable (used only by the constructor, defined in SurveyService.cpp) */
	class TaskUpdate;
private:
	runtime::LinkedHashMap<int32_t, runtime::Ref<model::templates::survey::SurveyItem>> activeItems{AION_LOCK_CLASS(SurveyService::activeItems)};
	SurveyService();
public:
	bool isActive(model::gameobjects::player::Player& player, int32_t survId);
	void requestSurvey(model::gameobjects::player::Player& player, int32_t survId);
	void taskUpdate();
	void showAvailable(model::gameobjects::player::Player& player);
	static SurveyService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services
