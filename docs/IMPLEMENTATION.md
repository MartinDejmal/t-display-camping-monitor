# Implementační milníky

| Krok | Stav SW | Zbývá ověřit na desce |
|---|---|---|
| 1. Výchozí stav | Původní build ověřen, lokální baseline tag | Původní příklad potvrzen uživatelem |
| 2. Struktura a flash | Moduly, připnuté přímé závislosti, dva OTA oddíly a LittleFS | První USB nahrání nové tabulky |
| 3. Napájení a spánek | Tři varianty, displej, GPIO14, timer + GPIO35 | VBUS, pull-up, odběr |
| 4. Měření a plán | Automaticky BME280 / BMP085/BMP180, minutový plán, RTC relace, chyby | Čerstvost a hodnoty BME, čas |
| 5. Historie | CRC, rotace, oprava konce, ochrana před formátem | Reálné odpojení při zápisu |
| 6. Ovládání | Tlačítka, timeout, nezávislý plán, nízká baterie | Debounce, držené tlačítko, ADC |
| 7. Graf | T + RH nebo tlak, 8 h, min/max, mezery | Čitelnost |
| 8. Servis | STA + AP, hystereze VBUS, návrat na baterii | Přechody napájení a sítě |
| 9. Web | Sedm samostatných stránek, pět prioritních sítí a sken, historie relací, stav, CSV, konfigurace, čas, volitelné přihlášení | Reálný prohlížeč a souběh exportu |
| 10. OTA a výdrž | Dual slot, aplikační guard, CI a host testy | Skutečné OTA/rollback, spotřeba a 72 h |

Aktuální varianta 0.5.0 s detekcí USB na pinu 33 je nahraná na COM14. BMP085/180 měří a ukládá; konfigurace a historie se načetly. Ověřeny změna UTC bez posunu měření, skutečný deep sleep a úspěšná/přerušená OTA včetně návratu po nepotvrzeném startu. Podrobnosti jsou v VALIDATION.md. Aplikační guard má omezení popsaná v README.

## Moduly

- `main.cpp`: životní cyklus, ovládání a plán.
- `clock_state.*`: RTC s CRC, skutečný čas spánku, UTC offset.
- `record.h`: v1/v2 formát, CRC, CSV a časové funkce bez Arduino závislostí.
- `sensor.*`, `sensor_type.h`: automatické rozpoznání BME280 a BMP085/BMP180, I2C a validace.
- `storage.*`: LittleFS, blokový log, obnova, retence podle relativního času, agregace T/RH/P a líně vytvářený souhrn historie/posledních 64 relací. Bateriové starty souhrn zbytečně neskenují.
- `power.*`: VBUS a měření společné napájecí větve.
- `display.*`: ST7789, hodnoty, graf a servisní informace.
- `settings.*`: konfigurace v jednom NVS blobu, migrace v1 → v2, pět profilů a zachování hesla podle SSID.
- `network.*`: pořadové připojování bez podmínky skenu, časové limity, jednorázové bateriové NTP a vypnutí rádia.
- `bounded_webserver.*`: omezený GET/JSON parser a blokový příjem jediného multipart firmwaru; délkové/časové limity a omezené socketové zápisy. Používá routování, Basic a odpovědi připnutého Arduino WebServeru bez jeho úpravy.
- `service.*`, `web_ui.h`: Wi-Fi, HTTP API a web bez internetových závislostí.
- `ota_guard.*`, `ota_marker.h`: zkušební boot a kontrola kompatibility.

## API

Při `auth_enabled=true` všechny endpointy vyžadují Basic přihlášení `admin`; výchozí hodnota je true i při načtení staré konfigurace. POST při obou režimech vyžadují `X-CSRF-Token` z `/api/status` a servisní napájení.

| Metoda | Cesta | Obsah |
|---|---|---|
| GET | `/`, `/history`, `/device`, `/wifi`, `/settings`, `/time`, `/firmware` | Samostatné stránky s navigačními záložkami |
| GET | `/assets/app.css`, `/assets/app.js` | Společné lokální prostředky |
| GET | `/api/status` | Hodnoty, čidlo, napájení, čas/relace, další měření, poslední zápis a platné měření, počty/rozsah historie, kapacita, veřejná konfigurace a CSRF; nedostupná humidity=null |
| GET | `/api/history?hours=8&session=N` | 480 košů: minT,maxT,minRH,maxRH,session,minP_hPa,maxP_hPa; výpadek=null, chybějící RH=null na jejích pozicích. Session volitelná, výchozí aktuální. Metadata time_basis=session_elapsed, reference_elapsed_s |
| GET | `/api/sessions` | Celkový počet a posledních 64 relací s počty a rozsahem elapsed/UTC |
| GET | `/api/export.csv` | Vše včetně chyb senzoru |
| GET | `/api/export.csv?hours=24` | Rozsah relativního času aktuální nebo zadané relace (session=N); 1–720 hodin |
| POST | `/api/config` | Částečná změna: interval_s, screen_timeout_s, brightness, wifi_profiles (pět objektů ssid/hidden/password/open), ssid (legacy), auth_enabled, auto_time, wifi_password, admin_password, clear_wifi; ostatní pole se zachovají |
| POST / GET | `/api/wifi/scan` | Spuštění / stav asynchronního skenu; networks obsahuje nejvýše 20 objektů ssid/rssi/secure. POST vyžaduje CSRF a USB. |
| POST | `/api/time` | `{ "epoch": <UTC seconds> }` nebo `{ "clear": true }` (vymaže UTC a vypne NTP, zachová monotónní čas) |
| POST | `/api/restart` | Restart po odeslání odpovědi |
| POST | `/api/update` | Multipart soubor v poli firmware |

CSV export pravidelně obsluhuje plán měření a přenáší malé chunky. Při odpojení nebo 60s limitu spojení ukončí bez závěrečného chunku, aby klient poznal neúplný přenos. Server je synchronní; velmi pomalý klient přesto může plán zpozdit o čekání na síťový zápis. Zmeškané termíny se nedoplňují falešnými vzorky.

V 0.3.0 přidává CSV poslední dvě pole `humidity_available` (platná hodnota v tomto záznamu) a `sensor_type`. V2 flagy: SENSOR_OK=1, UTC=2, HUMIDITY=4, BME280=0x10, BMP085/180=0x20. Verze v horních čtyřech bitech je 0x2000; v1=0x1000 je nadále čitelná. Rozdělení flash se nemění. Během nepotvrzeného OTA startu se do historie nezapisuje.

Ve 0.4.1 `/api/status` navíc obsahuje `uptime_s`, `free_heap_bytes`, `min_free_heap_bytes`, `largest_free_block_bytes`, `loop_stack_free_min_bytes` a `reset_reason` (ESP-IDF). Pole slouží dlouhodobému sledování přes `tools/soak.py`. Grafové koše displeje jsou statické, aby přibližně 5 KiB nespotřebovávaly 8KiB zásobník hlavní smyčky. Formát záznamů a nastavení zůstává kompatibilní.

Ve 0.4.2 `session_minmax` ve statusu obsahuje `temperature_min`, `temperature_max` (°C), `pressure_min_hpa`, `pressure_max_hpa`, `humidity_min`, `humidity_max` (% RH). Bez platného uloženého vzorku je celý objekt null; bez dostupné vlhkosti jsou null její dva extrémy. Stejný souhrn používá displej; rozsah je celá dostupná historie aktuální relace, nezávisle na grafovém okně a UTC.

Běžný HTTP požadavek má limit 3 s, jednotlivý socketový zápis 1,5 s. Při OTA se měření záměrně pozastaví; při 10 s bez příchozích dat nebo celkem 180 s se přenos přeruší a původní aplikace pokračuje.

Od 0.4.3 USB varianta přidává do kompatibilitní značky `SENSE=33`. Přechod ze servisní varianty nebo starého zapojení GPIO32 se provádí přes USB. Vynucený servisní build zůstává samostatnou zkušební variantou.


## Wi-Fi a NTP od 0.5.0

Pořadí sítí určuje pole pěti profilů; prázdné SSID pozici vypne. Připojení zkouší každou známou síť přímo, včetně skrytého SSID, bez předchozího filtru podle skenu. Přesun známého SSID s vynechaným/prázdným heslem zachová jeho původní tajemství. Nový SSID vyžaduje heslo nebo `open:true`; explicitní otevřená síť heslo odstraní. Veřejný status/export obsahuje pouze `has_password`, nikdy heslo. Profilová a legacy pole nelze v jednom požadavku míchat. Konfigurace v2 se zapisuje jako jediný blob; v1 se nejprve převádí pouze v RAM.

Nová bateriová relace zkusí NTP před prvním vzorkem: 5 × 15 s Wi-Fi, nejvýše 8 s NTP, celkový strop 83 s. Platí při auto_time=true, známém napětí ≥3,50 V a mimo brownout/nepotvrzený OTA start. Rádio se vždy vypne. Deep sleep relaci zachová a nové spojení nevyvolá. Skutečná odpověď SNTP callbacku je podmínkou synchronizace; starý systémový čas se za novou odpověď nevydává.

Sken se obsluhuje asynchronně s vlastním 15s limitem; během něj se nespouští reconnect. Předčasné WIFI_SCAN_FAILED z Arduino knihovny se do vypršení tohoto limitu nepovažuje za dokončený sken. Web při přechodném výpadku HTTP opakuje dotaz; výběr výsledku vyplní pole ručně editovatelného SSID. Prázdné názvy skrytých sítí se v nabídce neobjevují.
