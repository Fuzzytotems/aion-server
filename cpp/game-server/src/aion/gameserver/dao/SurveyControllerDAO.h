#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/templates/survey/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author KID
 */
class SurveyControllerDAO {
public:
	static std::vector<runtime::Ref<model::templates::survey::SurveyItem>> getAllUnused();
	static bool useItem(int32_t id);
};

} // namespace aion::gameserver::dao
