#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#define KLIC 1191

//Hlavièky funkcí pro semafory
int operaceP();  //Funkce OperaceP
int operaceV();  //Funkce OperaceV
int inicializace();  //Inicializuje semafor
int zruseni();  //Zru¹í semafor

//Hlavièky funkcí pro sdílenou pamì»
int alokace(int);  //Alokuje sdílenou pamì»
int dealokace();  //Uvolní sdílenou pamì»

void rovnice(char*);  //Funkce pro proces jedné rovnice

int ID_semaforu;  //ID vytvoøeného semaforu
int logovani = 0;  //Bude se provádìt logování?

int *sdilena_pamet;  //Ukazatel na sdílenou pamì»
int ID_segmentu;  //ID segmentu sdílené pamìti

//Union semun pro funkci semctl
union semun {
  int val;  //Parametr pro pøíkaz SETVAL
  struct semid_ds *buf;  //Buffer pro IPC_STAT a IPC_SET
  unsigned short *array;  //Pole pro GETALL a SETALL
};

//Funkce main
int main(int argc, char *argv[]) {

  char c;  //Znak pro funkci getopt
  char soubor[80] = {0};  //Název souboru s rovnicemi
  char radek[80] = {0};  //Naètený øádek ze souboru
  FILE *f;  //Deskriptor souboru
  int pocet_radku = 0;  //Poèet øádkù v souboru
  int i = 0;  //Promìnné pro cykly
  pid_t pid;  //Pid vytvoøeného procesu
  pid_t potomci[26] = {0};  //Pole pidù potomkù - pro zru¹ení pøi chybì
  
  setbuf(stdout, NULL);  //Vymazání výstupního bufferu
  
  while((c = getopt(argc, argv, "lf:")) != -1) {  //Cyklus pro funkci getopt
    switch(c) {  //Podle znaku pøepínaèe se urèí vìtev
      case 'l' : {  //Vìtev pro pøepínaè -l
        logovani = 1;  //Pokud byl pøepínaè -l, nastavím logování na 1
        break;  //Konec vìtve
      }
      case 'f' : {  //Vìtev pro pøepínaè -f <file>
        strcpy(soubor, optarg);  //Zkopíruji název souboru od pøepínaèe
        break;  //Konec vìtve
      }
      default : {  //Pokud nastala chyba v pøepínaèi
        fprintf(stderr, "Chyba pøepínaèe!!!\n");  //Vypí¹i chybu
        return 1;  //Vracím 1
      }
    }
  }
  
  if(strlen(soubor) == 0) {  //Pokud nebyl zadán pøepínaè -f <file>
    strcpy(soubor, "init.txt");  //Nastavím vstupní soubor na "init.txt"
  }
  
  if((f = fopen(soubor, "r")) == NULL) {  //Otevøení souboru, pokud se neotevøel
    fprintf(stderr, "Nepodaøilo se otevøít soubor %s!!!\n", soubor);  //Chyba
    return 1;  //Vracím 1
  }
  
  fgets(radek, 80, f);  //Naètu první øádek ze souboru - poèet rovnic
  
  pocet_radku = atoi(radek);  //Pøevedu èíslo z prvního øádku na int
  
  if((pocet_radku <= 0) || (pocet_radku > 26)) {
  //Pokud není poèet podprocesù v rozsahu <1,26>
    fprintf(stderr, "Nepovolený formát souboru - ¹patné èíslo poètu øádkù!!!\n");
    //Chyba
    fclose(f);  //Uzavøu soubor
    return 1;  //Vracím 1
  }

  if((ID_semaforu = semget(KLIC, 1, 0666 | IPC_CREAT)) == -1) {
  //Vytvoøím jeden semafor s klíèem KLIC a s právy 666, pokud nastala chyba
    fclose(f);  //Uzavøu soubor
    fprintf(stderr, "Nepodaøilo se vytvoøit semafor!!!\n");  //Vypí¹u chybu
    return 1;  //Vracím 1
  }
  
  if(!inicializace()) {  //Pokud se nepodaøilo inicializovat semafor
    fclose(f);  //Uzavøu soubor
    zruseni();  //Zkusím jej zru¹it
    return 1;  //Vracím 1
  }
  
  if(!alokace(pocet_radku)) {  //Pokud se nepodaøilo alokovat sdílenou pamì»
    fclose(f);  //Uzavøu soubor
    zruseni();  //Zru¹ím semafor
    return 1;  //Vracím 1
  }

  for(i = 0; i < pocet_radku; i++) {  //Cyklus pro naèítání rovnic ze souboru
    if(fgets(radek, 80, f) != NULL) {  //Pokud jsem naèetl øádek
      if(strlen(radek) < 6) {  //Kdy¾ je øádek krat¹í ne¾ 6 znakù
        dealokace();  //Dealokuji pamì»
        zruseni();  //Zru¹ím semafor
        fclose(f);  //Uzavøu soubor
        fprintf(stderr, "Nepovolený formát souboru!!!\n");  //Vypí¹u chybu
        return 1;  //Vracím 1
      } else {  //Pokud je øádek dlouhý alespoò 6 znakù
        if(
            (islower(radek[0]) == 0) ||  //Pokud není první znak písmeno
            (radek[1] != ' ') ||  //Pokud není druhý znak mezera
            ((radek[2] != '+') && (radek[2] != '-')) || //Pokud není 3. znak +/-
            (radek[3] != '=') ||  //Pokud není 4. znak '='
            (radek[4] != ' ') ||  //Pokud není 5. znak mezera
            ((islower(radek[5]) == 0) && (isdigit(radek[5]) == 0) && (radek[5] != '+') && (radek[5] != '-')) ||
            //Pokud není 6. znak [0-9a-z+-]
            ((islower(radek[5]) != 0) && (islower(radek[6]) != 0)) ||
            //Pokud je 6. a 7. znak písmeno
            (((radek[5] == '+') || (radek[5] == '-')) && (isdigit(radek[6]) == 0)) ||
            //Pokud je 6. znak +/- a 7. znak není èíslice
            ((radek[0]-'a') > (pocet_radku-1)) ||
            //Pokud je 1. písmeno mimo rozsah poètu øádkù
            ((radek[5]-'a') > (pocet_radku-1))
            //Pokud je 6. znak mimo rozsah poètu øádkù
          ) {
          dealokace();  //Uvolním pamì»
          zruseni();  //Zru¹ím semafor
          fclose(f);  //Uzavøu soubor
          fprintf(stderr, "Nepovolený formát souboru!!!\n");  //Vypí¹u chybu
          return 1;  //Vracím 1
        }
      }
      pid = fork();  //Vytvoøím podproces
      if(pid == 0) {  //Pokud jsem v podprocesu
        rovnice(radek);  //Spustím pro nìj funkci rovnice
      } else if (pid > 0) {  //Pokud jsem hlavní proces
        potomci[i] = pid;  //Ulo¾ím si jeho pid
      } else {  //Pokud se nevytvoøil podproces
        for(int j = (i-1); j >= 0; j--) {  //V cyklu zru¹ím v¹echny vytvoøené
          kill(potomci[j], SIGTERM);  //Za¹lu jim signál SIGTERM
        }
        dealokace();  //Uvolním pamì»
        zruseni();  //Zru¹ím semafor
        fclose(f);  //Uzavøu soubor
        fprintf(stderr, "Nepodaøilo se vytvoøit podproces!!!\n");
        //Vypí¹u chybu
        return 1;  //Vracím 1
      }
    } else {  //Kdy¾ jsem nenaèetl øádek
      for(int j = (i-1); j >= 0; j--) {  //V cyklu zru¹ím v¹echny vytvoøené
          kill(potomci[j], SIGTERM);  //Za¹lu jim signál SIGTERM
        }
        dealokace();  //Uvolním pamì»
        zruseni();  //Zru¹ím semafor
        fclose(f);  //Uzavøu soubor
        fprintf(stderr, "Nepovolený formát souboru!!!\n");  //Vypí¹u chybu
        return 1;  //Vracím 1
    }
  }
  
  if(fclose(f) == EOF) {  //Uzavøení souboru, pokud se nepodaøilo zavøít soubor
    for(int j = i; j >= 0; j--) {  //Zru¹ím v¹echny vytvoøené podprocesy
      kill(potomci[j], SIGTERM);  //Za¹lu jim signál SIGTERM
    }
    dealokace();  //Uvolním pamì»
    zruseni();  //Zru¹ím semafor
    fprintf(stderr, "Nepodaøilo se uzavøít soubor %s!!!\n", soubor);  //Chyba
    return 1;  //Vracím 1
  }
  
  if(!operaceV()) {  //Uvolním semafor pro podprocesy, kdy¾ se operace nezdaøí
    for(int j = i; j >= 0; j--) {  //Zru¹ím v¹echny vytvoøené podprocesy
      kill(potomci[j], SIGTERM);  //Za¹lu jim signál SIGTERM
    }
    dealokace();  //Uvolním pamì»
    zruseni();  //Zru¹ím semafor
    return 1;  //Vracím 1
  }
  
  for(i = 0; i < pocet_radku; i++) {  //V cyklu èekám na ukonèení podprocesù
    wait(NULL);  //Èekám na jakýkoliv podproces
  }
  
  for(i = 0; i < pocet_radku; i++) {  //Vypí¹u nové hodnoty promìnných v cyklu
    printf("%c = %d\n", ('a'+i), sdilena_pamet[i]);  //Promìnná = hodnota
  }
  
  if(!dealokace()) {  //Pokud se nepodaøilo uvolnit sdílenou pamì»
    zruseni();  //Zru¹ím semafor
    return 1;  //Vracím 1
  }
  
  if(!zruseni()) {  //Pokud se nepodaøilo zru¹it semafor
    return 1;  //Vracím 1
  }

  return 0;  //Program úspì¹nì skonèil
}

//Funkce pro nastavení semaforu o -1
//Pokud se operace zdaøila, vrací 1, jinak vrací 0
int operaceP() {
  struct sembuf operace;  //Struktura pro operaci semaforu
  
  operace.sem_num = 0;  //Poøadí semaforu
  operace.sem_op = -1;  //Operace P
  operace.sem_flg = SEM_UNDO;  //Nastavení flagu na SEM_UNDO

  if(semop(ID_semaforu, &operace, 1) == -1) {  //Pokud se operace nezdaøila
    fprintf(stderr, "Nezdaøila se operace P nad semaforem!!!\n");  //Chyba
    return 0;  //Vracím do programu 0
  }

  return 1;  //Operace P probìhla úspì¹nì
}

//Funkce pro nastavení semaforu o +1
//Pokud se operace zdaøila, vrací 1, jinak vrací 0
int operaceV() {
  struct sembuf operace;  //Struktura pro operaci semaforu
  
  operace.sem_num = 0;  //Poøadí semaforu
  operace.sem_op = 1;  //Operace V
  operace.sem_flg = SEM_UNDO;  //Nastavení flagu na SEM_UNDO

  if(semop(ID_semaforu, &operace, 1) == -1) {  //Pokud se operace nezdaøila
    fprintf(stderr, "Nezdaøila se operace V nad semaforem!!!\n");  //Chyba
    return 0;  //Vracím do programu 0
  }

  return 1;  //Operace V probìhla úspì¹nì
}

//Funkce pro inicializaci semaforu
//Pokud se operace zdaøila, vrací 1, jinak vrací 0
int inicializace() {
  union semun semafor;  //Union pro nastavení hodnoty semaforu

  semafor.val = 0;  //Hodnota bude 0
  
  if(semctl(ID_semaforu, 0, SETVAL, semafor) == -1) {  //Nastavím hodnotu
    fprintf(stderr, "Nepodaøilo se inicializovat semafor!!!\n");  //Chyba
    return 0;  //Vracím do programu 0
  }

  return 1;  //Inicializace probìhla úspì¹nì
}

//Funkce pro zru¹ení vytvoøeného semaforu
//Pokud se operace zdaøila, vrací 1, jinak vrací 0
int zruseni() {
  if(semctl(ID_semaforu, 0, IPC_RMID, NULL) == -1) {  //Zru¹ím semafor
    fprintf(stderr, "Nepodaøilo se zru¹it semafor!!!\n");  //Chyba
    return 0;  //Vracím do programu 0
  }

  return 1;  //Zru¹ení probìhlo úspì¹nì
}

//Funkce pro alokaci sdílené pamìti
//Pokud se operace zdaøila, vrací 1, jinak vrací 0
int alokace(int pocet) {
  int i;  //Index sdílené pamìti pro nastavení na 0

  if((ID_segmentu = shmget(KLIC, (pocet)*sizeof(int), 0666 | IPC_CREAT)) == -1) {
  //Vyhradím si pamì» a ulo¾ím si ID segmentu
    fprintf(stderr, "Nepodaøilo se získat sdílenou pamì»!!!\n");  //Chyba
    return 0;  //Vracím do programu 0
  }
  if((int)(sdilena_pamet = (int*)shmat(ID_segmentu, NULL, SHM_RND)) == -1) {
  //Pøipojím sdílenou pamì»
    fprintf(stderr, "Nepodaøilo se pøipojit sdílenou pamì»!!!\n");  //Chyba
    return 0;  //Vracím do programu 0
  }
  
  for(i = 0; i < pocet; i++) {  //V cyklu inicializuji promìnné pamìti na 0
    sdilena_pamet[i] = 0;  //Nastavení promìnné i na 0
  }
  
  return 1;  //Operace byla úspì¹ná
}

//Funkce pro uvolnìní sdílené pamìti
//Pokud se operace zdaøila, vrací 1, jinak vrací 0
int dealokace() {
  
  if(shmdt(sdilena_pamet) == -1) {  //Odpojím sdílenou pamì»
    fprintf(stderr, "Nepodaøilo se odpojit sdílenou pamì»!!!\n");  //Chyba
    return 0;  //Vracím do programu 0
  }
  if(shmctl(ID_segmentu, IPC_RMID, NULL) == -1) {  //Uvolním sdílenou pamì»
    fprintf(stderr, "Nepodaøilo se uvolnit sdílenou pamì»!!!\n");  //Chyba
    return 0;  //Vracím do programu 0
  }
  
  return 1;  //Operace byla úspì¹ná
}

//Funkce pro jednu rovnici podprocesu
void rovnice(char rovnice[80]) {
  int prvni = 0;  //Promìnná pro první operand
  int druhy = 0;  //Promìnné pro druhý operand
  
  if(logovani == 1) {  //Pokud je nastaveno logování
    fprintf(stdout, "%s : init\n", strtok(rovnice, "\n"));  //Vypí¹u init
  }
  if(!operaceP()) {  //Kdy¾ operaceV vrací 0 - chyba
    exit(1);  //Konèí proces s hodnotou 1
  }
/******************************** Zaèátek KS **********************************/
  
  prvni = sdilena_pamet[(rovnice[0]-'a')];  //Získám hodnotu prvního operandu
  
  if(islower(rovnice[5]) != 0) {  //Pokud je druhý operand písmeno
    druhy = sdilena_pamet[(rovnice[5])-'a'];  //Získám jeho hodnotu z pamìti
  } else if(isdigit(rovnice[5]) != 0) {  //Pokud je druhý operand èíslo
    druhy = atoi(rovnice+5);  //Pøevedu celý zbytek øádku na èíslo
  } else if(rovnice[5] == '+') {  //Pokud je èíslo se znaménkem +
    druhy = atoi(rovnice+6);  //Pøevedu celý zbytek øádku na èíslo
  } else if(rovnice[5] == '-') {  //Pokud je èíslo se znaménkem -
    druhy = atoi(rovnice+6);  //Pøevedu celý zbytek øádku na èíslo
    druhy = -druhy;  //Nastavím jeho hodnotu na zápornou
  }

  if(logovani == 1) {  //Pokud je nastaveno logování
   fprintf(stdout, "%s : %c = %d\n", strtok(rovnice, "\n"), rovnice[0], prvni);
   //Vypí¹u pùvodní hodnotu promìnné
  }
  
  switch(rovnice[2]) {  //Podle operace se rozhodnu pro vìtev
    case '+' : {  //Pokud je operace +
      prvni = prvni + druhy;  //seètu první operand s druhým
      break;  //Ukonèím vìtev
    }
    case '-' : {  //Pokud je operace -
      prvni = prvni - druhy;  //Odeètu druhý operand od prvního
      break;  //Ukonèím vìtev
    }
  }
  
  fprintf(stdout, "%s : %d\n", strtok(rovnice, "\n"), prvni);
  //Vypí¹u novou hodnotu promìnné
  
  sdilena_pamet[(rovnice[0]-'a')] = prvni;
  //Nastavím promìnnou v pamìti na novou hodnotu

/********************************* Konec KS ***********************************/

  if(operaceV()) {  //Kdy¾ operaceP vrací 1 - OK
    if(logovani == 1) {  //Pokud je nastaveno logování
      fprintf(stdout, "%s : finish\n", strtok(rovnice, "\n"));
      //Vypí¹u finish
    }
    kill(getppid(), SIGCONT);  //Po¹lu signál hlavnímu procesu
    exit(0);  //Konèí proces s hodnotou 0 - OK
  } else {  //Kdy¾ operaceP vrací 0 - chyba
    kill(getppid(), SIGCONT);  //Po¹lu signál hlavnímu procesu
    exit(1);  //Konèí proces s hodnotou 1 - chyba
  }
}
