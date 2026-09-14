#include "aion/gameserver/model/account/CharacterBanInfo.h"

#include <string>

namespace aion::gameserver::model::account {

CharacterBanInfo::CharacterBanInfo(int64_t startValue, int64_t duration, std::string_view reasonValue)
	: start(startValue), end(duration + startValue), reason(std::string(reasonValue)) {
}

CharacterBanInfo::~CharacterBanInfo() = default;

runtime::Ref<CharacterBanInfo> CharacterBanInfo::create(int64_t startValue, int64_t duration, std::string_view reasonValue) {
	return runtime::makeRef<CharacterBanInfo>(startValue, duration, reasonValue);
}

} // namespace aion::gameserver::model::account
