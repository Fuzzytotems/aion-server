#pragma once

#include <string>
#include <string_view>

#include "aion/gameserver/model/templates/mail/MailPartType.h"

namespace aion::gameserver::model::templates::mail {

/**
 * Java com.aionemu.gameserver.model.templates.mail.IMailFormatter.
 * <p>
 * C++: an interface (hub-headers.md §9.2). The static data MailPart shells cannot derive it (a base of a frozen shell); they provide the same
 * functions non-virtually (MailPart.h). The anonymous `new MailPart() { getParamValue ... }` formatters of MailFormatter implement this interface
 * and are passed to MailTemplate::getFormattedTitle/getFormattedMessage.
 *
 * @author Rolandas
 */
class IMailFormatter {
public:
	virtual MailPartType getType() const = 0;

	virtual std::string getFormattedString(MailPartType partType) const = 0;

	virtual std::string getParamValue(std::string_view name) const = 0;

	virtual ~IMailFormatter() = default;

protected:
	IMailFormatter() = default;
	IMailFormatter(const IMailFormatter&) = default;
	IMailFormatter& operator=(const IMailFormatter&) = default;
};

} // namespace aion::gameserver::model::templates::mail
