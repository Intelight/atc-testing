echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
while [ "1" == "1" ]
do
taskset -c 0 memtester 200M &
taskset -c 1 memtester 200M &
taskset -c 2 memtester 200M &
taskset -c 3 memtester 200M
done
