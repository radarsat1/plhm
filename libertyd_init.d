#!/bin/sh
#
# chkconfig: - 91 35
# description: Starts and stops the libertyd daemon for Polhemus Liberty device
#
# pidfile: /var/run/libertyd.pid


# Source function library.
if [ -f /etc/init.d/functions ] ; then
  . /etc/init.d/functions
elif [ -f /etc/rc.d/init.d/functions ] ; then
  . /etc/rc.d/init.d/functions
else
  exit 0
fi

# Avoid using root's TMPDIR
unset TMPDIR

# Source networking configuration.
. /etc/sysconfig/network

if [ -f /etc/sysconfig/samba ]; then
   . /etc/sysconfig/samba
fi

# Check that networking is up.
[ ${NETWORKING} = "no" ] && exit 0

RETVAL=0


start() {
	echo -n $"Starting libertyd service: "
	daemon libertyd $SMBDOPTIONS
	RETVAL=$?
	echo
	[ $RETVAL -eq 0 ] && touch /var/lock/subsys/libertyd || \
	   RETVAL=1
	return $RETVAL
}	

stop() {
        KIND="SMB"
	echo -n $"Shutting down libertyd service: "
	killproc libertyd
	RETVAL=$?
	[ $RETVAL -eq 0 ] && rm -f /var/lock/subsys/libertyd
	echo ""
	return $RETVAL
}	

restart() {
	stop
	start
}	

reload() {
        echo -n $"Killing libertyd: "
	killproc libertyd -HUP
	RETVAL=$?
	echo
	return $RETVAL
}	

rhstatus() {
	status libertyd
}	


# Allow status as non-root.
if [ "$1" = status ]; then
       rhstatus
       exit $?
fi

case "$1" in
  start)
  	start
	;;
  stop)
  	stop
	;;
  restart)
  	restart
	;;
  reload)
  	reload
	;;
  status)
  	rhstatus
	;;
  condrestart)
  	[ -f /var/lock/subsys/libertyd ] && restart || :
	;;
  *)
	echo $"Usage: $0 {start|stop|restart|reload|status|condrestart}"
	exit 1
esac

exit $?
