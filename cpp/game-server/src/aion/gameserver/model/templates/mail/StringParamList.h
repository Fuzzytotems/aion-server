#pragma once

#include <vector>

#include "aion/gameserver/model/templates/mail/StringParamList.xml.h"

namespace aion::gameserver::model::templates::mail {

/** Java com.aionemu.gameserver.model.templates.mail.StringParamList. @author Rolandas */
class StringParamList : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/mail/StringParamList.xml.inc"
public:
	/** Java returns Collections.emptyList() without params; the C++ list always exists */
	const std::vector<Param>& getParams() const { return params; }
};

} // namespace aion::gameserver::model::templates::mail
