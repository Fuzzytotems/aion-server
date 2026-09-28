#pragma once

#include <string>
#include <string_view>

#include "aion/gameserver/model/templates/mail/Sender.xml.h"

namespace aion::gameserver::model::templates::mail {

/** Java com.aionemu.gameserver.model.templates.mail.Sender. C++: overrides are non-virtual (MailPart.h). @author Rolandas */
class Sender : public ::aion::gameserver::model::templates::mail::MailPart {
#include "aion/gameserver/model/templates/mail/Sender.xml.inc"
public:
	/** Java @Override getType(): MailPartType.SENDER without a type attribute */
	MailPartType getType() const { return type.value_or(MailPartType::SENDER); }

	/** Java @Override getParamValue(name) */
	std::string getParamValue(std::string_view name) const { return ""; }
};

} // namespace aion::gameserver::model::templates::mail
