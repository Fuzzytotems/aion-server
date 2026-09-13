// Deliberately no #pragma once: the header only removes macros, so it can (and should) be included again after any later group of includes
// that may bring the macros back.

/**
 * Removes <windows.h> macros that break ordinary C++ identifiers, e.g. the enum values of ported Java enums:
 * <pre>
 * enum class ConnectType { ..., DELETE };               // winnt.h: #define DELETE (0x00010000L)
 * enum class PlayerTransferResultStatus { ..., ERROR }; // wingdi.h: #define ERROR 0
 * enum class ConditionOperation { ..., IN };            // minwindef.h: #define IN (empty)
 * void fromFrustum(float near, float far, ...);         // minwindef.h: #define near, #define far (empty)
 * </pre>
 * Asio includes <windows.h> (plus <winsock2.h>, <ws2tcpip.h> and <mswsock.h>) on Windows, so every header that includes Asio or Windows
 * headers must include this header after them, as its last include:
 * <pre>
 * #include &lt;asio/ip/tcp.hpp&gt;
 * #include &lt;windows.h&gt;
 *
 * #include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h
 * </pre>
 * The header first includes those Windows headers and &lt;cmath&gt; itself, so they are fully parsed while the macros still exist; including them
 * again later (e.g. through another Asio header) is a no-op, and the removed macros stay removed. Windows headers that are included for the
 * first time after this header may need the removed annotation macros (IN, OUT, OPTIONAL) and fail to compile: include such headers before it.
 * Code that needs one of the removed values uses the underlying constant (e.g. INFINITE is 0xFFFFFFFF) or includes its Windows header
 * before this one.
 * <p>
 * The list was determined by preprocessing <tt>&lt;asio.hpp&gt;</tt> and <tt>&lt;windows.h&gt;</tt> with the project's definitions
 * (WIN32_LEAN_AND_MEAN, NOMINMAX, _WIN32_WINNT=0x0A00) and intersecting the defined macros with the identifiers used in the Java sources of
 * the servers, plus common English words among the defined object-like macros. Not removed, because Windows headers and Windows code rely on
 * them: TRUE, FALSE, NULL, CONST, VOID, STRICT, CALLBACK/WINAPI/PASCAL, and the ERROR_xxx/S_OK/E_xxx status codes. Function-like macros
 * (FAILED(hr), RGB(r, g, b), Yield(), and the A/W function name macros like GetObject or DeleteFile) only expand when followed by '(', so they
 * do not break enum values; do not use those names for functions.
 * <p>
 * NOMINMAX (min/max) is set for all targets by cmake/AionCompilerOptions.cmake. Defining NOGDI there as well would keep &lt;wingdi.h&gt; (ERROR,
 * ABSOLUTE, RELATIVE, TRANSPARENT, OPAQUE, ...) from being included at all.
 */

#ifdef _WIN32

#include <cmath>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mswsock.h>

// minwindef.h: SAL-like parameter annotations and 16-bit pointer modifiers.
// IN, OUT and OPTIONAL are also defined by many other Windows headers with "#ifndef IN / #define IN", e.g. <bcrypt.h>, which Asio includes
// from asio/impl/connect_pipe.ipp. Defining them as themselves keeps those headers from defining them again, while the names still expand to
// the plain identifiers.
#undef IN
#undef OUT
#undef OPTIONAL
#define IN IN
#define OUT OUT
#define OPTIONAL OPTIONAL
#undef near
#undef far
// FAR and NEAR expand to far/near; keep them usable (empty) for Windows headers included later
#undef FAR
#undef NEAR
#define FAR
#define NEAR

// windef.h (unless NOMINMAX is defined)
#undef min
#undef max

// wingdi.h
#undef ERROR // region type, e.g. PlayerTransferResultStatus::ERROR
#undef ABSOLUTE // coordinate mode, e.g. MovementMask::ABSOLUTE
#undef RELATIVE // e.g. InstanceCoolTimeType::RELATIVE
#undef TRANSPARENT
#undef OPAQUE
#undef ALTERNATE
#undef WINDING

// winuser.h
#undef DIFFERENCE

// winnt.h
#undef DELETE // access right, e.g. ConnectType::DELETE

// winbase.h (INFINITE is kept: Asio's inline Windows implementation uses it in headers that may be included after this one)
#undef IGNORE

// winerror.h
#undef NO_ERROR

// wincon.h (console input event types)
#undef KEY_EVENT
#undef MOUSE_EVENT
#undef FOCUS_EVENT
#undef MENU_EVENT
#undef MOUSE_MOVED
#undef DOUBLE_CLICK

// corecrt_math.h (_matherr exception types, defined with the non-standard names)
#undef DOMAIN
#undef SING
#undef OVERFLOW
#undef UNDERFLOW
#undef TLOSS
#undef PLOSS

#endif
