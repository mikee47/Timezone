Timezone
========

Sming library to support local/UTC time conversions using rules

See :sample:`SystemClock_NTP` for example usage.

Port of https://github.com/JChristensen/Timezone for Sming.


Rules database
--------------

By default, a set of rules for all the main IANA timezones is created in the applications ``out/Timezone`` directory.
This can be access via ``#include <tzdata.h>``.

Customise using the following configuration variables:

.. envvar:: APP_TZDATA_DIR

   Default: $(PROJECT_DIR)/out/Timezone

   Location for ``tzdata.cpp`` and ``tzdata.h``.
   Set to empty string if you don't want these to be built.

.. envvar:: APP_TZDATA_OPTS

   Override to customise generated data.
   Typically this is only required for testing to include transition tables.


These files are generated using a python script ``tools/compile.py``.

.. note::

   The script uses the system IANA timezone database.

   For development systems without one installed (Windows), the ``tzdata`` package is required.

   See https://docs.python.org/3/library/zoneinfo.html.


Advanced usage
--------------

You can use the tool to lookup and generate the appropriate rules for a timezone, by name.

.. note::

   You can use ``tzselect`` to locate the appropriate timezone names.

If you only need a few timezone definitions then this may suffice::

   python tools/compile.py Europe/London

will produce the following output:

.. code-block:: c++

   /*
   * Source:  /usr/share/zoneinfo
   * Version: 2024a
   */

   namespace London {
   DEFINE_FSTR(location, "London")
   constexpr const TimeChangeRule std PROGMEM {"GMT", Last, Sun, Oct, 2, 0};
   constexpr const TimeChangeRule dst PROGMEM {"BST", Last, Sun, Mar, 1, 60};
   constexpr const Info info PROGMEM {std, dst};
   }

If you want the whole database::

   python tools/posix.py --all --output files

This will create ``tzdata.h`` and ``tzdata.cpp`` in the ``files`` directory (which must exist).
The files contain a lookup table (``TZ::areas``) which can be used to lookup timezones by name.
The library ``test`` application uses this during build - see the ``component.mk`` file.

Timezones are identified by area/location. For example:

   Zone name                     Area        Location
   Europe/London                 Europe      London
   America/Argentina/Mendoza     America     Argentina/Mendoza

The compiler produces one table for each area, such as ``TZ::Europe::zones``.
These are collected in the ``TZ::areas`` map.

For ease of use, you can use this data directly::

   #include "tzdata.h"

   void foo()
   {
      Timezone tz(TZ::Europe::London::info);
      time_t nowUtc = SystemClock.now(eTZ_UTC);
      time_t nowLocal = tz.toLocal(now);
   }

Additional information can be included in the generated ``Info`` structure using the following options:

- --names Include the zone area and location
- --tzstr Include the POSIX timezone strings
- --transitions Include transition times

See the test application for example usage.

The database can be output with vary levels of verbosity, depending on requirements.
Compiled for esp8266 gives these results: there are 488 zones in the source data::

   Option         size        increase    
                  irom0_attr  total    per zone
   ============== =========== =========================
   empty table	   74984
   just rules 	   91668       16684    34.2
   --name 		   95588       3920     8.03
   --tzstr 		   99408       3820     7.83
   --transitions  107252      7844     16.07


.. note::

   The IANA timezone database is updated regularly.
   Many zones are stable and the POSIX strings do not change.

   Applications should generally provide a mechanism for updating rules as required,
   for example using an on-disk database.
   This is beyond the scope of this library.


Testing
-------

The test application in this library builds the timezone table as describe above
and identifies the transition times to/from daylight savings for each zone.

Most zones do not use daylight savings and this is also checked for.
In this case the DST rule is just a reference to the STD rule.

Only conversions from UTC to local time are checked: it's impossible to go the other way
reliably since when jumping forward there's a gap where local time isn't valid,
and when jumping back there's a repeated hour.

This UTC-to-local conversion is checked against the standard C library routines which
interpret the POSIX strings directly.

.. note::

   At time of writing (May 2024) this check fails for several timezones when tested
   on an ESP8266 using gcc 10.2. The zones are:

      America/Godthab         <-02>2<-01>,M3.5.0/-1,M10.5.0/0
      America/Nuuk            <-02>2<-01>,M3.5.0/-1,M10.5.0/0
      America/Scoresbysund    <-02>2<-01>,M3.5.0/-1,M10.5.0/0

   These are the only zones with a negative TIME component, so clearly the newlib
   implementation cannot handle it. However, the glibc version can (as well as this library).

A second check is made using the timezone name itself (e.g. ``Europe/London``).
This is the "most correct" result available since it uses the full IANA database
information which cannot be expressed by a POSIX timezone string.
These are highlighted in the output for information purposes.


2024 Revision
-------------

Sming support for dates and times is via the :cpp:class:`DateTime` and :cpp:class:`SystemClockClass` classes.

Applications may need support for local time for example when updating integrated displays.
The system clock is maintained in UTC, but can provide local time provided the appropriate offset is
set via :cpp:func:`SystemClockClass::setTimeZoneOffset`.

.. note::
   
   Applications with web interfaces would normally deal with local time conversions in script.
   Web browsers have access to full time zone information e.g. via local IANA database (see below).

The clock itself can best be kept correct using NTP.
However, keeping the timezone offset correct is another matter.

The standard C library provides support for time conversions via POSIX strings,
but using those from C++ code is clunky and error-prone.

Also, `rules change <https://www.timeanddate.com/time/europe/eu-dst.html>`__.
Not often, but nowadays users expect systems to take care of this stuff itself.
But it can be complicated!
The easiest solution is just to let the user change the time (offset) manually if needed - always a good idea.

This library aims to improve the handling of timezones in the following ways:

-  Supporting setting the local timezone using a local identifier, such as ``TZ::Europe::London``.
-  Integrate with the standard C library calls to accommodate any existing code which may use them.
-  Ensure local time is always correct when queried via :cpp:func:`SystemClockClass::now()`,
   taking care of daylight savings changes for the selected timezone.
-  Allowing more comprehensive timezone support using a compact timezone database.
   This allows applications to provide menus for selecting timezones, etc.
   Note that this could also be accomplished by retrieving the database information from remote servers
   since it's generally only required during configuration or setup.
-  Support automatic updates to tiemzone rules. IANA publishes these in advance of the changes
   so user intervention should not be required.


Storage and updates
-------------------

Hard-coded TZ data is pretty straightforward, useful for simple systems deployed to limited areas.

The full database is best kept on a filing system as it's infrequently accessed.

The core data can all be stored in FWFS: readonly and compact.
Any changes can be stored in a volatile filesystem, directly or as a 'package' in an FWFS image.

There does not appear to be any standard way for propagating these changes.
GNU/Linux, Android, Windows, etc. all have their own mechanisms.
The root data source for these is the IANA database.

.. note::

   The current timezone database is always available via the IANA, but only in source form.
   The most compact source for this is 'tzdata.zi' but requires parsing.

   Compiled versions are more suitable. Here are some sources:

   https://github.com/python/tzdata/tree/master/src/tzdata/zoneinfo
   https://github.com/python/tzdata/raw/master/src/tzdata/zoneinfo/Europe/London

After selecting the timezone(s) of interest, the appropriate data can be fetched and used to determine
the appropriate timezone offsets and when the next changes occur.
When that change happens, the source can be re-queried.


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
~~~~~~~~~~~

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

Whilst it's fine for library code to use these in their implementations, it's something applications
should avoid:

   - They're not intuitive
   - They're error prone
   - They can introduce unexpected code/data bloat
   - By themselves, they don't take account of rule changes (you'd need multiple strings)
   - I'm using C++ dammit.


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


Useful commands
---------------

List transitions within a range of years::

   zdump -i -c 2024,2040 Europe/London
   zdump -V -c 2024,2040 Europe/London

Generate binary rule data in compact form, up to year 2040::

   zic -r @$(date +%s -d 2024-01-01)/@$(date +%s -d 2040-01-01) -b slim /usr/share/zoneinfo/tzdata.zi -d tmp

Output is in 'tmp'.
This is still more bloaty than we'd like.
For example, ``Europe/London`` has only one rule yet the tzdata is 740 bytes.
The corresponding POSIX rule is 'GMT0BST,M3.5.0/1,M10.5.0'.


Testing
-------

As this is all jolly complicated testing is important.
We certainly want to make sure that transitions are correct and correspond with those reported by
development system libraries.



References
----------

- `Theory and pragmatics of the tz code and data <https://data.iana.org/time-zones/theory.html>`__
- Current text timezone database `tzdata.zi<https://data.iana.org/time-zones/data/tzdata.zi>`
- `LLVM Time Zone Support <https://libcxx.llvm.org/DesignDocs/TimeZone.html>`__
- `PEP 495 – Local Time Disambiguation <https://peps.python.org/pep-0495/>`__


API Documentation
-----------------

.. doxygennamespace:: TZ
   :members:
