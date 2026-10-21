from pathlib import Path
import subprocess,os,json,time
out=Path('/tmp/lufus-windows-validation');base=Path('/caminho/dos/testes/.lufus-windows-validation-20261020');runtime=Path('/caminho/do/projeto/linux/runtime');dest=base/'wim-check';dest.mkdir(exist_ok=True)
env=dict(os.environ,PATH=str(runtime/'bin')+':'+os.environ['PATH'],LD_LIBRARY_PATH=str(runtime/'lib'));results=[]
cmd=[str(runtime/'bin/7z'),'e',str(base/'source/windows.iso'),'sources/install.esd','sources/boot.wim','-o'+str(dest),'-y']
with (out/'wim-extract.log').open('w') as f:subprocess.run(cmd,env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
for name in ['boot.wim','install.esd']:
 cmd=[str(runtime/'bin/wimlib-imagex'),'verify',str(dest/name)];start=time.monotonic()
 with (out/f'verify-{name}.log').open('w') as f:r=subprocess.run(cmd,env=env,stdout=f,stderr=subprocess.STDOUT)
 results.append({'image':name,'exit_code':r.returncode,'elapsed_seconds':time.monotonic()-start,'command':cmd});(out/'wim-verification.json').write_text(json.dumps(results,indent=2)+'\n')
 print(json.dumps(results[-1]),flush=True)
 assert r.returncode==0
