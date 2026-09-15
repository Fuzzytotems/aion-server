#pragma once

#include <string>
#include <string_view>

#include "aion/gameserver/model/templates/mail/Title.xml.h"

namespace aion::gameserver::model::templates::mail {

/** Java com.aionemu.gameserver.model.templates.mail.Title. C++: overrides are non-virtual (MailPart.h). @author Rolandas */
class Title : public ::aion::gameserver::model::templates::mail::MailPart {
#include "aion/gameserver/model/templates/mail/Title.xml.inc"
public:
	/** Java @Override getType(): MailPartType.TITLE without a type attribute */
	MailPartType getType() const { return type.value_or(MailPartType::TITLE); }

	/** Java @Override getParamValue(name) */
	std::string getParamValue(std::string_view name) const { return ""; }
};

} // namespace aion::gameserver::model::templates::mail
