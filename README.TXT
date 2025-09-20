This is a small utility developed for the Nintendo Wii which will synchronize the system time to the NTP time.

There are two options on how to use this utility:
1. Manual adjustment using the wii mote or gamecube controller.
Timezones are not remembered and will have to be adjusted manually by the user.

2. Configuration using a static offset:
Create an sntp.cfg in the same directory as the sntp application on the SD card.

Populate it with the following lines:

offset=OFFSET_SECONDS
autosave=1

Replace OFFSET_SECONDS with your UTC/GMT offset in seconds.
e.g. if your UTC offset is -4:30, OFFSET_SECONDS is -16200

3. Configuration using a timezone configuration file:
Create an sntp.cfg in the same directory as the sntp application on the SD card.

Populate it with the following lines:

tzdb_url=http://api.timezonedb.com/v2.1/get-time-zone?key=MY_API_KEY&format=json&by=zone&zone=MY_TIME_ZONE&fields=gmtOffset
autosave=1

Register an account at timezonedb.com
Change the MY_API_KEY to the api key you will receive.
Change the timezone from the list available at: https://timezonedb.com/time-zones

If you want the utility to exit automatically after adjustment keep the autosave, otherwise remove.

Implementation details:
Timekeeping is done by reading the (ever incrementing) Real Time Clock (RTC) and a counter bias stored in a system configuration.
If you adjust your time in the wii menu or by using this program the counter bias is changed.
There is always a risk that updating this counter bias could corrupt your system as it is part of the system configuration.

I shall not be held responsible if this would happen.
I have of course tested this on my Wii system and it works for me.

Happy hacking!
