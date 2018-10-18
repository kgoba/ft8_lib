import sys

for line in sys.stdin:
    line = line.strip()
    b = ['0x' + line[i*2:i*2+2] for i in range(len(line)/2)]
    print '{ %s },' % (', '.join(b))
