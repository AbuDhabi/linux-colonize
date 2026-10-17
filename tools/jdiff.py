import json,sys
def walk(a,b,p,out):
    if type(a)!=type(b): out.append((p,a,b)); return
    if isinstance(a,dict):
        for k in sorted(set(a)|set(b)): walk(a.get(k),b.get(k),p+'.'+k,out)
    elif isinstance(a,list):
        if len(a)!=len(b): out.append((p+'#len',len(a),len(b)))
        for i,(x,y) in enumerate(zip(a,b)): walk(x,y,'%s[%d]'%(p,i),out)
    elif isinstance(a,str) and p.endswith('_hex') and a!=b:
        ba,bb=bytes.fromhex(a.replace(' ','')),bytes.fromhex(b.replace(' ',''))
        out.append((p,[(i,ba[i],bb[i]) for i in range(min(len(ba),len(bb))) if ba[i]!=bb[i]][:12],''))
    elif a!=b: out.append((p,a,b))
out=[]; walk(json.load(open(sys.argv[1])),json.load(open(sys.argv[2])),'',out)
ign=set(sys.argv[4:]) if len(sys.argv)>4 else set()
lim=int(sys.argv[3]) if len(sys.argv)>3 else 80
print(len(out),'diffs (first, second)')
for d in out[:lim]: print(*d)
