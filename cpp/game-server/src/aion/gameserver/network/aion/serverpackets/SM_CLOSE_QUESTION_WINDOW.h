#pragma once

#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * C++: `Object... params` are formatted into Java strings when the packet is constructed (Java: String.valueOf in writeImpl, hub-headers.md §7.4):
 * strings as they are, integers and booleans like String.valueOf. A Java null parameter is written as a null string; C++ callers pass
 * the empty string.
 *
 * @author Neon
 */
class SM_CLOSE_QUESTION_WINDOW : public AionServerPacket {
private:
	static constexpr int32_t MAX_PARAM_COUNT = 3;
	int32_t msgId{};
	std::vector<std::string> params; // fieldmap.toml: Java Object[], formatted into Java strings at construction

	/** Java String.valueOf of one `Object...` parameter (strings as they are, integers and booleans) */
	template <class Param>
	static std::string toParam(Param&& param) {
		if constexpr (std::is_convertible_v<Param, std::string_view>)
			return std::string(std::string_view(param));
		else if constexpr (std::same_as<std::remove_cvref_t<Param>, bool>)
			return param ? "true" : "false";
		else {
			static_assert(std::is_integral_v<std::remove_cvref_t<Param>>, "SM_CLOSE_QUESTION_WINDOW parameters are strings, integers or booleans");
			return std::to_string(param);
		}
	}

public:
	/** %0 has withdrawn the challenge for a duel. */
	static SM_CLOSE_QUESTION_WINDOW STR_DUEL_REQUESTER_WITHDRAW_REQUEST(std::string_view value0);
	/** %0 declined your challenge. */
	static SM_CLOSE_QUESTION_WINDOW STR_DUEL_HE_REJECT_DUEL(std::string_view value0);
	static SM_CLOSE_QUESTION_WINDOW CLOSE_QUESTION_WINDOW();
	/** Java: SM_CLOSE_QUESTION_WINDOW(int msgId, Object... params) with the parameters already formatted */
	explicit SM_CLOSE_QUESTION_WINDOW(int32_t msgId, std::vector<std::string> params = {});

	/** Java: SM_CLOSE_QUESTION_WINDOW(int msgId, Object... params) with the values to format (hub-headers.md §7.4) */
	template <class... Params>
		requires(sizeof...(Params) > 0 && !(sizeof...(Params) == 1 && (std::same_as<std::remove_cvref_t<Params>, std::vector<std::string>> && ...)))
	explicit SM_CLOSE_QUESTION_WINDOW(int32_t msgId, Params&&... params)
		: SM_CLOSE_QUESTION_WINDOW(msgId, std::vector<std::string>{toParam(std::forward<Params>(params))...}) {}

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
