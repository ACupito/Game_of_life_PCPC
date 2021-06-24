# Game_of_life_PCPC
 Il Gioco della vita (Game of Life in inglese, noto anche solo come Life) è un automa cellulare sviluppato dal matematico inglese John Conway sul finire degli anni sessanta. Il Gioco della vita è l'esempio più famoso di automa cellulare: il suo scopo è quello di mostrare come comportamenti simili alla vita possano emergere da regole semplici e interazioni a molti corpi.

Si tratta in realtà di un "gioco senza giocatori", intendendo che la sua evoluzione è determinata dal suo stato iniziale, senza necessità di alcun input da parte di giocatori umani. Si svolge su una griglia di caselle quadrata (celle); definita **mondo**. Ogni cella ha otto **vicini**, che sono celle ad essa adiacenti. Ogni cella può trovarsi in due stati: **viva** o **morta**. Lo stato della griglia evolve in intervalli di tempo discreti, cioè scanditi in maniera netta. Gli stati di tutte le celle in un dato istante sono usati per calcolare lo stato delle celle all'istante successivo. Tutte le celle del mondo vengono quindi aggiornate simultaneamente nel passaggio da un istante a quello successivo: passa così una **generazione**.

Le transizioni dipendono unicamente dallo stato delle celle vicine in quella generazione:
* Qualsiasi cella viva con meno di due celle vive adiacenti muore, come per effetto d'isolamento;
* Qualsiasi cella viva co due o tre celle vive adiacenti sopravvive alla generazione successiva;
* Qualsiasi cella viva con più di tre celle vive adiacenti muore, come per effetto di sovrappopolazione;
* Qualsiasi cella morta con esattamente tre celle vive adiacenti diventa una cella viva, come per effetto di riproduzione.

<p align="center">
    <img src="https://github.com/ACupito/Game_of_life_PCPC/blob/main/images/Gol-gun.gif"/>
</p>

## Come compilare ed eseguire?
Per compilare il codice .c non sono necessari particolari sforzi. Eseguire il seguente comando:

```
mpicc game_of_life.c -o game_of_life.out
```

Per eseguire il file compilato è necessario eseguire il seguente comando:

```
mpirun -np {VCPUs} game_of_life.out {righe} {colonne} {generazioni}
```

## Descrizione della soluzione adottata

### Funzioni definite
Iniziamo con il definire in particolare quelle che saranno le funzioni principali definite per Game of Life. In particolare esse definiscono la logica del gioco in relazione alla presenza di celle vive o morte.

### is_alive(bool)
Data in input una cella della matrice valuto se essa è viva o morta.

**Signature del metodo**

```c
bool is_alive(bool cell);
```
| Parametro | Tipo | Descrizione |
|--|--|--|
| cell | bool | La cella che si vuole valutare |

```c
bool is_alive(bool cell)
{
    return cell == 1;
}
```
---
### game_update(bool*, bool*, int, int)
Data in input la sottomatrice originale, la sottomatrice da aggiornare, l'indice della cella corrente e il numero di celle vive attorno ad essa, il metodo si occupa di applicare le regole del gioco descritte in precedeza.

**Signature del metodo**

```c
void game_update(bool *receive_buffer, bool *updated_buffer, int cell_index, int count)
```

| Parametro | Tipo | Descrizione |
|--|--|--|
| receive_buffer | bool* | il buffer originale, quello in cui si trova la cella da analizzare |
| updated_buffer | bool* | il buffer di destinazione, quello in cui salvare lo stato della cella che stiamo analizzando |
| cell_index | int | la cella da analizzare |
| count| int | numero di celle vive vicine alla cella da analizzare |

```c
void game_update(bool *receive_buffer, bool *updated_buffer, int cell_index, int count)
{
    //se la cella è viva applico le regole del gioco
    if (isAlive(receive_buffer[cell_index]))
    {
        if (count < 2)
            updated_buffer[cell_index] = 0;
        else if (count > 3)
            updated_buffer[cell_index] = 0;
        else
            updated_buffer[cell_index] = 1;
    }
    else
    { //se la cella è morta valuto se resuscitarla o meno
        if (count == 3)
            updated_buffer[cell_index] = 1;
        else
            updated_buffer[cell_index] = 0;
    }
}
```

### Main
Procediamo la descrizione con la spiegazione del codice main definito per Game of Life. 
Iniziamo con la presentazione delle variabili usate per il programma.

```c
int main(int argc, char *argv[])
{
    //rank e size del comunicatore
    int my_rank = 0;
    int comm_size = 0;

    //valori per la matrice
    bool *matrix; //la considero come un array per semplicità nelle operazioni
    int row = atoi(argv[1]);
    int col = atoi(argv[2]);

    //generazioni da eseguire
    int generations = atoi(argv[3]); //generazione specificate in input

    //valori per calcolare le porzioni da inviare
    int steps = 0;      //numero di generazioni da eseguire
    int numElem = 0;    //numero di elementi da inviare per processo
    int rest = 0;       //se c'è resto invio degli elementi in più ai processi finchè il resto è diverso da 0
    int *send_counts;   //array contenente il numero di elementi da inviare per ogni processo
    int *displacements; //calcolo lo spostamento relativo al buffer da inviare con la scatterv
    int count = 0;      //utile per calcolare il displacement
    bool *rec_buf;      //buffer in ricezione per la scatterv
    bool *updated_buf;  //buffer aggiornato dopo lo step di gioco
    bool *top_row;      //prima riga da inviare al predecessore
    bool *bottom_row;   //ultima riga da inviare al successore
    int prev;           //valore del predecessore
    int next;           //valore del successore

    MPI_Request top_request, bottom_request; //request per invio della prima e ultima riga
    MPI_Status status;
    MPI_Datatype life_row;

}
```

Continuiamo con l'inizializzazione dell'ambiente MPI. Specificando ed ottenento il numero di processi in esecuzione (`comm_size`) e il rank del processo in esecuzione (`my_rank`).

```c
MPI_Init(&argc, &argv);
MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
```

Successivamente dichiaro un tipo MPI contigous derivato chiamato life_row. Fondamentale per inviare direttamente ai processi le righe da manipolare. Tale tipo si compone di "numero di colonne" elementi di tipo `bool`. Infine si esegue il commit.

```c
MPI_Type_contiguous(col, MPI_C_BOOL, &life_row);
MPI_Type_commit(&life_row);
```
