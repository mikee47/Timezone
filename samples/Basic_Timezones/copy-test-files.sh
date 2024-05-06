#!/bin/bash

tzdb_path="/stripe/sandboxes/tzdata/tzdb-2024a"

function copy()
{
    grep -ve '^\s*#' -e '^$' "$tzdb_path/$1" > "files/$1"
}

files=(
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
    iso3166.tab
    leapseconds
    leap-seconds.list
    southamerica
    to2050.tzs
    tzdata.zi
    version
    zone1970.tab
    zonenow.tab
    zone.tab
)

mkdir -p files
for file in "${files[@]}"; do
    copy "$file"
done
