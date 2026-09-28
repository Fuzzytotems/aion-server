#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

/**
 * Authenticates users against the HTTP service configured in Config::EXTERNAL_AUTH_URL: POSTs the JSON object {"user": ..., "password": ...}
 * (User-Agent AionLS) and expects status 200 with a JSON object {"accountId": "...", "aionAuthResponseId": n}.
 * <p>
 * Java: com.aionemu.loginserver.utils.ExternalAuth
 *
 * @author Neon
 */
namespace aion::loginserver::utils::ExternalAuth {

/** Java: record Response(String accountId, int aionAuthResponseId) */
struct Response {
	/** the account name to use, std::nullopt if the response has none (Java: null) */
	std::optional<std::string> accountId;
	/** an AionAuthResponse id, 0 if missing */
	int32_t aionAuthResponseId = 0;

	bool operator==(const Response&) const = default;
};

/**
 * Sends the credentials to the external authentication service (blocking).
 * <p>
 * Deviation: requests time out after 30 seconds (Java's HttpClient waits indefinitely, which would block a packet thread forever).
 *
 * @return the parsed response, or std::nullopt if the service returned another status code than 200 ("Server returned status code ..." is
 *         logged), could not be reached or returned an invalid response ("Could not login user ..." is logged), or returned an empty body or
 *         JSON null
 */
std::optional<Response> authenticate(std::string_view user, std::string_view password);

/**
 * Parses a response body like Java's JSON.parseObject(body, Response.class) (fastjson2): an empty body or null gives std::nullopt, missing fields
 * keep their defaults, numbers are accepted for accountId and numeric strings for aionAuthResponseId. Exposed for tests.
 *
 * @throws commons::utils::IllegalArgumentException (or a JSON exception) if the body is not a JSON object with fields of the right types
 */
std::optional<Response> parseResponse(std::string_view body);

} // namespace aion::loginserver::utils::ExternalAuth
