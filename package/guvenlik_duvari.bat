@echo off
rem Yonetici olarak calistirin (sag tik > Yonetici olarak calistir). UDP 27015 gelen trafigine izin verir (yalnizca Ozel ag).
netsh advfirewall firewall add rule name="SuperCan Coop UDP 27015" dir=in action=allow protocol=UDP localport=27015 profile=private
pause
