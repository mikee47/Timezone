Timezone
========

Arduino library to support local/UTC time conversions using rules

See :sample:`SystemClock_NTP` for example usage.

Port of https://github.com/JChristensen/Timezone for Sming.


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
