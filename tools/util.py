
'Counts the number of bytes needed to allocate a string plus alignment padding'
def alignstrlen(s, align = 8):
    if isinstance(s, str):
        s = s.encode()
    l = len(s) + 1 # take into account the null termination byte
    return l + (align - l % align)

def str2rgb(s):
    if s[0] == '#':
        s = s[1:]
    return int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16)