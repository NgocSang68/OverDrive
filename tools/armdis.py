import struct,re,sys
from capstone import *
md=Cs(CS_ARCH_ARM,CS_MODE_ARM)
def dis(fn,a,b):
    d=open(fn,'rb').read()
    for i in range(a,b,4):
        x=list(md.disasm(d[i:i+4],i))
        if not x:
            print("%05x  .word %08x"%(i,struct.unpack_from('<I',d,i)[0])); continue
        x=x[0]; note=''
        if x.mnemonic.startswith('ldr') and 'pc' in x.op_str:
            m=re.search(r'\[pc, #(-?)(0x[0-9a-f]+|\d+)\]',x.op_str)
            if m:
                imm=int(m.group(2),0)*(-1 if m.group(1) else 1)
                t=i+8+imm
                if 0<=t<len(d)-3: note='   ; =0x%08x'%struct.unpack_from('<I',d,t)[0]
        print("%05x  %-6s %s%s"%(i,x.mnemonic,x.op_str,note))
dis(sys.argv[1],int(sys.argv[2],16),int(sys.argv[3],16))
