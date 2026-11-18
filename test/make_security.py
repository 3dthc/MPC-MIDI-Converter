# Crafted files for the security tests (run in the test's scratch folder).
import struct
def vlq(v):
    o=[v&0x7F]; v>>=7
    while v: o.insert(0,(v&0x7F)|0x80); v>>=7
    return bytes(o)
def trk(body): return b'MTrk'+struct.pack('>I',len(body))+body
note=lambda d,n: vlq(d)+bytes([0x90,n,100])+vlq(10)+bytes([0x80,n,0])
end=vlq(0)+b'\xff\x2f\x00'
ok=trk(note(0,60)+end)
# header length that wraps a 32-bit long (the MPC's ARM): 0xFFFFFFF0
open('hdr_huge.mid','wb').write(b'MThd'+struct.pack('>I',0xFFFFFFF0)+struct.pack('>HHH',0,1,96)+ok)
# chunk length 0xFFFFFFFF on the first track, a good one after
open('chunk_huge.mid','wb').write(b'MThd'+struct.pack('>IHHH',6,1,2,96)+b'MTrk'+struct.pack('>I',0xFFFFFFFF)+note(0,60)+end)
# delta times that pass 2^31 ticks in total
big=b''.join(vlq(0x0FFFFFFF)+bytes([0x90,60,100])+vlq(1)+bytes([0x80,60,0]) for _ in range(20))
open('long_song.mid','wb').write(b'MThd'+struct.pack('>IHHH',6,0,1,96)+trk(big+end))
# two tracks with the same name, split
named=lambda nm,n: trk(vlq(0)+b'\xff\x03'+vlq(len(nm))+nm+note(0,n)+end)
open('same_names.mid','wb').write(b'MThd'+struct.pack('>IHHH',6,1,2,96)+named(b'Keys',60)+named(b'Keys',64))
# 500000 notes: over the event cap
many=(bytes([0x00,0x90,60,100,0x00,0x80,60,0]))*500000
open('too_many.mid','wb').write(b'MThd'+struct.pack('>IHHH',6,0,1,96)+trk(many+end))
