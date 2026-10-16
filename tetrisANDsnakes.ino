#include <Arduino.h>
#include <LedControl.h>

#define pinDIN     11
#define pinCLK     13
#define pinCS      10
#define numModules 4
LedControl lc(pinDIN, pinCLK, pinCS, numModules);

#define VRx A0
#define VRy A1
#define SW  8
#define btnA 2  // Tetris: hard drop & Snake: restart after game over
#define btnB 3  // Tetris: rotate | Snake: boost speed
#define btnC 4  // Restart current game
#define btnD 5  // Pause/Resume
#define btnE 6  // Switch game
#define btnF 7  // Sound on/off

#define buzzerPin 9

const int screenW = 8;
const int screenH = screenW * numModules;

uint8_t prevBuf[numModules][screenW];

#define NUM_GAMES 2
enum { GAME_TETRIS = 0, GAME_SNAKE = 1 };
byte currentGame = GAME_TETRIS;

bool paused = false;
const int BRIGHTNESS = 8;

bool soundOn = true;

const bool PITCH_GATING = false;

void silence() {
  noTone(buzzerPin);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, HIGH);   // HIGH = off
}

void buzzOn(unsigned int freq) {
  if (PITCH_GATING) tone(buzzerPin, freq);
  else              digitalWrite(buzzerPin, LOW);
}

void beep(unsigned int freq, unsigned long dur) {
  if (!soundOn) return;
  buzzOn(freq);
  delay(dur);
  silence();
}

const int melody[] = {
  659, 494, 523, 587, 523, 494, 440, 440, 523, 659, 587, 523,
  494, 523, 587, 659, 523, 440, 440, 494, 587, 523, 494, 440
};
const int noteDurations[] = {
  4, 8, 8, 4, 8, 8, 4, 8, 8, 4, 8, 8,
  4, 8, 8, 4, 8, 8, 4, 8, 8, 4, 8, 8
};
const int melodyLength = sizeof(melody) / sizeof(melody[0]);

int  currentNote = 0;
bool notePlaying = false;
bool noteSounding = false;
unsigned long noteStartTime = 0;
unsigned long noteLenMs = 0;
unsigned long noteGapMs = 0;

void updateMusic(unsigned long now) {
  if (!soundOn) return;

  if (!notePlaying) {
    noteLenMs = 1000 / noteDurations[currentNote];
    noteGapMs = noteLenMs * 1.4;
    buzzOn(melody[currentNote]);
    noteStartTime = now;
    notePlaying = true;
    noteSounding = true;
  } else if (noteSounding && now - noteStartTime >= noteLenMs) {
    silence();
    noteSounding = false;
  } else if (now - noteStartTime >= noteGapMs) {
    notePlaying = false;
    currentNote = (currentNote + 1) % melodyLength;
  }
}

void stopMusic() {
  notePlaying = false;
  noteSounding = false;
  silence();
}

void toggleSound() {
  soundOn = !soundOn;
  stopMusic();
  if (soundOn) beep(1000, 60);
}
// -------------------------------------

void clearAll() {
  for (int m = 0; m < numModules; m++) lc.clearDisplay(m);
}

void clearScreen() {
  clearAll();
  for (int m = 0; m < numModules; m++)
    for (int r = 0; r < screenW; r++) prevBuf[m][r] = 0;
}

bool readButtonPin(int pin) {
  if (digitalRead(pin) == LOW) {
    delay(20);
    if (digitalRead(pin) == LOW) {
      while (digitalRead(pin) == LOW);
      return true;
    }
  }
  return false;
}

void bufPlot(uint8_t (*buf)[screenW], int x, int y) {
  int mod = numModules - 1 - (y / screenW);
  buf[mod][x] |= (1 << (7 - (y % screenW)));
}

void flushBuf(uint8_t (*buf)[screenW]) {
  for (int m = 0; m < numModules; m++) {
    for (int r = 0; r < screenW; r++) {
      if (buf[m][r] != prevBuf[m][r]) {
        lc.setRow(m, r, buf[m][r]);
        prevBuf[m][r] = buf[m][r];
      }
    }
  }
}

uint8_t field[screenH];

unsigned long lastDrop     = 0;
unsigned long dropInterval = 500;
unsigned long lastMove     = 0;
const unsigned long moveInterval    = 200;
const unsigned long refreshInterval = 33;
unsigned long lastRefresh  = 0;

char heldType = 0;
bool holdUsedThisPiece = false;

struct Block {
  const int (*shape)[2];
  int len;
  int x, y;
  int rotation;
  char type;
} current;

const int I_SHAPE[2][4][2] = { {{0,0},{0,1},{0,2},{0,3}},       {{-1,1},{0,1},{1,1},{2,1}} };
const int O_SHAPE[1][4][2] = { {{0,0},{1,0},{0,1},{1,1}}                                   };
const int T_SHAPE[4][4][2] = { {{1,0},{0,1},{1,1},{2,1}},       {{1,0},{1,1},{1,2},{0,1}},
                               {{0,1},{1,1},{2,1},{1,2}},       {{1,0},{1,1},{1,2},{2,1}} };
const int L_SHAPE[4][4][2] = { {{0,0},{0,1},{0,2},{1,2}},       {{0,0},{1,0},{2,0},{0,1}},
                               {{0,0},{1,0},{1,1},{1,2}},       {{2,0},{0,1},{1,1},{2,1}} };
const int J_SHAPE[4][4][2] = { {{1,0},{1,1},{1,2},{0,2}},       {{0,0},{0,1},{1,1},{2,1}},
                               {{0,0},{1,0},{0,1},{0,2}},       {{0,0},{1,0},{2,0},{2,1}} };
const int S_SHAPE[2][4][2] = { {{1,0},{2,0},{0,1},{1,1}},       {{1,0},{1,1},{2,1},{2,2}} };
const int Z_SHAPE[2][4][2] = { {{0,0},{1,0},{1,1},{2,1}},       {{2,0},{1,1},{2,1},{1,2}} };

static const uint8_t PAT_G[8] = {0x3C,0x42,0x40,0x4E,0x42,0x42,0x3C,0x00};
static const uint8_t PAT_A[8] = {0x18,0x24,0x42,0x7E,0x42,0x42,0x42,0x00};
static const uint8_t PAT_M[8] = {0x42,0x66,0x5A,0x5A,0x42,0x42,0x42,0x00};
static const uint8_t PAT_E[8] = {0x7E,0x40,0x5C,0x40,0x40,0x40,0x7E,0x00};
static const uint8_t PAT_O[8] = {0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00};
static const uint8_t PAT_V[8] = {0x42,0x42,0x42,0x42,0x42,0x24,0x18,0x00};
static const uint8_t PAT_R[8] = {0x7C,0x42,0x42,0x7C,0x48,0x44,0x42,0x00};
static const uint8_t PAT_T[8] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00};
static const uint8_t PAT_I[8] = {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00};
static const uint8_t PAT_S[8] = {0x3E,0x40,0x40,0x3C,0x02,0x02,0x7C,0x00};

const uint8_t* letterPattern(char c) {
  switch (c) {
    case 'G': return PAT_G;
    case 'A': return PAT_A;
    case 'M': return PAT_M;
    case 'E': return PAT_E;
    case 'O': return PAT_O;
    case 'V': return PAT_V;
    case 'R': return PAT_R;
    case 'T': return PAT_T;
    case 'I': return PAT_I;
    case 'S': return PAT_S;
    default:  return PAT_E;
  }
}

void drawRotatedLetter(char c, int module) {
  const uint8_t* pat = letterPattern(c);
  uint8_t rot[8] = {};
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      if (pat[y] & (1 << x)) {
        int nx = 7 - y;
        int ny = x;
        rot[ny] |= (1 << nx);
      }
    }
  }
  for (int row = 0; row < 8; row++) lc.setRow(module, row, rot[row]);
}

void gameOverSequence() {
  stopMusic();
  for (int i = 0; i < 3; i++) {
    clearAll();
    delay(500);
    for (int m = 0; m < numModules; m++)
      for (int r = 0; r < screenW; r++)
        lc.setRow(m, r, 0xFF);
    delay(500);
  }

  const char* w1 = "GAME";
  for (int seg = 0; seg < 4; seg++) drawRotatedLetter(w1[seg], numModules - 1 - seg);
  delay(1000);
  const char* w2 = "OVER";
  for (int seg = 0; seg < 4; seg++) drawRotatedLetter(w2[seg], numModules - 1 - seg);
  delay(1000);

  while (digitalRead(SW) != LOW) delay(10);
  while (digitalRead(SW) == LOW) delay(10);
}

void startupSequence() {
  clearAll();
  for (int y = screenH - 1; y >= 0; y--) {
    int mod = numModules - 1 - (y / screenW);
    int rowInModule = y % screenW;
    lc.setRow(mod, rowInModule, 0xFF);
    delay(20);
  }
  delay(200);

  beep(523, 100); delay(100);
  beep(659, 100); delay(100);
  beep(784, 250); delay(100);

  clearAll();
  delay(150);

  const char* w1 = "TETR";
  for (int seg = 0; seg < 4; seg++) drawRotatedLetter(w1[seg], numModules - 1 - seg);
  delay(700);
  clearAll();
  delay(150);

  const char* w2 = "IS";
  for (int seg = 0; seg < 2; seg++) drawRotatedLetter(w2[seg], numModules - 1 - seg);
  delay(700);
  clearAll();
  delay(200);
}

void spawnType(char t) {
  int sx = screenW / 2 - 2;
  current.rotation = 0;
  switch (t) {
    case 'I': current = {I_SHAPE[0],4,sx,0,0,'I'}; break;
    case 'O': current = {O_SHAPE[0],4,sx,0,0,'O'}; break;
    case 'T': current = {T_SHAPE[0],4,sx,0,0,'T'}; break;
    case 'L': current = {L_SHAPE[0],4,sx,0,0,'L'}; break;
    case 'J': current = {J_SHAPE[0],4,sx,0,0,'J'}; break;
    case 'S': current = {S_SHAPE[0],4,sx,0,0,'S'}; break;
    case 'Z': current = {Z_SHAPE[0],4,sx,0,0,'Z'}; break;
  }
}

void spawnBlock() {
  const char types[7] = {'I','O','T','L','J','S','Z'};
  spawnType(types[random(7)]);
}

void resetGame() {
  memset(field, 0, sizeof(field));
  clearScreen();
  heldType = 0;
  holdUsedThisPiece = false;
  spawnBlock();
  lastDrop    = millis();
  lastRefresh = millis();
}

void writeBuffer() {
  uint8_t buf[numModules][screenW] = {};

  for (int y = 0; y < screenH; y++) {
    uint8_t row = field[y];
    if (!row) continue;
    int mod = numModules - 1 - (y / screenW);
    int bit = 1 << (7 - (y % screenW));
    for (int x = 0; x < screenW; x++) {
      if (row & (1 << x)) buf[mod][x] |= bit;
    }
  }

  for (int i = 0; i < current.len; i++) {
    int xx = current.x + current.shape[i][0];
    int yy = current.y + current.shape[i][1];
    if (xx < 0 || xx >= screenW || yy < 0 || yy >= screenH) continue;
    int mod = numModules - 1 - (yy / screenW);
    int bit = 1 << (7 - (yy % screenW));
    buf[mod][xx] |= bit;
  }

  flushBuf(buf);
}

bool checkCollision(int nx, int ny) {
  for (int i = 0; i < current.len; i++) {
    int xx = nx + current.shape[i][0];
    int yy = ny + current.shape[i][1];
    if (xx < 0 || xx >= screenW || yy >= screenH) return true;
    if (yy >= 0 && (field[yy] & (1 << xx))) return true;
  }
  return false;
}

void placeBlock() {
  for (int i = 0; i < current.len; i++) {
    int xx = current.x + current.shape[i][0];
    int yy = current.y + current.shape[i][1];
    if (yy >= 0 && yy < screenH) field[yy] |= (1 << xx);
  }
  for (int y = 0; y < screenH; y++) {
    if (field[y] == 0xFF) {
      for (int j = y; j > 0; j--) field[j] = field[j - 1];
      field[0] = 0;
    }
  }
}

void rotateBlock() {
  int limit = (current.type=='I'||current.type=='S'||current.type=='Z') ? 2
            : (current.type=='O' ? 1 : 4);
  int nr = (current.rotation + 1) % limit;
  const int (*ns)[2] = nullptr;
  if      (current.type=='I') ns = I_SHAPE[nr];
  else if (current.type=='O') ns = O_SHAPE[0];
  else if (current.type=='T') ns = T_SHAPE[nr];
  else if (current.type=='L') ns = L_SHAPE[nr];
  else if (current.type=='J') ns = J_SHAPE[nr];
  else if (current.type=='S') ns = S_SHAPE[nr];
  else if (current.type=='Z') ns = Z_SHAPE[nr];

  Block bak = current;
  current.shape    = ns;
  current.rotation = nr;
  if (checkCollision(current.x, current.y)) current = bak;
}

void lockAndSpawnNext() {
  bool hitTop = false;
  for (int i = 0; i < current.len; i++) {
    if (current.y + current.shape[i][1] == 0) { hitTop = true; break; }
  }
  if (hitTop) {
    gameOverSequence();
    resetGame();
    return;
  }
  placeBlock();
  spawnBlock();
  holdUsedThisPiece = false;
}

void tetrisLoop(unsigned long now) {
  if (readButtonPin(btnB)) rotateBlock();

  if (readButtonPin(SW) && !holdUsedThisPiece) {
    if (heldType == 0) {
      heldType = current.type;
      spawnBlock();
    } else {
      char temp = current.type;
      spawnType(heldType);
      heldType = temp;
    }
    holdUsedThisPiece = true;
  }

  if (readButtonPin(btnA)) {
    while (!checkCollision(current.x, current.y + 1)) current.y++;
    lockAndSpawnNext();
    lastDrop = now;
  }

  int ax = analogRead(VRx);
  if (now - lastMove > moveInterval) {
    if (ax < 400 && !checkCollision(current.x + 1, current.y)) {
      current.x++; lastMove = now;
    } else if (ax > 600 && !checkCollision(current.x - 1, current.y)) {
      current.x--; lastMove = now;
    }
  }

  int ay = 1023 - analogRead(VRy);
  dropInterval = 700 - constrain(map(ay, 512, 1023, 0, 690), 0, 690);

  if (now - lastDrop > dropInterval) {
    lastDrop = now;
    if (!checkCollision(current.x, current.y + 1)) current.y++;
    else lockAndSpawnNext();
  }

  if (now - lastRefresh >= refreshInterval) {
    writeBuffer();
    lastRefresh = now;
  }
}

// SNAKE
const int snakeW   = screenW;
const int snakeH   = screenH;
const int snakeMAX = screenW * screenH;

const unsigned int SNAKE_BOOST_INTERVAL = 80;   

enum { S_UP = 0, S_RIGHT = 1, S_DOWN = 2, S_LEFT = 3 };

uint8_t snakeBody[snakeMAX];   // cell index = y * 8 + x, [0] = head
int     snakeLen;
byte    snakeDir, snakeNextDir;
uint8_t snakeApple;
bool    snakeAlive, snakeAppleOn;
unsigned long snakeLastMove, snakeLastBlink;
unsigned int  snakeInterval;

void snakePlaceApple() {
  while (true) {
    uint8_t c = random(snakeW * snakeH);
    bool onSnake = false;
    for (int i = 0; i < snakeLen; i++)
      if (snakeBody[i] == c) { onSnake = true; break; }
    if (!onSnake) { snakeApple = c; return; }
  }
}

void snakeRender() {
  uint8_t buf[numModules][screenW] = {};
  for (int i = 0; i < snakeLen; i++)
    bufPlot(buf, snakeBody[i] & 7, snakeBody[i] >> 3);
  if (snakeAppleOn) bufPlot(buf, snakeApple & 7, snakeApple >> 3);
  flushBuf(buf);
}

void snakeSetup() {
  clearScreen();
  int cy = snakeH / 2;
  snakeLen = 3;
  snakeBody[0] = cy * 8 + 3;
  snakeBody[1] = (cy - 1) * 8 + 3;
  snakeBody[2] = (cy - 2) * 8 + 3;
  snakeDir = snakeNextDir = S_DOWN;
  snakeInterval = 300;
  snakeAlive = true;
  snakeAppleOn = true;
  snakeLastMove = snakeLastBlink = millis();
  randomSeed(analogRead(A2) + micros());
  snakePlaceApple();
  snakeRender();
}

void snakeReadJoystick() {
  int dx = 512 - analogRead(VRx);
  int dy = 512 - analogRead(VRy);
  if (abs(dx) < 120 && abs(dy) < 120) return;

  byte want;
  if (abs(dx) > abs(dy)) want = (dx > 0) ? S_RIGHT : S_LEFT;
  else                   want = (dy > 0) ? S_DOWN  : S_UP;

  if ((want + 2) % 4 != snakeDir) snakeNextDir = want;  
}

void snakeGameOver() {
  snakeAlive = false;
  snakeAppleOn = true;
  stopMusic();
  for (int i = 0; i < 3; i++) {
    clearScreen();
    beep(300 - i * 70, 150);
    delay(100);
    snakeRender();
    delay(200);
  }
  beep(130, 400);
  delay(100);
  snakeRender();
}

void snakeStep() {
  snakeDir = snakeNextDir;
  int hx = snakeBody[0] & 7;
  int hy = snakeBody[0] >> 3;
  if      (snakeDir == S_UP)    hy--;
  else if (snakeDir == S_DOWN)  hy++;
  else if (snakeDir == S_LEFT)  hx--;
  else                          hx++;

  if (hx < 0 || hx >= snakeW || hy < 0 || hy >= snakeH) { snakeGameOver(); return; }

  uint8_t np = hy * 8 + hx;
  bool eat = (np == snakeApple);

  int limit = eat ? snakeLen : snakeLen - 1;
  for (int i = 0; i < limit; i++)
    if (snakeBody[i] == np) { snakeGameOver(); return; }

  if (eat) snakeLen++;
  for (int i = snakeLen - 1; i > 0; i--) snakeBody[i] = snakeBody[i - 1];
  snakeBody[0] = np;

  if (eat) {
    beep(1200, 40);
    if (snakeInterval > 90) snakeInterval -= 8;
    if (snakeLen >= snakeW * snakeH) {        // board full = win
      for (int i = 0; i < 4; i++) { beep(800 + i * 300, 100); delay(80); }
      snakeSetup();
      return;
    }
    snakePlaceApple();
  }
  snakeRender();
}

void snakeLoop(unsigned long now) {
  if (!snakeAlive) {
    if (digitalRead(SW) == LOW || digitalRead(btnA) == LOW) {
      delay(200);
      snakeSetup();
    }
    return;
  }

  snakeReadJoystick();

  if (now - snakeLastBlink >= 120) {
    snakeLastBlink = now;
    snakeAppleOn = !snakeAppleOn;
    snakeRender();
  }

  bool boosting = (digitalRead(btnB) == LOW);
  unsigned int effectiveInterval = boosting ? min(snakeInterval, SNAKE_BOOST_INTERVAL)
                                             : snakeInterval;

  if (now - snakeLastMove >= effectiveInterval) {
    snakeLastMove = now;
    snakeStep();
  }
}

void startGame() {
  paused = false;
  stopMusic();
  if (currentGame == GAME_TETRIS) resetGame();
  else                            snakeSetup();
}

void switchGame() {
  currentGame = (currentGame + 1) % NUM_GAMES;
  stopMusic();
  beep(1000, 80); delay(60);
  beep(1500, 80); delay(60);
  startGame();
}

void setup() {
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, HIGH);   

  pinMode(SW, INPUT_PULLUP);
  pinMode(btnA, INPUT_PULLUP);
  pinMode(btnB, INPUT_PULLUP);
  pinMode(btnC, INPUT_PULLUP);
  pinMode(btnD, INPUT_PULLUP);
  pinMode(btnE, INPUT_PULLUP);
  pinMode(btnF, INPUT_PULLUP);

  randomSeed(analogRead(A2));

  for (int m = 0; m < numModules; m++) {
    lc.shutdown(m, false);
    lc.setIntensity(m, BRIGHTNESS);
    lc.clearDisplay(m);
    for (int r = 0; r < screenW; r++) prevBuf[m][r] = 0;
  }
  startupSequence();
  startGame();
}

void loop() {
  unsigned long now = millis();
  if (readButtonPin(btnF)) toggleSound();
  if (readButtonPin(btnE)) { switchGame(); return; }
  if (readButtonPin(btnC)) { startGame(); return; }
  if (readButtonPin(btnD)) {
    paused = !paused;
    if (paused) {
      stopMusic();
    } else {
      unsigned long t = millis();
      lastDrop = lastMove = lastRefresh = t;
      snakeLastMove = snakeLastBlink = t;
    }
  }
  if (paused) return;
  if (currentGame == GAME_TETRIS || snakeAlive) updateMusic(now);
  if (currentGame == GAME_TETRIS) tetrisLoop(now);
  else                            snakeLoop(now);
}
