import os
import re
root = 'template'
include_re = re.compile(r'^\s*#\s*include\s+["<]([^">]+)[">]')
headers = set()
for dirpath, _, files in os.walk(root):
    for f in files:
        if f.endswith(('.h','.hpp','.hxx','.c','.cpp','.cc','.inl','.ipp','.mm','.m','.cxx')):
            path = os.path.join(dirpath, f)
            try:
                with open(path, 'r', encoding='utf-8', errors='ignore') as fh:
                    for line in fh:
                        m = include_re.match(line)
                        if m:
                            headers.add(m.group(1))
            except Exception as e:
                print('Error reading {}: {}'.format(path, e))
headers = sorted(headers)
for h in headers:
    print(h)
print('\nTotal:', len(headers))
