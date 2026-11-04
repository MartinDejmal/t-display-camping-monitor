# T-Display environmental logger

Firmware pro **původní TTGO T-Display, ESP32, ST7789 240 × 135** a jedno čidlo BME280 nebo BMP085/BMP180. Typ se rozpozná automaticky podle ID. Není určen pro T-Display-S3.

Měří standardně jednou za minutu, ukládá historii do interní flash a mezi měřeními používá deep sleep. Displej se při časovaném probuzení neinicializuje. Při externím napájení lze zpřístupnit web, CSV, konfiguraci a OTA.

## Stav

Implementace 0.4.0: všechny tři varianty se sestavují. Hostitelské testy ověřují formát, skutečný kód hodin a úložiště, tlakové grafy, samostatné webové stránky a volitelné přihlášení. Na COM14 byla ověřena identifikace BMP085/BMP180, dvě minutová měření a uložené CSV: první vzorek bez UTC, druhý po synchronizaci ze sítě, stále s odstupem 60 s. **Funkční BME280, odběry, kompletní OTA na desce a 72hodinový bateriový test zbývají ověřit. Výdrž není změřená.**

Nová implementace je ve větvi `main`. Původní funkční příklad zachovává lokální tag `baseline-working-20261104`; zdrojový ZIP, referenční firmware a úplná 4MB záloha původní desky jsou v lokální ignorované složce `work/baseline/`. Zachována je konfigurace displeje; DHT22, externí baterie a původní obrazovky byly nahrazeny.

## Zapojení

Displej k sobě, USB dole. Použité popisky přímo na desce:

| T-Display | BME280 |
|---|---|
| `3V` | VCC / VIN |
| `G` | GND |
| `21` | SDA / SDI |
| `22` | SCL / SCK |

Šestipinový modul: CS / CSB na `3V`, SDO na `G` (adresa 0x76). Firmware zkouší i 0x77 a kontroluje BME280; BMP280 neumí vlhkost. Modul s LED nebo neúsporným regulátorem může zhoršit výdrž. Čidlo umísti mimo teplo desky a nabíjení.

### Podporovaná čidla

| Čidlo | ID / adresa | Měření |
|---|---|---|
| BME280 | ID 0x60, 0x76 nebo 0x77 | Teplota, tlak a vlhkost; forced mode x1 |
| BMP085/BMP180 | ID 0x55, 0x77 | Teplota a tlak; ultra low power, bez vlhkosti |

BMP085 a BMP180 nelze tímto společným ID rozlišit, proto je označení `BMP085/180`. Zapojení používá stejné 3V/G/21/22. Čidlo se nevybírá ručně. BMP280 s ID 0x58 zatím podporovaný není.

U BMP je vlhkost na displeji `RH: n/a`, na webu `Neměří` a v JSON `null` při `has_humidity=false`. Platné teploty a tlaky se normálně ukládají. Na displeji je druhým grafem tlak. Na webu lze vybrat teplotu, tlak nebo vlhkost; chybějící vlhkost má mezeru, případně zprávu bez dat. Historické BME hodnoty vlhkosti zůstávají dostupné. CSV nechává pole vlhkosti prázdné a přidává `humidity_available` a `sensor_type`.

Volitelná detekce externího napájení:

```text
skutečné USB VBUS --- 100 kOhm ---+--- pin 32
                                |
                             100 kOhm
                                |
USB GND ------------------------+--- G
```

**VBUS není vývod `5V` na T-Display.** Ten je součástí společné napájecí větve. Použij skutečné VBUS z průchozího USB adaptéru nebo ověřený bod na své revizi desky. ESP32 se nesmí připojit přímo na 5 V. Původní externí baterii/dělič z pinu `32` odpoj.

Akumulátor: 1S LiPo přes konektor desky s ověřenou polaritou. Pravé tlačítko používá GPIO35 a fyzický pull-up desky; GPIO35 nemá interní pull-up. Levé tlačítko je GPIO0 (boot strap).

## Varianty sestavení

| PlatformIO environment | Použití |
|---|---|
| `ttgo-t-display` | Bez detekčního děliče. Bateriový logger; Wi-Fi vypnutá i při USB. Nezapojený pin 32 nespouští servis. |
| `ttgo-t-display-usb-sense` | Finální varianta po zapojení VBUS děliče. Automatický servis na USB, úsporný logger na baterii. |
| `ttgo-t-display-service` | **Zkoušení před pájením.** Trvale zapnutý servis a Wi-Fi; předpokládá externí napájení. Nepoužívat pro měření výdrže ani ponechání na baterii. |

## První sestavení a nahrání

Použij PlatformIO Core. Platforma a přímé knihovny jsou připnuté v `platformio.ini`; CPU běží na 80 MHz. Lokálně ověřeno s Arduino-ESP32 2.0.17.

```sh
# Nyní, před pájením, na USB
pio run -e ttgo-t-display-service
pio run -e ttgo-t-display-service -t upload
pio device monitor -b 115200

# Po osazení BME280 a detekce VBUS
pio run -e ttgo-t-display-usb-sense -t upload
```

První nahrání nové verze musí proběhnout **přes USB**, protože zavádí novou tabulku oddílů a bootloader. OTA mění pouze aplikaci. Přechod mezi variantami zapojení proveď také přes USB; web OTA odmítne jinou variantu.

**Přechod z 0.2.x na 0.3.0 také proveď přes USB.** Nové záznamy používají schéma v2 a značku kompatibility `RECORD=2`; starý OTA server správně tento obraz odmítne. Velikost záznamu i tabulka oddílů zůstávají stejné, uložená historie a konfigurace se nemažou. Firmware od 0.3.0 čte v1 i v2; aktualizace 0.3.0 → 0.4.0 formát ani oddíly nemění. Downgrade na 0.2.x neumí v2 číst a starý firmware odmítne za takovou historií zapisovat. Při zkušebním startu OTA se nové záznamy zapisují až po potvrzení zdravého startu, aby předchozí verze mohla provést časný rollback.

Historie má 1216 KiB; oba OTA oddíly mají po 1408 KiB. LittleFS se automaticky inicializuje pouze tehdy, když je celý jeho oddíl prázdný (0xFF). Při poškození nebo nekompatibilních starších datech se **neformátuje**. Chyba je vidět jako `Log ERROR` a na webu. Před případnou ruční obnovou zálohuj flash.

Nepoužívej `uploadfs` pro běžnou aktualizaci: souborový oddíl obsahuje historii. Web je zabudovaný ve firmwaru.

## Ovládání

- Pravé tlačítko probudí a zobrazí čerstvé hodnoty. Další stisky přepínají hodnoty a dva grafy posledních 8 hodin.
- Levé tlačítko přepíná stav zařízení (napájení, relace, další měření, poslední zápis, čas) a servisní informace (síť, adresa a heslo). Pravé vrátí hodnoty.
- Po 15 s od posledního stisku displej zhasne. Na baterii následuje deep sleep, na USB web běží dál.
- Časovač změří, uloží a opět uspí zařízení bez rozsvícení.
- Držené pravé tlačítko nevyvolává okamžitou smyčku probouzení: dokud je držené, používá se pouze krátký časovač.

Ve finální variantě se připojení USB pozná při nejbližším měřicím probuzení, obvykle do minuty; tlačítkem dříve. Při nízké baterii se kontrola prodlužuje na 15 minut.

## Web

Při prvním servisu vznikne síť `tdisplay-xxxxxx`. Náhodné heslo se uloží do NVS. **Heslo AP i webu ukáže servisní obrazovka levého tlačítka**, webový uživatel je `admin`. Adresa AP je standardně `http://192.168.4.1`; řiď se displejem.

Po nastavení domácí Wi-Fi se zařízení připojí k ní. Pokud připojení do 15 s nevyjde nebo síť později vypadne, vytvoří záložní AP. mDNS v LAN: `tdisplay-xxxxxx.local`. Aktivované záložní AP zůstává až do restartu nebo odpojení USB. V servisu je uspávání rádia Wi-Fi vypnuté kvůli odezvě HTTP/OTA; na baterii se Wi-Fi vypne úplně.

Navigace obsahuje samostatné stránky, každá má vlastní URL a načte se ze zařízení:

| Záložka | URL | Obsah |
|---|---|---|
| Přehled | `/` | Aktuální hodnoty a základní stav |
| Historie | `/history` | Teplota / tlak / vlhkost, min/max, 8 h / 24 h / 7 dní / 30 dní; výběr relace a CSV |
| Zařízení | `/device` | Napájení, senzor, další měření, poslední zápis, počet a rozsah záznamů, úložiště |
| Nastavení | `/settings` | Měření, displej, síť, přístup, import/export bez hesel a restart |
| Čas | `/time` | Stav UTC, NTP, nastavení z prohlížeče nebo provoz jen s relativním časem |
| Firmware | `/firmware` | OTA souborem `.pio/build/<environment>/firmware.bin` |

Interval lze nastavit na 60–3600 s, jas 1–255 a timeout 5–120 s. Historie nabízí posledních 64 uložených relací; úplné CSV zahrnuje i starší dochované relace.

Konfigurace se validuje a zapisuje jako jediný NVS blob. Prázdné pole hesla ponechá existující heslo; otevřenou Wi-Fi lze nastavit explicitní prázdnou hodnotou přes API. Import nejprve vyplní formulář. Změna sítě a AP hesla vyžaduje restart; webové heslo platí ihned. Dlouhé vlastní heslo displej nevypisuje celé.

HTTP Basic autentizace je ve výchozím stavu zapnutá a lze ji vypnout v Nastavení → Přístup k webu. Přepínač platí i pro CSV a OTA; heslo webu a chráněného AP se zachová, AP tím nevznikne otevřené. Náhodný CSRF token se pro změny vyžaduje i bez přihlášení. POST vyžadují servisní napájení. HTTP nemá TLS; rozhraní je určeno do důvěryhodné místní sítě, nikoli k vystavení na internet.

## Historie, čas a baterie

Záznam má 24 B, formát v2 a CRC16-CCITT; čtení zůstává kompatibilní s v1. Obsahuje relaci, uplynulé sekundy, volitelné UTC, teplotu po 0,01 °C, vlhkost po 0,01 % RH, tlak v Pa a příznaky typu čidla a dostupnosti vlhkosti. Bez vlhkosti je uloženo 0xFFFF a její příznak není nastavený. U v1 platné měření znamená BME280 s vlhkostí, včetně skutečných 0 %. Rozlišení záznamu není přesností senzoru.

Minutový interval: 34 560 B/den, přibližně 1,04 MB/30 dní plus režie. Nejstarší bloky se odstraňují podle uplynulého času ve stejné relaci nebo při zaplnění 90 % kapacity. Retence je přibližná, po blocích až 1440 záznamů; může být o blok delší nebo zkrácená režií. U předchozích relací rozhoduje kapacita, protože skutečné stáří přes výpadek napájení neznáme. UTC se pro mazání nepoužívá; chybně nastavené datum tak historii nezestárne.

**Správné datum není podmínkou správného měření, intervalů ani historie.** Plán a grafy používají monotónně uplynulé sekundy v jedné relaci. RTC čas pokračuje přes deep sleep, včetně předčasného probuzení tlačítkem. Změna UTC plán neposune.

UTC je volitelný popisek získaný NTP nebo z prohlížeče. NTP lze vypnout a tlačítkem „Používat jen relativní čas“ také odstranit aktuální UTC. Záznamy bez UTC se zpětně nepřepisují; CSV ponechá datum prázdné. Dříve uložené datum se změnou nastavení nemění.

Po resetu/odpojení baterie vznikne nová relace. Délku vypnutí bez externích hodin nelze zjistit, proto se různé relace nespojují do zdánlivě souvislého grafu ani podle UTC. Starší relaci lze vybrat i bez data; její graf končí posledním uloženým vzorkem. CSV za rozsah filtruje podle relativního času vybrané relace, úplné CSV zachová všechny. Výpadky a chyby senzoru vytvářejí mezery.

Úložiště opravuje neúplný konec zápisu, přeskakuje chybné CRC a odmítne zapisovat za kompletní záznam neznámé verze. Po chybě zápisu zastaví logování do dalšího startu. Chybějící senzor neblokuje web a uloží se jako neplatné měření.

### Diagnostika senzoru

Od 0.2.1 ukazuje displej konkrétní chybu senzoru; `/api/status` obsahuje `sensor_error`, `sensor_address` a `sensor_chip_id`. Sériová linka 115200 vypisuje ACK, chyby Wire a identifikátor čipu (BME280 očekává 0x60). Timeout komunikace znovu vynutí inicializaci čidla.

Před inicializací se krátce ověří, zda lze obě linky nezávisle ovládat přes open-drain. Pokud některou linku nelze stáhnout nebo zůstává nízká, firmware uvolní piny, zobrazí chybu a zastaví další I2C pokusy do resetu nebo výslovné diagnostiky. Po opravě zapojení resetuj desku.

Po odeslání znaku `i` přes sériovou linku provede diagnostiku: kontrolu linek, softwarové čtení ID a hardwarový scan na 100/10 kHz, případně s prohozením SDA/SCL. Při elektrické chybě se test ihned zastaví; při opakovaných chybách řadiče se scan zkrátí. Na konci se obnoví standardní piny 21/22. Tato operace je určena pro samostatné připojené BME280 a nezapisuje do historie ani konfigurace; běžné měření poté pokračuje.

Znak `v` vypíše poslední uložený záznam jako CSV včetně dostupnosti vlhkosti a typu čidla. Diagnostiku lze používat i s BMP085/BMP180.

ADC34 měří společnou napájecí větev. Na USB se proto napětí baterie nezobrazuje; procenta nabití se neodhadují. Pod 3,40 V (konzervativní práh, nutná kalibrace) se měření pozastaví a napájení kontroluje po 15 minutách. Toto omezení spotřeby nenahrazuje hardwarovou ochranu akumulátoru.

## OTA a návrat

OTA zapisuje neaktivní oddíl. Kontroluje obraz ESP32, velikost a značku kompatibility projektu/varianty/formátu dat/tabulky oddílů. Značka chrání před chybným výběrem souboru; **není kryptografickým podpisem**. Při přenosu se neusíná a měření se pozastaví. Přerušený přenos se zahodí. Při odpojení USB během uploadu pokračuje pouze s dostatečnou baterií (měřených alespoň 3,65 V).

Předkompilovaný Arduino bootloader nemá zapnutý automatický rollback ESP-IDF. Použit je **aplikační boot guard v NVS**: uloží předchozí a cílový oddíl; první start nové aplikace zapne watchdog a musí potvrdit konfiguraci, připojené úložiště a dostupné AP/LAN při servisu. Chybějící BME280 rollback nezpůsobí. Po resetu nepotvrzeného startu guard vybere předchozí aplikaci.

Ochrana pokrývá selhání po spuštění guardu. Neřeší selhání před vstupem do aplikace, pozdější logické chyby po potvrzení ani změny bootloaderu/oddílů. Budoucí změny persistentních formátů musí umožnit návrat k předchozí verzi.

## Testy

```sh
pio run -e ttgo-t-display -e ttgo-t-display-usb-sense -e ttgo-t-display-service
python tools/check.py
```

Host kontroly potřebují C++11 `g++` a Node.js; mimo PATH použij `--cxx <cesta>` a `--node <cesta>`. Spouštějí skutečný kód formátu, hodin, plánování a úložiště nad souborovým testovacím prostředím a skutečný JS webu nad simulovaným API. Nepotvrzují chování flash, ADC, Wi-Fi ani spánku na ESP32.

GitHub Actions sestaví všechny varianty, provede kontroly a uloží firmware, bootloader a tabulku oddílů jako artefakty. Viz [plán ověření na desce](docs/VALIDATION.md) a [přehled implementace](docs/IMPLEMENTATION.md).
