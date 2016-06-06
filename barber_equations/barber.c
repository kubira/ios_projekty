#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

volatile sig_atomic_t usr_interrupt = 0;  //Promìnná pro synchronizaci
struct sigaction usr_action;  //Struktura pro signál
sigset_t mask, oldmask;  //Masky signálu

void synchronizace(int);  //Deklarace funkce pro synchronizaci
void cekani();  //Deklarace funkce pro èekání na signál
void zakaznik(int, pid_t);  //Deklarace funkce pro zákazníka
void holic(int, pid_t);  //Deklarace funkce pro holièe

int main(int argc, char *argv[]) {
  int i = 0;  //Promìnná pro cyklus s podprocesy
  pid_t *potomek;  //Ukazatel na budoucí pole pidù podprocesù
 
  pid_t pidPotomka;  //PID podprocesu
 
  if ((argc < 2) || (isdigit(argv[1][0]) == 0)) {
  //Pokud je poèet argumentù men¹í jak dva nebo není první znak èíslice
    fprintf(stderr, "Error in argument.\n");  //Oznámím chybu argumentù
    return -1;  //a vracím -1
  }

  if (atoi(argv[1]) < 0 || atoi(argv[1]) > 99) {
  //Pokud není zadán poèet podprocesù v intervalu <0,99>
    fprintf(stderr, "Too few or too many customers in waiting room.\n");
    //Oznámím chybu zadaného poètu podprocesù
    return -1;  //a vracím -1
  }

  if((potomek = (pid_t*)malloc(sizeof(pid_t)*atoi(argv[1]))) == NULL) {
  //Pokud se nepodaøilo alokovat pamì» pro pidy podprocesù
    fprintf(stderr, "Memory allocation error.\n");
    //Oznámím chybu pøi alokování pamìti pro pole pidù podprocesù
    return -1;  //a vracím -1
  }

  setbuf(stdout, NULL);  //Vyma¾u stream stdout

  //Nastavení signálu SIGUSR1
  sigfillset (&mask);
  usr_action.sa_handler = synchronizace;
  usr_action.sa_mask = mask;
  usr_action.sa_flags = 0;
  sigaction (SIGUSR1, &usr_action, NULL);

  while (i < atoi(argv[1])) {  //Cyklus pro tvorbu podprocesù
    pidPotomka = fork();  //Vytvoøím nový podproces

    if (pidPotomka > 0) {  //Pokud byla návratová hodnota vìt¹í jak 0
      potomek[i] = pidPotomka;  //Ulo¾ím si pid podprocesu do pole pidù
    }
    else if (pidPotomka == 0) {  //Pokud byla návratová hodnota rovna 0
      zakaznik(i, getppid());  //Zavolám funkci podprocesu
    }
    else {  //Pokud nebyla návratová hodnota vìt¹í nebo rovna 0
      while (i > 0) {  //V cyklu zru¹ím v¹echny doposud vytvoøené podprocesy
        kill(potomek[i-1],SIGTERM);  //Za¹lu signál SIGTERM podprocesu
        i--;  //Dekrementuji index pole pidù podprocesù
      }
      free(potomek);  //Uvolním alokované pole pidù potomkù
      fprintf(stderr, "Error in creating subprocesses.\n");
      //Oznámím chybu pøi vytváøení podprocesù
      return -1;  //a vracím -1
    }    
  i++;  //Inkrementuji index pole pidù podprocesù
  }
  
  i = 0;  //Nastavím index pole pidù zpìt na 0
  
  while (i < atoi(argv[1])) {  //Cyklem procházím pole pidù podprocesù
    holic(i, potomek[i]);  //Zavolám hlavní proces (holiè)
    i++;  //Inkrementuji index pole pidù podprocesù
  }

  printf("barber finished\n");  //Vypí¹u zprávu o ukonèení hlavního procesu

  free(potomek);  //Uvolním alokované pole pidù potomkù

  return 0;  //Vracím 0 - úspì¹né ukonèení programu
}

//Funkce pro synchronizaci
void synchronizace(int sig) {
  usr_interrupt = 1;
}

//Funkce pro èekání na signál
void cekani() {
  sigemptyset (&mask);  //Vyprázdním nastavení signálu
  sigaddset (&mask, SIGUSR1);  //Pøidám signál SIGUSR1

  //Nastavím strukturu usr_action
  usr_action.sa_handler = synchronizace;
  usr_action.sa_mask = mask;
  usr_action.sa_flags = 0;

  //Nastavím signál SIGUSR1
  sigaction (SIGUSR1, &usr_action, NULL);
  sigprocmask (SIG_BLOCK, &mask, &oldmask);
  while(!usr_interrupt) {
    sigsuspend(&oldmask);
  }
  usr_interrupt = 0;
  sigprocmask (SIG_UNBLOCK, &mask, NULL);

}

//Funkce pro podproces
//argument poradi
//  - poøádí podprocesu
//argument pidRodice
//  - pid hlavního procesu
//Funkce nemá návratovou hodnotu
void zakaznik(int poradi, pid_t pidRodice) {
  cekani();  //Podproces èeká na signál od hlavního procesu
  printf("customer %d is taking a chair\n", poradi);
  //Vypí¹u oznámení, ¾e zákazník usedá na ¾idli
  kill(pidRodice, SIGUSR1);  //Za¹lu signál hlavnímu pocesu o usednutí na ¾idli
  cekani();  //Podproces èeká na dal¹í signál od hlavního procesu
  printf("customer %d is leaving\n", poradi);
  //Vypí¹u oznámení, ¾e zákazník opou¹tí ¾idli
  exit(0);  //Ukonèení podprocesu - zákazník opou¹tí holièství
}

//Funkce pro hlavní proces
//argument poradi
//  - poøádí podprocesu pro obsluhu
//argument pidPotomka
//  - pid podprocesu
//Funkce nemá návratovou hodnotu
void holic(int poradi, pid_t pidPotomka) {
  kill(pidPotomka, SIGUSR1);
  //Za¹lu signál podprocesu, aby zákazník usedl na ¾idli
  cekani();  //Hlavní proces èeká na signál od podprocesu
  printf("barber cuts a customer number %d\n", poradi);
  //Vypí¹u oznámení, ¾e holiè støíhá zákazníka
  kill(pidPotomka, SIGUSR1);  //Za¹lu signál podprocesu, zákazník je ostøíhán
  waitpid(pidPotomka, NULL, 0);  //Èekám na ukonèení podprocesu
}

