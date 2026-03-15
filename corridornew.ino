#include <LiquidCrystal.h>

/* -------------------------------
   LCD setup
------------------------------- */
LiquidCrystal lcd(12, 11, 4, 5, 6, 7);

const int potX = A2; // movimento orizzontale
const int btnSW = 8; // pulsante SW
const int buzzerPin = 9; // pin buzzer

/* -------------------------------
   Custom Characters
------------------------------- */
byte playerChar[8] = { B00111,B01110,B11111,B10101,B11111,B01110,B00100,B00000 };
byte enemyChar[8] =  { B00000,B11111,B10101,B11111,B11111,B01110,B00100,B00000 };
byte mobileChar[8] = { B00000,B00000,B00000,B11111,B10001,B10101,B10001,B11111 };
byte armadioChar[8] = { B00000,B11111,B10101,B10101,B10101,B10101,B10101,B11111 };
byte carpetChar[8] =  { B00000,B00000,B00000,B00000,B00000,B00000,B00000,B11111 };
byte bombaChar[8] =   { B00000,B00000,B00010,B00100,B01110,B10001,B01110,B11111 };

/* -------------------------------
   ASCII door characters
------------------------------- */
const byte CLOSED_DOOR = 255;
const byte OPEN_DOOR   = 32;

/* -------------------------------
   Game constants
------------------------------- */
const int SCREEN_WIDTH = 16;
const int NUM_CORRIDOI = 5;

/* -------------------------------
   GameObject struct
------------------------------- */
struct GameObject {
    int pos;
    byte charId;
    String name;
    bool hasKey;
    bool exists;
    void (*interact)();
    bool specialDoor = false;
    bool isGuaranteedChest = false;
};

/* -------------------------------
   Corridor struct
------------------------------- */
struct Corridor {
    GameObject objects[3]; // 0: Mobile, 1: Armadio, 2: Porta
};

/* -------------------------------
   Game variables
------------------------------- */
Corridor corridors[NUM_CORRIDOI];
int currentCorridor = 0;
int playerX = 0;
int keysCollected = 0;

// -------------------------------
// Struct Piano / Floor
// -------------------------------
struct Floor {
    char id;               // A, B, C...
    byte difficulty;        // 1=facile, 2=medio, 3=difficile
    bool optional;          // true se piano opzionale
    bool hasTimer;          // true se piano con timer speciale
    bool miniBoss;          // true se mini-boss
    int numEnemies;         // numero nemici base
    int enemyType[3];       // 0=base,1=fantasma,2=blindato
    int numChests;          // numero forzieri
    bool hasArmadio;        // armadio presente
};

// Dichiarazione piani iniziali
Floor floorA = {'A', 1, false, false, false, 1, {0}, 1, true};
Floor floorB = {'B', 2, false, false, false, 1, {0,1}, 1, true};

// Piano corrente (iniziale)
Floor currentFloor = floorA;

/* -------------------------------
   Nemico variables
------------------------------- */
int enemyX;
int enemyCorridor;
unsigned long lastEnemyMove = 0;
const unsigned long ENEMY_DELAY = 500;
int enemyDir = 1;
bool enemyChasing = false;
int pauseAfterDoor = 0;
bool enemyAlive[NUM_CORRIDOI];



// -------------------------------
// Sistema vite
// -------------------------------
const byte MAX_LIVES = 3;
byte playerLives = 0;

int totalDoorsInFloor = 0;
int openedDoorsInFloor = 0;
bool lifeGrantedThisFloor = false;

/* -------------------------------
   Armadio variables
------------------------------- */
bool insideArmadio = false;
int savedPlayerX = 0;

/* -------------------------------
   TIC variables
------------------------------- */
unsigned long lastTic = 0;
const unsigned long TIC_DELAY = 1800;
unsigned long lastMove = 0;
const unsigned long MOVE_DELAY = 200;
unsigned long lastPlayerAction = 0;
const unsigned long AUTO_TIC_DELAY = 2000; // 2 secondi

// Pulsante SW

unsigned long lastClickTime = 0;
const unsigned long CLICK_DELAY = 250;

// HUD
bool showKeyPickup = false;
bool showNothing = false;
unsigned long keyPickupTime = 0;
unsigned long nothingTime = 0;
bool showDoorClosed = false;
unsigned long doorClosedTime = 0;
const unsigned long HUD_MSG_DURATION = 1000;

// HUD per bomba
bool showBombPickup = false;
unsigned long bombPickupTime = 0;

/* -------------------------------
   Bombe
------------------------------- */
int numBombe = 1;
bool bombaPosizionata = false;
int bombaX = -1;
int bombaTicCount = 0;
const int BOMBA_TIC_DELAY = 5;
const int BOMBA_RAGGIO = 1;
int bombaCorridor = -1;

/* -------------------------------
   Debug / Game Over
------------------------------- */
bool debugActive = false;
String debugMsg = "";
bool gameOverState = false;


/* -------------------------------
   Interazioni oggetti
------------------------------- */
void interactMobile() {
    Corridor &c = corridors[currentCorridor];
    
    // Se il mobile è già stato aperto
    if (!c.objects[0].exists) {
        showNothing = true;
        nothingTime = millis();
        showKeyPickup = false;
        showBombPickup = false;
        return;
    }

    // --- FORZIERE GARANTITO ---
    if (c.objects[0].isGuaranteedChest) {
        if (c.objects[0].hasKey) {
            keysCollected++;
            showKeyPickup = true;
            keyPickupTime = millis();
        } 
        else {
            numBombe++;
            showBombPickup = true;
            bombPickupTime = millis();
        }
        c.objects[0].hasKey = false;
        c.objects[0].isGuaranteedChest = false;
        c.objects[0].exists = false; // ora il mobile non è più interagibile
        showNothing = false;
        return;
    }

    // --- FORZIERE NORMALE ---
    if (c.objects[0].hasKey) {
        keysCollected++;
        showKeyPickup = true;
        keyPickupTime = millis();
    } 
    else {
        int drop = random(0, 3); // 0=niente,1=chiave,2=bomba
        if (drop == 1) {
            keysCollected++;
            showKeyPickup = true;
            keyPickupTime = millis();
        } 
        else if (drop == 2) {
            numBombe++;
            showBombPickup = true;
            bombPickupTime = millis();
        } 
        else {
            showNothing = true;
            nothingTime = millis();
            showKeyPickup = false;
            showBombPickup = false;
        }
    }

    // Dopo la prima interazione il mobile diventa vuoto
    c.objects[0].hasKey = false;
    c.objects[0].exists = false;
}


void interactArmadio() {
    if (!insideArmadio) {
        savedPlayerX = playerX;
        playerX = -1;
        insideArmadio = true;
        enemyChasing = false;
    } else {
        playerX = savedPlayerX;
        insideArmadio = false;
    }
}

/* -------------------------------
   Porta
------------------------------- */
void playDoorOpenSound() {
    tone(buzzerPin, 150, 30); delay(40);
    tone(buzzerPin, 300, 15); delay(20);
    noTone(buzzerPin);
}
void interactPorta() {
    Corridor &c = corridors[currentCorridor];

    if (c.objects[2].charId != OPEN_DOOR) {
        // Porta chiusa
        if (keysCollected > 0) {
            keysCollected--;
            c.objects[2].charId = OPEN_DOOR;
            playDoorOpenSound();

            openedDoorsInFloor++;

            // Bonus vita solo se almeno 3 porte nel piano
            if (totalDoorsInFloor >= 3 && !lifeGrantedThisFloor &&
                openedDoorsInFloor >= totalDoorsInFloor &&
                playerLives < MAX_LIVES) {

                playerLives++;
                lifeGrantedThisFloor = true;

                // Suono riconoscibile per acquisizione vita
                tone(buzzerPin, 1000, 200);  // durata 200ms
            }

        } else {
            // Porta chiusa senza chiave
            showDoorClosed = true;
            doorClosedTime = millis();
            showKeyPickup = false;
            showNothing = false;
        }
    } else {
        // Porta già aperta → cambio piano
        chooseNextFloor();
    }
}





/* -------------------------------
   Genera corridoi e oggetti
------------------------------- */
void generateCorridor(Corridor &c, Floor f, bool placeObstacle = false) {
    for(int i=0;i<3;i++){
        c.objects[i].exists = false;
        c.objects[i].pos = -1;
        c.objects[i].name = "";
        c.objects[i].charId = 0;
        c.objects[i].hasKey = false;
        c.objects[i].interact = nullptr;
        c.objects[i].specialDoor = false;
        c.objects[i].isGuaranteedChest = false;
    }

    // Mobile
    if(random(0,100) < 80){
        c.objects[0].exists = true;
        c.objects[0].pos = random(2, SCREEN_WIDTH-1);
        c.objects[0].charId = 3;
        c.objects[0].name = "mobile";
        c.objects[0].hasKey = random(0,2);
        c.objects[0].interact = interactMobile;
    }

    // Armadio
    if(random(0,100) < 50){
        c.objects[1].exists = true;
        c.objects[1].pos = random(2, SCREEN_WIDTH-1);
        c.objects[1].charId = 4;
        c.objects[1].name = "armadio";
        c.objects[1].interact = interactArmadio;
        c.objects[1].hasKey = false;
    }

    // Porta
    if(random(0,100) < 50){
        c.objects[2].exists = true;
        c.objects[2].pos = random(2, SCREEN_WIDTH-4);
        c.objects[2].charId = CLOSED_DOOR;
        c.objects[2].name = "porta";
        c.objects[2].interact = interactPorta;

        // Porta speciale solo per piano B
        if(f.id == 'B' && random(0,100) < 50){  
            c.objects[2].specialDoor = true;
            c.objects[2].pos = SCREEN_WIDTH / 2;  
        }
    }

    // Ostacolo garantito
    if(placeObstacle){
        c.objects[0].exists = true;
        c.objects[0].pos = SCREEN_WIDTH/2;
        c.objects[0].charId = 'g';       
        c.objects[0].name = "ostacolo";
        c.objects[0].interact = nullptr;
        c.objects[0].hasKey = false;
    }
}



// Assicura almeno una porta normale
void ensureAtLeastOneDoor() {
    bool hasDoor = false;
    for(int i=0; i<NUM_CORRIDOI; i++){
        if(corridors[i].objects[2].exists && !corridors[i].objects[2].specialDoor){
            hasDoor = true;
            break;
        }
    }

    if(!hasDoor){
        int idx = random(0, NUM_CORRIDOI);
        corridors[idx].objects[2].exists = true;
        corridors[idx].objects[2].charId = CLOSED_DOOR;
        corridors[idx].objects[2].name = "porta";
        corridors[idx].objects[2].interact = interactPorta;
        corridors[idx].objects[2].specialDoor = false;
    }
}

// Assicura almeno un forziere garantito
void ensureGuaranteedChest() {
    int idx = random(0, NUM_CORRIDOI);
    corridors[idx].objects[0].exists = true;
    corridors[idx].objects[0].pos = random(2, SCREEN_WIDTH-1);
    corridors[idx].objects[0].charId = 3;
    corridors[idx].objects[0].name = "mobile";
    corridors[idx].objects[0].interact = interactMobile;
    corridors[idx].objects[0].isGuaranteedChest = true;
    corridors[idx].objects[0].hasKey = random(0,2); // 0=bomba, 1=chiave
}

/* -------------------------------
   Interazione con tappeto per piazzare bomba
------------------------------- */
void interact() {
    Corridor &c = corridors[currentCorridor];
    bool clicked = false;

    for(int i=0;i<3;i++){
        if(c.objects[i].exists && playerX == c.objects[i].pos){
            c.objects[i].interact();
            clicked = true;
            break;
        }
    }

    if(!clicked && numBombe > 0){
        bombaPosizionata = true;
        bombaX = playerX;
        bombaTicCount = 0;
        bombaCorridor = currentCorridor;
        numBombe--;
    }
}


/* -------------------------------
   Nemico AI
------------------------------- */
void updateEnemy() {
    if (enemyCorridor < 0 || !enemyAlive[enemyCorridor]) return;

    if (pauseAfterDoor > 0) {
        pauseAfterDoor--;
        return;
    }

    if (insideArmadio){
        // Nemico si muove casualmente se sei nascosto
        enemyX += (enemyX < SCREEN_WIDTH/2) ? -2 : 2;
        if(enemyX < 0 || enemyX >= SCREEN_WIDTH){
            int newCorr;
            do { newCorr = random(0, NUM_CORRIDOI); } while(newCorr == currentCorridor);
            enemyCorridor = newCorr;
            enemyX = random(5, 11);
        }
        return;
    }

    if (!enemyChasing && enemyCorridor == currentCorridor)
        enemyChasing = true;

    if (enemyChasing){
        if (enemyCorridor != currentCorridor){
            enemyCorridor = currentCorridor;
            enemyX = (playerX < SCREEN_WIDTH/2) ? SCREEN_WIDTH-1 : 0;
            return;
        }

        if (enemyX < playerX) enemyX++;
        else if (enemyX > playerX) enemyX--;
    }

    // -------------------------------
    // Collisione nemico
    // -------------------------------
    if (!insideArmadio &&
        enemyCorridor == currentCorridor &&
        enemyX == playerX){

        // Se non hai vite → GAME OVER vero
        if (playerLives == 0){
            gameOverState = true;

            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("GAME OVER!");
            lcd.setCursor(0,1);
            lcd.print("Premi restart");

            while (!buttonClicked()) { }

            // Reset totale
            keysCollected = 0;
            numBombe = 1;
            playerLives = 0;

            generateFloor(floorA);   // riparte dal piano A
            gameOverState = false;
            return;
        }

        // Altrimenti perdi una vita
        playerLives--;

        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("HAI PERSO 1 VITA");
        lcd.setCursor(0,1);
        lcd.print("Vite: ");
        lcd.print(playerLives);
        delay(1500);

        // Ricarica piano corrente
        generateFloor(currentFloor);
        return;
    }
}


/* -------------------------------
   Aggiornamento bomba
------------------------------- */
void updateBomba() {
    if (!bombaPosizionata) return;

    Corridor &c = corridors[bombaCorridor];

    if(currentCorridor == bombaCorridor){
        bombaTicCount++;

        if(bombaTicCount >= BOMBA_TIC_DELAY){

            // Suono esplosione a toni decrescenti
            int freqs[] = {400,350,300,250,200,150,120,100};
            for(int i=0;i<8;i++){
                tone(buzzerPin,freqs[i],50);
                delay(50);
            }
            noTone(buzzerPin);

            // Area esplosione
            int start = max(0,bombaX-BOMBA_RAGGIO);
            int end   = min(SCREEN_WIDTH-1,bombaX+BOMBA_RAGGIO);

            for(int i=start;i<=end;i++){
                for(int j=0;j<3;j++){
                    if(c.objects[j].exists && c.objects[j].pos == i){

                        // Porta
                        if(c.objects[j].name=="porta" &&
                           c.objects[j].charId != OPEN_DOOR){

                            c.objects[j].charId = OPEN_DOOR;
                            playDoorOpenSound();

                            openedDoorsInFloor++;

                            // Bonus vita solo con almeno 3 porte totali
                            if(!lifeGrantedThisFloor &&
                               openedDoorsInFloor >= totalDoorsInFloor &&
                               totalDoorsInFloor >= 3 &&
                               playerLives < MAX_LIVES){

                                playerLives++;
                                lifeGrantedThisFloor = true;

                                // Feedback sonoro per vita bonus
                                tone(buzzerPin, 1000, 200);
                            }
                        }

                        // Ostacolo
                        else if(c.objects[j].name=="ostacolo"){
                            c.objects[j].exists = false;
                        }
                    }
                }

                // Nemico
                if(enemyCorridor == bombaCorridor && enemyX == i){
                    enemyAlive[enemyCorridor] = false;
                }

                // Effetto visivo esplosione
                lcd.setCursor(i,1);
                lcd.write(byte(255));
                delay(50);
            }

            // Reset bomba
            bombaPosizionata = false;
            bombaTicCount = 0;
            bombaX = -1;
            bombaCorridor = -1;
        }
    }
    else {
        bombaTicCount = 0;
    }
}




/* -------------------------------
   Tic
------------------------------- */
void tic() {
    updateEnemy();
    updateBomba();
    drawCorridor();
    lastTic = millis();
}

/* -------------------------------
   Draw corridor con HUD
------------------------------- */
void drawCorridor() {
    lcd.clear();
    Corridor &c = corridors[currentCorridor];

    // Tappeto
    lcd.setCursor(0,1);
    for(int i=0;i<SCREEN_WIDTH;i++) lcd.write(byte(2));

    // Oggetti
    for(int i=0;i<3;i++){
        if(c.objects[i].exists){
            lcd.setCursor(c.objects[i].pos,1);
            lcd.write(c.objects[i].charId);
        }
    }

    // Bomba sul tappeto
    if(bombaPosizionata && bombaX >= 0 && currentCorridor == bombaCorridor){
        lcd.setCursor(bombaX,1);
        lcd.write(byte(5));
    }

    // Player
    if(!insideArmadio && playerX >= 0){
        lcd.setCursor(playerX,1);
        lcd.write(byte(0));
    }

    // Nemico
    if(currentCorridor == enemyCorridor && enemyCorridor != -1 && enemyAlive[enemyCorridor]){
        lcd.setCursor(enemyX,1);
        lcd.write(byte(1));
    }

    // HUD riga 0
    lcd.setCursor(0,0);
    lcd.print("K");
    lcd.print(keysCollected);

    if (showKeyPickup && millis()-keyPickupTime<HUD_MSG_DURATION) {
        lcd.print(" +1chiave");
    } 
    else if (showBombPickup && millis()-bombPickupTime<HUD_MSG_DURATION){
        lcd.print(" +1bomba ");
    } 
    else if (insideArmadio){
        lcd.print(" nascosto >");
    } 
    else if (showNothing && millis()-nothingTime<HUD_MSG_DURATION){
        lcd.print(" niente   ");
    } 
    else if (showDoorClosed && millis()-doorClosedTime<HUD_MSG_DURATION){
        lcd.print(" chiusa   ");
    } 
    else{
        // Mostra porta speciale “o” se presente
        bool specialPrinted = false;
        for(int i=0;i<3;i++){
            if(c.objects[i].exists && c.objects[i].specialDoor && c.objects[i].charId != OPEN_DOOR){
                lcd.setCursor(c.objects[i].pos,0);
                lcd.write('o');
                specialPrinted = true;
            }
        }
        if(!specialPrinted){
            lcd.print("         "); // pulizia riga 0
        }

        // reset flag HUD
        showKeyPickup=false;
        showNothing=false;
        showDoorClosed=false;
        showBombPickup=false;
    }

    // Corridoio.
// Mostra piano + corridoio (es: A1, A2, B1)
lcd.setCursor(SCREEN_WIDTH-2,0);
lcd.print(currentFloor.id);       // <-- qui usi il char id della struct Floor
lcd.print(currentCorridor + 1);


    // Bomba posseduta
    if(numBombe>0){
        lcd.setCursor(SCREEN_WIDTH-3,0);
        lcd.write(byte(5));
    }
}
// -------------------------------
// Genera piano (floor)
// -------------------------------
void generateFloor(Floor f) {
    currentFloor = f;

    // Reset player
    currentCorridor = 0;
    playerX = 0;
    insideArmadio = false;

    // Reset bombe
    bombaPosizionata = false;
    bombaX = -1;
    bombaCorridor = -1;

    // Reset HUD
    showKeyPickup = false;
    showNothing = false;
    showDoorClosed = false;
    showBombPickup = false;

    // Porta speciale unica per piano B
    bool specialDoorPlaced = false;

    // Ostacolo garantito per piano B
    int obstacleCorridor = -1;
    if(f.id == 'B')
        obstacleCorridor = random(0, NUM_CORRIDOI);

    // Genera corridoi
    for(int i=0;i<NUM_CORRIDOI;i++){
        bool placeObstacle = (i == obstacleCorridor);
        generateCorridor(corridors[i], f, placeObstacle);

        enemyAlive[i] = true;

        // Porta speciale (una sola)
        if(f.id == 'B' && !specialDoorPlaced && corridors[i].objects[2].exists){
            if(random(0,100) < 50){
                corridors[i].objects[2].specialDoor = true;
                corridors[i].objects[2].pos = SCREEN_WIDTH/2;
                specialDoorPlaced = true;
            }
        }
    }

    // Spawn nemico lontano dal player
    do {
        enemyCorridor = random(0, NUM_CORRIDOI);
    } while (enemyCorridor == currentCorridor);

    do {
        enemyX = random(3, SCREEN_WIDTH - 3);
    } while (abs(enemyX - playerX) < 4);

    enemyChasing = false;
    pauseAfterDoor = 0;

    // -------------------------------
    // Sistema bonus vita (porte)
    // -------------------------------
    totalDoorsInFloor = 0;
    openedDoorsInFloor = 0;
    lifeGrantedThisFloor = false;

    for(int c=0; c<NUM_CORRIDOI; c++){
        for(int i=0; i<3; i++){
            if(corridors[c].objects[i].exists &&
               corridors[c].objects[i].name == "porta"){
                totalDoorsInFloor++;
            }
        }
    }

    drawCorridor();
}



void chooseNextFloor() {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Scegli piano:");

    char selected = 'A';
    bool confirmed = false;

    while(!confirmed){
        int xVal = analogRead(potX);

        // Selezione con potenziometro
        selected = (xVal < 512) ? 'A' : 'B';

        // Riga 2: pulizia area selezione (0–4)
        lcd.setCursor(0,1);
        lcd.print("     ");

        // Scritta fissa
        lcd.setCursor(0,1);
        lcd.print("A B");

        // Freccia
        if(selected == 'A') lcd.setCursor(0,1);
        else lcd.setCursor(2,1);
        lcd.print(">");

        // ❤️ VITE NELLA STESSA SCHERMATA
        drawLivesBottomRight();

        if(buttonClicked()) confirmed = true;
    }

    // Imposta piano
    currentFloor = (selected == 'A') ? floorA : floorB;
    generateFloor(currentFloor);
}


void drawLivesBottomRight() {
    // Pulisce area vite (colonne 13-15, riga 1)
    for(int i = 13; i < 16; i++){
        lcd.setCursor(i,1);
        lcd.print(" ");
    }

    // Disegna le vite (sprite player)
    for(int i = 0; i < playerLives && i < MAX_LIVES; i++){
        lcd.setCursor(15 - i, 1);
        lcd.write(byte(0));   // 👈 PLAYER CHAR
    }
}

bool buttonClicked() {
    if (digitalRead(btnSW) == LOW) {
        unsigned long now = millis();
        if (now - lastClickTime > CLICK_DELAY) {
            lastClickTime = now;
            return true;
        }
    }
    return false;
}


/* -------------------------------
   Setup
------------------------------- */
void setup() {
    lcd.begin(16,2);

    // Carica i caratteri custom
    lcd.createChar(0, playerChar);
    lcd.createChar(1, enemyChar);
    lcd.createChar(2, carpetChar);
    lcd.createChar(3, mobileChar);
    lcd.createChar(4, armadioChar);
    lcd.createChar(5, bombaChar);

    pinMode(btnSW, INPUT_PULLUP);
    randomSeed(analogRead(A0));
    pinMode(buzzerPin, OUTPUT);

    // Genera corridoi
for(int i=0;i<NUM_CORRIDOI;i++) generateCorridor(corridors[i], currentFloor);

    // Inserisci il forziere garantito
    ensureGuaranteedChest();

    // Inserisci almeno 1 porta normale
    ensureAtLeastOneDoor();

    // Inizializza nemici
    for(int i=0;i<NUM_CORRIDOI;i++) enemyAlive[i] = true;
    enemyCorridor = random(0,NUM_CORRIDOI);
    enemyX = random(5,11);
    enemyDir = 1;
    enemyChasing = false;

    drawCorridor();
    lastTic = millis();
}

/* -------------------------------
   Loop
------------------------------- */
void loop() {
    unsigned long now = millis();
    int xVal = analogRead(potX);
    bool moved = false;

    // Movimento player
    if(!debugActive && !insideArmadio && now - lastMove >= MOVE_DELAY){
        Corridor &c = corridors[currentCorridor]; // corridoio corrente

        // Muovi a destra
        if(xVal < 400){
            if(playerX < SCREEN_WIDTH-1){
                bool blocked = false;
                // Controllo ostacolo davanti
                for(int i=0;i<3;i++){
                    if(c.objects[i].exists && c.objects[i].name == "ostacolo" && c.objects[i].pos == playerX+1){
                        blocked = true;
                        break;
                    }
                }
                if(!blocked) playerX++;
            } else {
                currentCorridor++;
                if(currentCorridor >= NUM_CORRIDOI) currentCorridor = 0;
                playerX = 0;
            }
            moved = true;
        }

        // Muovi a sinistra
        if(xVal > 600){
            if(playerX > 0){
                bool blocked = false;
                // Controllo ostacolo davanti
                for(int i=0;i<3;i++){
                    if(c.objects[i].exists && c.objects[i].name == "ostacolo" && c.objects[i].pos == playerX-1){
                        blocked = true;
                        break;
                    }
                }
                if(!blocked) playerX--;
            } else {
                currentCorridor--;
                if(currentCorridor < 0) currentCorridor = NUM_CORRIDOI-1;
                playerX = SCREEN_WIDTH-1;
            }
            moved = true;
        }

        if(moved){
            lastMove = now;
            lastPlayerAction = now;
            tic(); // 1 tic
        }
    }

    // Gestione pulsante
    if(buttonClicked() && !gameOverState){
        lastPlayerAction = now;

        if(insideArmadio){
            playerX = savedPlayerX;
            insideArmadio = false;
            tic();
            drawCorridor();
        } else {
            interact();
            tic();
        }
    }

    // Tic automatico se fermo 2 secondi
    if(now - lastPlayerAction >= AUTO_TIC_DELAY && !gameOverState){
        lastPlayerAction = now;
        tic();
    }
}
