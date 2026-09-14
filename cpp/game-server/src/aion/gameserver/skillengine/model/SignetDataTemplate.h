#pragma once

#include "aion/gameserver/skillengine/model/SignetDataTemplate.xml.h"

namespace aion::gameserver::skillengine::model {

/** Java com.aionemu.gameserver.skillengine.model.SignetDataTemplate. */
class SignetDataTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/model/SignetDataTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::model
