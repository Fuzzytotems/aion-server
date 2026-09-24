#pragma once

#include <string_view>

/**
 * Access to the chatlog table. Thread safe.
 * <p>
 * Java: com.aionemu.chatserver.dao.ChatLogDAO
 */
namespace aion::chatserver::dao::ChatLogDAO {

/** Stores a chat message. Errors are logged ("Cannot insert chat message") and not thrown. */
void save(std::string_view sender, std::string_view message, std::string_view type);

} // namespace aion::chatserver::dao::ChatLogDAO
