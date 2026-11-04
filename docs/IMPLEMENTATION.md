# Implementační milníky

| Krok | Stav SW | Zbývá ověřit na desce |
|---|---|---|
| 1. Výchozí stav | Původní build ověřen, lokální baseline tag | Původní příklad potvrzen uživatelem |
| 2. Struktura a flash | Moduly, připnuté přímé závislosti, dva OTA oddíly a LittleFS | První USB nahrání nové tabulky |
| 3. Napájení a spánek | Tři varianty, displej, GPIO14, timer + GPIO35 | VBUS, pull-up, odběr |
| 4. Měření a plán | Automaticky BME280 / BMP085/BMP180, minutový plán, RTC relace, chyby | Čerstvost a hodnoty BME, čas |
| 5. Historie | CRC, rotace, oprava konce, ochrana před formátem | Reálné odpojení při zápisu |
| 6. Ovládání | Tlačítka, timeout, nezávislý plán, nízká baterie | Debounce, držené tlačítko, ADC |
| 7. Graf | T/RH, 8 h, min/max, mezery | Čitelnost |
| 8. Servis | STA + AP, hystereze VBUS, návrat na baterii | Přechody napájení a sítě |
| 9. Web | Historie, CSV, konfigurace, import/export, čas, přihlášení | Reálný prohlížeč a souběh exportu |
| 10. OTA a výdrž | Dual slot, aplikační guard, CI a host testy | Skutečné OTA/rollback, spotřeba a 72 h |

Implementace byla publikována do `main` a servisní varianta 0.2.0 nahrána přes COM14. Sériový start potvrdil funkční úložiště a konfiguraci; BME280 zatím není připojený. Podrobnosti jsou v VALIDATION.md. Aplikační guard je odchylka od původně zamýšleného rollbacku bootloaderu; omezení jsou v README.

## Moduly

- `main.cpp`: životní cyklus, ovládání a plán.
- `clock_state.*`: RTC s CRC, skutečný čas spánku, UTC offset.
- `record.h`: v1/v2 formát, CRC, CSV a časové funkce bez Arduino závislostí.
- `sensor.*`, `sensor_type.h`: automatické rozpoznání BME280 a BMP085/BMP180, I2C a validace.
- `storage.*`: LittleFS, blokový log, obnova, retence a agregace.
- `power.*`: VBUS a měření společné napájecí větve.
- `display.*`: ST7789, hodnoty, graf a servisní informace.
- `settings.*`: konfigurace v jednom NVS blobu.
- `service.*`, `web_ui.h`: Wi-Fi, HTTP API a web bez internetových závislostí.
- `ota_guard.*`, `ota_marker.h`: zkušební boot a kontrola kompatibility.

## API

Všechny endpointy vyžadují Basic přihlášení `admin`. POST navíc vyžadují `X-CSRF-Token` z `/api/status` a servisní napájení.

| Metoda | Cesta | Obsah |
|---|---|---|
| GET | `/` | Web |
| GET | `/api/status` | Hodnoty, sensor_type, has_humidity, čas, kapacita, veřejná konfigurace, CSRF; nedostupná humidity=null |
| GET | `/api/history?hours=8` | 480 košů: minT,maxT,minRH,maxRH,session; celý výpadek=null, chybějící RH=null pouze na jejích dvou pozicích |
| GET | `/api/export.csv` | Vše včetně chyb senzoru |
| GET | `/api/export.csv?hours=24` | Rozsah s určitelným časem; 1–720 hodin |
| POST | `/api/config` | interval_s, screen_timeout_s, brightness, ssid; volitelně wifi_password, admin_password, clear_wifi |
| POST | `/api/time` | `{ "epoch": <UTC seconds> }` |
| POST | `/api/restart` | Restart po odeslání odpovědi |
| POST | `/api/update` | Multipart soubor v poli firmware |

CSV export pravidelně obsluhuje plán měření a přenáší malé chunky. Při odpojení nebo 60s limitu spojení ukončí bez závěrečného chunku, aby klient poznal neúplný přenos. Server je synchronní; velmi pomalý klient přesto může plán zpozdit o čekání na síťový zápis. Zmeškané termíny se nedoplňují falešnými vzorky.

V 0.3.0 přidává CSV poslední dvě pole `humidity_available` (platná hodnota v tomto záznamu) a `sensor_type`. V2 flagy: SENSOR_OK=1, UTC=2, HUMIDITY=4, BME280=0x10, BMP085/180=0x20. Verze v horních čtyřech bitech je 0x2000; v1=0x1000 je nadále čitelná. Rozdělení flash se nemění. Během nepotvrzeného OTA startu se do historie nezapisuje.
