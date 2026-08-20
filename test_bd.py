import sys
with open('original_sources/SOUND/JINE.XMI', 'rb') as f:
    data = f.read()
print("0xBD count:", data.count(b'\xbd'))
print("0xFE count:", data.count(b'\xfe'))
print("0xF6 count:", data.count(b'\xf6'))
