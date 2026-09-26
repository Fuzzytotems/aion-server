#pragma once

#include <array>
#include <string>

#include "aion/gameserver/model/templates/mail/MailPartType.h"
#include "aion/gameserver/model/templates/mail/MailTemplate.xml.h"

#include "aion/gameserver/model/templates/mail/fwd.h"

namespace aion::gameserver::model::templates::mail {

/**
 * Java com.aionemu.gameserver.model.templates.mail.MailTemplate.
 * <p>
 * C++: the @XmlTransient `mailPartsMap` (a HashMap, looked up by key only) is a C++-only array of template pointers into the bound parts,
 * indexed by the MailPartType ordinal, which the hook fills and keeps (Java nulls the list).
 *
 * @author Rolandas
 */
class MailTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/mail/MailTemplate.xml.inc"
private:
	/** Java @XmlTransient Map<MailPartType, MailPart> mailPartsMap */
	std::array<const MailPart*, 6> mailPartsMap{};

	const MailPart* getPart(MailPartType type) const;

public:
	/** @return the part, nullptr (Java null) if the template has none */
	const MailPart* getSender() const { return getPart(MailPartType::SENDER); }

	const MailPart* getTitle() const { return getPart(MailPartType::TITLE); }

	const MailPart* getHeader() const { return getPart(MailPartType::HEADER); }

	const MailPart* getBody() const { return getPart(MailPartType::BODY); }

	const MailPart* getTail() const { return getPart(MailPartType::TAIL); }

	/** @throws NullPointerException (Java) without a title part */
	std::string getFormattedTitle(const IMailFormatter* customFormatter) const;

	/** @throws NullPointerException (Java) without a header, body or tail part */
	std::string getFormattedMessage(const IMailFormatter* customFormatter) const;
};

} // namespace aion::gameserver::model::templates::mail
