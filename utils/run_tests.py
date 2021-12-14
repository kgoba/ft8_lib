#!/usr/bin/env python3

import sys, os, subprocess

def parse(line):
    fields = line.strip().split()
    freq = fields[3]
    dest = fields[5] if len(fields) > 5 else ''
    source = fields[6] if len(fields) > 6 else ''
    report = fields[7] if len(fields) > 7 else ''
    if dest and dest[0] == '<' and dest[-1] == '>':
        dest = '<...>'
    if source and source[0] == '<' and source[-1] == '>':
        source = '<...>'
    return ' '.join([dest, source, report])

wav_dir = sys.argv[1]
wav_files = [os.path.join(wav_dir, f) for f in os.listdir(wav_dir)]
wav_files = [f for f in wav_files if os.path.isfile(f) and os.path.splitext(f)[1] == '.wav']
txt_files = [os.path.splitext(f)[0] + '.txt' for f in wav_files]

is_ft4 = False
if len(sys.argv) > 2 and sys.argv[2] == '-ft4':
    is_ft4 = True

n_extra = 0
n_missed = 0
n_total = 0
for wav_file, txt_file in zip(wav_files, txt_files):
    if not os.path.isfile(txt_file): continue
    print(wav_file)
    cmd_args = ['./decode_ft8', wav_file]
    if is_ft4:
        cmd_args.append('-ft4')
    result = subprocess.run(cmd_args, stdout=subprocess.PIPE)
    result = result.stdout.decode('utf-8').split('\n')
    result = [parse(x) for x in result if len(x) > 0]
    #print(result[0])
    result = set(result)
    
    expected = open(txt_file).read().split('\n')
    expected = [parse(x) for x in expected if len(x) > 0]
    #print(expected[0])
    expected = set(expected)
    
    extra_decodes = result - expected
    missed_decodes = expected - result
    print(len(result), '/', len(expected))
    if len(extra_decodes) > 0:
        print('Extra decodes: ', list(extra_decodes))
    if len(missed_decodes) > 0:
        print('Missed decodes: ', list(missed_decodes))
    
    n_total += len(expected)
    n_extra += len(extra_decodes)
    n_missed += len(missed_decodes)
    
    #break

print('Total: %d, extra: %d (%.1f%%), missed: %d (%.1f%%)' % 
        (n_total, n_extra, 100.0*n_extra/n_total, n_missed, 100.0*n_missed/n_total))
recall = (n_total - n_missed) / float(n_total)
print('Recall: %.1f%%' % (100*recall, ))
