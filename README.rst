Timezone
========

Arduino library to support local/UTC time conversions using rules

See :sample:`SystemClock_NTP` for example usage.

Port of https://github.com/JChristensen/Timezone for Sming.


IANA database
-------------

This library has been extended to include timezone descriptions from the
`IANA database <https://www.iana.org/time-zones>`__.

The rules for timezones and daylight savings are complex historically and can change at any time.
However, embedded applications rarely require a complete, historical timezone database.
There are exceptions of course
https://www.haraldkreuzer.net/en/news/esp32-library-offline-time-zone-search-given-gps-coordinates.

More likely is the requirement to display current local time.
This sort of thing might be conveniently held in a configuration file so if the rules change
it's easy to fix.

This is one big rabbit hole which sane people should probably avoid.
But here's a few tidbits you might find useful.


Data format
-----------

The primary source files in the IANA database are:

   africa
   antarctica
   asia
   australasia
   backward
   backzone
   calendars
   etcetera
   europe
   factory
   northamerica
   southamerica

These contain lots of historical notes including rules and zone definitions.
The format is described in ``zic.8.txt``.

The ``tzdata.zi`` file contains a compact version of this same information,
excluding comments and with abbreviated rule names.
It is therefore a bit simpler to parse, can be more easily updated in the future
and is the one ``tools/tzinfo.py`` uses.


POSIX timezone strings
----------------------

These provide a compact representation of a timezone.
For example, ``Europe/London`` has a timezone string of ``GMT0BST,M3.5.0/1,M10.5.0``,
and ``Pacific/Gambier`` is ``<-09>9``.

These are the strings which C library functions use to convert between local and UTC times::

   setenv("GMT0BST,M3.5.0/1,M10.5.0");
   tzset();

Functions like ``mktime`` and ``localtime`` will then produce the expected results.

.. note::

   On a desktop computer you can pass the name of a system timezone file,
   or a string such as ":Pacific/Auckland".
   These refer to binary files found in release builds of the IANA database
   and produce accurate results for historical and future dates.

Note that I avoid these functions for two reasons:

   They're not intuitive
   They're error prone
   They can introduce unexpected code/data bloat
   I'm using C++ dammit.

It's fine for library code to use these in their implementations, of course.

IANA says this:

   In POSIX.1-2017, time display in a process is controlled by the environment variable TZ.
   Unfortunately, the POSIX.1-2017 TZ string takes a form that is hard to describe and is
   error-prone in practice.
   Also, POSIX.1-2017 TZ strings cannot deal with daylight saving time rules not based on
   the Gregorian calendar (as in Morocco), or with situations where more than two time zone
   abbreviations or UT offsets are used in an area.

J. Christensen's library is much nicer in this regard since we don't have to do
any string decoding, and it's much clearer what the rules are for conversion.

The only way to ensure embedded applications are reliably portable is to use UTC internally.
Converting that UTC timestamp to a local time is then a simple matter of adding the
appropriate offset. That c:type:`time_t` value can then get passed to :cpp:class:`DateTime`
for display, etc.


Note: For ``Morocco`` read ``Africa/Casablanca`` or ``Africa/El_Aaiun``.


POSIX timezone strings do not correspond exactly with IANA descriptions.
In particular, an IANA rule may say ``Mar Sun>=22`` which gets translated as ``March week4 Sunday``.
This looks like there could be edge cases where the two interpretations are a week apart.
Need a proof for this!


Timezone abbreviations
----------------------

Some timezones do not have formal abbreviations, for which the IANA database uses numeric timecodes.
For example, ``Pacific/Guadalcanal`` has a POSIX time string of "<+11>-11".
However, https://www.timeanddate.com/time/zone/@2108832 uses "SBT".

IANA has this to say:

   Alphabetic time zone abbreviations should not be used as unique identifiers for UT offsets as they
   are ambiguous in practice.
   For example, in English-speaking North America "CST" denotes 6 hours behind UT, but in China it
   denotes 8 hours ahead of UT, and French-speaking North Americans prefer "HNC" to "CST".
   The tz database contains English abbreviations for many timestamps; unfortunately some of these
   abbreviations were merely the database maintainers' inventions, and these have been removed when
   possible.


References
----------

- `Theory and pragmatics of the tz code and data <https://data.iana.org/time-zones/theory.html>`__
- Current text timezone database `tzdata.zi<https://data.iana.org/time-zones/data/tzdata.zi>`
- `LLVM Time Zone Support <https://libcxx.llvm.org/DesignDocs/TimeZone.html>`__


API Documentation
-----------------

.. doxygenclass:: Timezone
   :members:

.. doxygenstruct:: TimeChangeRule
   :members:

.. doxygenenum:: week_t
.. doxygenenum:: dow_t
.. doxygenenum:: month_t
