#/bin/sh

killall -9 nettomation
rm -rf web/*.log
sleep 1
./nettomation --dir web --password pwd --port 8800
