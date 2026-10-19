/*
  TP1 – Ali Sihadi
*/

const int PIN_BOUTON = 4;   // D2 Bouton
const int PIN_BUZZER = 5;   // D1 Buzzer a travers transistor

// Registres à décalage en cascade
const int PIN_DS1   = 13;   // D7 les Données
const int PIN_STCP1 = 12;   // D6 (Latch)
const int PIN_SHCP1 = 14;   // D5 (Clock)

// VARIABLES DU JEU
int score = 0;       // 0 à 3 points
int prises = 0;      // 0 à 2 prises
int cibleActive = 1; // Cible choisie pour le round (1, 2 ou 3)

enum EtatJeu {
  INIT_JEU,
  AFFICHER_CIBLE,
  ATTENTE_APPUI,
  CHARGEMENT_PUISSANCE,
  RESULTAT_RONDE,
  FIN_PARTIE
};

EtatJeu etatActuel = INIT_JEU;
unsigned long tempsAppuiDebut = 0;
int niveauPuissance = 0;

void ecrireRegistres(uint8_t reg2Data, uint8_t reg1Data) {
  digitalWrite(PIN_STCP1, LOW);
  shiftOut(PIN_DS1, PIN_SHCP1, MSBFIRST, reg2Data);
  shiftOut(PIN_DS1, PIN_SHCP1, MSBFIRST, reg1Data);
  digitalWrite(PIN_STCP1, HIGH);
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_BOUTON, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  
  pinMode(PIN_DS1, OUTPUT);
  pinMode(PIN_STCP1, OUTPUT);
  pinMode(PIN_SHCP1, OUTPUT);

  ecrireRegistres(0x00, 0x00);
  randomSeed(analogRead(A0));
}

void loop() {
  switch (etatActuel) {
    
    // 1. DÉBUT DE PARTIE
    case INIT_JEU: {
      score = 0;
      prises = 0;
      
      fanfareDebut();
      
      for (int i = 0; i < 3; i++) {
        ecrireRegistres(0b11111110, 0b00111110);
        delay(300);
        ecrireRegistres(0x00, 0x00);
        delay(300);
      }
      
      delay(1000);
      etatActuel = AFFICHER_CIBLE;
      break;
    }

    // 2. CHOIX ET AFFICHAGE DE LA CIBLE
    case AFFICHER_CIBLE: {
      cibleActive = random(1, 4); // Aléatoire entre 1 et 3
      
      // Masque cible : Q1, Q2 ou Q3
      uint8_t maskCible = (1 << cibleActive); 
      uint8_t maskScorePrise = getScorePriseMask();
      
      ecrireRegistres(maskCible | maskScorePrise, 0x00);
      tone(PIN_BUZZER, 440, 200);
      delay(1000); 

      // Éteindre la cible, conserver score et prise
      ecrireRegistres(maskScorePrise, 0x00);
      etatActuel = ATTENTE_APPUI;
      break;
    }

    // 3. ATTENTE DE L'APPUI SUR LE BOUTON
    case ATTENTE_APPUI: {
      if (digitalRead(PIN_BOUTON) == LOW) {
        delay(30);
        if (digitalRead(PIN_BOUTON) == LOW) {
          tempsAppuiDebut = millis();
          niveauPuissance = 0;
          etatActuel = CHARGEMENT_PUISSANCE;
        }
      }
      break;
    }

    // 4. CHARGEMENT DE LA PUISSANCE
    case CHARGEMENT_PUISSANCE: {
      unsigned long tempsMaintenu = millis() - tempsAppuiDebut;
      niveauPuissance = (tempsMaintenu / 400) + 1; // Monte d'un niveau de puissance chaque 400ms

      if (niveauPuissance > 5) {
        niveauPuissance = 5;
        jouerSonTirRate();
        enregistrerResultat(false);
        etatActuel = RESULTAT_RONDE;
        break;
      }

      // Allumer la barre de puissance (Q1 à Q5 -> bits 1 à 5)
      uint8_t maskPuissance = 0;
      for (int i = 1; i <= niveauPuissance; i++) {
        maskPuissance |= (1 << i);
      }
      
      ecrireRegistres(getScorePriseMask(), maskPuissance);
      tone(PIN_BUZZER, 200 + (niveauPuissance * 100), 80);

      // Si l'utilisateur lache le bouton
      if (digitalRead(PIN_BOUTON) == HIGH) {
        delay(30);
        jouerSonVol();

        // On évalue le succès selon la cible et la puissance atteinte
        bool succes = false;
        if (cibleActive == 1 && (niveauPuissance == 1 || niveauPuissance == 2)) succes = true;
        if (cibleActive == 2 && (niveauPuissance == 3 || niveauPuissance == 4)) succes = true;
        if (cibleActive == 3 && niveauPuissance == 5) succes = true;

        enregistrerResultat(succes);
        etatActuel = RESULTAT_RONDE;
      }
      break;
    }

    // 5. RÉSULTAT DU ROUND
    case RESULTAT_RONDE: {
      ecrireRegistres(getScorePriseMask(), 0x00);
      delay(1000);

      if (score >= 3) {
        fanfareVictoire();
        etatActuel = FIN_PARTIE;
      } else if (prises >= 2) {
        fanfareDefaite();
        etatActuel = FIN_PARTIE;
      } else {
        etatActuel = AFFICHER_CIBLE;
      }
      break;
    }

    // 6. FIN DE PARTIE
    case FIN_PARTIE: {
      ecrireRegistres(0x00, 0x00);
      delay(4000);
      etatActuel = INIT_JEU;
      break;
    }
  }
}

// FONCTIONS UTILISÉS

uint8_t getScorePriseMask() {
  uint8_t mask = 0;
  // Score sur Q4, Q5, Q6 (bits 4, 5, 6)
  for (int i = 0; i < score; i++) {
    mask |= (1 << (4 + i));
  }
  // Indicateur de prise sur Q7
  if (prises >= 1) {
    mask |= (1 << 7);
  }
  return mask;
}

void enregistrerResultat(bool succes) {
  if (succes) {
    score++;
    jouerSonSucces();
    // Clignotement de la cible réussie
    uint8_t targetMask = (1 << cibleActive);
    for(int i = 0; i < 3; i++) {
      ecrireRegistres(targetMask | getScorePriseMask(), 0x00);
      delay(150);
      ecrireRegistres(getScorePriseMask(), 0x00);
      delay(150);
    }
  } else {
    prises++;
    jouerSonEchec();
    // Clignotement de la prise rouge
    for(int i = 0; i < 3; i++) {
      ecrireRegistres(0b10000000 | getScorePriseMask(), 0x00);
      delay(150);
      ecrireRegistres(getScorePriseMask(), 0x00);
      delay(150);
    }
  }
}

// EFFETS SONORES
void fanfareDebut() {
  tone(PIN_BUZZER, 262, 150); delay(180);
  tone(PIN_BUZZER, 330, 150); delay(180);
  tone(PIN_BUZZER, 392, 150); delay(180);
  tone(PIN_BUZZER, 523, 300); delay(350);
  noTone(PIN_BUZZER);
}

void jouerSonVol() {
  for (int f = 800; f > 200; f -= 150) {
    tone(PIN_BUZZER, f, 40);
    delay(40);
  }
  noTone(PIN_BUZZER);
}

void jouerSonSucces() {
  tone(PIN_BUZZER, 523, 120); delay(140);
  tone(PIN_BUZZER, 659, 120); delay(140);
  tone(PIN_BUZZER, 784, 250); delay(280);
  noTone(PIN_BUZZER);
}

void jouerSonEchec() {
  tone(PIN_BUZZER, 200, 200); delay(220);
  tone(PIN_BUZZER, 150, 350); delay(380);
  noTone(PIN_BUZZER);
}

void jouerSonTirRate() {
  tone(PIN_BUZZER, 100, 400);
  delay(450);
  noTone(PIN_BUZZER);
}

void fanfareVictoire() {
  tone(PIN_BUZZER, 440, 150); delay(180);
  tone(PIN_BUZZER, 554, 150); delay(180);
  tone(PIN_BUZZER, 659, 150); delay(180);
  tone(PIN_BUZZER, 880, 450); delay(500);
  noTone(PIN_BUZZER);
}

void fanfareDefaite() {
  tone(PIN_BUZZER, 300, 300); delay(330);
  tone(PIN_BUZZER, 250, 300); delay(330);
  tone(PIN_BUZZER, 200, 600); delay(650);
  noTone(PIN_BUZZER);
}