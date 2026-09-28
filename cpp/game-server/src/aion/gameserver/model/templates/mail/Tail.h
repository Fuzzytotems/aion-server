#pragma once

#include <string>
#include <string_view>

#include "aion/gameserver/model/templates/mail/Tail.xml.h"

namespace aion::gameserver::model::templates::mail {

/** Java com.aionemu.gameserver.model.templates.mail.Tail. C++: overrides are non-virtual (MailPart.h). @author Rolandas */
class Tail : public ::aion::gameserver::model::templates::mail::MailPart {
#include "aion/gameserver/model/templates/mail/Tail.xml.inc"
public:
	/** Java @Override getType(): MailPartType.TAIL without a type attribute */
	MailPartType getType() const { return type.value_or(MailPartType::TAIL); }

	/** Java @Override getParamValue(name) */
	std::string getParamValue(std::string_view name) const { return ""; }
};

} // namespace aion::gameserver::model::templates::mail
