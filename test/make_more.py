import struct,random,sys
def vlq(v):
    o=[v&0x7F]; v>>=7
    while v: o.insert(0,(v&0x7F)|0x80); v>>=7
    return bytes(o)
def trk(evs,name=None,tempo=None,running=False):
    evs=sorted(evs,key=lambda e:e[0]); out=b''; last=0; st=None
    if name: out+=vlq(0)+b'\xff\x03'+vlq(len(name))+name.encode()
    if tempo: out+=vlq(0)+b'\xff\x51\x03'+int(60e6/tempo).to_bytes(3,'big')
    for t,m in evs:
        out+=vlq(t-last); last=t
        if running and m[0]==st: out+=m[1:]
        else: out+=m; st=m[0]
    out+=vlq(0)+b'\xff\x2f\x00'
    return b'MTrk'+struct.pack('>I',len(out))+out
def smf(fmt,ppq,tracks): return b'MThd'+struct.pack('>IHHH',6,fmt,len(tracks),ppq)+b''.join(tracks)
def notes(ch,lst): 
    e=[]
    for t,l,n,v in lst: e+=[(t,bytes([0x90|ch,n,v])),(t+l,bytes([0x90|ch,n,0]))]  # note-off as vel 0
    return e
P=480
bass=notes(1,[(i*P,P-10,36+(i%4)*2,100) for i in range(16)])
keys=notes(0,[(i*4*P,4*P,n,80) for i in range(4) for n in (60,64,67)])
drm=notes(9,[(i*P//2+7,60,[36,42,38,42][i%4],110) for i in range(32)])  # 7 ticks late -> quantize test
open('multi_f1.mid','wb').write(smf(1,P,[trk([],tempo=97.5,name='Tempo'),trk(keys,'Keys'),trk(bass,'Bass/Sub'),trk(drm,'Drums',running=True)]))
open('multi_f0.mid','wb').write(smf(0,P,[trk(keys+bass+drm,tempo=120,running=True)]))
# lead-in of 2 empty bars + transpose edge
open('leadin.mid','wb').write(smf(0,96,[trk(notes(0,[(8*96+5,96,120,64),(9*96,96,5,64)]))]))
d=open('multi_f1.mid','rb').read()
open('truncated.mid','wb').write(d[:len(d)//2])
open('notmidi.mid','wb').write(b'hello world')
import os; os.makedirs('fuzz', exist_ok=True)
random.seed(1)
for i in range(300):
    b=bytearray(d); 
    for _ in range(random.randint(1,20)): b[random.randrange(len(b))]=random.randrange(256)
    open(f'fuzz/f{i}.mid','wb').write(b)
