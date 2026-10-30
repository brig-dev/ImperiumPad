# Genera i file della DLL proxy da una DLL di sistema a 32 bit: il .def con tutte le esportazioni
# (stessi nomi e stessi ordinali, anche quelle solo per ordinale) e il file .inc con gli stub
# e la tabella dei nomi usata da ImperiumPad.cpp. Linguaggio: Python 3, richiede pefile.
# Uso: python -I gen_proxy.py C:\Windows\SysWOW64\wininet.dll ImperiumPad\wininet.def ImperiumPad\ProxyExports.inc
import sys
import pefile

src, def_path, inc_path = sys.argv[1:4]
pe = pefile.PE(src)
dll = src.replace('/', '\\').split('\\')[-1].rsplit('.', 1)[0].lower()
symbols = sorted(pe.DIRECTORY_ENTRY_EXPORT.symbols, key=lambda s: s.ordinal)

with open(def_path, 'w', newline='\r\n') as d:
    d.write(f'; {dll}.def - esportazioni della proxy di {dll}.dll, generato da tools/gen_proxy.py da {src}\n')
    d.write('; Ogni esportazione punta a uno stub che salta alla funzione vera della DLL di sistema;\n')
    d.write('; nomi e ordinali sono quelli della DLL di Windows (anche le esportazioni solo per ordinale).\n')
    d.write(f'LIBRARY {dll}\nEXPORTS\n')
    for i, s in enumerate(symbols):
        if s.name:
            # le funzioni COM standard vanno PRIVATE (LNK4104); con l'ordinale il linker avvisa comunque (LNK4222),
            # ma si tiene lo stesso ordinale della DLL di Windows, quindi l'avviso e' atteso e innocuo
            private = ' PRIVATE' if s.name.decode() in ('DllCanUnloadNow', 'DllGetClassObject', 'DllInstall',
                                                        'DllRegisterServer', 'DllUnregisterServer') else ''
            d.write(f'    {s.name.decode():<48} = ProxyStub{i} @{s.ordinal}{private}\n')
        else:
            d.write(f'    {"ProxyOrd" + str(s.ordinal):<48} = ProxyStub{i} @{s.ordinal} NONAME\n')

with open(inc_path, 'w', newline='\r\n') as f:
    f.write(f'// ProxyExports.inc - generato da tools/gen_proxy.py da {src}: non modificare a mano.\n')
    f.write(f'// {len(symbols)} esportazioni di {dll}.dll. PROXY_ENTRY(indice, nome o nullptr, ordinale).\n')
    f.write('#ifdef PROXY_ENTRY\n')
    for i, s in enumerate(symbols):
        name = f'"{s.name.decode()}"' if s.name else 'nullptr'
        f.write(f'PROXY_ENTRY({i}, {name}, {s.ordinal})\n')
    f.write('#endif\n')
    f.write('#ifdef PROXY_STUB\n')
    for i, _ in enumerate(symbols):
        f.write(f'PROXY_STUB({i})\n')
    f.write('#endif\n')
print(f'{dll}: {len(symbols)} esportazioni, {sum(1 for s in symbols if not s.name)} solo per ordinale')
