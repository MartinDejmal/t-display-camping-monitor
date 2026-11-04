# Ověření na skutečném T-Display

Fyzické ověření je částečně zahájené; dosud potvrzené body jsou níže. Spotřeba a kompletní funkční zkoušky zbývají.

## První nahrání — 4. 11. 2026

- Před nahráním vytvořena úplná záloha původní 4MiB flash z COM14 v lokální `work/baseline/device-COM14-before.bin`. SHA256: `7e77368f034b8bd7ccbd63fceef59f76694340e3901ff51ddb80bd3ffe6d6e72`.
- Zdrojový ZIP revize 826f932 a původní referenční firmware jsou ve stejné ignorované složce.
- Na COM14 detekován ESP32-D0WDQ6 revize 1.0. Nový oddíl pro historii byl před nahráním prázdný.
- Nahrána servisní varianta 0.2.0, bootloader i nová tabulka oddílů. Kontrola hashů zápisu prošla.
- Po hard resetu sériový výstup potvrdil `bench=1 history=ok settings=ok` a `sensor=missing log=ok`. Chybějící BME280 odpovídá dosud neprovedenému zapojení.
- Sériový záznam je lokálně v `work/service-boot-COM14.log`. Web, skutečné hodnoty senzoru, displej a spotřeba tímto nejsou ověřené.
- GitHub Actions pro kódový commit `2f7b414` dokončily sestavení a hostitelské kontroly úspěšně také na Linuxu.

## Provedené hostitelské kontroly — 4. 11. 2026

- Všechny tři varianty: úspěšné sestavení, PlatformIO Core 6.2.0 / Espressif32 6.13.0 / Arduino-ESP32 2.0.17.
- Velikost výsledných obrazů: baterie 1 024 608 B, servis 1 024 624 B, USB sense 1 024 720 B; každý se vejde do 1 441 792 B OTA oddílu.
- Core test: CRC referenční vektor, poškození každého bajtu, verze schématu, záporné teploty, přeskočené termíny, předčasné probuzení a neznámé časové mezery.
- OTA marker: shoda/neshoda variant a všechny hranice chunků; přítomnost správné značky ve všech binárních obrazech.
- Skutečný storage.cpp nad hostitelským souborovým testovacím prostředím: první inicializace, odmítnutí formátu poškozeného oddílu, krátký zápis, CRC oprava, ochrana neznámé verze, kapacitní rotace, časová retence a mezery grafu.
- Skutečný JavaScript rozhraní nad simulovaným DOM/API: první vykreslení, historie, CSRF, ukládání konfigurace a spuštění CSV exportu; kontrola syntaxe Node.js.
- Tabulka oddílů: zarovnání, žádné překryvy, dva stejně velké OTA oddíly a nominální prostor pro 30 dní minutových záznamů.
- git diff --check: bez chyb whitespace.

Tato ověření nenahrazují níže uvedené testy na skutečném zařízení.

## Diagnostika připájeného BME280 — 4. 11. 2026

Modul uživatele: LaskaKit LA131017, výchozí adresa 0x77, vlastní pull-up rezistory. Na COM14 otestovány obě adresy, přímé čtení ID, dvě rychlosti a prohození SDA/SCL. Hardwarový řadič vracel Wire=5 (timeout); nebylo možné přečíst ID.

Při testu mimo hardwarový I2C řadič: GPIO21 šel stáhnout do nuly, GPIO22 zůstal v jedničce i při příkazu stáhnout jej přes open-drain. Stejné chování GPIO22 potvrdila zkouška v obrácené roli SDA. Softwarové I2C neobdrželo ACK na 0x76 ani 0x77. To ukazuje na elektrický problém kolem pinu 22/SCL, například spoj s napájením nebo vadný vývod; přesnou příčinu je nutné ověřit měřením a izolováním vodiče. Zda jde o závadu modulu, pájení, vodiče nebo desky, zatím není potvrzeno.

FW 0.2.1 doplňuje diagnostiku a zastavení komunikace při chybě linek. Průběžný záznam diagnostiky je lokálně v `work/sensor-diagnostic-COM14.log`. Po opětovném připojení COM14 byla nahrána i finální ochrana: stejná chyba GPIO22 se potvrdila po výslovném uvolnění GPIO hold a firmware zastavil další komunikaci. Úložiště i konfigurace zůstaly funkční. Uživatel ověřil kontinuitu vodiče 22–SCL; kolísající odpor mezi 22 a 3V v zapojeném obvodu neurčuje příčinu. Dalším krokem je rozpojení vodiče SCL a opakování testu samotného GPIO22, aby se oddělila deska od modulu/propojení.

Po odpájení spoje 22–SCL uživatelem proběhl nový test na COM14: samotný GPIO22 se úspěšně stáhne do nuly (`SCL driven LOW: 1/0`), stejně jako GPIO21 (`SDA driven LOW: 0/1`). Kontrola linek prošla opakovaně a I2C místo timeoutu vrací Wire=2 (NACK) na 0x76/0x77, což je při odpojeném SCL očekávané. Chyba je tím lokalizovaná na připojenou vnější část nebo původní pájený spoj; samotný výstup GPIO22 tímto testem funguje. Příčinu na modulu/vodiči/pájení zbývá zjistit.

Po výměně uživatelem za jiný modul (pravděpodobně BMP180) byla na COM14 opakovaně přečtena adresa 0x77 a chip ID 0x55. Obě linky prošly kontrolou a I2C komunikace je funkční. ID odpovídá rodině BMP180/BMP085, nikoli BME280 (0x60); firmware proto správně odmítá načíst BME280 data a ponechává měření neplatné. Toto náhradní čidlo neměří vlhkost. Ověření skutečných hodnot BME280 a přesné příčiny závady původního modulu zbývá.

## Podpora BMP085/BMP180 ve 0.3.0 — 4. 11. 2026

- Přidáno automatické rozpoznání BME280 (0x60) a BMP085/BMP180 (0x55); u BMP se měří T/P a vlhkost se označuje jako nedostupná.
- Nový formát v2 má stále 24 B; čte se i v1. Host testy ověřily původní BME záznam s reálnými 0 % RH, nové BMP bez RH, smíšené grafové koše, opětovné otevření historie a přesné CSV řádky. Webový JS test ověřil `humidity=null`, zobrazení Neměří, zachování T/P a žádné nečíselné souřadnice grafu.
- Všechny varianty se sestavily: baterie 1 032 688 B, servis 1 032 704 B, USB sense 1 032 800 B; vejdou se do OTA oddílů. Kompletní host kontroly prošly.
- Servisní 0.3.0 nahrána na COM14 přes USB s kontrolou hashů. Historie zůstala připojená a konfigurace se načetla. Nedošlo k formátování úložiště.
- Při skutečném 68s testu prošla identifikace čidla i dvě plánovaná měření: v čase 0 s T=23,50 °C, P=98 567 Pa; v čase 60 s T=23,90 °C, P=98 574 Pa. Obě měla `sensor=ok log=ok`, typ BMP085/180 a RH=NA.
- Sériový příkaz `v` dvakrát přečetl poslední uložený záznam. CSV obsahovalo platné T/P, prázdné RH, sensor_ok=1, humidity_available=0 a sensor_type=BMP085/180. Kontrolní skript ověřil postupující čas a rozumné rozsahy hodnot.
- Záznam skutečného testu je lokálně v `work/bmp-live-verification.log`. Kontrola webu se zatím opírá o hostitelský test JS/API; fyzický BME280 a celé OTA nadále zbývá ověřit.

## Web, tlakové grafy a relativní čas ve 0.4.0 — 5. 11. 2026

- Všechny tři varianty sestaveny: baterie a servis 1 058 048 B, USB sense 1 058 128 B; OTA slot má 1 441 792 B. RAM 60 920 B. Host kontroly prošly.
- Skutečný kód hodin nad testovacím RTC ověřil chybějící UTC, posun dopředu/dozadu, vymazání UTC, NTP změny, předčasné probuzení ze spánku a novou relaci po resetu. Intervaly a monotónní čas se při změně UTC neposunuly.
- Skutečné úložiště ověřilo tlaková minima/maxima, výběr starších relací bez UTC, průběžné souhrny a zachování historie při nesmyslném budoucím datu jiné relace. Retence i graf používají relativní čas jedné relace.
- Webový test používá skutečných šest HTML fragmentů a skutečný JS. Ověřil stránky, graf tlaku/0 % RH/chybějící RH, volbu relace, údaje zařízení, částečné změny konfigurace, import bez hesel, přepínač přihlášení a ruční/relativní čas. Samostatný test politiky přístupu ověřil oba režimy přihlášení a nutnost napájení + CSRF pro zápis.
- V prohlížeči vizuálně ověřeny Historie (včetně přepnutí na tlak), Zařízení, Čas a Nastavení nad skutečnými šablonami s jasně označenými ukázkovými daty. Nejde o fyzický test formulářů na desce.
- Servisní varianta 0.4.0 nahrána přes COM14, hash zápisu ověřen; historie i konfigurace se načetly. V 68s testu dva platné BMP vzorky: elapsed=0, T=22,30 °C, P=98 552 Pa bez UTC; elapsed=60, T=22,20 °C, P=98 538 Pa s UTC ze sítě. Oba `sensor=ok log=ok`, RH=NA; sériové CSV potvrzuje uložené hodnoty.
- Na LAN 192.168.0.120 ověřeny odpovědi všech šesti stránek, CSS/JS a statusu podle aktuálního přepínače přihlášení. Status, seznam relací a aktuální historie vrátily skutečné počty, session_elapsed a sedmiprvkové koše s tlakem; při delší kontrole se objevily síťové timeouty a ping ztratil 7 z 8 paketů. V servisním režimu proto vypnuto uspávání rádia Wi-Fi; na baterii se Wi-Fi nadále vypíná úplně.
- Po vypnutí uspávání Wi-Fi: 8/8 pingů, 6–58 ms; celý skutečný HTTP test prošel (status, relace, tlakové koše, výběr předchozí relace, stránky a prostředky) s odpověďmi 0,06–0,23 s. Změna zlepšila aktuálně pozorované spojení; dlouhodobou stabilitu je nutné sledovat.
- Záznamy kontrol jsou lokálně v `work/tabs-http-verification.log`, `work/tabs-checks.log`, `work/tabs-build.log`, `work/tabs-upload.log` a `work/tabs-live-verification.log`. Kompletní OTA/rollback, spotřeba a bateriová výdrž stále nejsou ověřené.

## Před pájením: USB a servisní varianta — kontrolní body

- [x] Nahrát `ttgo-t-display-service` přes USB s novou tabulkou oddílů.
- [ ] Ověřit displej a tlačítka, graf a servisní údaje.
- [ ] Bez BME280 vidět chybu senzoru; web musí fungovat.
- [ ] Připojit AP a admin; ověřit CSV s příznakem chybějícího čidla.
- [ ] Změnit konfiguraci a restartovat; ověřit limity a zachování.
- [ ] Import/export konfigurace bez hesel, čas z prohlížeče.
- [ ] Domácí Wi-Fi a záložní AP při chybném hesle.

## Po zapojení

- [ ] Na pinu 32 přibližně 2,5 V při USB a 0 V bez USB; ověřit skutečné VBUS.
- [ ] BME280 na 3V/G/21/22, správné a čerstvé T/RH/P.
- [ ] Nahrát `ttgo-t-display-usb-sense` přes USB.
- [ ] Servis na USB a vypnutá Wi-Fi po odpojení.
- [ ] Na baterii 15s timeout, wake pravým tlačítkem a minutové měření bez blikání.
- [ ] Probuzení tlačítkem po 5 s z plánovaného 60s spánku neposune další termín.
- [ ] Držené tlačítko nevytvoří rychlou smyčku resetů.
- [ ] Připojení USB ve spánku: servis při nejbližší kontrole nebo po tlačítku.
- [ ] ADC porovnat s multimetrem, kalibrovat práh nízké baterie.

## Historie

- [ ] Více než 8 hodin provozu: minutové termíny a obě osy grafu.
- [ ] Restart i odpojení baterie zachovají historii; neznámou mezeru graf nespojí.
- [ ] Odpojený senzor vytvoří mezeru a příznak chyby v CSV.
- [ ] Výpadky napájení během zápisu zachovají starší dokončené záznamy.
- [ ] Rotace při naplnění oddílu a časová retence.
- [ ] Úplný CSV export při měření: počet a obsah záznamů.
- [ ] Nečitelný filesystem: chyba bez automatického formátu.

## OTA

- [ ] Nová verze stejné varianty zachová data a konfiguraci.
- [ ] Jiná varianta, poškozený nebo neplatný obraz se odmítnou.
- [ ] Přerušený přenos ponechá funkční původní aplikaci.
- [ ] Odpojení USB: dostatečná baterie dokončí přenos, nízká jej přeruší.
- [ ] Záměrné selhání po startu guardu před potvrzením: watchdog a návrat.
- [ ] Chybějící senzor rollback nezpůsobí, nečitelná konfigurace/úložiště ano.

## Spotřeba a výdrž

- [ ] Proud měřit na straně baterie, bez USB, ideálně jako záznam v čase.
- [ ] Klidový odběr, délka/proud měření, odběr displeje i servisu.
- [ ] Ověřit skutečný vliv GPIO14, uspání ST7789 a podsvícení.
- [ ] Plný 600mAh akumulátor; zaznamenat četnost displeje a okolní podmínky.
- [ ] Alespoň 72 hodin provozu: úplnost historie, restarty, napětí.
- [ ] Naměřený náboj/průměrný proud a dosaženou výdrž doplnit do dokumentace.
