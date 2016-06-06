#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

volatile sig_atomic_t usr_interrupt = 0;  //Prom�nn� pro synchronizaci
struct sigaction usr_action;  //Struktura pro sign�l
sigset_t mask, oldmask;  //Masky sign�lu

void synchronizace(int);  //Deklarace funkce pro synchronizaci
void cekani();  //Deklarace funkce pro �ek�n� na sign�l
void zakaznik(int, pid_t);  //Deklarace funkce pro z�kazn�ka
void holic(int, pid_t);  //Deklarace funkce pro holi�e

int main(int argc, char *argv[]) {
  int i = 0;  //Prom�nn� pro cyklus s podprocesy
  pid_t *potomek;  //Ukazatel na budouc� pole pid� podproces�
 
  pid_t pidPotomka;  //PID podprocesu
 
  if ((argc < 2) || (isdigit(argv[1][0]) == 0)) {
  //Pokud je po�et argument� men�� jak dva nebo nen� prvn� znak ��slice
    fprintf(stderr, "Error in argument.\n");  //Ozn�m�m chybu argument�
    return -1;  //a vrac�m -1
  }

  if (atoi(argv[1]) < 0 || atoi(argv[1]) > 99) {
  //Pokud nen� zad�n po�et podproces� v intervalu <0,99>
    fprintf(stderr, "Too few or too many customers in waiting room.\n");
    //Ozn�m�m chybu zadan�ho po�tu podproces�
    return -1;  //a vrac�m -1
  }

  if((potomek = (pid_t*)malloc(sizeof(pid_t)*atoi(argv[1]))) == NULL) {
  //Pokud se nepoda�ilo alokovat pam� pro pidy podproces�
    fprintf(stderr, "Memory allocation error.\n");
    //Ozn�m�m chybu p�i alokov�n� pam�ti pro pole pid� podproces�
    return -1;  //a vrac�m -1
  }

  setbuf(stdout, NULL);  //Vyma�u stream stdout

  //Nastaven� sign�lu SIGUSR1
  sigfillset (&mask);
  usr_action.sa_handler = synchronizace;
  usr_action.sa_mask = mask;
  usr_action.sa_flags = 0;
  sigaction (SIGUSR1, &usr_action, NULL);

  while (i < atoi(argv[1])) {  //Cyklus pro tvorbu podproces�
    pidPotomka = fork();  //Vytvo��m nov� podproces

    if (pidPotomka > 0) {  //Pokud byla n�vratov� hodnota v�t�� jak 0
      potomek[i] = pidPotomka;  //Ulo��m si pid podprocesu do pole pid�
    }
    else if (pidPotomka == 0) {  //Pokud byla n�vratov� hodnota rovna 0
      zakaznik(i, getppid());  //Zavol�m funkci podprocesu
    }
    else {  //Pokud nebyla n�vratov� hodnota v�t�� nebo rovna 0
      while (i > 0) {  //V cyklu zru��m v�echny doposud vytvo�en� podprocesy
        kill(potomek[i-1],SIGTERM);  //Za�lu sign�l SIGTERM podprocesu
        i--;  //Dekrementuji index pole pid� podproces�
      }
      free(potomek);  //Uvoln�m alokovan� pole pid� potomk�
      fprintf(stderr, "Error in creating subprocesses.\n");
      //Ozn�m�m chybu p�i vytv��en� podproces�
      return -1;  //a vrac�m -1
    }    
  i++;  //Inkrementuji index pole pid� podproces�
  }
  
  i = 0;  //Nastav�m index pole pid� zp�t na 0
  
  while (i < atoi(argv[1])) {  //Cyklem proch�z�m pole pid� podproces�
    holic(i, potomek[i]);  //Zavol�m hlavn� proces (holi�)
    i++;  //Inkrementuji index pole pid� podproces�
  }

  printf("barber finished\n");  //Vyp�u zpr�vu o ukon�en� hlavn�ho procesu

  free(potomek);  //Uvoln�m alokovan� pole pid� potomk�

  return 0;  //Vrac�m 0 - �sp�n� ukon�en� programu
}

//Funkce pro synchronizaci
void synchronizace(int sig) {
  usr_interrupt = 1;
}

//Funkce pro �ek�n� na sign�l
void cekani() {
  sigemptyset (&mask);  //Vypr�zdn�m nastaven� sign�lu
  sigaddset (&mask, SIGUSR1);  //P�id�m sign�l SIGUSR1

  //Nastav�m strukturu usr_action
  usr_action.sa_handler = synchronizace;
  usr_action.sa_mask = mask;
  usr_action.sa_flags = 0;

  //Nastav�m sign�l SIGUSR1
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
//  - po��d� podprocesu
//argument pidRodice
//  - pid hlavn�ho procesu
//Funkce nem� n�vratovou hodnotu
void zakaznik(int poradi, pid_t pidRodice) {
  cekani();  //Podproces �ek� na sign�l od hlavn�ho procesu
  printf("customer %d is taking a chair\n", poradi);
  //Vyp�u ozn�men�, �e z�kazn�k used� na �idli
  kill(pidRodice, SIGUSR1);  //Za�lu sign�l hlavn�mu pocesu o usednut� na �idli
  cekani();  //Podproces �ek� na dal�� sign�l od hlavn�ho procesu
  printf("customer %d is leaving\n", poradi);
  //Vyp�u ozn�men�, �e z�kazn�k opou�t� �idli
  exit(0);  //Ukon�en� podprocesu - z�kazn�k opou�t� holi�stv�
}

//Funkce pro hlavn� proces
//argument poradi
//  - po��d� podprocesu pro obsluhu
//argument pidPotomka
//  - pid podprocesu
//Funkce nem� n�vratovou hodnotu
void holic(int poradi, pid_t pidPotomka) {
  kill(pidPotomka, SIGUSR1);
  //Za�lu sign�l podprocesu, aby z�kazn�k usedl na �idli
  cekani();  //Hlavn� proces �ek� na sign�l od podprocesu
  printf("barber cuts a customer number %d\n", poradi);
  //Vyp�u ozn�men�, �e holi� st��h� z�kazn�ka
  kill(pidPotomka, SIGUSR1);  //Za�lu sign�l podprocesu, z�kazn�k je ost��h�n
  waitpid(pidPotomka, NULL, 0);  //�ek�m na ukon�en� podprocesu
}

