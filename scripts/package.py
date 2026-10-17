#!/usr/bin/env python3
"""Bundle dependencies using relative install names; do not install system-wide."""
from pathlib import Path
import shutil, subprocess, os, plistlib
from concurrent.futures import ThreadPoolExecutor
from release import VERSION, BUILD, PACKAGE_NAME
root=Path(__file__).resolve().parents[1]
base=root/'work/fcitx5-macos'
build=base/'build/arm64'
out=root/'outputs'
bundles=root/'work/packaged.noindex'
bundles.mkdir(exist_ok=True)
stage=root/'work/stage.noindex'
stage.mkdir(exist_ok=True)
# Upstream absolute installation destination is safely staged under work/.
env=os.environ|{'DESTDIR':str(stage)}
subprocess.run(['cmake','--install',str(build)],env=env,check=True,stdout=(root/'work/package-install.log').open('w'))
app=stage/'Library/Input Methods/Fcitx5.app'
if not app.exists():raise RuntimeError('Missing staged app')

# Index once instead of walking the build tree for each library dependency.
built_libraries = {}
for library in build.rglob('*.dylib'):
    built_libraries.setdefault(library.name,library)
for library in build.rglob('*.so'):
    built_libraries.setdefault(library.name,library)

def bundle_dependencies(app):
    contents=app/'Contents';libdir=contents/'lib';libdir.mkdir(exist_ok=True)
    binaries=[p for p in contents.rglob('*') if p.is_file() and (p.suffix in ('.dylib','.so') or p.parent.name in ('MacOS','bin') or p.name in ('get_im','switch_im'))]
    processed=set();macho=set()
    while binaries:
        binary=binaries.pop()
        key=str(binary.resolve())
        if key in processed:continue
        processed.add(key)
        cmd=subprocess.run(['otool','-L',str(binary)],capture_output=True,text=True)
        if cmd.returncode:continue
        macho.add(binary.resolve())
        changes=[]
        for line in cmd.stdout.splitlines()[1:]:
            old=line.strip().split(' (')[0]
            if old.startswith(('/System/','/usr/lib/')):continue
            if old.startswith('@rpath/'):
                name=old.split('/')[-1]
                source=built_libraries.get(name)
            elif old.startswith('/'):
                source=Path(old);name=source.name
            else:continue
            if source is None or not source.exists():
                if (libdir/name).exists(): source=libdir/name
                else: raise RuntimeError(f'Unresolved dependency {old} for {binary}')
            dest=libdir/name
            if not dest.exists():shutil.copy2(source.resolve(),dest)
            new='@loader_path/'+os.path.relpath(dest,binary.parent)
            changes.extend(['-change',old,new])
            binaries.append(dest)
        if binary.suffix in ('.dylib','.so'):
            changes.extend(['-id','@rpath/'+binary.name])
        if changes:subprocess.run(['install_name_tool',*changes,str(binary)],check=True)
    libraries = {p.resolve() for p in contents.rglob('*') if p.is_file() and p.suffix in ('.dylib','.so')}
    def sign(binary):
        subprocess.run(['codesign','--force','--sign','-',str(binary)],check=True,stderr=subprocess.DEVNULL)
    with ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(sign,macho))
    subprocess.run(['codesign','--force','--deep','--sign','-',str(app)],check=True,stderr=subprocess.DEVNULL)

out.mkdir(exist_ok=True)
for name,source in [('TailKanaInput.app',app),('TailKanaPreview.app',build/'apps.noindex/TailKanaPreview.app')]:
    dest=bundles/name
    if dest.exists():shutil.rmtree(dest)
    shutil.copytree(source,dest,symlinks=True)
    if name=='TailKanaPreview.app':
        for resource in ('share','Resources','bin'):
            if (app/'Contents'/resource).exists():shutil.copytree(app/'Contents'/resource,dest/'Contents'/resource,dirs_exist_ok=True)
    for symbols in dest.rglob("*.dSYM"):shutil.rmtree(symbols)
    # Development archives/headers are kept in work/, not the runtime bundle.
    if (dest/'Contents/include').exists():shutil.rmtree(dest/'Contents/include')
    for archive in (dest/'Contents/lib').rglob('*.a') if (dest/'Contents/lib').exists() else []:archive.unlink()
    if (dest/'Contents/lib/cmake').exists():shutil.rmtree(dest/'Contents/lib/cmake')
    # Preserve source/license attribution in the package.
    licenses=dest/'Contents/Resources/TailKana-Licenses';licenses.mkdir(parents=True,exist_ok=True)
    for p in (root/'LICENSE',root/'THIRD_PARTY.md',root/'REQUIREMENTS.md',root/'work/fcitx5-mozc/mozc/LICENSE'):
        shutil.copy2(p,licenses/(p.name if p.parent==root else 'Mozc-LICENSE'))
    shutil.copytree(root/'third_party/azookey-typo',licenses/'AzooKey-Typo',dirs_exist_ok=True)
    if name=='TailKanaInput.app':
        plist=dest/'Contents/Info.plist'
        info=plistlib.loads(plist.read_bytes())
        info['tsInputMethodCharacterRepertoireKey']=['Latn','Hira','Kana','Hani']
        info.update(CFBundleExecutable='Fcitx5',CFBundlePackageType='APPL',CFBundleIdentifier='org.tailkana.inputmethod.TailKana',CFBundleName='TailKana',CFBundleDisplayName='TailKana',CFBundleShortVersionString=VERSION,CFBundleVersion=BUILD,LSMinimumSystemVersion='15.0',TISInputSourceID='org.tailkana.inputmethod.TailKana',TISIntendedLanguage='ja',LSUIElement=True,InputMethodConnectionName='org.tailkana.inputmethod.TailKana_Connection')
        modes=info['ComponentInputModeDict']['tsInputModeListKey']
        source='org.tailkana.inputmethod.TailKana.japanese'
        mode=modes.get(source) or modes['org.tailkana.inputmethod.japanese']
        mode.update(TISInputSourceID=source,tsInputModeDefaultStateKey=True,tsInputModeIsVisibleKey=True,tsInputModePrimaryInScriptKey=True,tsInputModeScriptKey='smJapanese',tsInputModeAlternateMenuIconFileKey='menu_icon.pdf',tsInputModeKeyEquivalentKey='',tsInputModeKeyEquivalentModifiersKey=4608)
        info['ComponentInputModeDict']={'tsInputModeListKey':{source:mode},'tsVisibleInputModeOrderedArrayKey':[source]}
        plist.write_bytes(plistlib.dumps(info))
    # Give every surface the same original mark and product name.
    resources=dest/'Contents/Resources'
    shutil.copy2(root/'assets/TailKana.icns',resources/'TailKana.icns')
    shutil.copy2(root/'assets/tailkana-menu.pdf',resources/'menu_icon.pdf')
    # Upstream About still loads this resource name; keep it as the new mark.
    shutil.copy2(root/'assets/TailKana.icns',resources/'fcitx.icns')
    plist=dest/'Contents/Info.plist'
    info=plistlib.loads(plist.read_bytes())
    info['CFBundleIconFile']='TailKana.icns'
    if name=='TailKanaPreview.app':
        info.update(CFBundleName='TailKana 试打',CFBundleDisplayName='TailKana 试打')
    plist.write_bytes(plistlib.dumps(info))
    brand='TailKana 试打' if name=='TailKanaPreview.app' else 'TailKana'
    for localized in resources.glob('*.lproj/InfoPlist.strings'):
        # Branding is identical in every locale; UTF-16 LE + BOM per translate skill.
        localized.write_bytes(b'\xff\xfe'+(f'"CFBundleName" = "{brand}";\n"CFBundleDisplayName" = "{brand}";\n"org.tailkana.inputmethod.TailKana.japanese" = "TailKana";\n').encode('utf-16-le'))
    ai_notice=licenses/'AI';ai_notice.mkdir(exist_ok=True)
    shutil.copy2(root/'experimental/ai/licenses/llama.cpp-LICENSE',ai_notice/'llama.cpp-LICENSE')
    shutil.copy2(root/'experimental/ai/model-lock.json',ai_notice/'model-lock.json')
    for source in ('ai_service.cpp','ai_worker.h','ai_worker.cpp'):
        shutil.copy2(root/'src'/source,ai_notice/source)
    (ai_notice/'NOTICE.txt').write_text('TailKana 0.4.0 AI 上下文推荐\nZenz v3 small Q5_K_M by Miwa-Keita, CC-BY-SA-4.0. Original weights, revision and SHA256 in model-lock.json (zenz.gguf entry).\nhttps://huggingface.co/Miwa-Keita/zenz-v3-small-gguf\nLicense: https://creativecommons.org/licenses/by-sa/4.0/\nRuntime: azooKey/llama.cpp, commit 88b97a47dc7f5892e2d5a6856fbe9cfe237f9e5c, MIT. See llama.cpp-LICENSE.\nOnly Zenz is bundled. E5 and the browser trial are not required or installed.\n')
    bundle_dependencies(dest)
    print(dest,flush=True)
# Keep development bundles and older packages out of the delivery folder.
archive=root/'work/previous-outputs.noindex';archive.mkdir(exist_ok=True)
for legacy in ('Fcitx5.app','TailKanaPreview.app','启动试打.command','TailKana-0.2.0-arm64.pkg'):
    old=out/legacy
    if old.exists():
        target=archive/legacy
        if target.exists():
            if target.is_dir():shutil.rmtree(target)
            else:target.unlink()
        shutil.move(old,target)
