# Wersja rozproszona narzędzia arp-scan

# Opis projektu
Architektura rozwiązania oparta jest na scentralizowanym serwerze, który identyfikuje hosty dostępne w sieci lokalnej, a następnie rozdziela zakresy adresów IP do wybranych serwerów przetwarzających (tzw. node’ów). Każdy z node’ów odpowiada za wykonanie skanowania ARP wyłącznie dla przydzielonego zakresu oraz przesłanie wyników z powrotem do serwera centralnego, który agreguje zebrane informacje.

Dzięki równoległemu wykonywaniu skanowania przez wiele node’ów, czas potrzebny na pełne przeskanowanie sieci zostaje istotnie skrócony, co może być szczególnie przydatne w przypadku dużych środowisk sieciowych.

# Zawartość plików źródłowych i kompilacja
Projekt składa się z dwóch plików źródłowych:
-  centralized_server.c zawiera kod źródłowy serwera centralnego.
-  server_node.c zawiera kod źródłowy node'a wykonującego skanowanie.

Przygotowany został plik Makefile, który pozwoli skompilować oba pliki źródłowe:
-  polecenie: make arpscan, w przypadku kompilacji serwera centralnego
-  polecenie: make node, w przypadku kompilacji node'a (wymagana jest wcześniejsza instalacja bibliotek libnet oraz libpcap)

# Sposób uruchomienia

Aby wykonać rozproszony arp-scan niezbędne będzie wcześniejsze przygotowanie poszczególnych node'ow. Skompilowany program node'a uruchamia się poleceniem: ./node INTERFEJS

Po uruchomieniu node'ow, można uruchomić serwer centralny: ./arpscan -i INTERFEJS -l -n ADRES IP NODE'A 1 -n ADRES IP NODE'A 2 (możliwe wskazanie wielu node'ow)
