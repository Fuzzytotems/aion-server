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
 * Opens a yes/no question window on the client. Question based on the code given, defined in client_strings.xml
 * <p>
 * S0c declaration header (hub-headers.md §12). C++ difference: `Object... params` are only formatted with String.valueOf in writeImpl, so
 * they are formatted into Java strings when the packet is constructed (hub-headers.md §7.4, like SM_SYSTEM_MESSAGE): strings as they are,
 * numbers through toJavaString. Callers that pass a Java `Object[]` on (`requestParams`) pass the formatted `std::vector<std::string>`.
 *
 * @author Ben, avol, Lyahim, Neon
 */
class SM_QUESTION_WINDOW : public AionServerPacket {
public:
	static constexpr int32_t STR_DUEL_DO_YOU_ACCEPT_REQUEST = 50028;
	static constexpr int32_t STR_DUEL_DO_YOU_WITHDRAW_REQUEST = 50030;
	static constexpr int32_t STR_PARTY_DO_YOU_ACCEPT_INVITATION = 60000;
	static constexpr int32_t STR_PARTY_ALLIANCE_DO_YOU_ACCEPT_HIS_INVITATION = 70000;
	static constexpr int32_t STR_PARTY_ALLIANCE_CHANGE_LOOT_TO_FREE_HE_ASKED = 70001;
	static constexpr int32_t STR_PARTY_ALLIANCE_CHANGE_LOOT_TO_RANDOM_HE_ASKED = 70002;
	static constexpr int32_t STR_PARTY_ALLIANCE_PICKUP_ITEM_HE_ASKED = 70003;
	static constexpr int32_t STR_FORCE_DO_YOU_ACCEPT_INVITATION = 70004;
	static constexpr int32_t STR_GUILD_CREATE_DO_YOU_ACCEPT_PAY = 80000;
	static constexpr int32_t STR_GUILD_INVITE_DO_YOU_ACCEPT_INVITATION = 80001;
	static constexpr int32_t STR_GUILD_TRANSFER_GUILDMASTER = 80005;
	static constexpr int32_t STR_GUILD_DO_YOU_LEAVE = 80006;
	static constexpr int32_t STR_GUILD_DO_YOU_BANISH = 80007;
	static constexpr int32_t STR_GUILD_DISPERSE_STAYMODE = 80008;
	static constexpr int32_t STR_GUILD_DISPERSE_STAYMODE_CANCEL = 80009;
	static constexpr int32_t STR_GUILD_CHANGE_LEVEL_DO_YOU_ACCEPT_PAY = 80010;
	static constexpr int32_t STR_GUILD_CHANGE_MASTER_DO_YOU_ACCEPT_OFFER = 80011;
	static constexpr int32_t STR_BUY_SELL_CONFIRM_PURCHASE_EXCESSIVE_PRICE = 90000;
	static constexpr int32_t STR_EXCHANGE_DO_YOU_ACCEPT_EXCHANGE = 90001;
	static constexpr int32_t STR_QUEST_GIVEUP = 150000;
	static constexpr int32_t STR_QUEST_GIVEUP_WHEN_DELETE_QUEST_ITEM = 150001;
	static constexpr int32_t STR_ASK_RECOVER_EXPERIENCE = 160011;
	static constexpr int32_t STR_ASK_REGISTER_RESURRECT_POINT = 160012;
	static constexpr int32_t STR_TELEPORT_NEED_CONFIRM = 160013;
	static constexpr int32_t STR_ASK_GROUP_GATE_DO_YOU_ACCEPT_MOVE = 160014;
	static constexpr int32_t STR_HOUSE_GATE_ACCEPT_MOVE_DONT_RETURN = 904435;
	static constexpr int32_t STR_ASK_USE_ARTIFACT = 160016;
	static constexpr int32_t STR_ASK_PASS_BY_GATE = 160017;
	static constexpr int32_t STR_ASK_REGISTER_BINDSTONE = 160018;
	static constexpr int32_t STR_ASK_PASS_BY_DIRECT_PORTAL = 160019;
	static constexpr int32_t STR_ASK_DOOR_REPAIR_DO_YOU_ACCEPT_REPAIR = 160021;
	static constexpr int32_t STR_ASK_DOOR_REPAIR_POPUPDIALOG = 160027;
	static constexpr int32_t STR_ASK_ARTIFACT_POPUPDIALOG = 160028;
	static constexpr int32_t STR_ASK_JOIN_NEW_FACTION = 160033;
	static constexpr int32_t STR_CONFIRM_LOOT = 900495;
	static constexpr int32_t STR_WAREHOUSE_EXPAND_WARNING = 900686;
	static constexpr int32_t STR_CRAFT_ADDSKILL_CONFIRM = 900852;
	static constexpr int32_t STR_AIONJEWEL_SHOP_BUY_CONFIRM = 901972;
	static constexpr int32_t STR_SUMMON_PARTY_DO_YOU_ACCEPT_REQUEST = 901721;
	static constexpr int32_t STR_INSTANCE_DUNGEON_WITH_DIFFICULTY_ENTER_CONFIRM = 902050;
	static constexpr int32_t STR_MSGBOX_UNION_INVITE_ME = 902249;
	static constexpr int32_t STR_SOUL_BOUND_ITEM_DO_YOU_WANT_SOUL_BOUND = 95006;
	static constexpr int32_t STR_ITEM_CHARGE_ALL_CONFIRM = 903026;
	static constexpr int32_t STR_ITEM_CHARGE2_ALL_CONFIRM = 904039;
	static constexpr int32_t STR_ITEM_CHARGE_CONFIRM_SOME_ALREADY_CHARGED = 903028;
	static constexpr int32_t STR_ASSEMBLY_ITEM_POPUP_CONFIRM = 903441;
	static constexpr int32_t STR_HOUSING_TELEPORT_HOME_CONFIRM = 903533;
	static constexpr int32_t STR_HOUSING_TELEPORT_BUDDY_CONFIRM = 903534;
	static constexpr int32_t STR_HOUSING_TELEPORT_RANDOM_CONFIRM = 903535;
	static constexpr int32_t STR_HOUSING_TELEPORT_GUILD_CONFIRM = 903536;
	static constexpr int32_t STR_ASK_PASS_BY_SVS_DIRECT_PORTAL = 905067;
	static constexpr int32_t STR_BUDDYLIST_ADD_BUDDY_REQUEST = 1401498;
private:
	static constexpr int32_t MAX_PARAM_COUNT = 3;
	int32_t code{};
	int32_t senderId{};
	int32_t rangeOrCooldownSeconds{};
	std::vector<std::string> params{}; // fieldmap: Java Object[], formatted into Java strings at construction (hub-headers.md §7.4)

	/** Java String.valueOf of one `Object...` parameter (strings as they are, numbers through toJavaString) */
	template <class Param>
	static std::string toParam(Param&& param) {
		if constexpr (std::is_convertible_v<Param, std::string_view>)
			return std::string(std::string_view(param));
		else
			return toJavaString(std::forward<Param>(param));
	}

public:
	/** Java String.valueOf(int) */
	static std::string toJavaString(int32_t value);
	/** Java String.valueOf(long) */
	static std::string toJavaString(int64_t value);

	/**
	 * Creates a new <tt>SM_QUESTION_WINDOW</tt> packet (Java: `Object... params` with the parameters already formatted)
	 *
	 * @param code
	 *          - code The string code to display, found in client_strings.xml
	 * @param senderId
	 *          - sender Object id
	 * @param rangeOrCooldownSeconds
	 *          - the valid range for this dialog (relative to the senderId) or the stone cooldown time for artifact or door repair windows
	 * @param params
	 *          - params The parameters for the string, if any
	 * @throws IllegalArgumentException if there are more than MAX_PARAM_COUNT parameters
	 */
	SM_QUESTION_WINDOW(int32_t code, int32_t senderId, int32_t rangeOrCooldownSeconds, std::vector<std::string> params);

	/** Java: SM_QUESTION_WINDOW(int code, int senderId, int rangeOrCooldownSeconds, Object... params) with the values to format (§7.4) */
	template <class... Params>
		requires(!(sizeof...(Params) == 1 && (std::same_as<std::remove_cvref_t<Params>, std::vector<std::string>> && ...)))
	SM_QUESTION_WINDOW(int32_t code, int32_t senderId, int32_t rangeOrCooldownSeconds, Params&&... params)
		: SM_QUESTION_WINDOW(code, senderId, rangeOrCooldownSeconds, std::vector<std::string>{toParam(std::forward<Params>(params))...}) {}

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
