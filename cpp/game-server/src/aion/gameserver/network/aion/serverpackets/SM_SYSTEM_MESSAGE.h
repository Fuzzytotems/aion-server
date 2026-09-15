#pragma once

#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * System message packet.
 * <p>
 * The hand-written part of chunk P4-06 (handlers-and-porting-plan.md §2.3): the class, its constructors, writeImpl and the three factories
 * tools/gen/sysmsg.py does not generate; the 4,117 generated factories are the member block SM_SYSTEM_MESSAGE.gen.h (definitions in
 * SM_SYSTEM_MESSAGE.gen0..7.cpp). A K2 server packet (hub-headers.md §12): a stack temporary, `sendPacket(player, SM_SYSTEM_MESSAGE::STR_X(...))`.
 * <p>
 * C++ differences:
 * - `Object... params` are formatted into Java strings when the packet is constructed (Java: `toString()` in writeImpl), so `params` holds
 *   strings; the generated code passes them as `std::vector<std::string>`, other code may pass the values to the variadic constructor
 *   (strings as they are, numbers through toJavaString, hub-headers.md §7.4). A Java null parameter is written as a null string in Java; C++
 *   callers pass the empty string.
 * - ChatType.getId() is a local table in the .cpp until the ChatType companion (P4-05) exists.
 * - getParams() is C++ only (tests).
 *
 * @author -Nemesiss-, EvilSpirit, Luno :D, Avol!, Simple :), Sarynth
 */
class SM_SYSTEM_MESSAGE final : public AionServerPacket {
public:
	/** Java String.valueOf(int) */
	static std::string toJavaString(int32_t value);
	/** Java String.valueOf(long) */
	static std::string toJavaString(int64_t value);
	/** Java String.valueOf(byte) (signed) */
	static std::string toJavaString(int8_t value);
	/** Java String.valueOf(float) (Float.toString, geomath JavaFloat::toString) */
	static std::string toJavaString(float value);

#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.gen.h"

public:
	/** Java hand-written factory: %0 %1 used %3 in %2 (the SubZone string of the player's position) */
	static SM_SYSTEM_MESSAGE STR_SKILL_ABYSS_SKILL_IS_FIRED(model::gameobjects::player::Player& player, std::string_view skill);

	/** Java hand-written factory: %0 %1 %2 has died in %3 (the SubZone string of the victim's position) */
	static SM_SYSTEM_MESSAGE STR_ABYSS_ORDER_RANKER_DIE(model::gameobjects::player::Player& victim);

	/** Java hand-written factory: %0 %1 %2 has died in %3. */
	static SM_SYSTEM_MESSAGE STR_ABYSS_ORDER_RANKER_DIE(model::gameobjects::player::Player& victim, std::string_view zoneName);

private:
	int32_t msgId;
	int8_t chatType;
	int32_t senderObjId;
	std::vector<std::string> params;        // fieldmap.toml: Java Object[], formatted into Java strings at construction (sysmsg.py contract)
	std::vector<std::string> specialParams; // Java String[]

	/** Java String.valueOf / toString() of one `Object...` parameter (strings as they are, numbers through toJavaString) */
	template <class Param>
	static std::string toParam(Param&& param) {
		if constexpr (std::is_convertible_v<Param, std::string_view>)
			return std::string(std::string_view(param));
		else
			return toJavaString(std::forward<Param>(param));
	}

public:
	/** Java: SM_SYSTEM_MESSAGE(int msgId, Object... params) with the parameters already formatted (ChatType.GOLDEN_YELLOW, no sender) */
	SM_SYSTEM_MESSAGE(int32_t msgId, std::vector<std::string> params);

	/** Java: SM_SYSTEM_MESSAGE(int msgId, Object... params) with the values to format (hub-headers.md §7.4) */
	template <class... Params>
		requires(sizeof...(Params) > 0 && !(sizeof...(Params) == 1 && (std::same_as<std::remove_cvref_t<Params>, std::vector<std::string>> && ...)))
	explicit SM_SYSTEM_MESSAGE(int32_t msgId, Params&&... params)
		: SM_SYSTEM_MESSAGE(msgId, std::vector<std::string>{toParam(std::forward<Params>(params))...}) {}

	/** Java: SM_SYSTEM_MESSAGE(ChatType chatType, VisibleObject sender, int msgId, Object... params) */
	SM_SYSTEM_MESSAGE(model::ChatType chatType, runtime::Ptr<model::gameobjects::VisibleObject> sender, int32_t msgId,
		std::vector<std::string> params);

	/**
	 * Java: SM_SYSTEM_MESSAGE(ChatType chatType, VisibleObject sender, int msgId, Object[] params, String... specialParams)
	 *
	 * @param chatType The chat channel the message will be displayed in.
	 * @param sender Object that sends the message, can be null (will display a speech bubble above his head, if the chat type is not a SysMsg
	 *          chat type)
	 * @param msgId The ID of the client message to send.
	 * @param params Parameters for this client message, like names, level values, etc. (Java: can be null; C++: empty)
	 * @param specialParams Special parameters, currently only known to work with client messages that want a [%target] parameter
	 */
	SM_SYSTEM_MESSAGE(model::ChatType chatType, runtime::Ptr<model::gameobjects::VisibleObject> sender, int32_t msgId,
		std::vector<std::string> params, std::initializer_list<std::string_view> specialParams);

protected:
	void writeImpl(AionConnection* con) override;

public:
	int32_t getId() const noexcept { return msgId; }

	/** C++ only: the formatted parameters (tests) */
	const std::vector<std::string>& getParams() const noexcept { return params; }
};

} // namespace aion::gameserver::network::aion::serverpackets
