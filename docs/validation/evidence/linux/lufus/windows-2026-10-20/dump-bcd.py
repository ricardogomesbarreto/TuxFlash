from pathlib import Path
import ctypes as c,json,sys
lib=c.CDLL('/caminho/do/projeto/linux/runtime/lib/libhivex.so.0')
H=c.c_void_p;N=c.c_size_t
for name,args,res in [('hivex_open',[c.c_char_p,c.c_int],H),('hivex_root',[H],N),('hivex_node_children',[H,N],c.POINTER(N)),('hivex_node_values',[H,N],c.POINTER(N)),('hivex_node_name',[H,N],c.c_char_p),('hivex_value_key',[H,N],c.c_char_p),('hivex_value_value',[H,N,c.POINTER(c.c_int),c.POINTER(N)],H),('hivex_close',[H],c.c_int)]:
 fn=getattr(lib,name);fn.argtypes=args;fn.restype=res
h=lib.hivex_open(sys.argv[1].encode(),0);assert h
out={}
def walk(n,path=''):
 values=lib.hivex_node_values(h,n);i=0
 while values and values[i]:
  v=values[i];typ=c.c_int();length=N();ptr=lib.hivex_value_value(h,v,c.byref(typ),c.byref(length));data=c.string_at(ptr,length.value)
  entry={'type':typ.value,'bytes':length.value,'hex':data.hex()}
  if typ.value in [1,2,7]:entry['string']=data.decode('utf-16le',errors='replace')
  if typ.value==4:entry['dword']=int.from_bytes(data,'little')
  out[path+'/'+lib.hivex_value_key(h,v).decode()]=entry;i+=1
 children=lib.hivex_node_children(h,n);i=0
 while children and children[i]:
  child=children[i];walk(child,path+'/'+lib.hivex_node_name(h,child).decode());i+=1
walk(lib.hivex_root(h));lib.hivex_close(h)
Path(sys.argv[2]).write_text(json.dumps(out,indent=2)+'\n')
for k,v in out.items():
 if any(k.endswith('/'+x+'/Element') for x in ['23000003','24000001','11000001','12000002','21000001','22000002']) or k.endswith('/Description/Type'):print(k,v)
