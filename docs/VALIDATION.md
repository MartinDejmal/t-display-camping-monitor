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

## Před pájením: USB a servisní varianta

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
