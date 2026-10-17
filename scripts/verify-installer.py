#!/usr/bin/env python3
"""Audit the extracted pkg, without installing or enabling any input source."""
from pathlib import Path
import tempfile, subprocess, json, plistlib, hashlib, sqlite3, os
from release import VERSION, BUILD, PACKAGE_NAME
root=Path(__file__).resolve().parents[1]
pkg=root/'outputs'/PACKAGE_NAME
validation=root/'work/validation.noindex'
validation.mkdir(exist_ok=True)
temporary=tempfile.TemporaryDirectory(prefix='check-',dir=validation)
folder=Path(temporary.name)/'expanded'
subprocess.run(['pkgutil','--expand-full',str(pkg),str(folder)],check=True)
payload=folder/'TailKana-component.pkg/Payload'
report={'packageSha256':hashlib.sha256(pkg.read_bytes()).hexdigest(),'apps':[],'scripts':list(str(p.relative_to(folder)) for p in folder.rglob('Scripts')),'developmentSymbols':list(str(p.relative_to(folder)) for p in folder.rglob('*.dSYM'))}
assert not report['scripts'] and not report['developmentSymbols']
for relative in ['Library/Input Methods/TailKanaInput.app','Applications/TailKana.app']:
    app=payload/relative
    assert app.is_dir(),relative
    subprocess.run(['codesign','--verify','--deep','--strict',str(app)],check=True)
    info=plistlib.loads((app/'Contents/Info.plist').read_bytes())
    executable=app/'Contents/MacOS'/info['CFBundleExecutable']
    assert executable.is_file() and executable.stat().st_mode & 0o111
    report['apps'].append({'path':'/'+relative,'identifier':info['CFBundleIdentifier'],'executable':info['CFBundleExecutable'],'signatureVerified':True})
    icon=app/'Contents/Resources'/info['CFBundleIconFile']
    assert icon.read_bytes()==(root/'assets/TailKana.icns').read_bytes()
    for localized in (app/'Contents/Resources').glob('*.lproj/InfoPlist.strings'):
        assert '"CFBundleName" = "TailKana";' in localized.read_text(encoding='utf-16')
    if 'InputMethodConnectionName' in info:
        assert info['LSUIElement'] is True and info['LSBackgroundOnly'] is True
        assert info['InputMethodConnectionName'] == info['CFBundleIdentifier'] + '_Connection'
        assert info['TISInputSourceID'] == info['CFBundleIdentifier']
        resources=app/'Contents/share/tailkana'
        report['languageData']={}
        for name in ('pinyin.dict','pinyin.lm','ja-zh.sqlite'):
            data=(resources/name).read_bytes()
            assert hashlib.sha256(data).digest()==hashlib.sha256((root/'assets/language'/name).read_bytes()).digest()
            report['languageData'][name]=len(data)
        model=resources/'ai/zenz.gguf'
        assert hashlib.sha256(model.read_bytes()).hexdigest()=='501f605d088f5b988791a00ae19ed46985ed7c48144f364b2f3f1f951c9b2083'
        service=app/'Contents/bin/TailKanaAI'
        assert service.is_file() and service.stat().st_mode & 0o111
        subprocess.run(['codesign','--verify','--strict',str(service)],check=True)
        assert not list((app/'Contents').rglob('*.onnx'))
        assert (app/'Contents/Resources/TailKana-Licenses/AI/NOTICE.txt').exists()
        report['nativeAI']={'modelBytes':model.stat().st_size,'helper':'bin/TailKanaAI','usesPython':False,'modelHashVerified':True}
        inputs=[{'warm':True},{'raw':'hashi','context':'ご飯を食べるために','max_length':32},{'raw':'hashi','context':'川を渡るために','max_length':32},{'raw':'nihongo','context':'','max_length':1}]
        native=subprocess.run([str(service),str(model)],input=''.join(json.dumps(q)+'\n' for q in inputs),text=True,capture_output=True,timeout=45,check=True,env=os.environ|{'TAILKANA_MOZC_PROFILE':str(root/'work/package-ai-profile')},cwd='/tmp')
        answers=[json.loads(line) for line in native.stdout.splitlines()]
        assert answers[0]['status']=='ready' and answers[1]['text']=='箸' and answers[2]['text']=='橋' and answers[3]['status']=='length',answers
        report['nativeAI']['relocatedInferenceVerified']=True

        assert info['CFBundleShortVersionString']==VERSION
        assert (resources/'Wiktionary-NOTICE.txt').is_file()
        assert (resources/'OpenCC-NOTICE.txt').is_file()
        glossary=json.loads((resources/'glossary-source.json').read_text())
        assert glossary['meaningLanguage']=='zh-Hans' and glossary['conversion']['profile']=='t2s'
        with sqlite3.connect(resources/'ja-zh.sqlite') as dictionary:
            assert dictionary.execute("SELECT meaning,source FROM gloss WHERE word='日本語' LIMIT 1").fetchone()==('日语','日本語')
        report['simplifiedChineseMeanings']=True
        attribution=app/'Contents/Resources/TailKana-Licenses/AzooKey-Typo'
        for name in ('LICENSE','TypoCorrection.swift','PROVENANCE.json'):
            assert (attribution/name).read_bytes()==(root/'third_party/azookey-typo'/name).read_bytes()
        report['correctionAttributionVerified']=True
        assert (app/'Contents/Resources/menu_icon.pdf').read_bytes()==(root/'assets/tailkana-menu.pdf').read_bytes()
        assert list(info['ComponentInputModeDict']['tsInputModeListKey'])==['org.tailkana.inputmethod.TailKana.japanese']
        installed=Path('/Library/Input Methods/TailKanaInput.app/Contents/Info.plist')
        if installed.exists():
            current=plistlib.loads(installed.read_bytes())
            for key in ('CFBundleIdentifier','CFBundleExecutable','TISInputSourceID','InputMethodConnectionName','InputMethodServerControllerClass','NSPrincipalClass','ComponentInputModeDict','LSUIElement','LSBackgroundOnly'):
                assert info.get(key)==current.get(key),f'Unexpected registration change: {key}'
            report['registrationMatchesInstalled']=True
        assert (root/'work/packaged.noindex/TailKanaInput.app/Contents/MacOS/Fcitx5').read_bytes()==executable.read_bytes()
        files=[executable,service,*[p for p in (app/'Contents/lib').rglob('*') if p.is_file() and p.suffix in ('.dylib','.so')]]
        for binary in files:
            lines=subprocess.run(['otool','-L',str(binary)],check=True,capture_output=True,text=True).stdout.splitlines()[1:]
            for line in lines:
                dep=line.strip().split(' (')[0]
                if dep.startswith(('/System/','/usr/lib/')):continue
                if binary.suffix in ('.dylib','.so') and line==lines[0]:continue # dylib's own install ID, including symlink aliases
                assert dep.startswith('@loader_path/'),f'Nonportable dependency: {binary}: {dep}'
                assert (binary.parent/dep.removeprefix('@loader_path/')).exists(),f'Missing dependency {dep}'
    else:
        for name in ('register_im','enable_im'):
            helper=app/'Contents/Helpers'/name
            assert helper.is_file() and helper.stat().st_mode & 0o111
            subprocess.run(['codesign','--verify','--strict',str(helper)],check=True)
            source=app/'Contents/Resources/Fcitx5-Installer-Source'/(name+'.swift')
            assert source.read_bytes()==(root/'third_party/fcitx5-macos-installer'/(name+'.swift')).read_bytes()
        assert (app/'Contents/Resources/Fcitx5-Installer-Source/LICENSE').is_file()
assert len(list((payload/'Applications').glob('*.app')))==1
report['singleApplicationEntry']=True
report['unifiedIcons']=True
report['passed']=True
(root/'outputs/installer-validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2))
print(json.dumps(report,ensure_ascii=False,indent=2))

# Never retain extracted applications; they otherwise become indexed app entries.
temporary.cleanup()
subprocess.run(["python3",str(root/"scripts/cleanup-apps.py")],check=True)
