#include "aion/gameserver/model/templates/survey/SurveyItem.h"

namespace aion::gameserver::model::templates::survey {

SurveyItem::SurveyItem() = default;

runtime::Ref<SurveyItem> SurveyItem::create() {
	return runtime::makeRef<SurveyItem>();
}

SurveyItem::~SurveyItem() = default;

} // namespace aion::gameserver::model::templates::survey
