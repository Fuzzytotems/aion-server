#include "aion/gameserver/model/templates/mail/MailTemplate.h"

#include <memory>
#include <string>

#include "aion/gameserver/model/templates/mail/MailPart.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::mail {

namespace {
/** Java calls the part the map returned: NullPointerException if there is none */
const MailPart& required(const MailPart* part, const char* name) {
	if (part == nullptr)
		throw runtime::NullPointerException(std::string("MailTemplate has no ") + name + " part");
	return *part;
}
} // namespace

void MailTemplate::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const std::unique_ptr<MailPart>& part : mailParts)
		mailPartsMap[static_cast<size_t>(part->getType())] = part.get();
}

const MailPart* MailTemplate::getPart(MailPartType type) const {
	return mailPartsMap[static_cast<size_t>(type)];
}

std::string MailTemplate::getFormattedTitle(const IMailFormatter* customFormatter) const {
	return required(getTitle(), "title").getFormattedString(customFormatter);
}

std::string MailTemplate::getFormattedMessage(const IMailFormatter* customFormatter) const {
	std::string headerStr = required(getHeader(), "header").getFormattedString(customFormatter);
	std::string bodyStr = required(getBody(), "body").getFormattedString(customFormatter);
	std::string tailStr = required(getTail(), "tail").getFormattedString(customFormatter);
	std::string message = headerStr;
	if (message.empty())
		message = bodyStr;
	else if (!bodyStr.empty()) {
		message += "," + bodyStr;
	}
	if (message.empty())
		message = tailStr;
	else if (!tailStr.empty()) {
		message += "," + tailStr;
	}
	return message;
}

} // namespace aion::gameserver::model::templates::mail
