import struct
def vlq(v):
    o=[v&0x7F]; v>>=7
    while v: o.insert(0,(v&0x7F)|0x80); v>>=7
    return bytes(o)
def smf(ppq, evs, bpm=90, name='T'):
    evs=sorted(evs); out=b''; last=0
    out+=vlq(0)+b'\xff\x03'+vlq(len(name))+name.encode()
    out+=vlq(0)+b'\xff\x51\x03'+int(60e6/bpm).to_bytes(3,'big')
    for t,msg in evs: out+=vlq(t-last)+msg; last=t
    out+=vlq(0)+b'\xff\x2f\x00'
    return b'MThd'+struct.pack('>IHHH',6,0,1,ppq)+b'MTrk'+struct.pack('>I',len(out))+out
def note(evs,ch,t,ln,n,v): evs+= [(t,bytes([0x90|ch,n,v])),(t+ln,bytes([0x80|ch,n,0]))]
# drums: 2 bars, PPQ 480, ch10, kick36 snare38 hat42
e=[];P=480
for bar in range(2):
  b=bar*4*P
  for s in range(8): note(e,9,b+s*P//2,60,42,100 if s%2==0 else 70)
  for k in (0,2.5*P,P*0 if False else 0): pass
  for k in (0,int(1.5*P),int(2.5*P)): note(e,9,b+k,120,36,120)
  for s in (P,3*P): note(e,9,b+s,120,38,110)
open('test_drums_90bpm.mid','wb').write(smf(P,e,90,'Drums'))
# chords: PPQ 96, Cmaj7 Am7 Fmaj7 G7, one bar each
e=[];P=96
for i,ch in enumerate([(60,64,67,71),(57,60,64,67),(53,57,60,64),(55,59,62,65)]):
  for n in ch: note(e,0,i*4*P,4*P-6,n,96)
open('test_chords_80bpm.mid','wb').write(smf(P,e,80,'Keys'))
