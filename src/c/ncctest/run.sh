for i in `seq -w 0 30`
do
	f="b$i.x"
	if [ -f $f ]
	then
		n=`echo $i | sed 's/^0\+//'`
		./$f
		o="$?"
		if [ $o != $n ]
		then
			echo "$f	$o"
		fi
	fi
done

for x in t??.x
do
	./$x
	o=$?
	if [ "$o" != "0" ]
	then
		echo "$x	$o"
	fi
done
