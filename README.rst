Timezone
========

Sming library to support local/UTC time conversions using rules

See :sample:`SystemClock_NTP` for example usage.

Port of https://github.com/JChristensen/Timezone for Sming.


Rules database
--------------

By default, a set of rules for all the main IANA timezones is created in the applications ``out/Timezone`` directory.
This can be access via ``#include <tzdata.h>``.

Customise using the following build variables:

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


Further information:

- `Theory and pragmatics of the tz code and data<https://data.iana.org/time-zones/tzdb/theory.html>`__


API Documentation
-----------------

.. doxygenclass:: Timezone
   :members:

.. doxygenstruct:: TimeChangeRule
   :members:

.. doxygenenum:: week_t
.. doxygenenum:: dow_t
.. doxygenenum:: month_t
