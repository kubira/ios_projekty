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

//Hlavi�ky funkc� pro semafory
int operaceP();  //Funkce OperaceP
int operaceV();  //Funkce OperaceV
int inicializace();  //Inicializuje semafor
int zruseni();  //Zru�� semafor

//Hlavi�ky funkc� pro sd�lenou pam�
int alokace(int);  //Alokuje sd�lenou pam�
int dealokace();  //Uvoln� sd�lenou pam�

void rovnice(char*);  //Funkce pro proces jedn� rovnice

int ID_semaforu;  //ID vytvo�en�ho semaforu
int logovani = 0;  //Bude se prov�d�t logov�n�?

int *sdilena_pamet;  //Ukazatel na sd�lenou pam�
int ID_segmentu;  //ID segmentu sd�len� pam�ti

//Union semun pro funkci semctl
union semun {
  int val;  //Parametr pro p��kaz SETVAL
  struct semid_ds *buf;  //Buffer pro IPC_STAT a IPC_SET
  unsigned short *array;  //Pole pro GETALL a SETALL
};

//Funkce main
int main(int argc, char *argv[]) {

  char c;  //Znak pro funkci getopt
  char soubor[80] = {0};  //N�zev souboru s rovnicemi
  char radek[80] = {0};  //Na�ten� ��dek ze souboru
  FILE *f;  //Deskriptor souboru
  int pocet_radku = 0;  //Po�et ��dk� v souboru
  int i = 0;  //Prom�nn� pro cykly
  pid_t pid;  //Pid vytvo�en�ho procesu
  pid_t potomci[26] = {0};  //Pole pid� potomk� - pro zru�en� p�i chyb�
  
  setbuf(stdout, NULL);  //Vymaz�n� v�stupn�ho bufferu
  
  while((c = getopt(argc, argv, "lf:")) != -1) {  //Cyklus pro funkci getopt
    switch(c) {  //Podle znaku p�ep�na�e se ur�� v�tev
      case 'l' : {  //V�tev pro p�ep�na� -l
        logovani = 1;  //Pokud byl p�ep�na� -l, nastav�m logov�n� na 1
        break;  //Konec v�tve
      }
      case 'f' : {  //V�tev pro p�ep�na� -f <file>
        strcpy(soubor, optarg);  //Zkop�ruji n�zev souboru od p�ep�na�e
        break;  //Konec v�tve
      }
      default : {  //Pokud nastala chyba v p�ep�na�i
        fprintf(stderr, "Chyba p�ep�na�e!!!\n");  //Vyp�i chybu
        return 1;  //Vrac�m 1
      }
    }
  }
  
  if(strlen(soubor) == 0) {  //Pokud nebyl zad�n p�ep�na� -f <file>
    strcpy(soubor, "init.txt");  //Nastav�m vstupn� soubor na "init.txt"
  }
  
  if((f = fopen(soubor, "r")) == NULL) {  //Otev�en� souboru, pokud se neotev�el
    fprintf(stderr, "Nepoda�ilo se otev��t soubor %s!!!\n", soubor);  //Chyba
    return 1;  //Vrac�m 1
  }
  
  fgets(radek, 80, f);  //Na�tu prvn� ��dek ze souboru - po�et rovnic
  
  pocet_radku = atoi(radek);  //P�evedu ��slo z prvn�ho ��dku na int
  
  if((pocet_radku <= 0) || (pocet_radku > 26)) {
  //Pokud nen� po�et podproces� v rozsahu <1,26>
    fprintf(stderr, "Nepovolen� form�t souboru - �patn� ��slo po�tu ��dk�!!!\n");
    //Chyba
    fclose(f);  //Uzav�u soubor
    return 1;  //Vrac�m 1
  }

  if((ID_semaforu = semget(KLIC, 1, 0666 | IPC_CREAT)) == -1) {
  //Vytvo��m jeden semafor s kl��em KLIC a s pr�vy 666, pokud nastala chyba
    fclose(f);  //Uzav�u soubor
    fprintf(stderr, "Nepoda�ilo se vytvo�it semafor!!!\n");  //Vyp�u chybu
    return 1;  //Vrac�m 1
  }
  
  if(!inicializace()) {  //Pokud se nepoda�ilo inicializovat semafor
    fclose(f);  //Uzav�u soubor
    zruseni();  //Zkus�m jej zru�it
    return 1;  //Vrac�m 1
  }
  
  if(!alokace(pocet_radku)) {  //Pokud se nepoda�ilo alokovat sd�lenou pam�
    fclose(f);  //Uzav�u soubor
    zruseni();  //Zru��m semafor
    return 1;  //Vrac�m 1
  }

  for(i = 0; i < pocet_radku; i++) {  //Cyklus pro na��t�n� rovnic ze souboru
    if(fgets(radek, 80, f) != NULL) {  //Pokud jsem na�etl ��dek
      if(strlen(radek) < 6) {  //Kdy� je ��dek krat�� ne� 6 znak�
        dealokace();  //Dealokuji pam�
        zruseni();  //Zru��m semafor
        fclose(f);  //Uzav�u soubor
        fprintf(stderr, "Nepovolen� form�t souboru!!!\n");  //Vyp�u chybu
        return 1;  //Vrac�m 1
      } else {  //Pokud je ��dek dlouh� alespo� 6 znak�
        if(
            (islower(radek[0]) == 0) ||  //Pokud nen� prvn� znak p�smeno
            (radek[1] != ' ') ||  //Pokud nen� druh� znak mezera
            ((radek[2] != '+') && (radek[2] != '-')) || //Pokud nen� 3. znak +/-
            (radek[3] != '=') ||  //Pokud nen� 4. znak '='
            (radek[4] != ' ') ||  //Pokud nen� 5. znak mezera
            ((islower(radek[5]) == 0) && (isdigit(radek[5]) == 0) && (radek[5] != '+') && (radek[5] != '-')) ||
            //Pokud nen� 6. znak [0-9a-z+-]
            ((islower(radek[5]) != 0) && (islower(radek[6]) != 0)) ||
            //Pokud je 6. a 7. znak p�smeno
            (((radek[5] == '+') || (radek[5] == '-')) && (isdigit(radek[6]) == 0)) ||
            //Pokud je 6. znak +/- a 7. znak nen� ��slice
            ((radek[0]-'a') > (pocet_radku-1)) ||
            //Pokud je 1. p�smeno mimo rozsah po�tu ��dk�
            ((radek[5]-'a') > (pocet_radku-1))
            //Pokud je 6. znak mimo rozsah po�tu ��dk�
          ) {
          dealokace();  //Uvoln�m pam�
          zruseni();  //Zru��m semafor
          fclose(f);  //Uzav�u soubor
          fprintf(stderr, "Nepovolen� form�t souboru!!!\n");  //Vyp�u chybu
          return 1;  //Vrac�m 1
        }
      }
      pid = fork();  //Vytvo��m podproces
      if(pid == 0) {  //Pokud jsem v podprocesu
        rovnice(radek);  //Spust�m pro n�j funkci rovnice
      } else if (pid > 0) {  //Pokud jsem hlavn� proces
        potomci[i] = pid;  //Ulo��m si jeho pid
      } else {  //Pokud se nevytvo�il podproces
        for(int j = (i-1); j >= 0; j--) {  //V cyklu zru��m v�echny vytvo�en�
          kill(potomci[j], SIGTERM);  //Za�lu jim sign�l SIGTERM
        }
        dealokace();  //Uvoln�m pam�
        zruseni();  //Zru��m semafor
        fclose(f);  //Uzav�u soubor
        fprintf(stderr, "Nepoda�ilo se vytvo�it podproces!!!\n");
        //Vyp�u chybu
        return 1;  //Vrac�m 1
      }
    } else {  //Kdy� jsem nena�etl ��dek
      for(int j = (i-1); j >= 0; j--) {  //V cyklu zru��m v�echny vytvo�en�
          kill(potomci[j], SIGTERM);  //Za�lu jim sign�l SIGTERM
        }
        dealokace();  //Uvoln�m pam�
        zruseni();  //Zru��m semafor
        fclose(f);  //Uzav�u soubor
        fprintf(stderr, "Nepovolen� form�t souboru!!!\n");  //Vyp�u chybu
        return 1;  //Vrac�m 1
    }
  }
  
  if(fclose(f) == EOF) {  //Uzav�en� souboru, pokud se nepoda�ilo zav��t soubor
    for(int j = i; j >= 0; j--) {  //Zru��m v�echny vytvo�en� podprocesy
      kill(potomci[j], SIGTERM);  //Za�lu jim sign�l SIGTERM
    }
    dealokace();  //Uvoln�m pam�
    zruseni();  //Zru��m semafor
    fprintf(stderr, "Nepoda�ilo se uzav��t soubor %s!!!\n", soubor);  //Chyba
    return 1;  //Vrac�m 1
  }
  
  if(!operaceV()) {  //Uvoln�m semafor pro podprocesy, kdy� se operace nezda��
    for(int j = i; j >= 0; j--) {  //Zru��m v�echny vytvo�en� podprocesy
      kill(potomci[j], SIGTERM);  //Za�lu jim sign�l SIGTERM
    }
    dealokace();  //Uvoln�m pam�
    zruseni();  //Zru��m semafor
    return 1;  //Vrac�m 1
  }
  
  for(i = 0; i < pocet_radku; i++) {  //V cyklu �ek�m na ukon�en� podproces�
    wait(NULL);  //�ek�m na jak�koliv podproces
  }
  
  for(i = 0; i < pocet_radku; i++) {  //Vyp�u nov� hodnoty prom�nn�ch v cyklu
    printf("%c = %d\n", ('a'+i), sdilena_pamet[i]);  //Prom�nn� = hodnota
  }
  
  if(!dealokace()) {  //Pokud se nepoda�ilo uvolnit sd�lenou pam�
    zruseni();  //Zru��m semafor
    return 1;  //Vrac�m 1
  }
  
  if(!zruseni()) {  //Pokud se nepoda�ilo zru�it semafor
    return 1;  //Vrac�m 1
  }

  return 0;  //Program �sp�n� skon�il
}

//Funkce pro nastaven� semaforu o -1
//Pokud se operace zda�ila, vrac� 1, jinak vrac� 0
int operaceP() {
  struct sembuf operace;  //Struktura pro operaci semaforu
  
  operace.sem_num = 0;  //Po�ad� semaforu
  operace.sem_op = -1;  //Operace P
  operace.sem_flg = SEM_UNDO;  //Nastaven� flagu na SEM_UNDO

  if(semop(ID_semaforu, &operace, 1) == -1) {  //Pokud se operace nezda�ila
    fprintf(stderr, "Nezda�ila se operace P nad semaforem!!!\n");  //Chyba
    return 0;  //Vrac�m do programu 0
  }

  return 1;  //Operace P prob�hla �sp�n�
}

//Funkce pro nastaven� semaforu o +1
//Pokud se operace zda�ila, vrac� 1, jinak vrac� 0
int operaceV() {
  struct sembuf operace;  //Struktura pro operaci semaforu
  
  operace.sem_num = 0;  //Po�ad� semaforu
  operace.sem_op = 1;  //Operace V
  operace.sem_flg = SEM_UNDO;  //Nastaven� flagu na SEM_UNDO

  if(semop(ID_semaforu, &operace, 1) == -1) {  //Pokud se operace nezda�ila
    fprintf(stderr, "Nezda�ila se operace V nad semaforem!!!\n");  //Chyba
    return 0;  //Vrac�m do programu 0
  }

  return 1;  //Operace V prob�hla �sp�n�
}

//Funkce pro inicializaci semaforu
//Pokud se operace zda�ila, vrac� 1, jinak vrac� 0
int inicializace() {
  union semun semafor;  //Union pro nastaven� hodnoty semaforu

  semafor.val = 0;  //Hodnota bude 0
  
  if(semctl(ID_semaforu, 0, SETVAL, semafor) == -1) {  //Nastav�m hodnotu
    fprintf(stderr, "Nepoda�ilo se inicializovat semafor!!!\n");  //Chyba
    return 0;  //Vrac�m do programu 0
  }

  return 1;  //Inicializace prob�hla �sp�n�
}

//Funkce pro zru�en� vytvo�en�ho semaforu
//Pokud se operace zda�ila, vrac� 1, jinak vrac� 0
int zruseni() {
  if(semctl(ID_semaforu, 0, IPC_RMID, NULL) == -1) {  //Zru��m semafor
    fprintf(stderr, "Nepoda�ilo se zru�it semafor!!!\n");  //Chyba
    return 0;  //Vrac�m do programu 0
  }

  return 1;  //Zru�en� prob�hlo �sp�n�
}

//Funkce pro alokaci sd�len� pam�ti
//Pokud se operace zda�ila, vrac� 1, jinak vrac� 0
int alokace(int pocet) {
  int i;  //Index sd�len� pam�ti pro nastaven� na 0

  if((ID_segmentu = shmget(KLIC, (pocet)*sizeof(int), 0666 | IPC_CREAT)) == -1) {
  //Vyhrad�m si pam� a ulo��m si ID segmentu
    fprintf(stderr, "Nepoda�ilo se z�skat sd�lenou pam�!!!\n");  //Chyba
    return 0;  //Vrac�m do programu 0
  }
  if((int)(sdilena_pamet = (int*)shmat(ID_segmentu, NULL, SHM_RND)) == -1) {
  //P�ipoj�m sd�lenou pam�
    fprintf(stderr, "Nepoda�ilo se p�ipojit sd�lenou pam�!!!\n");  //Chyba
    return 0;  //Vrac�m do programu 0
  }
  
  for(i = 0; i < pocet; i++) {  //V cyklu inicializuji prom�nn� pam�ti na 0
    sdilena_pamet[i] = 0;  //Nastaven� prom�nn� i na 0
  }
  
  return 1;  //Operace byla �sp�n�
}

//Funkce pro uvoln�n� sd�len� pam�ti
//Pokud se operace zda�ila, vrac� 1, jinak vrac� 0
int dealokace() {
  
  if(shmdt(sdilena_pamet) == -1) {  //Odpoj�m sd�lenou pam�
    fprintf(stderr, "Nepoda�ilo se odpojit sd�lenou pam�!!!\n");  //Chyba
    return 0;  //Vrac�m do programu 0
  }
  if(shmctl(ID_segmentu, IPC_RMID, NULL) == -1) {  //Uvoln�m sd�lenou pam�
    fprintf(stderr, "Nepoda�ilo se uvolnit sd�lenou pam�!!!\n");  //Chyba
    return 0;  //Vrac�m do programu 0
  }
  
  return 1;  //Operace byla �sp�n�
}

//Funkce pro jednu rovnici podprocesu
void rovnice(char rovnice[80]) {
  int prvni = 0;  //Prom�nn� pro prvn� operand
  int druhy = 0;  //Prom�nn� pro druh� operand
  
  if(logovani == 1) {  //Pokud je nastaveno logov�n�
    fprintf(stdout, "%s : init\n", strtok(rovnice, "\n"));  //Vyp�u init
  }
  if(!operaceP()) {  //Kdy� operaceV vrac� 0 - chyba
    exit(1);  //Kon�� proces s hodnotou 1
  }
/******************************** Za��tek KS **********************************/
  
  prvni = sdilena_pamet[(rovnice[0]-'a')];  //Z�sk�m hodnotu prvn�ho operandu
  
  if(islower(rovnice[5]) != 0) {  //Pokud je druh� operand p�smeno
    druhy = sdilena_pamet[(rovnice[5])-'a'];  //Z�sk�m jeho hodnotu z pam�ti
  } else if(isdigit(rovnice[5]) != 0) {  //Pokud je druh� operand ��slo
    druhy = atoi(rovnice+5);  //P�evedu cel� zbytek ��dku na ��slo
  } else if(rovnice[5] == '+') {  //Pokud je ��slo se znam�nkem +
    druhy = atoi(rovnice+6);  //P�evedu cel� zbytek ��dku na ��slo
  } else if(rovnice[5] == '-') {  //Pokud je ��slo se znam�nkem -
    druhy = atoi(rovnice+6);  //P�evedu cel� zbytek ��dku na ��slo
    druhy = -druhy;  //Nastav�m jeho hodnotu na z�pornou
  }

  if(logovani == 1) {  //Pokud je nastaveno logov�n�
   fprintf(stdout, "%s : %c = %d\n", strtok(rovnice, "\n"), rovnice[0], prvni);
   //Vyp�u p�vodn� hodnotu prom�nn�
  }
  
  switch(rovnice[2]) {  //Podle operace se rozhodnu pro v�tev
    case '+' : {  //Pokud je operace +
      prvni = prvni + druhy;  //se�tu prvn� operand s druh�m
      break;  //Ukon��m v�tev
    }
    case '-' : {  //Pokud je operace -
      prvni = prvni - druhy;  //Ode�tu druh� operand od prvn�ho
      break;  //Ukon��m v�tev
    }
  }
  
  fprintf(stdout, "%s : %d\n", strtok(rovnice, "\n"), prvni);
  //Vyp�u novou hodnotu prom�nn�
  
  sdilena_pamet[(rovnice[0]-'a')] = prvni;
  //Nastav�m prom�nnou v pam�ti na novou hodnotu

/********************************* Konec KS ***********************************/

  if(operaceV()) {  //Kdy� operaceP vrac� 1 - OK
    if(logovani == 1) {  //Pokud je nastaveno logov�n�
      fprintf(stdout, "%s : finish\n", strtok(rovnice, "\n"));
      //Vyp�u finish
    }
    kill(getppid(), SIGCONT);  //Po�lu sign�l hlavn�mu procesu
    exit(0);  //Kon�� proces s hodnotou 0 - OK
  } else {  //Kdy� operaceP vrac� 0 - chyba
    kill(getppid(), SIGCONT);  //Po�lu sign�l hlavn�mu procesu
    exit(1);  //Kon�� proces s hodnotou 1 - chyba
  }
}
