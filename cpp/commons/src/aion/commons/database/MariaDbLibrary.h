#pragma once

namespace aion::commons::database::MariaDbLibrary {

/**
 * Initializes MariaDB Connector/C (mysql_library_init) if necessary. Must be called before creating connection handles, since mysql_init is
 * not thread safe when it has to initialize the library itself. Thread safe. Called by Connection::open.
 */
void acquire();

/** Signals that a connection handle obtained after acquire() was closed. */
void release() noexcept;

/**
 * Frees the library's global resources (mysql_library_end) if no connection handle is alive anymore; a later acquire() initializes it
 * again. Called by DatabaseFactory::shutdown.
 * @return true if the library was shut down (or was not initialized)
 */
bool shutdownIfUnused() noexcept;

} // namespace aion::commons::database::MariaDbLibrary
