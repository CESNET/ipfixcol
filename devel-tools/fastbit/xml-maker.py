#!/usr/bin/env python
import csv

print '<?xml version="1.0" encoding="UTF-8"?>'
f = open('elements2.txt', 'r+');
print '<ipfix-elements>'
for line in f:
	reader = csv.reader([line], skipinitialspace=True)
	for r in reader:
		if len(r) < 4 :
			break
        	print '\t<element>'
                print '\t\t<enterprise>',r[0],'</enterprise>'
                print '\t\t<id>',r[1],'</id>'
		print '\t\t<name>',r[2],'</name>'
                print '\t\t<dataType>',r[3],'</dataType>'
		r4=''
		if len(r) > 4:
			r4=r[4]
		
                print '\t\t<semantic>',r4,'</semantic>'
                #<alias></alias>
        	print '\t</element>'
print '</ipfix-elements>'

