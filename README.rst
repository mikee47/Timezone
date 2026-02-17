Timezone
========

.. highlight:: c++

Sming library to support local/UTC time conversions using rules

See :sample:`SystemClock_NTP` for example usage.

Port of https://github.com/JChristensen/Timezone for Sming.


Rules database
--------------

Timezones are identified by area/location. For example:

   Zone name                     Area        Location
   ----------------------------  ----------  --------
   Europe/London                 Europe      London
   America/Argentina/Mendoza     America     Argentina/Mendoza


By default, a set of rules for all the main IANA timezones is created in the applications ``out/Timezone`` directory.
This can be accessed by adding ``#include <tzdata.h>`` to **one** source file of your project::

   #include <tzdata.h>

   void foo()
   {
      // Demonstrate basic usage of a timezone definition
      TZ::Europe::London tz();
      time_t nowUtc = SystemClock.now(eTZ_UTC);
      time_t nowLocal = tz.toLocal(nowUtc);
   }

   void init()
   {
      ...

      /*
       * This sets `SystemClock` timezone so that whenever local time is queried it
       * is expressed using the selected timezone.
       * Transitions to/from daylight savings are managed transparently.
       */
      TZ::System::setTimezone(TZ::Europe::London());

      ...
   }

This is memory efficient as only the timezone information required by the application is linked into the firmware.


Lookup by name
--------------

To dynamically locate timezone information at runtime, use :cpp:func:`TZ::findZone` This will result in the full database being compiled into your application.

The compiler produces one table for each area, such as ``TZ::Europe::zones``.
These are collected in the ``TZ::areas`` map.


Customisation
-------------

Customise using the following configuration variables:

.. envvar:: APP_TZDATA_DIR

   Default: $(PROJECT_DIR)/out/Timezone

   Location for ``tzdata.cpp`` and ``tzdata.h``.
   Set to empty string if you don't want these to be built.

.. envvar:: APP_TZDATA_OPTS

   Override to customise generated data.
   Typically this is only required for testing to include transition tables.


Files are generated using a python script ``tools/compile.py``.

.. note::

   The script uses the system IANA timezone database.

   For development systems without one installed (Windows), the ``tzdata`` package is required.

   See https://docs.python.org/3/library/zoneinfo.html.


Additional information can be generated using the following options:

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

.. doxygennamespace:: TZ
   :members:
