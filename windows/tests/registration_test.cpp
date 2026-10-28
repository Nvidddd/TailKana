// SPDX-License-Identifier: GPL-3.0-or-later
#define INITGUID
#include <windows.h>
#include <msctf.h>
#include <objbase.h>
#include <msi.h>
#include <msiquery.h>
#include <iostream>
#include <string>
struct DatabaseHandle {
    MSIHANDLE value=0;
    ~DatabaseHandle() {if (value) MsiCloseHandle(value);}
};
std::wstring property(MSIHANDLE database,const wchar_t* name) {
    const std::wstring query=std::wstring(L"SELECT `Value` FROM `Property` WHERE `Property`='")+name+L"'";
    DatabaseHandle view,record;
    if (MsiDatabaseOpenViewW(database,query.c_str(),&view.value)!=ERROR_SUCCESS || MsiViewExecute(view.value,0)!=ERROR_SUCCESS || MsiViewFetch(view.value,&record.value)!=ERROR_SUCCESS) return {};
    DWORD length=0;const auto status=MsiRecordGetStringW(record.value,1,nullptr,&length);
    if (status!=ERROR_MORE_DATA && status!=ERROR_SUCCESS) return {};
    std::wstring value(length+1,L'\0');DWORD capacity=static_cast<DWORD>(value.size());
    if (MsiRecordGetStringW(record.value,1,value.data(),&capacity)!=ERROR_SUCCESS) return {};
    value.resize(capacity);return value;
}
int wmain(int argc,wchar_t** argv) {
    if (argc==5 && std::wstring(argv[1])==L"--msi") {
        DatabaseHandle database;
        if (MsiOpenDatabaseW(argv[2],MSIDBOPEN_READONLY,&database.value)!=ERROR_SUCCESS) {std::cerr<<"Cannot open MSI database\n";return 10;}
        if (property(database.value,L"ProductName")!=L"TailKana") {std::cerr<<"Wrong MSI product name\n";return 11;}
        if (property(database.value,L"UpgradeCode")!=argv[3]) {std::wcerr<<L"Wrong MSI upgrade identity: "<<property(database.value,L"UpgradeCode")<<L'\n';return 12;}
        if (property(database.value,L"ProductVersion")!=argv[4]) {std::wcerr<<L"Wrong MSI version: "<<property(database.value,L"ProductVersion")<<L'\n';return 13;}
        std::cout<<"PASS: native MSI metadata, product, version and upgrade identity\n";return 0;
    }
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
