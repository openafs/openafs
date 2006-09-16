#!/bin/sh

PODSRC=/scratch/chas/openafs/doc/man-pages
SECTS='1 5 8'

for sect in ${SECTS}
do
	#rm -f head${sect}.xml
	#rm -f body${sect}.xml

	if [ ! -d sect${sect} ]; then
		mkdir sect${sect}
	fi

	for pod in ${PODSRC}/pod${sect}/*.pod
	do
		base=`basename $pod`
		xml=`echo $base | sed -e 's/\.pod/.xml/'`
		echo ./pod2refentry -section=$sect $pod sect${sect}/${xml}
		./pod2refentry -section=$sect $pod sect${sect}/${xml}

		#tag=`echo $xml | sed -e 's/\.xml//'`
		#echo "<!ENTITY ${tag}${sect} SYSTEM \"sect${sect}/$xml\">" >> head${sect}.xml
		#echo "    &${tag}${sect};" >> body${sect}.xml
	done
done
