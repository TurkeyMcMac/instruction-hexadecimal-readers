testdir=tests

is_tty() {
	[ -t 1 ]
}

style_print() {
	style=$1
	shift
	message=$*
	if is_tty; then
		printf "\e[${style}m${message}\e[0m\n"
	else
		printf "${message}\n"
	fi
}

files_of_type() {
	ls -1 | grep "\\.$1$"
}

run_test() {
	testfile=$1
	log="${testfile}.log"
	style_print 31 "$testfile failure:" > $log
	./$testfile 2>>$log >/dev/null
	if [ $? -eq 0 ]; then
		style_print 32 $testfile succeeded.
		rm $log
	else
		style_print 31 $testfile failed.
	fi
}

cd $testdir

testfiles=`files_of_type o`

for testfile in $testfiles; do
	run_test $testfile &
done
wait

logs=`files_of_type log`
if [ "$logs" ]; then
	style_print "31;7" Some tests failed.
	for log in $logs; do
		cat $log
	done
	exit 1
else
	style_print "32;7" All tests passed.
	exit 0
fi
