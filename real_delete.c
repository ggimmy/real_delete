#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define PAGE_SIZE 4096

int main(int argc, char* argv[]) {

    //se piu di un args -> uso sbagliato
    if (argc < 2) {
        fprintf(stderr, "Usage: %s 'filename'\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    //prendo il nome del file dato in input
    const char* filename = argv[1];

    //apro il file in modalità sia scrittura che lettura
    int file_fd = open(filename, O_RDWR);
    if (file_fd < 0) {
        perror("File not found!");
        exit(EXIT_FAILURE);
    }

    //inizializzo la struct del file system che contiene tutte le informazioni di un file per ottenere la sua dimensione senza utilizzare seeks
    struct stat file_st;
    if(fstat(file_fd, &file_st) < 0){
        perror("Error fetching file data!");
        close(file_fd);
        exit(EXIT_FAILURE);
    }

    off_t file_size = file_st.st_size;
    if(file_size == 0){ //se dimensione 0, allora elimino solo il linking
        close(file_fd);
        unlink(filename);
        exit(EXIT_SUCCESS);
    }

    /*
     * ---------------------------------------------------------------------------------------------------
     * Questa implementazione del programma real_delete, punta ad essere senza seek()
     * visto che in caso di file di discrete dimensioni si possono avere decine di
     * migliaia di seek che oltre a rallentare l'esecuzione, usurano i dispositivi
     * di memoria di massa.
     *
     * L'idea è quella di mappare il file in memoria utilizzando mmap(), sovrascrivere questo file
     * in chunk da 4kb (per sfruttare l'efficienza della paginazione UNIX), per poi flushare i
     * cambiamenti in memoria.
     * ---------------------------------------------------------------------------------------------------
     *
     */



    //se dimesione file <= pagina, faccio singola iterazione
    if(file_size <= PAGE_SIZE){

        char* file_map = (char*)mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_fd, 0); //mappo il file in memoria
        if(file_map == MAP_FAILED){
            perror("Data mapping failed!");
            close(file_fd);
            exit(EXIT_FAILURE);
        }

        //sovrascrivo
        memset(file_map, 0, file_size);

        if (msync(file_map, file_size, MS_SYNC) == -1) { // forzo flush
            perror("Error flushing data in msync");
            close(file_fd);
            munmap(file_map, file_size);
            exit(EXIT_FAILURE);
        }

        if (munmap(file_map, file_size) == -1) {
            perror("Error unmapping file data!");
            close(file_fd);
            exit(EXIT_FAILURE);
        }

        //free(zeros); //rilascio la memoria allocata per gli 0

    }
    else{

        off_t offset = 0; //offset di partenza per ogni mmap
        //affinche l'offset sia < della dimensione
        while(offset < file_size){

            size_t to_map = file_size - offset; //calcolo quanto ancora c'è da mappare
            if(to_map >= PAGE_SIZE){
                to_map = PAGE_SIZE; //se i byte da mappare sono >= della dimensione di una pagina, allora mappo pagina per pagina.
            }

            char* file_map = (char*)mmap(NULL, to_map, PROT_READ | PROT_WRITE, MAP_SHARED, file_fd, offset);
            if(file_map == MAP_FAILED){
                perror("Data mapping failed!");
                close(file_fd);
                exit(EXIT_FAILURE);
            }

            memset(file_map, 0, to_map); //sovrascrivo con 0
            offset += to_map;

            if (msync(file_map, to_map, MS_SYNC) == -1) { // forzo flush
                perror("Error flushing data in msync");
                close(file_fd);
                munmap(file_map, to_map);
                exit(EXIT_FAILURE);
            }

            //rimuovo la mappa dalle memoria
            if (munmap(file_map, to_map) == -1) {
                perror("munmap");
                close(file_fd);
                exit(EXIT_FAILURE);
            }

        }

    }


    close(file_fd);

    unlink(filename);

    return 0;
}
