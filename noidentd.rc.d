#!/bin/bash

. /etc/rc.conf
. /etc/rc.d/functions

name=noidentd
. /etc/conf.d/noidentd
PID=$(pidof -o %PPID /usr/sbin/noidentd)

case $1 in
    start)
        stat_busy "Starting $name daemon"
        [[ -z $PID ]] && /usr/sbin/noidentd $NOIDENTD_ARGS & disown
        sleep 1
        PID=$(pidof -o %PPID /usr/sbin/noidentd)
        if [[ -z $PID ]]; then
            stat_fail
            exit 1
        else
            add_daemon $name
            stat_done
        fi
        ;;
    stop)
        stat_busy "Stopping $name daemon"
        [[ -n $PID ]] && kill $PID &>/dev/null
        if [[ $? != 0 ]]; then
            stat_fail
            exit 1
        else
            rm_daemon $name
            stat_done
        fi
        ;;
    restart)
        $0 stop
        sleep 1
        $0 start
        ;;
    *)
        echo "usage: $0 {start|stop|restart}"
        ;;
esac
exit 0
