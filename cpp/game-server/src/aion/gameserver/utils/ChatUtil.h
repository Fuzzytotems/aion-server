#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/fwd.h"
#include "aion/gameserver/utils/JavaColor.h"
#include "aion/gameserver/utils/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::utils {

/**
 * C++: a static-only class (fieldmap K5). Strings are UTF-8; everything Java measures or cuts in UTF-16 code units (split, getRealCharName) is
 * computed on UTF-16 and converted back.
 * <ul>
 * <li>DF (`DecimalFormat(".##")`, Locale.US) is the private helper formatDecimal: at most two fraction digits rounded HALF_EVEN from the exact
 * binary value, no integer zero before a fraction (".5"), "0" for zero, a kept minus sign ("-0" for -0.001).</li>
 * <li>l10n returns "" where Java returns null (id 0), the convention of L10n::getL10n. The client string id is two UTF-16 code units, kept as
 * WTF-8 (commons StringUtils::toWtf8) so that a lone surrogate unit reaches the wire unchanged through writeS; split decodes and encodes
 * WTF-8 as well.</li>
 * <li>Nullable String parameters are `std::optional<std::string_view>`; getPosition and parsedCoordsToWorldPosition return the new
 * WorldPosition as a Ref, null for invalid input.</li>
 * </ul>
 *
 * @author antness, Neon
 */
class ChatUtil {
private:
	static constexpr char16_t ASMO_NAME_PREFIX = u'';
	static constexpr char16_t ELYOS_NAME_PREFIX = u'';

public:
	ChatUtil() = delete;

	/** @see #color(String, int, int, int); a null color (std::nullopt) is WHITE */
	static std::string color(std::string_view message, std::optional<JavaColor> color);

	/** @see #color(String, int, int, int) */
	static std::string color(std::string_view message, int32_t rgb);

	/**
	 * This creates a colored text message.<br>
	 * Does not work in public chats. The valid message length is limited. In fact the message limit depends on the length of the internal link
	 * parameters, the whole link becomes invalid when exceeding x characters.
	 *
	 * @param r
	 *          red component (0-255)
	 * @param g
	 *          green component (0-255)
	 * @param b
	 *          blue component (0-255)
	 * @return The color encoded message.
	 */
	static std::string color(std::string_view message, int32_t r, int32_t g, int32_t b);

	/**
	 * @return Formatted string for the client to display the corresponding text for male/female characters. The provided male string must not
	 *         contain spaces, otherwise only the part after the last space will be replaced. Does not work in {@link SM_MESSAGE}
	 */
	static std::string genderize(std::string_view wordForMales, std::string_view textForFemales);

	/** @return A clickable character name for system {@link ChatType chat types}.<br> */
	static std::string charName(model::gameobjects::player::Player& player);

	/**
	 * @return String identifier for the localized client string with the given ID ("" for 0, Java: null).<br/>
	 *         The client will display the corresponding localized message instead of the string identifier. This works with {@link SM_MESSAGE}
	 *         (only if the senderObjectId is 0), {@link SM_SYSTEM_MESSAGE} and {@link SM_QUESTION_WINDOW},
	 */
	static std::string l10n(int32_t l10nId);

	/** @see #path(String, int) */
	static std::string path(model::gameobjects::VisibleObject& object, bool withIdInName);

	/** @see #path(String, int) */
	static std::string path(int32_t npcId, bool withIdInName);

private:
	static std::string path(const model::templates::VisibleObjectTemplate* template_, bool withIdInName);

public:
	/** @return The objects spawn point link, based on its localized name (client does a lookup in its current language) */
	static std::string path(std::string_view npcName);

	/** @return The objects spawn point link, based on its ID. */
	static std::string path(std::string_view linkName, int32_t npcId);

	static std::string position(std::string_view label, world::WorldPosition& pos);

	static std::string position(std::string_view label, int64_t worldId, float x, float y, float z);

	/**
	 * @param posLink
	 *          can be an Aion link like "{@code [pos:Teleporter;0 400010000 2128.8 1924.3 0.0 2]}"
	 * @return The {@link WorldPosition} or null if input was invalid.
	 */
	static runtime::Ref<world::WorldPosition> getPosition(std::optional<std::string_view> posLink);

	static runtime::Ref<world::WorldPosition> parsedCoordsToWorldPosition(int32_t mapAndInstanceId, float x, float y, std::optional<float> z,
		std::optional<int32_t> zSearchOffset);

	static std::string item(int32_t itemId);

	static std::string itemName(int32_t itemId);

	static std::string recipe(int32_t recipeId);

	static std::string quest(int32_t questId);

	/**
	 * @param itemStr
	 *          can be ID string or Aion link like "{@code [item: 100000094]}"
	 * @return The item ID or 0 if {@code itemStr} did not contain a valid ID.
	 */
	static int32_t getItemId(std::optional<std::string_view> itemStr);

	/**
	 * @param questStr
	 *          can be ID string or Aion link like "{@code [quest: 1006]}"
	 * @return The quest ID or 0 if {@code questStr} did not contain a valid ID.
	 */
	static int32_t getQuestId(std::optional<std::string_view> questStr);

private:
	static int32_t getIdFromString(std::optional<std::string_view> input, std::string_view linkAccessor, std::string_view validationPattern);

public:
	/** @return The character name without custom tags. */
	static std::string getRealCharName(std::string_view name);

	static std::string getRealCharName(std::string_view name, bool nameIsFromGMCommand);

	/** @return The player name with an icon in front to distinguish between elyos and asmodians. Only added if the reader is a staff member. */
	static std::string toFactionPrefixedName(model::gameobjects::player::Player& reader, model::gameobjects::player::Player& player);

	/** @return The string padded to the given width for display in chat. */
	static std::string leftPad(int64_t number, int32_t width);

	/**
	 * Splits a chat message into parts that fit within the client's 1022 character display limit.<br>
	 * It makes a best-effort estimate to account for links and l10n identifiers, which often expand into longer rendered text on the client
	 * side.<br>
	 * Splitting occurs at a newline or space, if present, to avoid breaking links or words.
	 */
	static std::vector<std::string> split(std::string_view chatMessage);

private:
	/** Java's indexes are UTF-16 code unit indexes into chatMessage */
	static int32_t findSplitIndex(std::u16string_view chatMessage, int32_t startIndex, int32_t endIndex);

	/** C++ only: Java DF.format(double) with the DecimalFormat pattern ".##" (Locale.US), see the class comment */
	static std::string formatDecimal(double value);
};

} // namespace aion::gameserver::utils
