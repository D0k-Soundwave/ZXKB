# Keycap data (KAM Wraith ZX Spectrum caps on Keychron Q3 ISO) and the SVG keyboard renderer used by gen_keyboard_docs.py
import html
U=72; G=5; e=html.escape
GREY=('#5f7690','#4a5e75'); F58=('#7d97b4','#647d99'); BLACK=('#2b2f35','#1b1e22')
WH='#f2f5f8'; RD='#ff5a4a'; GN='#7be07f'; BL='#59a8ff'; MG='#ff6fd8'; YL='#ffe94d'
# id,x,y,w,colour,main,sub,keyword,(main colour)
def k(i,x,y,w,c,m='',s='',kw='',mc=WH,sc=RD): return dict(i=i,x=x,y=y,w=w,c=c,m=m,s=s,kw=kw,mc=mc,sc=sc)
K=[k('esc',0,0,1,GREY,'ESC','EJECT')]
for n,x in enumerate([2,3,4,5,6.5,7.5,8.5,9.5,11,12,13,14]): K.append(k(f'f{n+1}',x,0,1,F58 if 4<=n<=7 else GREY,f'F{n+1}'))
K+= [k('prt',16.25,0,1,GREY,'PRINT'),k('scr',17.25,0,1,GREY,'LOCK'),k('pau',18.25,0,1,GREY,'PAUSE'),k('grv',0,1.25,1,GREY,'`','~')]
for n,(d,s) in enumerate(zip('1234567890','!@#$%^&*()')): K.append(k(f'n{d}',1+n,1.25,1,BLACK,d,s))
K+= [k('min',11,1.25,1,BLACK,'-','_'),k('eq',12,1.25,1,BLACK,'=','+'),k('bks',13,1.25,2,GREY,'DELETE','FORMAT'),
k('ins',16.25,1.25,1,BLACK,'BLUE','','EDIT',BL),k('hom',17.25,1.25,1,BLACK,'RED','','LOCK',RD),k('pgu',18.25,1.25,1,BLACK,'MAG','','TR.VID',MG),
k('tab',0,2.25,1.5,GREY,'TAB','','',GN)]
kw2=dict(Q='PLOT',W='DRAW',E='REM',R='RUN',T='RAND',Y='RETURN',U='IF',I='INPUT',O='POKE',P='PRINT',A='NEW',S='SAVE',D='DIM',F='FOR',G='GOTO',H='GOSUB',J='LOAD',K='LIST',L='LET',Z='COPY',X='CLEAR',C='COUNT',V='CLS',B='BORDER',N='NEXT',M='PAUSE')
for n,ch in enumerate('QWERTYUIOP'): K.append(k(ch.lower(),1.5+n,2.25,1,GREY if ch=='W' else BLACK,ch,'',kw2[ch]))
K+= [k('lbr',11.5,2.25,1,BLACK,'[','{'),k('rbr',12.5,2.25,1,BLACK,']','}'),
k('del',16.25,2.25,1,BLACK,'GREEN','','INV.VID',GN),k('end',17.25,2.25,1,BLACK,'GRAPH','CAT'),k('pgd',18.25,2.25,1,BLACK,'BLACK','','DELETE'),
k('cap',0,3.25,1.75,GREY,'CAPS LOCK','FN')]
for n,ch in enumerate('ASDFGHJKL'): K.append(k(ch.lower(),1.75+n,3.25,1,GREY if ch in 'ASD' else BLACK,ch,'',kw2[ch]))
K+= [k('sem',10.75,3.25,1,BLACK,';',':'),k('quo',11.75,3.25,1,BLACK,"'",'"'),k('hsh',12.75,3.25,1,BLACK,'\\','|'),
k('lsh',0,4.25,1.25,GREY,'CAPS','','SHIFT'),k('iso',1.25,4.25,1,GREY,'\\','|')]
for n,ch in enumerate('ZXCVBNM'): K.append(k(ch.lower(),2.25+n,4.25,1,BLACK,ch,'',kw2[ch]))
K+= [k('com',9.25,4.25,1,BLACK,',','<'),k('dot',10.25,4.25,1,BLACK,'.','>'),k('sl',11.25,4.25,1,BLACK,'/','?'),k('rsh',12.25,4.25,2.75,GREY,''),
k('up',17.25,4.25,1,GREY,'⇧'),
k('lct',0,5.25,1.25,GREY,'PEEK','OUT','',GN),k('lwn',1.25,5.25,1.25,GREY,'CODE','IN','',GN),k('lal',2.5,5.25,1.25,GREY,'RND','MERGE','',GN),k('spc',3.75,5.25,6.25,GREY,''),
k('ral',10,5.25,1.25,GREY,'RND','MERGE','',GN),k('rwn',11.25,5.25,1.25,GREY,'CODE','IN','',GN),k('fn',12.5,5.25,1.25,GREY,'EDIT','DEF FN','',GN),k('rct',13.75,5.25,1.25,GREY,'PEEK','OUT','',GN),
k('lft',16.25,5.25,1,GREY,'⇦'),k('dn',17.25,5.25,1,GREY,'⇩'),k('rt',18.25,5.25,1,GREY,'⇨')]
def t(x,y,s,sz,c,a='start',b=True): return f'<text x="{x}" y="{y}" font-size="{sz}" fill="{c}" text-anchor="{a}" font-family="DejaVu Sans,Arial"{" font-weight=\"bold\"" if b else ""}>{e(s)}</text>'
def render(title,sub,F,knob,hold,out,note=''):
    s=[]
    for d in K:
        px,py,pw,ph=d['x']*U+G/2,d['y']*U+G/2,d['w']*U-G,U-G
        f=F.get(d['i']); c1,c2=d['c']
        hl=None
        if f: hl=f[1]
        stroke={None:'#0b0d10','fn':'#29b6ff','win':'#ffb020','spec':'#3ddc6a','core':'#c48bff','hold':'#ffffff','reset':'#ff4d4d'}[hl]
        op='1' if (hl or not F.get('_dim')) else '0.35'
        s.append(f'<g opacity="{op}"><rect x="{px}" y="{py}" width="{pw}" height="{ph}" rx="7" fill="{c2}" stroke="{stroke}" stroke-width="{3.5 if hl else 1}"/>')
        s.append(f'<rect x="{px+5}" y="{py+3}" width="{pw-10}" height="{ph-12}" rx="5" fill="{c1}"/>')
        big=len(d['m'])<=2 or d['i'] in ('f10','f11','f12')
        if d['m']:
            if d['i']=='pgd':
                s.append(f'<rect x="{px+pw/2-23}" y="{py+8}" width="46" height="14" fill="{WH}"/>'); s.append(t(px+pw/2,py+19,'BLACK',10,'#111','middle'))
            elif big: s.append(t(px+9,py+24,d['m'],18,d['mc']))
            else: s.append(t(px+pw/2,py+20,d['m'],10 if len(d['m'])<9 else 9,d['mc'],'middle'))
        if d['s']:
            if big: s.append(t(px+pw-9,py+22,d['s'],10 if len(d['s'])>1 else 12,d['sc'],'end'))
            else: s.append(t(px+pw/2,py+33,d['s'],9,d['sc'],'middle'))
        if d['kw']: s.append(t(px+pw/2,py+(37 if not big else 40),d['kw'],8.5,WH,'middle',False))
        if f and f[0]:
            col=stroke; lab=f[0]
            s.append(f'<rect x="{px+2}" y="{py+ph-21}" width="{pw-4}" height="19" rx="4" fill="{col}"/>')
            fs=10 if len(lab)<=9 else (8.6 if len(lab)<=12 else 7.4)
            if d['w']>=1.5: fs=min(11,fs+1.5)
            s.append(t(px+pw/2,py+ph-7.5,lab,fs,'#0b0d10','middle'))
        s.append('</g>')
    # ISO enter
    pts=[(13.5,2.25),(15,2.25),(15,4.25),(13.75,4.25),(13.75,3.25),(13.5,3.25)]
    f=F.get('ent'); hl=f[1] if f else None
    stroke={None:'#0b0d10','fn':'#29b6ff','win':'#ffb020','spec':'#3ddc6a','core':'#c48bff'}[hl]
    op='1' if (hl or not F.get('_dim')) else '0.35'
    s.append(f'<g opacity="{op}"><polygon points="'+' '.join(f'{a*U+G/2},{b*U+G/2}' for a,b in pts)+f'" fill="{GREY[0]}" stroke="{stroke}" stroke-width="{3.5 if hl else 1}"/>'+t(14.38*U,3.1*U,'ENTER',12,WH,'middle'))
    if f and f[0]:
        s.append(f'<rect x="{13.8*U}" y="{3.95*U}" width="{1.15*U}" height="19" rx="4" fill="{stroke}"/>'+t(14.38*U,3.95*U+14,f[0],10,'#0b0d10','middle'))
    s.append('</g>')
    # knob
    kl,kc=knob; ks={'fn':'#29b6ff','win':'#ffb020','core':'#c48bff',None:'#555'}[kc]
    cx,cy=15.62*U,U/2
    s.append(f'<circle cx="{cx}" cy="{cy}" r="{U*0.4}" fill="#16181b" stroke="{ks}" stroke-width="3"/>')
    s.append(t(cx,cy+4,'KNOB',10,ks,'middle')); note='Knob: '+kl.replace('\n',' | ')
    W=int(19.25*U+40); H=int(6.25*U+150)
    nt=t(20,H-28,note,15,'#ffffff','start',True) if note else ''
    svg=f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}"><rect width="100%" height="100%" fill="#0d1117"/>'+t(20,34,title,22,'#fff')+t(20,58,sub,13,'#8b949e','start',False)+f'<rect x="10" y="70" width="{W-20}" height="{6.25*U+20}" rx="12" fill="#111316"/><g transform="translate(20,80)">'+''.join(s)+'</g>'+nt+'</svg>'
    open(out+'.html','w').write(f'<html><body style="margin:0">{svg}</body></html>')
    return W,H
