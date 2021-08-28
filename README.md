# fwifiscan
Free WIFI Scanner

This simple free WIFI scanner for Linux detects unsecured networks, outputs comma separated values, and produces beeps - three beeps will be produced when three or more networks are detected. Output currently includes ESSID, UNIX time, and longitude and latitude if a GPS device is available. Program can accomodate an ESSID blacklist file. Great program for criminals, and filling out free WIFI geospatial databases.

TODO:
- change blacklist loading to linked list instead of counting commas
- make it so there's only one sound file
- add access point IDs (really useful if there's lots with the same ESSID)
