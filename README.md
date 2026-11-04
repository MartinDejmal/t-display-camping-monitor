# T-Display environmental logger

Firmware pro **původní TTGO T-Display, ESP32, ST7789 240 × 135** a jeden BME280. Není určen pro T-Display-S3.

Měří standardně jednou za minutu, ukládá historii do interní flash a mezi měřeními používá deep sleep. Displej se při časovaném probuzení neinicializuje. Při externím napájení lze zpřístupnit web, CSV, konfiguraci a OTA.

## Stav

Implementace 0.2.0: všechny tři varianty se sestavují, hostitelské testy ověřují formát, časování, úložiště a akce webu. **Fyzické zapojení, odběry, OTA na desce a 72hodinový bateriový test zbývají ověřit. Výdrž není změřená.**

Původní funkční příklad zachovává lokální tag `baseline-working-20261104`; nová implementace vzniká ve větvi `firmware-logger`. Zachována je konfigurace displeje; DHT22, externí baterie a původní obrazovky byly nahrazeny.

## Zapojení

Displej k sobě, USB dole. Použité popisky přímo na desce:

| T-Display | BME280 |
|---|---|
| `3V` | VCC / VIN |
| `G` | GND |
| `21` | SDA / SDI |
| `22` | SCL / SCK |

Šestipinový modul: CS / CSB na `3V`, SDO na `G` (adresa 0x76). Firmware zkouší i 0x77 a kontroluje BME280; BMP280 neumí vlhkost. Modul s LED nebo neúsporným regulátorem může zhoršit výdrž. Čidlo umísti mimo teplo desky a nabíjení.

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

Historie má 1216 KiB; oba OTA oddíly mají po 1408 KiB. LittleFS se automaticky inicializuje pouze tehdy, když je celý jeho oddíl prázdný (0xFF). Při poškození nebo nekompatibilních starších datech se **neformátuje**. Chyba je vidět jako `Log ERROR` a na webu. Před případnou ruční obnovou zálohuj flash.

Nepoužívej `uploadfs` pro běžnou aktualizaci: souborový oddíl obsahuje historii. Web je zabudovaný ve firmwaru.

## Ovládání

- Pravé tlačítko probudí a zobrazí čerstvé hodnoty. Další stisky přepínají hodnoty a dva grafy posledních 8 hodin.
- Levé tlačítko zobrazí servisní informace, síť, adresu a heslo.
- Po 15 s od posledního stisku displej zhasne. Na baterii následuje deep sleep, na USB web běží dál.
- Časovač změří, uloží a opět uspí zařízení bez rozsvícení.
- Držené pravé tlačítko nevyvolává okamžitou smyčku probouzení: dokud je držené, používá se pouze krátký časovač.

Ve finální variantě se připojení USB pozná při nejbližším měřicím probuzení, obvykle do minuty; tlačítkem dříve. Při nízké baterii se kontrola prodlužuje na 15 minut.

## Web

Při prvním servisu vznikne síť `tdisplay-xxxxxx`. Náhodné heslo se uloží do NVS. **Heslo AP i webu ukáže levé tlačítko**, webový uživatel je `admin`. Adresa AP je standardně `http://192.168.4.1`; řiď se displejem.

Po nastavení domácí Wi-Fi se zařízení připojí k ní. Pokud připojení do 15 s nevyjde nebo síť později vypadne, vytvoří záložní AP. mDNS v LAN: `tdisplay-xxxxxx.local`. Aktivované záložní AP zůstává až do restartu nebo odpojení USB.

Web nabízí hodnoty a stav zařízení; graf 8 h / 24 h / 7 dní / 30 dní s min/max; CSV za rozsah nebo vše; interval 60–3600 s, jas 1–255, timeout 5–120 s, Wi-Fi a heslo; import/export konfigurace bez hesel; čas z prohlížeče nebo NTP; OTA nahráním `.pio/build/<environment>/firmware.bin`.

Konfigurace se validuje a zapisuje jako jediný NVS blob. Prázdné pole hesla ponechá existující heslo; otevřenou Wi-Fi lze nastavit explicitní prázdnou hodnotou přes API. Import nejprve vyplní formulář. Změna sítě a AP hesla vyžaduje restart; webové heslo platí ihned. Dlouhé vlastní heslo displej nevypisuje celé.

Použita je HTTP Basic autentizace a náhodný CSRF token pro změny. POST vyžadují servisní napájení. HTTP nemá TLS; rozhraní je určeno do důvěryhodné místní sítě, nikoli k vystavení na internet.

## Historie, čas a baterie

Záznam má 24 B, formát v1 a CRC16-CCITT. Obsahuje relaci, uplynulé sekundy, volitelné UTC, teplotu po 0,01 °C, vlhkost po 0,01 % RH, tlak v Pa a příznaky. Rozlišení záznamu není přesností senzoru.

Minutový interval: 34 560 B/den, přibližně 1,04 MB/30 dní plus režie. Nejstarší bloky se odstraňují podle času nebo při zaplnění 90 % kapacity. Retence je přibližná, po blocích až 1440 záznamů; může být o blok delší nebo zkrácená režií. U starých relací bez UTC rozhoduje kapacita, protože skutečné stáří neznáme.

RTC čas pokračuje přes deep sleep, včetně předčasného probuzení tlačítkem. UTC se získá NTP nebo z prohlížeče. Po resetu/odpojení baterie vznikne nová relace. Záznamy bez UTC se zpětně nepřepisují; úplný CSV je zachová s prázdným UTC. Graf aktuální relace používá uplynulý čas; předchozí relace zařadí pouze se známým UTC. Výpadky a chyby senzoru vytvářejí mezery.

Úložiště opravuje neúplný konec zápisu, přeskakuje chybné CRC a odmítne zapisovat za kompletní záznam neznámé verze. Po chybě zápisu zastaví logování do dalšího startu. Chybějící senzor neblokuje web a uloží se jako neplatné měření.

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

Host kontroly potřebují C++11 `g++` a Node.js; mimo PATH použij `--cxx <cesta>` a `--node <cesta>`. Spouštějí skutečný kód formátu, plánování a úložiště nad souborovým testovacím prostředím a skutečný JS webu nad simulovaným API. Nepotvrzují chování flash, ADC, Wi-Fi ani spánku na ESP32.

GitHub Actions sestaví všechny varianty, provede kontroly a uloží firmware, bootloader a tabulku oddílů jako artefakty. Viz [plán ověření na desce](docs/VALIDATION.md) a [přehled implementace](docs/IMPLEMENTATION.md).
