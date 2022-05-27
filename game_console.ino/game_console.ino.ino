#include <LedControl.h>
#include <LiquidCrystal_I2C.h>

#define left 2
#define up 3
#define down 4
#define right 5

#define buzzer 9

#define MDIN 11
#define MCS 12
#define MCLK 13

LiquidCrystal_I2C lcd(0x27, 20, 4);
LedControl lc = LedControl(MDIN, MCLK, MCS, 2);

enum {
  MENU,
  SIMON,
  HOLE,
  INVALID
} states;

void setupButtons() {
  pinMode(left, INPUT);
  digitalWrite(left, HIGH);
  pinMode(up, INPUT);
  digitalWrite(up, HIGH);
  pinMode(down, INPUT);
  digitalWrite(down, HIGH);
  pinMode(right, INPUT);
  digitalWrite(right, HIGH);

  *digitalPinToPCMSK(left) |= bit (digitalPinToPCMSKbit(left));

  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT19);
  PCMSK2 |= (1 << PCINT20);
  PCMSK2 |= (1 << PCINT21);
  PCMSK2 |= (1 << PCINT22);
  sei();
}

void setupLedMatrix() {
  lc.shutdown(0, false);
  lc.shutdown(1, false);
  lc.setIntensity(0, 0);
  lc.setIntensity(1, 0);
  lc.clearDisplay(0);
  lc.clearDisplay(1);
}

int state;

void setup() {

  setupButtons();

  setupLedMatrix();
  pinMode(buzzer, OUTPUT);

  lcd.init();
  lcd.backlight();
  lcd.clear();

  state = MENU;
  randomSeed(analogRead(0));
}

long pressDiff = 200;
long lastPress = 0;
bool pleft = false, pright = false, pup = false, pdown = false;

ISR(PCINT2_vect) {
  if (millis() - lastPress < pressDiff)
    return;

  // cod întrerupere de tip pin change
  if ((PIND & (1 << PD2)) == 0) {
    pleft = true;
  } else if ((PIND & (1 << PD3)) == 0) {
    pup = true;
  } else if ((PIND & (1 << PD4)) == 0) {
    pdown = true;
  } else if ((PIND & (1 << PD5)) == 0) {
    pright = true;
  }

  lastPress = millis();
}

int cursorRow = 0;
int options = 2;
void menu() {
  if (pdown) {
    cursorRow = (++cursorRow > options - 1) ? options - 1 : cursorRow;
    pdown = false;
    lcd.clear();
  }
  else if (pup) {
    cursorRow = (--cursorRow < 0) ? 0 : cursorRow;
    pup = false;
    lcd.clear();
  } else if (pright) {
      state = cursorRow + 1;
      pright = false;
  }

  lcd.setCursor(1,0);
  lcd.print("Simon Says");
  lcd.setCursor(1,1);
  lcd.print("Hole In The Wall");

  lcd.setCursor(0, cursorRow);
  lcd.print('>');
}

unsigned char number3[4]={B00111100,B01100110,B00000110,B00011110};
unsigned char number2[4]={B00111100,B01111110,B00000110,B00111110};

void countdown(char dev) {
  for(int i = 0; i < 8; i++)
    lc.setRow(dev, i, number3[min(i, 8 - (i + 1))]);
  delay(1000);
  for(int i = 0; i < 8; i++) {
    if (i < 4)
      lc.setRow(dev, i, number2[i]);
    else
      lc.setRow(dev, i, mirror(number2[8 - (i + 1)]));
  }
  delay(1000);
  lc.clearDisplay(dev);
  lc.setColumn(dev,4,B11000000);
  lc.setColumn(dev,5,B11111111);
  lc.setColumn(dev,6,B11111111);
  delay(1000);
  lc.clearDisplay(dev);
}

unsigned char mirror(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}


void dispArrowVert(char dev, bool arrowUp) {
  lc.clearDisplay(dev);
  char sz = 8;
  char st = arrowUp ? 1 : 2;
  char stopCond = arrowUp ? (sz - (st + 1)) : (sz - st + 1);

  for (char i = st; i < stopCond; i++)
    for (char j = 0; j < sz; j++)
      if (arrowUp) {
        if (j + i >= sz / 2 && j - i < sz / 2)
          lc.setLed(dev, i, j, 1);
      } else {
        if (j + i <= sz + st && j - i > -(sz /2 ))
          lc.setLed(dev, i, j, 1);
      }
}


void dispArrowHor(char dev, bool arrowLeft) {
  lc.clearDisplay(dev);
  char sz = 8;
  char st = arrowLeft ? 1 : 2;
  char stopCond = arrowLeft ? (sz - (st + 1)) : (sz - st + 1);

  for (char i = 0; i < sz; i++)
    for (char j = st; j < stopCond; j++)
      if (arrowLeft) {
        if (j + i >= sz / 2 && j + sz / 2 > i)
          lc.setLed(dev, i, j, 1);
      } else {
        if (j <= i + st + 1 && j + i <= sz + st)
          lc.setLed(dev, i, j, 1);
      }
}

unsigned char checkmark[8] = {B01111100,B10000001,B10000010,B10000100,B10101001,B10010001,B10000001,B01111110};
unsigned char wrong[4] = {B11100111,B11100111,B01111110,B00111100};
void dispResult(char dev, bool correct) {
  for(char i = 0; i < 8; i++) {
    if (correct)
      lc.setRow(dev, i, checkmark[i]);
    else {
      lc.setRow(dev, i, wrong[min(i, 8 - (i + 1))]);
    }
  }
}

enum {
  NONE,
  UP,
  DOWN,
  LEFT,
  RIGHT
} posMoves;

void genMove(char lastMove, char *moves) {
  moves[lastMove] = random(0, 4);
}

void displayMoves(char count, char dev, char* moves) {
  for(char i = 0; i < count; i++) {
    char val = moves[i] - 1;
    if (val / 2 == 0) {
      dispArrowVert(dev, val % 2 == 0);
      delay(1000);
    } else {
      dispArrowHor(dev, val % 2 == 0);
      delay(1000);
    }
    lc.clearDisplay(dev);
    delay(100);
  }
}

char computeMove() {
  char count = 0;
  if (pup) count++;
  if (pdown) count++;
  if (pleft) count++;
  if (pright) count++;

  if (count > 1) {
    pup = pdown = pleft = pright = false;
    return -1;
  }
  else if (count == 0)
    return 0;

  if (pup) {pup = false; return UP;}
  if (pdown) {pdown = false; return DOWN;}
  if (pleft) {pleft = false; return LEFT;}
  if (pright) {pright = false; return RIGHT;}
}

bool waitForInput = false;
bool correct = true;
char moveToCheck = 0;
bool gameStarted = false;
char countGen = 0;
char moves[20] = {0};

int score = 0;

void clearSimon() {
  waitForInput = false;
  correct = true;
  moveToCheck = 0;
  gameStarted = false;
  countGen = 0;
  moves[20] = {0};
  score = 0;
}

void playEffectWrong() {
  tone(buzzer,500);
  delay(200);
  tone(buzzer,125);
  delay(300);
  noTone(buzzer);
  delay(1500);
}

void playEffectCorrect() {
  tone(buzzer,1000);
  delay(200);
  tone(buzzer,1500);
  delay(300);
  noTone(buzzer);
  delay(500);
}

void simonsays() {
  if (!gameStarted) {
    countdown(0);
    gameStarted = true;
  }

  if (!waitForInput) {
    if (countGen < 20) {
      genMove(countGen, moves);
      countGen++;
    }
    displayMoves(countGen, 0, moves);
    waitForInput = true;
  } else {
    char sel = computeMove();
    
    if (sel == -1) {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Only one move!");
    } else if (sel > 0) {
        lcd.clear();
        correct = (moves[moveToCheck] == sel);
        if (!correct) {
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Score: ");
          lcd.print(score);
          
          clearSimon();
          
          dispResult(0, false);
          playEffectWrong();
          
          lc.clearDisplay(0);
          state = MENU;
        } else {
          score++;
          moveToCheck++;
          if (moveToCheck >= countGen) {
            waitForInput = false;
            moveToCheck = 0;
            dispResult(0, true);
            
            playEffectCorrect();
            lc.clearDisplay(0);
          }
        }
    }
  }
}

unsigned char walls[16] = {0};
char lastGen = 15;
char playerPos = 4;
char ballCol = 0, ballRow = 0;
long genWallDelay = 3000;
long updateBallDelay = 250;
long lastBallUpdate = 0, lastWallGen = 0;
bool ballExists = false;

void resetWalls() {
  for(char i = 0; i < 16; i++)
    walls[i] = 0;
  lastGen = 15;
  playerPos = 4;
  ballCol = ballRow = 0;
  lastBallUpdate = lastWallGen = 0;
  ballExists = false;
  score = 0;
  gameStarted = false;
}

void moveWalls() {
  for(char i = 2; i < 16; i++) {
    walls[i - 1] = walls[i];
    if (i - 1 == ballCol)
      if (walls[ballCol] + (1 << ballRow) == 255) {
        walls[ballCol] = 0;
        lastGen++;
        score++;
        dispWalls();
        ballExists = false;
      }
  }
}

void genWall() {
  walls[15] = ~(1 << random(0,8));
}

void dispWalls() {
  for(char i = 1; i < 16; i++)
    lc.setColumn(i / 8, i % 8, walls[i]);
}

void dispPlayer() {
  lc.setColumn(0, 0, 1 << playerPos);
}

void setBall(bool show) {
  lc.setLed(ballCol / 8, 7 - ballRow, ballCol % 8, show);
}

void holeinthewall() {
  if (!gameStarted) {
    countdown(0);
    gameStarted = true;
  }
  dispPlayer();
  char sel = computeMove();

  if (sel > 0) {
    if (sel == UP)
      playerPos = min(playerPos + 1, 7);
    else if (sel == DOWN)
      playerPos = max(playerPos - 1, 0);
    else if (sel == RIGHT && !ballExists) {
      ballCol = 1;
      ballRow = playerPos;
      ballExists = true;
      setBall(true);
      lastBallUpdate = millis();
    }

    dispPlayer();
  }

  if (millis() - lastWallGen > max(genWallDelay - score * 10, updateBallDelay * 3)) {
    lastWallGen = millis();
    moveWalls();
    genWall();
    lastGen--;
    dispWalls();
    if (ballExists)
      setBall(true);

    if (lastGen == 1) {
       lcd.clear();
       lcd.setCursor(0,0);
       lcd.print("Score: ");
       lcd.print(score);
          
       resetWalls();
          
       dispResult(0, false);
       playEffectWrong();
          
       lc.clearDisplay(0);
       lc.clearDisplay(1);
       state = MENU;
       lcd.clear();
    }
  }else if (ballExists && millis() - lastBallUpdate > updateBallDelay) {
    lastBallUpdate = millis();
    setBall(false);
    ballCol++;

    if (walls[ballCol] != 0) {
      if (walls[ballCol] + (1 << ballRow) == 255) {
        walls[ballCol] = 0;
        lastGen++;
        score++;
        dispWalls();
        ballExists = false;
      } else {
        ballExists = false;
      }
    } else {
      setBall(true);
    }
  }
}


void loop() {
  if (state == MENU)
    menu();
  else if (state == SIMON) {
    if (!gameStarted)
      lcd.clear();
    simonsays();
  } else if (state == HOLE) {
    if (!gameStarted)
      lcd.clear();
    holeinthewall();
  }
}
