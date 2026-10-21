#!/usr/bin/env python3
"""Build a relocatable TuxFlash bundle from trusted native binaries and packages.

The bundle uses a private glibc loader, not the host GTK or glibc. Build each
CPU architecture separately. This does not claim support for every Linux kernel.
"""
from pathlib import Path
import argparse,subprocess,shutil,os,json,re,hashlib,platform,stat

parser=argparse.ArgumentParser()
parser.add_argument('output',type=Path)
parser.add_argument('--runtime',type=Path)
parser.add_argument('--tools-root',type=Path)
parser.add_argument('--busybox',type=Path,required=True,help='Trusted statically linked BusyBox')
args=parser.parse_args()
linux=Path(__file__).resolve().parents[1];project=linux.parent
runtime_source=args.runtime or linux/'runtime'
output=args.output.resolve();assert not output.exists(),'Use a new output directory'
assert output!=project and project not in output.parents,'Build outside the source tree'
version=re.search(r"version: '([^']+)'",(linux/'meson.build').read_text()).group(1)
output.mkdir(parents=True)
runtime=output/'runtime';shutil.copytree(runtime_source,runtime,symlinks=False)
lib=runtime/'lib';lib.mkdir(exist_ok=True)
(runtimemarker:=runtime/'.tuxflash-portable').write_text('TuxFlash private-loader runtime v1\n')
owned_files=set();dependencies={};programs={};pending=[];seen=set()
if Path('/usr/bin/busybox').is_file() and hashlib.sha256(args.busybox.read_bytes()).digest()==hashlib.sha256(Path('/usr/bin/busybox').read_bytes()).digest():owned_files.add('/usr/bin/busybox')

def copy_file(source,dest):
    source=Path(source);dest=Path(dest);dest.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(source,dest);os.chmod(dest,stat.S_IMODE(source.stat().st_mode)&0o777)
    owned_files.add(str(source));return dest

def add_program(name,source):
    dest=runtime/'bin'/name
    if dest.exists():return
    copy_file(source,dest)

extras='uname lsblk findmnt mount umount blkid blockdev wipefs sfdisk mkfs.vfat mkfs.ntfs mkfs.ext4 ntfs-3g bash gzip xz bzip2 zstd uuidgen hexdump xxd dd stat cat cut tr head tail sed grep awk find mkdir mktemp dirname basename realpath readlink rm rmdir chmod sync cmp sha256sum md5sum wc sort printf date od touch tee env echo cp ln e2fsck fsck.vfat'.split()
search='/usr/sbin:/usr/bin:/sbin:/bin'
if args.tools_root:search+=':'+str(args.tools_root/'usr/sbin')+':'+str(args.tools_root/'usr/bin')+':'+str(args.tools_root/'sbin')+':'+str(args.tools_root/'bin')
for name in extras:
    tool=shutil.which('gnu'+name,path=search) or shutil.which(name,path=search)
    if tool is None:raise SystemExit('Required build tool is missing: '+name)
    add_program(name,tool)
# We never redistribute setuid authentication programs.
for name in ['pkexec','sudo','su']:
    assert not (runtime/'bin'/name).exists()

# Portable SVG loading and TLS must use modules built for the bundled libraries.
modules=Path('/usr/lib')
triplet=subprocess.check_output(['cc','-dumpmachine'],text=True).strip()
for source in Path('/usr/lib',triplet,'gio/modules').glob('libgiognutls.so'):
    copy_file(source,lib/'gio/modules'/source.name)
cache=lib/'gio/modules/giomodule.cache';cache.parent.mkdir(parents=True,exist_ok=True)
cache.write_text('libgiognutls.so: gio-tls-backend\n')
svg=Path('/usr/lib',triplet,'gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader_svg.so')
if svg.is_file():
    copy_file(svg,lib/'gdk-pixbuf/loaders'/svg.name)
    old_cache=Path('/usr/lib',triplet,'gdk-pixbuf-2.0/2.10.0/loaders.cache').read_text()
    blocks=old_cache.split('\n\n');block=next(x for x in blocks if '"'+str(svg)+'"' in x)
    block=block.replace(str(svg),'@LIBDIR@/gdk-pixbuf/loaders/'+svg.name)
    (lib/'gdk-pixbuf/loaders.template').write_text('# GdkPixbuf modules for TuxFlash\n\n'+block+'\n')

# Add all ELF dependencies, preserving SONAME filenames and rejecting collisions.
env=dict(os.environ,LD_LIBRARY_PATH=str(runtime_source/'lib'))
for p in runtime.rglob('*'):
    if p.is_file() and p.read_bytes()[:4]==b'\x7fELF':pending.append(p)
while pending:
    obj=pending.pop()
    key=str(obj.resolve())
    if key in seen:continue
    seen.add(key)
    result=subprocess.run(['ldd',str(obj)],env=env,text=True,capture_output=True)
    if 'not found' in result.stdout:raise RuntimeError('Unresolved dependency: '+obj.name+'\n'+result.stdout)
    for line in result.stdout.splitlines():
        match=re.search(r'=> (/\S+)\s+\(',line) or re.match(r'\s*(/\S+)\s+\(',line)
        if not match:continue
        source=Path(match.group(1));target=lib/source.name
        if target.exists():continue
        copy_file(source,target);pending.append(target);dependencies[source.name]=target

interp=subprocess.check_output(['readelf','-l',str(runtime/'bin/tuxflash')],text=True)
loader_source=Path(re.search(r'Requesting program interpreter: ([^\]]+)',interp).group(1))
assert 'ld-linux' in loader_source.name,'This portable builder currently requires a glibc build host'
loader=lib/loader_source.name
if not loader.exists():copy_file(loader_source,loader)
# 7-Zip locates its plugin beside /proc/self/exe, which is the private loader.
if (runtime/'bin/7z.so').exists():shutil.copy2(runtime/'bin/7z.so',lib/'7z.so')

# Character conversion, public fonts, schemas, locales and trust anchors.
gconv=Path('/usr/lib',triplet,'gconv')
if gconv.exists():shutil.copytree(gconv,lib/'gconv',dirs_exist_ok=True)
locale=lib/'locale';locale.mkdir(exist_ok=True)
if Path('/usr/lib/locale/C.utf8').exists():shutil.copytree('/usr/lib/locale/C.utf8',locale/'C.utf8')
for language in ['en_US','pt_BR','zh_CN','es_ES']:
    subprocess.run(['localedef','--no-archive','-i',language,'-f','UTF-8',str(locale/(language+'.UTF-8'))],check=True,stdout=subprocess.DEVNULL)
fonts=runtime/'share/tuxflash/fonts';fonts.mkdir(parents=True)
for name in ['DejaVuSans.ttf','DejaVuSans-Bold.ttf','DejaVuSansMono.ttf']:
    copy_file(Path('/usr/share/fonts/truetype/dejavu')/name,fonts/name)
# Chinese UI must render even on hosts with no CJK fonts installed.
cjk=Path('/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc')
if not cjk.is_file():raise SystemExit('NotoSansCJK-Regular.ttc is required to package the Chinese interface')
copy_file(cjk,fonts/cjk.name)
(fonts/'fonts.conf').write_text('<?xml version="1.0"?><!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd"><fontconfig><dir prefix="relative">.</dir><cachedir prefix="xdg">fontconfig</cachedir><alias><family>sans-serif</family><prefer><family>DejaVu Sans</family></prefer></alias><alias><family>monospace</family><prefer><family>DejaVu Sans Mono</family></prefer></alias></fontconfig>\n')
# Do not copy unrelated installed applications or their desktop identities.
for theme in ['Adwaita']:
    source=Path('/usr/share/icons')/theme
    if source.exists():shutil.copytree(source,runtime/'share/icons'/theme,dirs_exist_ok=True)
# Our icon wins over all system themes and retains the supplied PNG bytes.
shutil.copytree(linux/'assets/icons/hicolor',runtime/'share/icons/hicolor',dirs_exist_ok=True)
schemas=runtime/'share/glib-2.0/schemas';schemas.mkdir(parents=True,exist_ok=True)
for source in Path('/usr/share/glib-2.0/schemas').glob('org.gnome.desktop.*.xml'):copy_file(source,schemas/source.name)
compiler=shutil.which('glib-compile-schemas')
if compiler is None:
    compiler=os.environ.get('TUXFLASH_GLIB_COMPILE_SCHEMAS')
assert compiler,'glib-compile-schemas is required'
subprocess.run([compiler,str(schemas)],check=True)
certs=runtime/'share/tuxflash/certs';certs.mkdir(parents=True)
anchors=sorted(Path('/usr/share/ca-certificates/mozilla').glob('*.crt'));assert anchors
(certs/'ca-certificates.crt').write_bytes(b'\n'.join(p.read_bytes() for p in anchors))
owned_files.update(str(p) for p in anchors)
(runtime/'share/tuxflash/mke2fs.conf').write_text('[defaults]\nbase_features = sparse_super,large_file,filetype,resize_inode,dir_index,ext_attr\nblocksize = 4096\ninode_size = 256\ninode_ratio = 16384\n[fs_types]\next4 = {\nfeatures = has_journal,extent,huge_file,flex_bg,uninit_bg,dir_nlink,extra_isize\n}\n')
copy_file(args.busybox,runtime/'libexec/busybox')
assert 'statically linked' in subprocess.check_output(['file',str(args.busybox)],text=True)

# Every bundled dynamic program gets an explicit private-loader invocation.
for p in list(runtime.rglob('*')):
    if not p.is_file() or p.read_bytes()[:4]!=b'\x7fELF':continue
    if lib==p.parent or lib in p.parents or p.name=='busybox':continue
    if not p.stat().st_mode&0o111:continue
    if p.name in ['lufus','lufus-helper']:
        p.unlink();continue
    real=p.with_name('.'+p.name+'.bin');p.rename(real)
    rel=str(real.relative_to(runtime));name=p.name
    depth=len(p.parent.relative_to(runtime).parts)
    up='/'.join(['..']*depth) or '.'
    script='#!/bin/sh\nset -eu\nd=${0%/*}\n[ "$d" != "$0" ] || d=.\nr=$(CDPATH= cd -- "$d/'+up+'" && pwd)\nexec "$r/run-elf" "'+name+'" "$r/'+rel+'" "$@"\n'
    p.write_text(script);p.chmod(0o755);programs[str(p.relative_to(runtime))]=rel
for old,new in [('bin/lufus','tuxflash'),('libexec/lufus-helper','tuxflash-helper')]:
    p=runtime/old
    if p.exists():p.unlink()
    p.symlink_to(new)
# Runtime third-party scripts use the bundled Bash via PATH instead of a fixed host path.
for p in (runtime/'libexec/bcd-sys').rglob('*.sh'):
    text=p.read_text()
    if text.startswith('#!'):text='#!/usr/bin/env bash\n'+text.split('\n',1)[1]
    p.write_text(text)
p=runtime/'libexec/bcd-sys/AppRun';text=p.read_text().replace('exec ./bcd-sys.sh','exec bash ./bcd-sys.sh');p.write_text(text);p.chmod(0o755)

runner=r'''#!/bin/sh
set -eu
r=$(CDPATH= cd -- "${0%/*}" && pwd)
name=$1
program=$2
shift 2
unset LD_LIBRARY_PATH LD_PRELOAD LD_AUDIT GTK_PATH GTK_EXE_PREFIX GTK_IM_MODULE_FILE
export TUXFLASH_HOST_PATH="${TUXFLASH_HOST_PATH:-$PATH}"
export PATH="$r/bin:$r/sbin:$TUXFLASH_HOST_PATH"
export GIO_MODULE_DIR="$r/lib/gio/modules"
export GSETTINGS_SCHEMA_DIR="$r/share/glib-2.0/schemas"
export GCONV_PATH="$r/lib/gconv"
export LOCPATH="$r/lib/locale"
export FONTCONFIG_FILE="$r/share/tuxflash/fonts/fonts.conf"
export MKE2FS_CONFIG="$r/share/tuxflash/mke2fs.conf"
export XDG_DATA_DIRS="$r/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
# A software renderer avoids depending on a matching host GPU-driver stack.
export GSK_RENDERER="${GSK_RENDERER:-cairo}"
if { [ "$name" = tuxflash ] && [ "$#" -eq 0 ]; } || [ "$name" = tuxflash-gui-test ]; then
    b="$r/libexec/busybox"
    if [ -f "$r/lib/gdk-pixbuf/loaders.template" ]; then
        c="${XDG_CACHE_HOME:-${HOME:-/tmp}/.cache}/tuxflash"
        if ! "$b" mkdir -p -- "$c" 2>/dev/null; then
            c=$("$b" mktemp -d "${TMPDIR:-/tmp}/tuxflash-cache.XXXXXX")
        fi
        key=$("$b" printf '%s' "$r" | "$b" sha256sum)
        key=${key%% *}
        cache="$c/pixbuf-$key.cache"
        escaped=$("$b" printf '%s' "$r/lib" | "$b" sed 's/[\\&|]/\\&/g')
        "$b" sed "s|@LIBDIR@|$escaped|g" "$r/lib/gdk-pixbuf/loaders.template" > "$cache"
        export GDK_PIXBUF_MODULE_FILE="$cache"
    fi
fi
if [ "$name" = 7z ]; then name="$r/bin/7z"; fi
exec "$r/lib/@LOADER@" --argv0 "$name" --library-path "$r/lib" "$program" "$@"
'''
(runtime/'run-elf').write_text(runner.replace('@LOADER@',loader.name));(runtime/'run-elf').chmod(0o755)
(output/'tuxflash').write_text('#!/bin/sh\nset -eu\nr=$(CDPATH= cd -- "${0%/*}" && pwd)\nif [ ! -x "$r/runtime/bin/tuxflash" ] || [ ! -x "$r/runtime/lib/@LOADER@" ]; then\n    printf \'%s\\n\' "TuxFlash portable runtime is incomplete. Extract the complete package again." >&2\n    exit 127\nfi\nexec "$r/runtime/bin/tuxflash" "$@"\n'.replace('@LOADER@',loader.name));(output/'tuxflash').chmod(0o755)

# Ship component notices with package identities, never build-user paths or environment dumps.
licenses=runtime/'share/doc/tuxflash/dependencies';licenses.mkdir(parents=True)
packages=set()
answer=subprocess.run(['dpkg-query','-S',*sorted(owned_files)],capture_output=True,text=True)
for line in answer.stdout.splitlines():
    if ': ' in line:packages.add(line.rsplit(': ',1)[0])
records=[]
for package in sorted(packages):
    answer=subprocess.run(['dpkg-query','-W','-f=${Package}\t${Version}\t${source:Package}\t${source:Version}',package],capture_output=True,text=True)
    if answer.returncode:continue
    fields=answer.stdout.split('\t');records.append(dict(zip(['package','version','source_package','source_version'],fields)))
    name=fields[0];notice=Path('/usr/share/doc')/name/'copyright'
    if notice.exists():copy_file(notice,licenses/(name+'.copyright'))
(runtime/'share/doc/tuxflash/dependencies.json').write_text(json.dumps(records,indent=2)+'\n')
# Ship the current organized source tree and its single documentation entry.
# The extracted bundle uses USAGE.md and source/PROJECT.md to avoid duplicate
# README entry points when it lives under a development checkout's dist/.
shutil.copy2(project/'LICENSE.txt',output/'LICENSE.txt')
shutil.copytree(project/'scripts',output/'scripts',ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
source=output/'source';source.mkdir()
for folder in ['tests','tools','scripts','upstream','docs','linux/src','linux/po','linux/tests','linux/tools','linux/assets','linux/packaging','linux/third_party','linux/runtime/share/lufus','linux/runtime/share/locale','linux/runtime/libexec/bcd-sys']:
    shutil.copytree(project/folder,source/folder,symlinks=False,ignore=shutil.ignore_patterns('__pycache__','*.pyc','integrity.json'))
for name in ['tuxflash.sh','Makefile','LICENSE.txt','SECURITY.md','.gitignore','.editorconfig','linux/meson.build','linux/build-local.sh']:
    target=source/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(project/name,target)
(source/'PROJECT.md').write_text((project/'README.md').read_text())
# Local links in the standalone guide point to the included corresponding source.
readme=(project/'README.md').read_text()
def package_link(value):
    if value.startswith(('http:','https:','mailto:','#','/')):return value
    path,sep,fragment=value.partition('#')
    if path=='README.md':return 'USAGE.md'+(sep+fragment if sep else '')
    return 'source/'+path+(sep+fragment if sep else '')
readme=re.sub(r'(\]\()([^\n)]+)(\))',lambda m:m[1]+package_link(m[2])+m[3],readme)
readme=re.sub(r'(src=")([^"]+)(")',lambda m:m[1]+package_link(m[2])+m[3],readme)
readme=readme.replace('./tuxflash.sh','./tuxflash')
(output/'USAGE.md').write_text(readme)
shutil.copytree(project/'docs/legal',output/'docs/legal')
# Identical multicall executables share storage; wrappers retain their argv[0].
seen_content={}
for item in runtime.rglob('*'):
    if not item.is_file() or item.is_symlink() or item.stat().st_size<100000:continue
    key=(hashlib.sha256(item.read_bytes()).hexdigest(),stat.S_IMODE(item.stat().st_mode))
    if key in seen_content:
        item.unlink();os.link(seen_content[key],item)
    else:seen_content[key]=item
manifest={str(p.relative_to(output)):hashlib.sha256(p.read_bytes()).hexdigest() for p in output.rglob('*') if p.is_file() and not p.is_symlink()}
(output/'SHA256SUMS.json').write_text(json.dumps(dict(sorted(manifest.items())),indent=2)+'\n')
(output/'package-info.json').write_text(json.dumps({'application':'TuxFlash','version':version,'architecture':platform.machine(),'private_loader':loader.name,'program_wrappers':len(programs),'dependency_libraries':len(dependencies),'external_symlinks':False,'universal_linux_compatibility_claimed':False},indent=2)+'\n')
print(json.dumps({'application':'TuxFlash','architecture':platform.machine(),'program_wrappers':len(programs),'dependency_libraries':len(dependencies),'files_hashed':len(manifest)},indent=2))

manifest={str(p.relative_to(output)):hashlib.sha256(p.read_bytes()).hexdigest() for p in output.rglob('*') if p.is_file() and not p.is_symlink() and p.name!='SHA256SUMS.json'}
(output/'SHA256SUMS.json').write_text(json.dumps(dict(sorted(manifest.items())),indent=2)+'\n')
