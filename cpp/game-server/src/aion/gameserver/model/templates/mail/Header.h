#pragma once

#include <string>
#include <string_view>

#include "aion/gameserver/model/templates/mail/Header.xml.h"

namespace aion::gameserver::model::templates::mail {

/** Java com.aionemu.gameserver.model.templates.mail.Header. C++: overrides are non-virtual (MailPart.h). @author Rolandas */
class Header : public ::aion::gameserver::model::templates::mail::MailPart {
#include "aion/gameserver/model/templates/mail/Header.xml.inc"
public:
	/** Java @Override getType(): MailPartType.HEADER without a type attribute */
	MailPartType getType() const { return type.value_or(MailPartType::HEADER); }

	/** Java @Override getParamValue(name) */
	std::string getParamValue(std::string_view name) const { return ""; }
};

} // namespace aion::gameserver::model::templates::mail
