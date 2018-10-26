import sys
import re

rx1 = re.compile(r'"([0-9a-z]+)"', re.I)

for line in sys.stdin:
    line = line.strip()
    m = rx1.search(line)
    if m == None:
        continue

    line = m.group(1)
    if len(line) % 2 == 1:
	line += '0'
    b = ['0x' + line[i*2:i*2+2] for i in range(len(line)/2)]
    print '{ %s },' % (', '.join(b))
