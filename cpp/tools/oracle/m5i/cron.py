"""m5i cron (m5i-plan.md Z-04): Quartz cron expressions as the Java server reads them, written from Quartz's documented semantics
(org.quartz.CronExpression, the 2.x javadoc) and the Java callers - never from the C++ port's services/cron/CronExpression.cpp, so a gate that
compares the server's schedule with this module tests the port.

Supported: the six or seven fields `seconds minutes hours day-of-month month day-of-week [year]`; in each field `*`, a value, a list `a,b`, a
range `a-b`, a step `x/y`, `*/y` or `a-b/y`; month names JAN-DEC and day names SUN-SAT (case-insensitive; Quartz numbers days 1 = SUN to
7 = SAT); `?` in exactly one of day-of-month and day-of-week (Quartz refuses both or neither). Anything else - `L`, `W`, `#`, `C`, a value out of
range - raises OracleError: the oracle models only what the shipped schedules use, and an unknown shape must stop a gate, not mislead it.

`next_fire_times(expr, tz, after, count)` answers the next instants strictly after `after` (Quartz getTimeAfter), computed on the wall clock
of `tz` (UTC and fixed offsets built in, other names through zoneinfo). Wall times that do not exist in a DST gap are skipped and ambiguous ones take the first occurrence; the M5i gates run in
UTC (D13), where neither happens.

`preparation_cron(siege_time, fortress_id)` is SiegeService.getPreparationCronString (SiegeService.java:697-716): minutes minus 5 (minus 10 for a
Panesterra fortress, id >= 10000), borrowing an hour; a preparation over midnight raises OracleError, as Java throws.
"""
from __future__ import annotations

import datetime as dt
import re

from staticdata_oracle import OracleError

MONTHS = {name: i + 1 for i, name in enumerate(('JAN', 'FEB', 'MAR', 'APR', 'MAY', 'JUN', 'JUL', 'AUG', 'SEP', 'OCT', 'NOV', 'DEC'))}
DAYS = {name: i + 1 for i, name in enumerate(('SUN', 'MON', 'TUE', 'WED', 'THU', 'FRI', 'SAT'))}
# (name, minimum, maximum, names) per field; Quartz's year field accepts 1970-2099 in 2.x
FIELDS = (('seconds', 0, 59, None), ('minutes', 0, 59, None), ('hours', 0, 23, None), ('day-of-month', 1, 31, None), ('month', 1, 12, MONTHS),
          ('day-of-week', 1, 7, DAYS), ('year', 1970, 2299, None))
YEAR_LIMIT = 2299


def _value(token: str, field: tuple, expr: str) -> int:
    name, low, high, names = field
    if names and token.upper() in names:
        return names[token.upper()]
    if not token.isdigit():
        raise OracleError(f"cron {expr!r}: {name} {token!r} is not a shape the oracle models")
    value = int(token)
    if value < low or value > high:
        raise OracleError(f"cron {expr!r}: {name} value {value} outside {low}-{high}")
    return value


def _field(text: str, field: tuple, expr: str) -> set[int] | None:
    """the set of values, or None for `?`"""
    name, low, high, _ = field
    if text == '?':
        if name not in ('day-of-month', 'day-of-week'):
            raise OracleError(f"cron {expr!r}: '?' is only allowed in day-of-month and day-of-week")
        return None
    values: set[int] = set()
    for part in text.split(','):
        step = 1
        stepped = '/' in part
        if stepped:
            part, step_text = part.split('/', 1)
            if not step_text.isdigit() or int(step_text) == 0:
                raise OracleError(f"cron {expr!r}: {name} step {step_text!r}")
            step = int(step_text)
            if part == '':
                raise OracleError(f"cron {expr!r}: {name} step without a start")
        if part == '*':
            start, end = low, high
        elif '-' in part:
            a, b = part.split('-', 1)
            start, end = _value(a, field, expr), _value(b, field, expr)
            if end < start:
                raise OracleError(f"cron {expr!r}: {name} range {part!r} wraps, which the oracle does not model")
        else:
            start = _value(part, field, expr)
            # Quartz: "x/y" runs from x to the field's maximum; a plain value is itself
            end = high if stepped else start
        values.update(range(start, end + 1, step))
    return values


def zone(tz: str) -> dt.tzinfo:
    """'UTC' (GMT, Z) and fixed offsets like +02:00 resolve without a time zone database (Windows ships none for zoneinfo, as m5f/travel.py's
    Zone says); any other name needs zoneinfo's database"""
    if tz in ('UTC', 'GMT', 'Z'):
        return dt.timezone.utc
    m = re.fullmatch(r'(?:UTC|GMT)?([+-])(\d{1,2})(?::?(\d{2}))?', tz)
    if m:
        sign = 1 if m.group(1) == '+' else -1
        return dt.timezone(sign * dt.timedelta(hours=int(m.group(2)), minutes=int(m.group(3) or 0)))
    try:
        import zoneinfo
        return zoneinfo.ZoneInfo(tz)
    except Exception as e:  # ZoneInfoNotFoundError, or no tz database on this machine
        raise OracleError(f"--tz {tz}: not a zone this Python can resolve ({e}); use 'UTC' or a fixed offset like +02:00") from e


class Cron:
    def __init__(self, expr: str):
        parts = expr.split()
        if len(parts) not in (6, 7):
            raise OracleError(f"cron {expr!r}: {len(parts)} fields, Quartz takes 6 or 7")
        self.expr = expr
        self.seconds, self.minutes, self.hours, self.dom, self.months, self.dow = (_field(p, f, expr) for p, f in zip(parts[:6], FIELDS[:6]))
        self.years = _field(parts[6], FIELDS[6], expr) if len(parts) == 7 else None
        if (self.dom is None) == (self.dow is None):
            raise OracleError(f"cron {expr!r}: exactly one of day-of-month and day-of-week must be '?' (Quartz)")

    def _day_matches(self, day: dt.date) -> bool:
        if self.years is not None and day.year not in self.years:
            return False
        if day.month not in self.months:
            return False
        if self.dom is not None:
            return day.day in self.dom
        quartz_dow = (day.isoweekday() % 7) + 1  # Monday 1 -> 2, Sunday 7 -> 1
        return quartz_dow in self.dow

    def next_fire_times(self, tz: str, after: dt.datetime, count: int = 1) -> list[dt.datetime]:
        if after.tzinfo is None:
            raise OracleError("cron: `after` needs a time zone")
        tzinfo = zone(tz)
        local_after = after.astimezone(tzinfo)
        times = sorted((h, m, s) for h in self.hours for m in self.minutes for s in self.seconds)
        found: list[dt.datetime] = []
        day = local_after.date()
        while len(found) < count and day.year <= YEAR_LIMIT:
            if self._day_matches(day):
                for h, m, s in times:
                    candidate = dt.datetime(day.year, day.month, day.day, h, m, s, tzinfo=tzinfo)
                    # a wall time in a DST gap does not exist: zoneinfo shifts it, the round trip shows it
                    if candidate.astimezone(dt.timezone.utc).astimezone(tzinfo).replace(tzinfo=None) != candidate.replace(tzinfo=None):
                        continue
                    if candidate > local_after:
                        found.append(candidate)
                        if len(found) == count:
                            break
            day += dt.timedelta(days=1)
        return found


def next_fire_times(expr: str, tz: str, after: dt.datetime, count: int = 1) -> list[dt.datetime]:
    return Cron(expr).next_fire_times(tz, after, count)


def preparation_cron(siege_time: str, fortress_id: int) -> str:
    """SiegeService.getPreparationCronString (SiegeService.java:697-716): the siege time minus the preparation (5 or 10 minutes)"""
    parts = siege_time.split(' ')
    try:
        minutes, hours = int(parts[1]), int(parts[2])  # Java: Byte.parseByte (a list or a range is a NumberFormatException)
    except (ValueError, IndexError) as e:
        raise OracleError(f"Failed converting cron expression: {siege_time}") from e
    minutes -= 5 if fortress_id < 10000 else 10
    if minutes < 0:
        minutes += 60
        hours -= 1
        if hours < 0:
            raise OracleError(f"Failed converting cron expression: {siege_time}\nPreparation over midnight not supported.")
    parts[1] = str(minutes)
    parts[2] = str(hours)
    return ' '.join(parts)


def quiet_window(exprs: list[str], tz: str, after: dt.datetime, run: dt.timedelta, spare: dt.timedelta,
                 horizon: dt.timedelta = dt.timedelta(days=8)) -> dt.datetime:
    """m5i-plan.md G-04 (D13): the earliest start at or after `after` such that no job of `exprs` fires within [start - spare, start + run + spare]
    - the gate waits for it, so no cron fires during a run. Raises OracleError when no such window starts within `horizon`."""
    fires: list[dt.datetime] = []
    end = after + horizon + run + spare
    for expr in exprs:
        cron = Cron(expr)
        cursor = after - spare
        while True:
            nxt = cron.next_fire_times(tz, cursor, 1)
            if not nxt or nxt[0] > end:
                break
            fires.append(nxt[0].astimezone(dt.timezone.utc))
            cursor = nxt[0]
    fires.sort()
    start = after
    for fire in fires:
        if fire < start - spare:
            continue
        if fire <= start + run + spare:
            start = fire + spare + dt.timedelta(seconds=1)  # the window opens after this job (and its spare)
        else:
            break
    if start > after + horizon:
        raise OracleError(f"no {run} window within {horizon} after {after.isoformat()}")
    return start
