#include "aion/gameserver/skillengine/model/SignetDataTemplate.h"

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::skillengine::model {

const SignetData* SignetDataTemplate::getSignetDataForSignetLevel(int32_t level) const {
	// Java iterates signetDataList, which is null without <signet_data> children (a bound list is never empty otherwise)
	if (signetDataList.empty())
		throw runtime::NullPointerException("SignetDataTemplate.signetDataList is null");
	for (const SignetData& data : signetDataList) {
		if (data.getLevel() == level)
			return &data;
	}
	return nullptr;
}

} // namespace aion::gameserver::skillengine::model
