AC_DEFUN(ah_HAVE_GETCONF,
	[AC_ARG_WITH(getconf,
		[  --with-getconf          Enable automagical buildtime configuration],
		[if test "$withval" = "yes"; then
			AC_PATH_PROG(GETCONF, getconf)
		elif test ! "$withval" = "no";then 
			AC_MSG_CHECKING([getconf])
			AC_MSG_RESULT([$withval])
			GETCONF=$withval
		fi],
		[AC_PATH_PROG(GETCONF, getconf)]
	)
	AC_SUBST(GETCONF)
])

AC_DEFUN(ah_NUM_CPUS,
	[AC_REQUIRE([ah_HAVE_GETCONF])
	AC_MSG_CHECKING([number of cpus])
	AC_ARG_WITH(cpus,
		[  --with-cpus             The number of cpus to be used for building(see --with-procs, default 1)],
		[if test "$withval" = "yes"; then
			if test "$GETCONF";then
				NUM_CPUS=`$GETCONF _NPROCESSORS_ONLN 2>/dev/null`
			else
				NUM_CPUS=1
			fi
		elif test ! "$withval" = "no";then
			NUM_CPUS=$withval
		fi],
		[if test "$GETCONF";then
			NUM_CPUS=`$GETCONF _NPROCESSORS_ONLN 2>/dev/null`
		else
			NUM_CPUS=1
		fi]
	)
	if test $NUM_CPUS = 1 ;then
		default_PROC_MULTIPLY=1
	else
		default_PROC_MULTIPLY=2
	fi
	AC_MSG_RESULT([$NUM_CPUS])
	AC_SUBST(NUM_CPUS)
])
AC_DEFUN(ah_PROC_MULTIPLY,
	[AC_REQUIRE([ah_NUM_CPUS])
	AC_MSG_CHECKING([processor multiplier])
	AC_ARG_WITH(proc-multiply,
		[  --with-proc-multiply    Multiply this * number of cpus for parallel making(default 2).],
		[if test "$withval" = "yes"; then
			PROC_MULTIPLY=$default_PROC_MULTIPLY
		elif test ! "$withval" = "no";then
			PROC_MULTIPLY=$withval
		fi],
		[PROC_MULTIPLY=$default_PROC_MULTIPLY]
	)
	AC_MSG_RESULT([$PROC_MULTIPLY])
	AC_SUBST(PROC_MULTIPLY)
])

AC_DEFUN(ah_NUM_PROCS,
	[AC_REQUIRE([ah_PROC_MULTIPLY])
	AC_REQUIRE([ah_NUM_CPUS])
	AC_MSG_CHECKING([number of processes to run during make])
	AC_ARG_WITH(procs,
		[  --with-procs            The number of processes to run in parallel during make(num_cpus * multiplier).],
		[if test "$withval" = "yes"; then
			NUM_PROCS=`expr $NUM_CPUS \* $PROC_MULTIPLY`
		elif test ! "$withval" = "no";then
			NUM_PROCS=$withval
		fi],
		[NUM_PROCS=`expr $NUM_CPUS \* $PROC_MULTIPLY`]
	)
	AC_MSG_RESULT([$NUM_PROCS])
	AC_SUBST(NUM_PROCS)
])
