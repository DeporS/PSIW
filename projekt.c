#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <errno.h>
#include <sys/stat.h>

// Tworzenie struktury kolejki komunikatow
struct mesg_buffer{
	long mesg_type;
	char mesg_text[100];
} message;


int main(int argc, char* argv[]){
    if (argc != 2) {
        printf("Usage: %s <username>\n", argv[0]);
        return 1;
    }
	message.mesg_type = 1;

    char *username = argv[1];
    int key, fd;

    // Otwieranie pliku do odczytu
    int fp = open("lore.txt", O_RDONLY);
    if (fp == -1) {
        perror("Error opening file");
        return 1;
    }

    // Czytanie pliku linia po lini
	char buffer[32];
	int buf_id = 0;
    while ((read(fp, &buffer[buf_id], 1)) == 1) {
		if(buffer[buf_id] == '\n'){
			buffer[buf_id] = '\0';
			char name[5];
			sscanf(buffer, "%4s : %d", name, &key);
			if (strcmp(name,username)==0){
				printf("Key for %s: %d\n", username, key);
				break;
        	}
			buf_id = 0;
		}
		else{
			buf_id++;
		}
    }	

    // Zamykanie pliku
    int fc = close(fp);
	if (fc == -1){
		perror("Error closing file");
		return 1;
	}

    if(fork() == 0){
		while (1)
		{
			// Odbieranie wiadomosci z drugiego procesu
			//
			int msgid = msgget(key, 0666 | IPC_CREAT);
			if (msgid == -1) {
				perror("Error getting message queue");
				return 1;
			}

			// Otrzymanie wiadomosci z kolejki komunikatow
			int result = msgrcv(msgid, &message, sizeof(message), 1, 0);
			if (result == -1) {
				perror("Error receiving message");
				return 1;
			}

			char cmd[30], fifo[30];
			// Podanie jaka wiadomosc sie otrzymalo od drugiego procesu
			printf("received message: %s\n", message.mesg_text);

			sscanf(message.mesg_text, "'%[^']' %s", cmd, fifo);
			printf("cmd: %s\n", cmd);

			printf("path: %s\n", fifo);

			// Otwarcie kolejki fifo z mozliwoscia zapisu
			int desk = open(fifo, O_WRONLY);
			if (desk == -1) {
				perror("Error opening FIFO");
				return 1;
			}

			// Otwarcie pipea
			int f_pipe[2];
			if (pipe(f_pipe) == -1){
				perror("Error making pipe");
				return 1;
			}
			
			// Uzycie forka do pipe'a
			if(fork() == 0){
				// Zamkniecie pipe do odczytu
				close(f_pipe[0]);
				// Przekierowanie wyjscia standardowego na wyjscie pipe
				dup2(f_pipe[1], 1);
				// Uruchomienie polecenia otrzymanego z drugiego procesu
				if (execlp("/bin/sh", "sh", "-c", cmd, NULL) == -1){
					perror("Error running cmd");
					return 1;
				}
				// Zamkniecie pipe do zapisu
				close(f_pipe[1]);
			}
			else{
				// Zamkniecie pipe do zapisu
				close(f_pipe[1]);
				char bufFifo[1024];
				int x;
				// Czytanie outputu z pipe
				while((x = read(f_pipe[0], bufFifo, sizeof(bufFifo))) > 0){
					// Zapisywanie wszystkiego po kolei w pliku fifo
					if (write(desk, bufFifo, x) == -1){
						perror("Error writing to FIFO");
						return 1;
					}
				}
				// Zamkniecie pipe do odczytu
				close(f_pipe[0]);
			}

			
			close(desk);
			printf("Command output written to FIFO\n");
		}
    }
    else{
			while (1)
			{			
				// Wysylanie wiadomosci do drugiego procesu
				//
				char usr[30];
				char cmd[30];
				char fifo[30];
				
			
				// Zbieranie inputu od usera
				printf("Enter input in such form: usr cmd path\n");
				char input[100];
				scanf("%[^\n]%*c", input);
				sscanf(input, "%s \"%[^\"]\" \"%[^\"]\"", usr, cmd, fifo);
				
				// Otwieranie pliku z kluczami dostepu do odczytu
				fp = open("lore.txt", O_RDONLY);
				if (fp == -1) {
					perror("Error opening file");
					return 1;
				}

				// Czytanie pliku linia po lini aby uzyskac klucz
				char buffer2[32];
				int key2;
				int buf_id2 = 0;
				while ((read(fp, &buffer2[buf_id2], 1)) == 1) {
					if(buffer2[buf_id2] == '\n'){
						buffer2[buf_id2] = '\0';
						char name[5];
						sscanf(buffer2, "%4s : %d", name, &key2);
						if (strcmp(name,usr)==0){
							printf("Key for %s: %d\n", usr, key2);
							break;
						}
						buf_id2 = 0;
					}
					else{
						buf_id2++;
					}
				}	

				
				// Zamykanie pliku
				close(fp);

				// Przepisanie wiadomosci otrzymanej tak aby ulatwilo to pozniejszy jej odczyt
				snprintf(message.mesg_text, sizeof(message.mesg_text), "'%s' %s", cmd, fifo);

				// Dostanie sie do kolejki komunikatow
				int msgid = msgget(key2, 0666 | IPC_CREAT);
				if (msgid == -1) {
					perror("Error getting message queue");
					return 1;
				}

				// Wysylanie wiadomosci w kolejce komunikatow
				int result_msgsnd = msgsnd(msgid, &message, sizeof(message), 0);
				if (result_msgsnd == -1) {
					perror("Error sending message");
					return 1;
				}

				// Otwieranie kolejki fifo
				int result_fifo = mkfifo(fifo, 0666);
				if (result_fifo == -1) {
					perror("Error creating FIFO");
					return 1;
				}

				int fd_fifo;
				char buffer_fifo[1024];

				// Otwieranie Fifo do odczytu
				fd_fifo = open(fifo, O_RDONLY);
				if (fd_fifo == -1) {
					perror("Error opening FIFO");
					return 1;
				}	

				// Odczytywanie z fifo
				int bytes_read = read(fd_fifo, buffer_fifo, sizeof(buffer_fifo));
				if (bytes_read == -1) {
					perror("Error reading from FIFO");
					return 1;
				}

				printf("Read from FIFO:\n %s", buffer_fifo);

				// Zamkniecie kolejki fifo
				close(fd_fifo);

				// Usuniecie kolejki fifo
				unlink(fifo);
			}
	}


    

	return 0;
}
