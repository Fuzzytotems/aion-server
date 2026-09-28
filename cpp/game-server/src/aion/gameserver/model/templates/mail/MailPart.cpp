#include "aion/gameserver/model/templates/mail/MailPart.h"

#include <vector>

#include "aion/gameserver/model/templates/mail/Body.h"
#include "aion/gameserver/model/templates/mail/Header.h"
#include "aion/gameserver/model/templates/mail/IMailFormatter.h"
#include "aion/gameserver/model/templates/mail/Sender.h"
#include "aion/gameserver/model/templates/mail/Tail.h"
#include "aion/gameserver/model/templates/mail/Title.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::mail {

MailPartType MailPart::getType() const {
	std::string_view className = javaClassName();
	if (className == "Sender")
		return static_cast<const Sender&>(*this).getType();
	if (className == "Title")
		return static_cast<const Title&>(*this).getType();
	if (className == "Header")
		return static_cast<const Header&>(*this).getType();
	if (className == "Body")
		return static_cast<const Body&>(*this).getType();
	if (className == "Tail")
		return static_cast<const Tail&>(*this).getType();
	return MailPartType::CUSTOM;
}

std::string MailPart::getParamValue(std::string_view name) const {
	std::string_view className = javaClassName();
	if (className == "Sender")
		return static_cast<const Sender&>(*this).getParamValue(name);
	if (className == "Title")
		return static_cast<const Title&>(*this).getParamValue(name);
	if (className == "Header")
		return static_cast<const Header&>(*this).getParamValue(name);
	if (className == "Body")
		return static_cast<const Body&>(*this).getParamValue(name);
	if (className == "Tail")
		return static_cast<const Tail&>(*this).getParamValue(name);
	throw runtime::IllegalStateException("MailPart " + std::string(className) + " has no getParamValue"); // Java: abstract
}

std::string MailPart::getFormattedString(const IMailFormatter* customFormatter) const {
	std::string result = getFormattedString(getType());
	std::vector<std::string> paramValues;
	paramValues.reserve(getParams().size());
	for (const Param& param : getParams())
		paramValues.push_back(customFormatter != nullptr ? customFormatter->getParamValue(param.getId()) : getParamValue(param.getId()));
	std::string joinedParams; // Java: String.join(",", paramValues)
	for (size_t i = 0; i < paramValues.size(); ++i) {
		if (i > 0)
			joinedParams += ',';
		joinedParams += paramValues[i];
	}
	if (result.empty())
		return joinedParams;
	else if (!joinedParams.empty())
		result += "," + joinedParams;
	return result;
}

std::string MailPart::getFormattedString(MailPartType partType) const {
	if (!id) // Java: unboxing the null Integer in `id > 0`
		throw runtime::NullPointerException("MailPart.id is null");
	std::string result;
	if (*id > 0)
		result += std::to_string(*id);
	return result;
}

} // namespace aion::gameserver::model::templates::mail
