/**
 * IOS projekt c. 2, 2011/2012 
 * 
 * Soubor:  readerWriter.c
 *
 * Autor:   Radim Kubis (xkubis03@stud.fit.vutbr.cz) 
 *
 */
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/types.h>

/* Navratove hodnoty programu */
#define EXIT_OK        0
#define EXIT_ARG_CHYBA 1
#define EXIT_CHYBA     2

/* Navratove hodnoty funkci */
#define OK     0
#define CHYBA -1

sem_t CTENARU;
sem_t PISARU;
sem_t ZAMEK;
sem_t CTENI;
sem_t ZAPIS;
sem_t VYSTUP;

/**
 * Funkce pro vypis chyby na standardni chybovy vystup programu
 * 
 * @param zprava - chybove hlaseni pro vypis
 */
void tiskChyby(char zprava[]) {
  fprintf(stderr, "readerWriter: error: %s\n", zprava);
}

/**
 * Funkce pro prevod retezce na cislo
 * 
 * @param chyba   - chybovy kod funkce
 * @param retezec - retezec znaku pro prevod na cislo
 */
long dejCislo(int *chyba, char *retezec) {
  char *konec = NULL;  /* Ukazatel na konec zpracovane casti funkci strtol */
  char *zacatek = NULL;  /* Ukazatel na zacatek retezce pro prevod */
  long cislo = 0;  /* Inicializace promenne pro vyslednou hodnotu */

  /* Nastavim ukazatel na zacatek retezce */
  zacatek = retezec;

  /* Nastavim bezchybny stav */
  errno = 0;
  /* Provedu prevod funkci strtol */
  cislo = strtol(zacatek, &konec, 10);

  /* Provedu kontrolu chyb, ktere mohly nastat ve funkci strtol */
  if((errno == ERANGE && (cislo == LONG_MAX || cislo == LONG_MIN)) || (errno != 0 && cislo == 0)) {
    /* Pokud nastala chyba, provedu vypis */
    perror("strtol");
    /* Nastavim chybovy kod */
    *chyba = EXIT_ARG_CHYBA;
    /* Vracim cislo 0 - nepouzije se dale v programu */
    return 0;
  }

  /* Pokud je konec roven zacatku, nic se neprevedlo */
  if(konec == zacatek) {
    /* Nastavim chybovy kod */
    *chyba = EXIT_ARG_CHYBA;
    /* Vracim cislo 0 - nepouzije se dale v programu */
    return 0;
  }

  /* Preskocim bile znaky za prevedenym cislem */
  while(*konec != '\0') {
    /* Pokud je bily znak */
    if(isspace(*konec) != 0) {
      /* Posunu se na dalsi znak */
      konec++;
    } else {
      /* Jinak nastavim chybovy kod */
      *chyba = EXIT_ARG_CHYBA;
      /* Vracim cislo 0 - nepouzije se dale v programu */
      return 0;
    }
  }

  /* Chybovy ok nastaven na OK - bez chyby */
  *chyba = EXIT_OK;

  /* Vracim prevedene cislo */
  return cislo;
}

/***
 * Struktura pro sdilenou pamet
 *
 * poradiAkce - urcuje poradi provadene akce procesem
 * hodnota    - hodnota sdilene promenne mezi procesy
 * semaforId  - ID semaforu
 * ctenaru    - pocet ctenaru, kteri ctou
 * pisaru     - pocet pisaru, kteri zapisuji
 * cyklu      - pocet cyklu pro provedeni pisare
 */
typedef struct struktura {
  unsigned int poradiAkce;
  int hodnota;
  int ctenaru;
  int pisaru;
  long int cyklu;
} struktura;

/* Ukazatel na misto v pameti - globalne kvuli odchytavani signalu */
struktura *misto = NULL;

/**
 * Funkce pro proces pisare
 * 
 * @param poradi - poradove cislo pisare
 * @param vystup - vystupni proud procesu
 * @param rozsah - maximalni cekaci doba procesu
 * @param cyklu  - pocet opakovani pisare
 */
void pisar(int poradi, FILE *vystup, int rozsah) {

  /* Inicializace generatoru nahodnych cisel */
  srand((unsigned int)getpid());

  /* Provadim proces pisare cyklu-krat */
  for(long int c = 0; c < misto->cyklu; c++) {

    /* Uzamknu vystupni proud */
    if(sem_wait(&VYSTUP) == CHYBA) exit(EXIT_CHYBA);
    /* Vypisi informaci o vypoctu nove hodnoty */
    fprintf(vystup, "%u: writer: %d: new value\n", misto->poradiAkce, poradi);
    /* Zvysim pocet akci */
    misto->poradiAkce++;
    /* Uvolnim vystupni proud */
    if(sem_post(&VYSTUP) == CHYBA) exit(EXIT_CHYBA);

    /* Proces pisare ceka po nahodnou dobu 0-rozsah */
    usleep((rand() % (rozsah+1)) * 1000);

    /* Uzamknu vystupni proud */
    if(sem_wait(&VYSTUP) == CHYBA) exit(EXIT_CHYBA);
    /* Vypisu informaci o pripravenosti pisare */
    fprintf(vystup, "%u: writer: %d: ready\n", misto->poradiAkce, poradi);
    /* Zvysim citac akci */
    misto->poradiAkce++;
    /* Uvolnim vystupni proud */
    if(sem_post(&VYSTUP) == CHYBA) exit(EXIT_CHYBA);

    /* Uzamknu pocet pisaru */
    if(sem_wait(&PISARU) == CHYBA) exit(EXIT_CHYBA);
    /* Zvysim pocet pisaru o 1 */
    misto->pisaru++;
    /* Pokud jsem prvni pisar */
    if(misto->pisaru == 1) {
      /* Uzamknu cteni hodnoty */
      if(sem_wait(&CTENI) == CHYBA) exit(EXIT_CHYBA);
    }
    /* Uvolnim pocet pisaru */
    if(sem_post(&PISARU) == CHYBA) exit(EXIT_CHYBA);

    /* Uzamknu zapisovani hodnoty */
    if(sem_wait(&ZAPIS) == CHYBA) exit(EXIT_CHYBA);

    /* Uzamknu vystupni proud */
    if(sem_wait(&VYSTUP) == CHYBA) exit(EXIT_CHYBA);
    /* Vypisu informaci o zapisu nove hodnoty */
    fprintf(vystup, "%u: writer: %d: writes a value\n", misto->poradiAkce, poradi);
    /* Zvysim pocet akci */
    misto->poradiAkce++;
    /* Uvolnim vystupni proud */
    if(sem_post(&VYSTUP) == CHYBA) exit(EXIT_CHYBA);

    /* Ulozim novou hodnotu */
    misto->hodnota = poradi;
    
    /* Uzamknu vystupni proud */
    if(sem_wait(&VYSTUP) == CHYBA) exit(EXIT_CHYBA);
    /* Vypisu informaci o zapsani nove hodnoty */
    fprintf(vystup, "%u: writer: %d: written\n", misto->poradiAkce, poradi);
    /* Zvysim pocet akci */
    misto->poradiAkce++;
    /* Uvolnim vystupni proud */
    if(sem_post(&VYSTUP) == CHYBA) exit(EXIT_CHYBA);

    /* Uvolnim zapis hodnoty */
    if(sem_post(&ZAPIS) == CHYBA) exit(EXIT_CHYBA);

    /* Uzamknu pocet pisaru */
    if(sem_wait(&PISARU) == CHYBA) exit(EXIT_CHYBA);
    /* Snizim pocet pisaru */
    misto->pisaru--;
    /* Pokud jsem posledni pisar */
    if(misto->pisaru == 0) {
      /* Uvolnim cteni hodnoty */
      if(sem_post(&CTENI) == CHYBA) exit(EXIT_CHYBA);
    }
    /* Uvolnim zamek poctu pisaru */
    if(sem_post(&PISARU) == CHYBA) exit(EXIT_CHYBA);
  }

  /* Vse probehlo v poradku */
  exit(EXIT_OK);
}

/**
 * Funkce pro proces ctenare
 * 
 * @param poradi - poradove cislo ctenare
 * @param vystup - vystupni proud procesu
 * @param rozsah - maximalni cekaci doba procesu
 */
void ctenar(int poradi, FILE *vystup, int rozsah) {
  int h = -1;  /* Hodnota prectena ctenarem */

  /* Inicializace generatoru nahodnych cisel */
  srand((unsigned int)getpid());

  /* Proces ctenare se opakuje, dokud neni prectena hodnota 0 */
  while(1) {
    /* Uzamknu vystupni proud */
    if(sem_wait(&VYSTUP) == CHYBA) exit(EXIT_CHYBA);
    /* Vypisu informaci o pripravnosti ctenare */
    fprintf(vystup, "%u: reader: %d: ready\n", misto->poradiAkce, poradi);
    /* Zvysim poradi akce */
    misto->poradiAkce++;
    /* Uvolnim vystupni proud */
    if(sem_post(&VYSTUP) == CHYBA) exit(EXIT_CHYBA);
    /* Uzamknu zamek */
    if(sem_wait(&ZAMEK) == CHYBA) exit(EXIT_CHYBA);
    /* Uzamknu si cteni */
    if(sem_wait(&CTENI) == CHYBA) exit(EXIT_CHYBA);
    /* Uzamknu pocet ctenaru */
    if(sem_wait(&CTENARU) == CHYBA) exit(EXIT_CHYBA); 
    /* Pokud jsem prvni ctenar */
    printf("X %d %d\n", poradi, misto->ctenaru);
    /* Zvysim pocet ctenaru o 1 */
    misto->ctenaru++;
    if(misto->ctenaru == 1) {
      /* Uzamknu zapis hodnoty */
      if(sem_wait(&ZAPIS) == CHYBA) exit(EXIT_CHYBA);
    }
    printf("Y %d %d\n", poradi, misto->ctenaru);
    /* Uvolnim pocet ctenaru */
    if(sem_post(&CTENARU) == CHYBA) exit(EXIT_CHYBA);
    /* Uvolnim cteni */
    if(sem_post(&CTENI) == CHYBA) exit(EXIT_CHYBA);
    /* Uvolnim zamek */
    if(sem_post(&ZAMEK) == CHYBA) exit(EXIT_CHYBA);

    /* Uzamknu vystupni proud */
    if(sem_wait(&VYSTUP) == CHYBA) exit(EXIT_CHYBA);
    /* Vypisu informaci o cteni hodnoty */
    fprintf(vystup, "%u: reader: %d: reads a value\n", misto->poradiAkce, poradi);
    /* Zvysim pocet akci */
    misto->poradiAkce++;
    /* Uvolnim vystupni proud */
    if(sem_post(&VYSTUP) == CHYBA) exit(EXIT_CHYBA);

    /* Prectu hodnotu v pameti */
    h = misto->hodnota;

    /* Uzamknu vystupni proud */
    if(sem_wait(&VYSTUP) == CHYBA) exit(EXIT_CHYBA);
    /* Vypisu prectenou hodnotu */
    fprintf(vystup, "%u: reader: %d: read: %d\n", misto->poradiAkce, poradi, h);
    /* Zvysim citac akci */
    misto->poradiAkce++;
    /* Uvolnim vystupni proud */
    if(sem_post(&VYSTUP) == CHYBA) exit(EXIT_CHYBA);

    /* Uzamknu pocet ctenaru */
    if(sem_wait(&CTENARU) == CHYBA) exit(EXIT_CHYBA);
    /* Snizim pocet ctenaru o 1 */
      misto->ctenaru--;
    /* Pokud jsem posledni ctenar */
    if(misto->ctenaru == 0) {
      /* Uvolnim zapis hodnoty */
      if(sem_post(&ZAPIS) == CHYBA) exit(EXIT_CHYBA);
      printf("U %d %d\n", poradi, misto->ctenaru);
    }
    /* Uvolnim pocet ctenaru */
    if(sem_post(&CTENARU) == CHYBA) exit(EXIT_CHYBA);

    /* Proces ctenare ceka po nahodnou dobu 0-rozsah */
    usleep((rand() % (rozsah+1)) * 1000);

    /* Pokud byla prectena hodnota 0, proces konci */
    if(h == 0) {
      break;
    }
  }

  /* Vse probehlo v poradku */
  exit(EXIT_OK);
}

/**
 * Funkce pro akci pri zachyceni signalu SIGINT, SIGTERM nebo SIGHUP
 * 
 * @param sig - cislo zachyceneho signalu
 */
void konci(int sig) {
  /* Konec cyklu pisare */
  misto->cyklu = -1;
  /* Konec procesu ctenare */
  misto->hodnota = 0;
  /* Vypis informace o zachyceni signalu */
  fprintf(stderr, "readerWriter: Zachycen signal: %d\n", sig);
}

int main(int argc, char *argv[])
{
  int pid  = 0;  /* Pomocna promenna pro ukladani pidu z funkce fork */
  int klic = 0;  /* Unikatni klic ke sdilene pameti procesu */
  int chyba = OK;  /* Chybovy kod parametru programu a funkci */
  int pametId = 0;  /* ID sdilene pameti */
  int ftokId  = 'k';  /* ID pro funkci ftok */
  long int cyklu  = 0;  /* Pocet cyklu procesu */
  long int pisaru = 0;  /* Pocet pisaru */
  char *soubor = NULL;  /* Nazev souboru pro vystup, pokud je zadan */
  long int ctenaru = 0;  /* Pocet ctenaru */
  char *cesta  = "/tmp";  /* Cesta pro funkci ftok */
  FILE *vystup = stdout;  /* Vystup programu */
  struct sigaction akce;  /* Promenna pro zachytavani signalu */
  void *pamet = (void *)0;  /* Ukazatel na sdilenou pamet */
  int returnKod = EXIT_OK;  /* Navratova hodnota programu */
  long int rozsahPisar  = 0;  /* Rozsah cekani pisare - simulace zpracovani */
  long int rozsahCtenar = 0;  /* Rozsah cekani ctenare - simulace zpracovani */

  /* Pokud nema program presne 7 parametru, byly spatne zadany */
  if(argc != 7) {
    /* Chyba */
    tiskChyby("Spatny pocet vstupnich parametru");
    /* A konec programu */
    exit(EXIT_ARG_CHYBA);
  }

  /* Ziskani poctu pisaru ze zadanych argumentu */
  pisaru = dejCislo(&chyba, argv[1]);
  /* Osetreni chyb argumentu */
  if(chyba == EXIT_ARG_CHYBA || pisaru <= 0) {
    tiskChyby("Spatne zadany parametr W - pocet pisaru");
    return EXIT_ARG_CHYBA;
  }

  /* Ziskani poctu ctenaru ze zadanych argumentu */
  ctenaru = dejCislo(&chyba, argv[2]);
  /* Osetreni chyb argumentu */
  if(chyba == EXIT_ARG_CHYBA || ctenaru <= 0) {
    tiskChyby("Spatne zadany parametr R - pocet ctenaru");
    return EXIT_ARG_CHYBA;
  }

  /* Ziskani cyklu ze zadanych argumentu */
  cyklu = dejCislo(&chyba, argv[3]);
  /* Osetreni chyb argumentu */
  if(chyba == EXIT_ARG_CHYBA || cyklu <= 0) {
    tiskChyby("Spatne zadany parametr C - pocet cyklu");
    return EXIT_ARG_CHYBA;
  }

  /* Ziskani rozsahu pisaru ze zadanych argumentu */
  rozsahPisar = dejCislo(&chyba, argv[4]);
  /* Osetreni chyb argumentu */
  if(chyba == EXIT_ARG_CHYBA || rozsahPisar < 0) {
    tiskChyby("Spatne zadany parametr SW - rozsah pisare");
    return EXIT_ARG_CHYBA;
  }

  /* Ziskani rozsahu ctenaru ze zadanych argumentu */
  rozsahCtenar = dejCislo(&chyba, argv[5]);
  /* Osetreni chyb argumentu */
  if(chyba == EXIT_ARG_CHYBA || rozsahCtenar < 0) {
    tiskChyby("Spatne zadany parametr SR - rozsah ctenare");
    return EXIT_ARG_CHYBA;
  }

  /* Zjisteni vystup - stdout/soubor */
  if(strcmp("-", argv[6]) != 0) {
    /* Ulozim si nazev zadaneho souboru */
    soubor = argv[6];

    /* Pokusim se otevrit soubor pro zapis */
    if((vystup = fopen(soubor, "w")) == NULL) {
      /* Chyba otevreni */
      fprintf(stderr, "readerWriter: error: Nepodarilo se otevrit soubor %s pro zapis\n", soubor);
      /* Konec programu */
      return EXIT_CHYBA;
    }
  }

  /* Ziskani unikatniho klice pro sdilenou pamet a semafory */
  if((klic = ftok(cesta, ftokId)) == (key_t)CHYBA) {
    /* Pokud je klic -1, nastala chyba */
    tiskChyby("Chyba funkce ftok()");
    /* Konec programu */
    return EXIT_CHYBA;
  }

  /* Vytvoreni bloku sdilene pameti s klicem klic a ulozeni identifikatoru */
  if((pametId = shmget(klic, sizeof(struktura), 0666 | IPC_CREAT)) == CHYBA) {
    /* Pokud je identifikator -1, nastala chyba */
    tiskChyby("Chyba funkce shmget()");
    /* Konec programu */
    return EXIT_CHYBA;
  }

  /* Pripojeni bloku sdilene pameti do adresoveho prostoru procesu */
  if((pamet = shmat(pametId, (void *)0, 0)) == (void *)CHYBA) {
    /* Pokud je ukazatel na prvni bajt -1, nastala chyba */
    tiskChyby("Chyba funkce shmat()");
    /* Odstraneni segmentu sdilene pameti */
    if(shmctl(pametId, IPC_RMID, 0) == CHYBA) {
      /* Pokud funkce vraci -1, nastala chyba */
      tiskChyby("Chyba funkce shmctl()");
    }
    /* Konec programu */
    return EXIT_CHYBA;
  }

  /* Prirazeni sdilene pameti na ukazatel */
  misto = (struktura *)pamet;
  /* Inicializace obsahu polozky ve sdilene pameti */
  misto->poradiAkce = 1;  /* Poradi akce procesu */
  misto->hodnota    = -1;  /* Sdilena hodnota */
  misto->ctenaru = 0;  /* Pocet ctenaru, kteri ctou */
  misto->pisaru  = 0;  /* Pocet pisaru kteri zapisuji */
  misto->cyklu   = cyklu; /* Pocet cyklu pisare */

  akce.sa_handler = konci;  /* Nastaveni funkce po signalu */
  sigemptyset(&akce.sa_mask);  /* Prazdna mnozina signalu */
  akce.sa_flags = 0;  /* Modifikatory akce */

  /* Prirazeni akce signalu SIGINT */
  sigaction(SIGINT, &akce, 0);
  /* Prirazeni akce signalu SIGTERM */
  sigaction(SIGTERM, &akce, 0);
  /* Prirazeni akce signalu SIGHUP */
  sigaction(SIGHUP, &akce, 0);

  sem_init(&CTENARU, 1, 1);
  sem_init(&PISARU, 1, 1);
  sem_init(&ZAPIS, 1, 1);
  sem_init(&CTENI, 1, 1);
  sem_init(&ZAMEK, 1, 1);
  sem_init(&VYSTUP, 1, 1);

  /* Vypnuti bufferu vystupniho proudu */
  setbuf(vystup, NULL);

  /* Cyklus pro vytvoreni procesu pro pisare */
  for(int i = 1; i <= pisaru && returnKod == EXIT_OK; i++) {
    /* Vytvorim novy proces sebe sama */
    pid = fork();

    /* Podle pidu vraceneho funkci fork se rozhodnu, ktery proces jsem */
    switch(pid) {
      /* Pokud je pid cislo 0, jedna se o novy proces pisare */
      case 0: {
        pisar(i, vystup, rozsahPisar);
      }
      /* Pokud je pid cislo -1, nastala chyba a program konci */
      case CHYBA: {
        tiskChyby("Chyba funkce fork() pri tvorbe pisaru");
        pisaru = (i - 1);
        misto->cyklu = -1;
        returnKod = EXIT_CHYBA;
      }
      /* Pokud je pid jiny, jedna se o rodice (hlavni proces) */
      default: {
        ;
      }
    }
  }

  /* Cyklus pro vytvoreni procesu pro ctenare */
  for(int i = 1; i <= ctenaru && returnKod == EXIT_OK; i++) {
    /* Vytvorim novy proces sebe sama */
    pid = fork();

    /* Podle pidu vraceneho funkci fork se rozhodnu, ktery proces jsem */
    switch(pid) {
      /* Pokud je pid cislo 0, jedna se o novy proces ctenare */
      case 0: {
        ctenar(i, vystup, rozsahCtenar);
      }
      /* Pokud je pid cislo -1, nastala chyba a program konci */
      case CHYBA: {
        tiskChyby("Chyba funkce fork() pri tvorbe ctenaru");
        ctenaru = (i - 1);
        misto->cyklu = -1;
        returnKod = EXIT_CHYBA;
      }
      /* Pokud je pid jiny, jedna se o rodice (hlavni proces) */
      default: {
        ;
      }
    }
  }

  /* Cyklus pro cekani na ukonceni pisaru */
  for(int i = 0; i < pisaru; i++) {
    /* Cekam na jakehokoliv pisare, az se ukonci */
    waitpid(-1, &chyba, 0);
    /* Zjistim, zda skoncil bez problemu */
    if(WIFEXITED(chyba)) {
      /* Zjistim jeho navratovy kod */
      chyba = WEXITSTATUS(chyba);
    } else {
      /* Pokud neskoncil bez problemu, nastavim priznak chyby */
      returnKod = EXIT_CHYBA;
    }

    /* Pokud navratovy kod potomka nebyl OK */
    if(chyba != EXIT_OK) {
      /* Nastavim priznak chyby */
      returnKod = EXIT_CHYBA;
    }
  }

  /* Az skonci vsichni pisari, ulozim do sdileneho prostoru cislo 0 */
  misto->hodnota = 0;

  /* Cyklus pro cekani na ukonceni ctenaru */
  for(int i = 0; i < ctenaru; i++) {
    /* Cekam na jakehokoliv ctenare, az se ukonci */
    waitpid(-1, &chyba, 0);
    /* Zjistim, zda skoncil bez problemu */
    if(WIFEXITED(chyba)) {
      /* Zjistim jeho navratovy kod */
      chyba = WEXITSTATUS(chyba);
    } else {
      /* Pokud neskoncil bez problemu, nastavim priznak chyby */
      returnKod = EXIT_CHYBA;
    }

    /* Pokud navratovy kod potomka nebyl OK */
    if(chyba != EXIT_OK) {
      /* Nastavim priznak chyby */
      returnKod = EXIT_CHYBA;
    }
  }

  /* Pokud byl zapis vystupu do souboru, uzavru jej */
  if(soubor != NULL) {
    /* Kdyz se uzavreni souboru nezdari, nastala chyba */
    if(fclose(vystup) != 0) {
      tiskChyby("Nepodarilo se uzavrit vystupni soubor");
      returnKod = EXIT_CHYBA;
    }
  }

  sem_destroy(&CTENARU);
  sem_destroy(&PISARU);
  sem_destroy(&ZAPIS);
  sem_destroy(&CTENI);
  sem_destroy(&ZAMEK);
  sem_destroy(&VYSTUP);

  /* Odpojeni sdilene pameti od aktualniho procesu */
  if(shmdt(pamet) == CHYBA) {
    /* Pokud funkce vraci -1, nastala chyba */
    tiskChyby("Chyba funkce shmdt()");
    /* Konec programu */
    returnKod = EXIT_CHYBA;
  }

  /* Odstraneni segmentu sdilene pameti */
  if(shmctl(pametId, IPC_RMID, 0) == CHYBA) {
    /* Pokud funkce vraci -1, nastala chyba */
    tiskChyby("Chyba funkce shmctl()");
    /* Konec programu */
    returnKod = EXIT_CHYBA;
  }

  /* Konec programu s navratovym kodem */
  return returnKod;
}
