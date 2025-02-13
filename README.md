# TankBusters

<!--toc:start-->
- [TankBusters](#tankbusters)
  - [Opis gry](#opis-gry)
  - [Zależności](#zależności)
  - [Pobranie źródeł, budowanie projektu](#pobranie-źródeł-budowanie-projektu)
  - [Uruchomienie](#uruchomienie)
<!--toc:end-->

## Opis gry

Gra Tank Busters (nasza własna przeróbka gry asteroids PvPvE dla 2-4 graczy)

Klient po uruchomieniu gry wybiera jeden z dostępnych pokoi:

  1. Rozgrywka jest już w toku -
     gracz dołącza jako obserwator i czeka do końca danej rudy.
  2. Gdy runda się zakończy -
     serwer zaprasza wszystkich graczy w pokoju do nowej rundy
  3. Rozgrywka jeszcze nie wystartowała -
     gracz dołącza do lobby, jeżeli w pokoju
     jest przynajmniej 2 gotowych graczy,
     to rozgrywka wystartuje automatycznie
     po upływie danego czasu. Jeżeli gracz
     nie zdąży zgłosić swojej gotowości to staje się obserwatorem.

Gracz może zdecydować się opuścić dany pokój
przed rozpoczęciem gry i wrócić do wyboru dostępnych pokoi.

Przebieg rozgrywki:
Serwer rozgłasza rozpoczęcie nowej rozgrywki i rozpoczyna nową rundę.
Po rozpoczęciu rundy serwer wymienia informacje z klientami o obiektach
(graczach oraz “asteroidach”) znajdujących się na mapie gry.

Jeżeli dany obiekt zostanie trafiony to serwer rozgłasza informacje do
wszystkich graczy o zniszczeniu tego obiektu. W przypadku, kiedy to dany
gracz zostanie trafiony, zmienia on swoją rolę na obserwującego.

Rozgrywkę wygrywa gracz, który jako ostatni pozostanie przy życiu
(nie zostanie trafiony żadnym pociskiem ani asteroidą).
Pojawia się krótkie podsumowanie rundy (kto wygrał daną rozgrywkę).
Następnie serwer restartuje pokój (ponownie czeka, aż przynajmniej
2 graczy zgłosi gotowość, aby rozpocząć odliczanie do nowej rundy).

Interakcja między graczami:
Gracze mogą się poruszać, strzelać pociskami, niszczyć “asteroidy”, innych
graczy lub być sami zniszczeni przez innych graczy.

## Zależności

Zewnętrzne:

- `C++ 17`
- `cmake 3.20`

Pobierane przy pierwszej kompilacji:

- `raylib` <https://github.com/raysan5/raylib/>
- `nlohmann/json` <https://github.com/nlohmann/json/>

Wszystkie funkcje sieciowe opierają się wyłącznie na POSIX (BSD) Sockets API.

## Pobranie źródeł, budowanie projektu

```bash
git clone https://github.com/KamieniarzJakub/TankBusters.git TankBusters
cd TankBusters
cmake -B build
cmake --build build -j
```

## Uruchomienie

Klient:

```bash
./build/TankBustersClient
```

Serwer:

```bash
./build/TankBustersServer [PORT]
```
