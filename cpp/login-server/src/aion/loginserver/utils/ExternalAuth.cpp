#include "aion/loginserver/utils/ExternalAuth.h"

#include <chrono>
#include <cmath>
#include <limits>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/loginserver/configs/Config.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::loginserver::utils::ExternalAuth {

using commons::utils::IllegalArgumentException;
using nlohmann::json;

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.utils.ExternalAuth"));
	return *logger;
}

int32_t toInt(const json& value) {
	if (value.is_number_integer()) {
		if (value.is_number_unsigned()) {
			uint64_t number = value.get<uint64_t>();
			if (number <= static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))
				return static_cast<int32_t>(number);
		} else {
			int64_t number = value.get<int64_t>();
			if (number >= std::numeric_limits<int32_t>::min() && number <= std::numeric_limits<int32_t>::max())
				return static_cast<int32_t>(number);
		}
		throw IllegalArgumentException("aionAuthResponseId out of int range: " + value.dump());
	}
	if (value.is_number_float()) {
		double number = value.get<double>();
		if (std::isfinite(number) && number >= std::numeric_limits<int32_t>::min() && number <= std::numeric_limits<int32_t>::max())
			return static_cast<int32_t>(number);
		throw IllegalArgumentException("aionAuthResponseId out of int range: " + value.dump());
	}
	if (value.is_string()) {
		std::string text = value.get<std::string>();
		if (text.empty())
			return 0;
		return commons::utils::parseInt(text);
	}
	throw IllegalArgumentException("aionAuthResponseId is not a number: " + value.dump());
}

} // namespace

std::optional<Response> parseResponse(std::string_view body) {
	if (commons::utils::StringUtils::isBlank(body))
		return std::nullopt;
	json object = json::parse(body);
	if (object.is_null())
		return std::nullopt;
	if (!object.is_object())
		throw IllegalArgumentException("Response is not a JSON object: " + std::string(body));
	Response response;
	if (auto accountId = object.find("accountId"); accountId != object.end() && !accountId->is_null()) {
		if (accountId->is_string())
			response.accountId = accountId->get<std::string>();
		else if (accountId->is_number())
			response.accountId = accountId->dump();
		else
			throw IllegalArgumentException("accountId is not a string: " + accountId->dump());
	}
	if (auto id = object.find("aionAuthResponseId"); id != object.end() && !id->is_null())
		response.aionAuthResponseId = toInt(*id);
	return response;
}

std::optional<Response> authenticate(std::string_view user, std::string_view password) {
	std::optional<Response> info;
	try {
		const std::string requestBody = json{{"user", user}, {"password", password}}.dump();
		cpr::Response response = cpr::Post(cpr::Url{configs::Config::EXTERNAL_AUTH_URL},
			cpr::Header{{"User-Agent", "AionLS"}, {"Content-Type", "application/json"}}, cpr::Body{requestBody},
			cpr::Timeout{std::chrono::seconds(30)}, cpr::Redirect{false});
		if (response.error.code != cpr::ErrorCode::OK)
			throw commons::utils::IOException(response.error.message.empty() ? "HTTP request failed" : response.error.message);
		if (response.status_code == 200) {
			info = parseResponse(response.text);
		} else {
			log().warn("Server returned status code " + std::to_string(response.status_code) + (response.text.empty() ? "" : ": " + response.text));
		}
	} catch (...) {
		info.reset();
		log().errorCurrentException("Could not login user " + std::string(user));
	}
	return info;
}

} // namespace aion::loginserver::utils::ExternalAuth
