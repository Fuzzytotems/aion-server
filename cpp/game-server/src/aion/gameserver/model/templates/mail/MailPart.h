#pragma once

#include <string>
#include <string_view>

#include "aion/gameserver/model/templates/mail/MailPart.xml.h"

#include "aion/gameserver/model/templates/mail/MailPartType.h"
#include "aion/gameserver/model/templates/mail/fwd.h"

namespace aion::gameserver::model::templates::mail {

/**
 * Java com.aionemu.gameserver.model.templates.mail.MailPart.
 * <p>
 * C++: Java implements IMailFormatter, whose methods Sender, Title, Header, Body and Tail override. New virtual functions or bases of a frozen
 * shell are layout changes, so getType() and getParamValue() of this class select the concrete class by javaClassName() (the generated virtual)
 * and call its non-virtual function of the same name; both static and dynamic calls give Java's result. A custom formatter is an IMailFormatter.
 *
 * @author Rolandas
 */
class MailPart : public ::aion::gameserver::model::templates::mail::StringParamList {
#include "aion/gameserver/model/templates/mail/MailPart.xml.inc"
public:
	/** Java IMailFormatter.getType(): CUSTOM, overridden by the concrete parts */
	MailPartType getType() const;

	/** Java abstract IMailFormatter.getParamValue(name), implemented by the concrete parts */
	std::string getParamValue(std::string_view name) const;

	/**
	 * @param customFormatter the formatter of the parameter values, nullptr (Java null) to use this part
	 * @throws NullPointerException (Java) for a part without an id
	 */
	std::string getFormattedString(const IMailFormatter* customFormatter) const;

	/** Java IMailFormatter.getFormattedString(partType): the id. @throws NullPointerException (Java) for a part without an id */
	std::string getFormattedString(MailPartType partType) const;
};

} // namespace aion::gameserver::model::templates::mail
