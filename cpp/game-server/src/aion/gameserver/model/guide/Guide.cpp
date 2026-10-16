#include "aion/gameserver/model/guide/Guide.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::guide {

Guide::Guide(int32_t value, int32_t player_idValue, std::string_view titleValue)
	: guide_id(value), player_id(player_idValue), title(std::string(titleValue)) {
}

} // namespace aion::gameserver::model::guide
