#include <Ticker.h>
#include <WEMOS_Matrix_LED.h>
//#include "Timer.h"
#include "types.h"
#include "digits.h"
#include "letters.h"

// this pin should be connected to push button with a pull-down resistor
#define BUTTON_PIN D3
#define ON_BOARD_LED_PIN 4


MLED mled(5); //setting intensity

// gravity
const float kG = 0.005;

// button gives this much lift
const float kLift = -0.05;

Game gGame;

// this bird is stuck on col 1 & 2
const byte kXpos = 1;

// the global timer
//Ticker gTimer;

//Declaring this here so I can make the timer events within scope
void updateBirdPosition();
void moveWallOne();
void moveWallTwo();
void reactToUserInput();
void startWallOne();
void startWallTwo();

// timer events
Ticker gTimer(reactToUserInput, 30, 0, MILLIS);
Ticker gUpdateEvent(updateBirdPosition, 50, 0, MILLIS);
Ticker gMoveWallOneEvent(moveWallOne, 200, 0, MILLIS);
Ticker gMoveWallTwoEvent(moveWallTwo, 200, 0, MILLIS);
Ticker gStartWallOneEvent(startWallOne, 2500, 1, MILLIS);
Ticker gStartWallTwoEvent(startWallTwo, 3500, 1, MILLIS);


void setup()
{
  Serial.begin(74880);
  Serial.println("starting");
  pinMode (BUTTON_PIN, INPUT_PULLUP);
//  randomSeed(analogRead(0));
  gGame.state = STOPPED_GAME;
  gTimer.start();
  startGame(true);
  
}

void startGame(boolean doit)
{
  if (doit) {
    gGame.score = 0;
    gGame.state = STARTED_GAME;
    gGame.birdY = 0.5;
    gGame.wallOne.xpos = 7;
    gGame.wallOne.bricks = generateWall();
    gGame.wallTwo.xpos = 7;
    gGame.wallTwo.bricks = generateWall();

    gUpdateEvent.start();
    //gUpdateEvent = gTimer.every(50, updateBirdPosition);
    gStartWallOneEvent.start();
    gStartWallTwoEvent.start();
    Serial.println("did it");
  }
  else {
    gGame.state = STOPPED_GAME;
    gUpdateEvent.stop();
    gMoveWallOneEvent.stop();
    gMoveWallTwoEvent.stop();
  }
}

void startWallOne()
{
  gMoveWallOneEvent.start();
  //gMoveWallOneEvent = gTimer.every(200, moveWallOne);
}

void startWallTwo()
{
  gMoveWallTwoEvent.start();
  //gMoveWallTwoEvent = gTimer.every(200, moveWallTwo);
}

void moveWallOne()
{
  moveWall(&gGame.wallOne);
}

void moveWallTwo()
{
  moveWall(&gGame.wallTwo);
}

void moveWall(Wall *wall)
{
  if (wall->xpos == 255) {
    // wall has come past screen
    eraseWall(wall, 0);
    wall->bricks = generateWall();
    wall->xpos = 7;
  }
  else if (wall->xpos < 7) {
    eraseWall(wall, wall->xpos + 1);
  }

  drawWall(wall, wall->xpos);

  // check if the wall just slammed into the bird.
  if (wall->xpos == 2) {
    byte ypos = 7 * gGame.birdY;
    if (wall->bricks & (0x80 >> ypos)) {
      // Boom! Game over.
      explode();
      gameOver();
    }
    else {
      // no collision: score!
      gGame.score++;
    }
  }
  wall->xpos = wall->xpos - 1;
}

void drawWall(Wall *wall, byte x)
{
  for (byte row = 0; row < 8; row++) {
    if (wall->bricks & (0x80 >> row)) {
      mled.dot(row, x);
      //gMatrix.setLed(0, row, x, HIGH);
    }
  }
  mled.display();
}

void eraseWall(Wall *wall, byte x)
{
  for (byte row = 0; row < 8; row++) {
    if (wall->bricks & (0x80 >> row)) {
      mled.dot(row, x, 0);
      //gMatrix.setLed(0, row, x, LOW);
    }
  }
  mled.display();

}

byte generateWall()
{
  // size of the hole in the wall
  byte gap = random(4, 7);

  // the hole expressed as bits
  byte punch = (1 << gap) - 1;

  // the hole's offset
  byte slide = random(1, 8 - gap);

  // the wall without the hole
  return 0xff & ~(punch << slide);
}

void reactToUserInput()
{
  Serial.println("reacting");
  static boolean old = false;
  //static unsigned long lastMillis = 0;

  boolean buttonPressed = digitalRead(BUTTON_PIN);
  buttonPressed = !buttonPressed;
  if (buttonPressed) {
    if (gGame.state == STARTED_GAME) {
      //unsigned long dt = millis() - lastMillis;
      if (!old) {
        // button was not pressed last time we checked
        if (gGame.vy > 0) {
          // initial bounce
          gGame.vy = kLift;
        }
        else {
          // keep adding lift
          gGame.vy += kLift;
        }
      }
    }
    else {
      // game is not playing. start it.
      transition();
      startGame(true);
    }
  }
  old = buttonPressed;
//  digitalWrite(ON_BOARD_LED_PIN, buttonPressed);
}

void updateBirdPosition()
{
//  Serial.println("updating bird");
  // initial position (simulated screen size 0..1)
  static float y = 0.5;

  // apply gravity
  gGame.vy = gGame.vy + kG;

  float oldY = gGame.birdY;

  // calculate new y position
  gGame.birdY = gGame.birdY + gGame.vy;

  // peg y to top or bottom
  if (gGame.birdY > 1) {
    gGame.birdY = 1;
    gGame.vy = 0;
  }
  else if (gGame.birdY < 0) {
    gGame.birdY = 0;
    gGame.vy = 0;
  }

  // convert to screen position
  byte ypos = 7 * gGame.birdY;

  Direction direction;
  if (abs(oldY - gGame.birdY) < 0.01) {
    direction = STRAIGHT;
  }
  else if (oldY < gGame.birdY) {
    direction = UP;
  }
  else {
    direction = DOWN;
  }

  drawBird(direction, ypos);
}

void drawBird(Direction direction, byte yHead)
{
  // previous position of tail and head (one pixel each)
  static byte cTail, cHead;
  byte yTail;
  yTail = constrain(yHead - direction, 0, 7);

  // erase it from old position
  mled.dot(cHead, kXpos, 0);
  //gMatrix.setLed(0, cTail, kXpos, LOW);
  mled.dot(cHead, kXpos + 1, 0);
  //gMatrix.setLed(0, cHead, kXpos + 1, LOW);

  // draw it in new position
  mled.dot(yTail, kXpos);
  //gMatrix.setLed(0, yTail, kXpos, HIGH);
  mled.dot(yHead, kXpos + 1);
  //gMatrix.setLed(0, yHead, kXpos + 1, HIGH);

  // remember current position
  cTail = yTail;
  cHead = yHead;
  mled.display();
}

void explode()
{
  for (int i = 0; i < 15; ++i) {
    allOn(true);
    delay(20);
    allOn(false);
    delay(20);
  }
}

void gameOver()
{
  showScore(gGame.score);
  startGame(false);
}

void allOn(boolean on)
{
  byte val = on ? 0xff : 0;
  for (byte n = 0; n < 8; ++n) {
    mled.dot(n, val);
    //gMatrix.setRow(0, n, val);
  }
  mled.display();
}

void showScore(byte value)
{
  if (value > 99) {
    error();
    return ;
  }
  byte tens = value / 10;
  byte ones = value % 10;
  for (int row = 0; row < 8; row++){
    byte composite = kDigits[tens][row] | (kDigits[ones][row] >> 4);
    updateFrameRow(row, composite);
  }
}

void loop()
{
  gTimer.update();
  gUpdateEvent.update();
  gMoveWallOneEvent.update();
  gMoveWallTwoEvent.update();
  gStartWallOneEvent.update();
  gStartWallTwoEvent.update();
}

// drag out the framebuffer sideways
void transition()
{
  for (int step = 0; step < 8; step++){
    for (int row = 0; row < 8; row++){
      byte pixels = gGame.framebuffer[row];
      if (row % 2) {
        pixels = pixels >> 1;
      }
      else {
        pixels = pixels << 1;
      }
      updateFrameRow(row, pixels);
    }
    delay(50);
  }
}

// making this function to draw whole rows
void setRow(byte row, byte pixels) {
  for (int i = 0; i < 8; i++) {
    byte output = (pixels << i) & 0x80;
    if (output != 0) {
      mled.dot(row, i);
    }
    else {
      mled.dot(row, i, 0);
    }
  }
  mled.display();
}

// this function advances the rows?????
void updateFrameRow(byte row, byte pixels)
{
  setRow(row, pixels);
  gGame.framebuffer[row] = pixels;
}

// show the letter E to signal error condition
void error()
{
for(int row=0;row<8;row++){
    mled.dot(row, kLetters[0][row]);
    mled.display();
  }
}
