// SPDX-License-Identifier: GPL-3.0-or-later
#define INITGUID
#include <windows.h>
#include <msctf.h>
#include <objbase.h>
#include <iostream>
#include <string>
int wmain(int argc,wchar_t** argv) {
    if (argc!=3) return 2;
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) return 3;
    CLSID service{};GUID profile{};
    if (FAILED(CLSIDFromString(argv[1],&service)) || FAILED(CLSIDFromString(argv[2],&profile))) return 4;
    ITfInputProcessorProfiles* profiles=nullptr;
    HRESULT result=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_ITfInputProcessorProfiles,reinterpret_cast<void**>(&profiles));
    BSTR description=nullptr;
    if (SUCCEEDED(result)) result=profiles->GetLanguageProfileDescription(service,MAKELANGID(LANG_JAPANESE,SUBLANG_DEFAULT),profile,&description);
    if (FAILED(result) || !description || std::wstring(description)!=L"TailKana") {std::cerr<<"TSF profile not registered: "<<std::hex<<result<<'\n';return 5;}
    SysFreeString(description);profiles->Release();
    IUnknown* instance=nullptr;
    result=CoCreateInstance(service,nullptr,CLSCTX_INPROC_SERVER,IID_IUnknown,reinterpret_cast<void**>(&instance));
    if (FAILED(result) || !instance) {std::cerr<<"Registered COM class cannot load: "<<std::hex<<result<<'\n';return 6;}
    instance->Release();CoUninitialize();std::cout<<"PASS: TailKana TSF profile and COM DLL load on Windows\n";
}
