#pragma once

#include "aion/gameserver/skillengine/model/SignetDataTemplate.xml.h"

#include <cstdint>

namespace aion::gameserver::skillengine::model {

/** Java com.aionemu.gameserver.skillengine.model.SignetDataTemplate. */
class SignetDataTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/model/SignetDataTemplate.xml.inc"
public:
	/** @return the first signet data of the level, nullptr (Java null) if there is none */
	const SignetData* getSignetDataForSignetLevel(int32_t level) const;
};

} // namespace aion::gameserver::skillengine::model
