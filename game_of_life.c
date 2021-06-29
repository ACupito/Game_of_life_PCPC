#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

/* Regole del gioco per ogni generazione:
 * Muore con meno di 2 celle vicine vive
 * Vive con 2 o 3 celle vive vicine
 * Muore con più di 3 celle vive vicine
 * Se morta torna in vita con 3 celle vive vicine
 */

//check per verificare se la cella è viva o meno
bool is_alive(bool cell)
{
    return cell == 1;
}

//applico le regole del gioco per la generazione attuale
void game_update(bool *receive_buffer, bool *updated_buffer, int cell_index, int count)
{
    //se la cella è viva applico le regole del gioco
    if (is_alive(receive_buffer[cell_index]))
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

int main(int argc, char *argv[])
{
    //rank e size del comunicatore
    int my_rank = 0;
    int comm_size = 0;

    //valori per la matrice
    bool *matrix; //la considero come un array per semplicità nelle operazioni
    int row = atoi(argv[1]);
    int col = atoi(argv[2]);
    
    /* Matrice usata per la verifica della correttezza
     * bool matrix[60] = {0,0,0,0,0,0,
     *                   0,0,0,1,1,0,
     *                   0,0,0,0,1,0,
     *                   0,0,0,0,0,0,
     *                   0,0,0,0,0,0,
     *                   0,0,0,1,1,0,
     *                   0,0,1,1,0,0,
     *                   0,0,0,0,0,1,
     *                   0,0,0,0,1,0,
     *                   0,0,0,0,0,0};
     */

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
    MPI_Group world_group; //gruppo primario per la comunicazione
    MPI_Group new_group;
    MPI_Comm NEW_MPI_COMM_WORLD; 

    MPI_Init(&argc, &argv);

    //dichiaro un tipo derivato da usare per comunicare i dati ai processi, mi serve perchè io devo inviare le righe intere ai vari processi
    MPI_Type_contiguous(col, MPI_C_BOOL, &life_row);
    MPI_Type_commit(&life_row);
    
    MPI_Comm_group(MPI_COMM_WORLD, &world_group);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
    
    if(comm_size > row){
        
        int new_rank[row];

        for(int i = 0; i < row; i++)
            new_rank[i] = i;

        //creo un nuovo gruppo con i soli processi di cui ho bisogno
        MPI_Group_incl(world_group, row, new_rank, &new_group);

        //creo il nuovo comunicatore
        MPI_Comm_create(MPI_COMM_WORLD, new_group, &NEW_MPI_COMM_WORLD);
    }
    else
    {
        MPI_Comm_create(MPI_COMM_WORLD, world_group, &NEW_MPI_COMM_WORLD);
    }

    if (NEW_MPI_COMM_WORLD == MPI_COMM_NULL)
    {
        // elimino i processi in eccesso
        MPI_Finalize();
        exit(0);
    }

    MPI_Comm_rank(NEW_MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(NEW_MPI_COMM_WORLD, &comm_size); 

    //inizializzo il seed della rand
    srand(time(NULL) + my_rank);

    if (my_rank == 0)
    {
        matrix = calloc(row * col, sizeof(bool));

        //inizializzo randomicamente la matrice, con valori 0 o 1 per ogni celle
        for (int i = 0; i < row; i++)
        {
            for (int j = 0; j < col; j++)
            {
                matrix[i * col + j] = rand() % 2;
            }
        }

        //stampa della matrice iniziale
        for (int i = 0; i < row; i++)
        {
            for (int j = 0; j < col; j++)
            {
                (matrix[i * col + j]) ? printf("\u25FC") : printf("\u25FB");
            }
            printf("\n");
        }
    }

    numElem = row / comm_size;
    rest = row % comm_size;

    send_counts = calloc(comm_size, sizeof(int));
    displacements = calloc(comm_size, sizeof(int));

    //assegno ad ogni processo il numero corretto di elementi ed incremento lo spostamento
    for (int i = 0; i < comm_size; i++)
    {
        send_counts[i] = numElem;

        if (rest > 0)
        {
            send_counts[i]++;
            rest--;
        }

        displacements[i] = count;
        count += send_counts[i];
    }

    //check per verificare che la divisione è corretta
    if (my_rank == 0)
    {
        for (int i = 0; i < comm_size; i++)
        {
            printf("sendcount[%d] = %d\tdispls[%d] = %d\n", i, send_counts[i], i, displacements[i]);
        }
    }

    //alloco la memoria per i buffer da inviare ai vari processi
    rec_buf = calloc(send_counts[my_rank] * col, sizeof(bool));
    updated_buf = calloc(send_counts[my_rank] * col, sizeof(bool));
    top_row = calloc(col, sizeof(bool));
    bottom_row = calloc(col, sizeof(bool));

    //se sono il processo 0 allora il mio predecessore è il processo con rank ultimo altrimenti rank - 1
    prev = (my_rank == 0) ? comm_size - 1 : my_rank - 1;

    //se sono l'ultimo processo il mio successore è rank 0 altrimenti rank + 1
    next = (my_rank + 1) == comm_size ? 0 : my_rank + 1;

    //eseguo tante volte quante sono le generazioni 
    while (steps < generations)
    {
        //invio ad ogni processo le righe che gli spettano per il calcolo
        MPI_Scatterv(matrix, send_counts, displacements, life_row, rec_buf, send_counts[my_rank], life_row, 0, NEW_MPI_COMM_WORLD);

        //numero di righe per processo
        int rowsNumber = send_counts[my_rank];

        //se predecessore e successore sono diversi 
        if (prev != next)
        {
            //invio la prima riga della matrice ricevuta al mio predecessore
            MPI_Isend(rec_buf, 1, life_row, prev, 1, NEW_MPI_COMM_WORLD, &top_request);
            //invio l'ultima riga al mio successore
            MPI_Isend(rec_buf + (col * (rowsNumber - 1)), 1, life_row, next, 1, NEW_MPI_COMM_WORLD, &bottom_request);
        }
        else
        {
            //nel caso di uno o due processi devo invertire prima e ultima riga da inviare
            MPI_Isend(rec_buf + (col * (rowsNumber - 1)), 1, life_row, prev, 1, NEW_MPI_COMM_WORLD, &top_request);
            MPI_Isend(rec_buf, 1, life_row, next, 1, NEW_MPI_COMM_WORLD, &bottom_request);
        }

        //prendo l'ultima riga del predecessore
        MPI_Recv(top_row, col, life_row, prev, 1, NEW_MPI_COMM_WORLD, &status);
        //prendo la prima riga del successore
        MPI_Recv(bottom_row, col, life_row, next, 1, NEW_MPI_COMM_WORLD, &status);

        //per ogni riga che ho
        for (int i = 0; i < rowsNumber; i++)
        {
            //per ogni colonna
            for (int j = 0; j < col; j++)
            {
                //sono nella cella
                //conto quante celle vive vicine ci sono per ogni cella
                int count = 0;

                //se sto nella prima riga devo considerare la ghost row ottenuta dal predecessore
                if (i == 0)
                {
                    //considero la ghost row top
                    for (int active_col = j - 1; active_col < j + 2; active_col++)
                    {
                        if (active_col > -1 && active_col < col)
                            count = is_alive(top_row[active_col]) ? count + 1 : count;
                    }

                    //valuto il vicino sinistro
                    if (j > 0)
                        count = is_alive(rec_buf[i * col + (j - 1)]) ? count + 1 : count;

                    //valuto il vicino destro
                    if (j < col - 1)
                        count = is_alive(rec_buf[i * col + (j + 1)]) ? count + 1 : count;;

                    //caso in cui ho soltanto una riga, non voglio che entri
                    if (i != rowsNumber - 1)
                    {
                        //considero la riga sottostante
                        for (int active_col = j - 1; active_col < j + 2; active_col++)
                        {
                            if (active_col > -1 && active_col < col)
                                count = is_alive(rec_buf[(i + 1) * col + active_col]) ? count + 1 : count;
                        }
                    }
                }

                if (i > 0 && i < rowsNumber - 1)
                {
                    //considero la riga in top
                    for (int active_col = j - 1; active_col < j + 2; active_col++)
                    {
                        if (active_col > -1 && active_col < col)
                            count = is_alive(rec_buf[(i - 1) * col + active_col]) ? count + 1 : count;
                    }

                    //valuto il vicino sinistro
                    if (j > 0)
                        count = is_alive(rec_buf[i * col + (j - 1)]) ? count + 1 : count;

                    //valuto il vicino destro
                    if (j < col - 1)
                        count = is_alive(rec_buf[i * col + (j + 1)]) ? count + 1 : count;

                    //valuto la row bottom
                    for (int active_col = j - 1; active_col < j + 2; active_col++)
                    {
                        if (active_col > -1 && active_col < col)
                            count = is_alive(rec_buf[(i + 1) * col + active_col]) ? count + 1 : count;
                    }
                }

                //nell'ultima considero la ghost ricevuta dal successore
                if (i == rowsNumber - 1)
                {
                    //caso in cui ho soltanto una riga, non voglio che entri
                    if (i != 0)
                    {
                        //considero la riga in top
                        for (int active_col = j - 1; active_col < j + 2; active_col++)
                        {
                            if (active_col > -1 && active_col < col)
                                count = is_alive(rec_buf[(i - 1) * col + active_col]) ? count + 1 : count;
                        }

                        //valuto il vicino sinistro
                        if (j > 0)
                            count = is_alive(rec_buf[i * col + (j - 1)]) ? count + 1 : count;

                        //valuto il vicino destro
                        if (j < col - 1)
                            count = is_alive(rec_buf[i * col + (j + 1)]) ? count + 1 : count;
                    }

                    //valuto la ghost row bottom
                    for (int active_col = j - 1; active_col < j + 2; active_col++)
                    {
                        if (active_col > -1 && active_col < col)
                            count = is_alive(bottom_row[active_col]) ? count + 1 : count;
                    }
                }

                //applico le regole del gioco
                game_update(rec_buf, updated_buf, i * col + j, count);
            }
        }

        MPI_Barrier(NEW_MPI_COMM_WORLD);
        //gather per ricombinare la matrice, prendo gli elementi dal buffer aggiornato e ricostruisco matrix
        MPI_Gatherv(updated_buf, send_counts[my_rank], life_row, matrix, send_counts, displacements, life_row, 0, NEW_MPI_COMM_WORLD);

        //stampa ad ogni generazione della matrice ottenuta
        if (my_rank == 0)
        {
            for (int i = 0; i < row; i++)
            {
                for (int j = 0; j < col; j++)
                {
                    is_alive(matrix[i * col + j]) ? printf("\u25FC") : printf("\u25FB");
                }
                printf("\n");
            }
            printf("\n");
        }

        //incremento il numero di step
        steps++;
    }

    free(rec_buf);
    free(updated_buf);
    free(top_row);
    free(bottom_row);
    free(send_counts);
    free(displacements);

    MPI_Finalize();

    //la matrix iniziale viene inizializzata soltanto dal master, il quale procederà con la free
    if(my_rank == 0){
        free(matrix);
    }

    return 0;
}
