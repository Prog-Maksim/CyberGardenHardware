#include <Arduino.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <VirtualWire.h>

#define JOY_PIN_X A3
#define JOY_PIN_Y A2
#define JOY_SW_PIN 10


LiquidCrystal_I2C lcd(0x27,16,2);

// Названия вкладок главного меню
#define MENU_COUNT 4
const String menus[] = {"Setup", "Automatic", "Info", "Manual"};

enum menu {
  Main,
  Setup,
  Automatic,
  Info,
  Manual,
  RgbEdit
};
menu currentMenu = Main;
bool needRefresh = true;

byte selection = 0;

byte topMenu = 0;         
bool joyMovedUp = false;
bool joyMovedDown = false;

// Меню с параметрами датчиков (пока константы)
#define INFO_COUNT 3
const String infoParams[] = {"Temp: 24C", "Humidity: 60%", "Water: 75%"};
const byte infoCount = 3;

byte currentInfo = 0;
byte topInfo = 0;

// Параметры и предустановки
#define AUTO_PARAM_COUNT 3
const String autoParams[] = {"Water", "Sensor", "Other"};


#define MAX_PRESETS 3
byte presetValues[MAX_PRESETS][AUTO_PARAM_COUNT] = {
  {5, 10, 15},   // Preset 1
  {3, 7, 20},    // Preset 2
  {0, 0, 0}      // Preset 3 (пустой)
};
byte activePreset = 0; // выбранная предустановка

// Setup меню
byte currentPreset = 0;
byte topPreset = 0;
bool inPresetMenu = false;  

// Automatic параметры
byte currentParam = 0;     
byte topParam = 0;         
bool editingParam = false; 
String editBuffer = "";

// Клавиатура
const byte ROWS = 4; 
const byte COLS = 4; 

const char hexaKeys[ROWS][COLS] = {
  {'1','2','3','('},
  {'4','5','6',')'},
  {'7','8','9','-'},
  {'A','0','B','+'}
};

const byte rowPins[ROWS] = {6, 7, 8, 9}; 
const byte colPins[COLS] = {5, 4, 3, 2}; 
Keypad customKeypad = Keypad(makeKeymap(hexaKeys), colPins, rowPins, COLS, ROWS);

// Manual меню
#define MANUAL_COUNT 3
const String manualParams[] = {"Light", "Color", "Watering"};

// Состояние теплицы
bool lightOn = false;
bool waterOn = false;
byte R = 128, G = 128, B = 128;

byte currentManual = 0;
byte topManual = 0;
String editBufferManual = "";

byte rgbCursor = 0; // 0 = R, 1 = G, 2 = B
byte editValue = 0;

char buffer[10];
int idx=0;

bool lastSwState;

unsigned short joyX;
unsigned short joyY;
bool swState;

void setup()
{
  Serial.begin(9600);
  pinMode(JOY_PIN_X, INPUT);
  pinMode(JOY_PIN_Y, INPUT);
  pinMode(JOY_SW_PIN, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  drawMenu();
}

void loop()
{
  swState = !digitalRead(JOY_SW_PIN); 
  unsigned short X = analogRead(JOY_PIN_X);
  unsigned short Y = analogRead(JOY_PIN_Y);
  
  joyX = analogRead(JOY_PIN_X);
  joyY = analogRead(JOY_PIN_Y);

  switch (currentMenu) {
    case Main:
      handleNavigationMenu(X);
      break;
    case Setup: // Setup
      handleSetupMenu(X, Y);
      break;
    case Automatic: // Automatic
      handleAutomaticMenu(X, Y);
      break;
    case Info: // Info
      handleInfoMenu(X, Y);
      break;
    case Manual: // Manual
      handleManualMenu(X, Y); 
      break; 
    case RgbEdit:
      drawManualEditRGB();
      break;
  }

  
  delay(50);
  lastSwState = swState;
}

void send (char *message)
{
  digitalWrite(LED_BUILTIN,HIGH);
  Serial.print("Transmited: ");
  Serial.println(message);
  vw_send((uint8_t *)message, strlen(message));
  vw_wait_tx(); // Wait until the whole message is gone
  digitalWrite(LED_BUILTIN, LOW);
}

// Generic подменю (Info, Manual)
void handleGenericSubMenu(unsigned short X, unsigned short Y) {
  if (Y < 100) {
    drawMenu();
    delay(300);
  }
}

// Главное меню
void handleNavigationMenu(unsigned short X)
{
  if (X <= 200 && !joyMovedDown) {
    if (selection < MENU_COUNT - 1) {
      selection++;
      if (selection >= topMenu + 2) 
        topMenu++;
      needRefresh = true;
    }
    joyMovedDown = true;
  } else if (X > 200 && joyMovedDown) {
    joyMovedDown = false;
  }

  if (X >= 800 && !joyMovedUp) {
    if (selection > 0) {
      selection--;
      if (selection < topMenu) topMenu--;
      needRefresh = true;
    }
    joyMovedUp = true;
  } else if (X < 800 && joyMovedUp) {
    joyMovedUp = false;
  } 

  if (needRefresh) {
    needRefresh = false;
    drawMenu();
  }

  if (swState && swState != lastSwState) {
    switch (selection) {
      case 0: 
        currentMenu = Setup;
        break;
      case 1: 
        currentMenu = Automatic;
        break;
      case 2: 
        currentMenu = Info;
        break;
      case 3: 
        currentMenu = Manual;
        needRefresh = true;
        break;
    }
  }
}

void drawMenu()
{
  lcd.clear();
  for (byte i = 0; i < 2; ++i) {
    byte menuIndex = topMenu + i;

    if (menuIndex < MENU_COUNT) {
      lcd.setCursor(0, i);
      lcd.print((menuIndex == selection ? "- " : "  ") + menus[menuIndex]);
    }
  }
}

// Setup меню
void drawSetupMenu() {
  lcd.clear();
  for (byte i = 0; i < 2; ++i) {
    byte presetIndex = topPreset + i;

    if (presetIndex < MAX_PRESETS) {
      lcd.setCursor(0, i);
      String name = "Preset " + String(presetIndex+1);

      if (presetIndex == activePreset)
        name += "*";

      lcd.print((presetIndex == currentPreset ? "- " : "  ") + name);
    }
  }
}

void handleSetupMenu(unsigned short X, unsigned short Y) {
  if (!inPresetMenu) {
    // Навигация по списку предустановок
    if (X < 100 && !joyMovedDown) {
      if (currentPreset < MAX_PRESETS - 1) {
        currentPreset++;

        if (currentPreset >= topPreset + 2) 
          topPreset++;

        drawSetupMenu();
      }

      joyMovedDown = true;
    } else if (X > 200 && joyMovedDown) {
      joyMovedDown = false;
    }

    if (X > 900 && !joyMovedUp) {
      if (currentPreset > 0) {
        currentPreset--;
        if (currentPreset < topPreset) topPreset--;
        drawSetupMenu();
      }
      joyMovedUp = true;
    } else if (X < 800 && joyMovedUp) {
      joyMovedUp = false;
    }

    // Нажатие джойстика = выбор активной предустановки
    if (swState) {
      activePreset = currentPreset; 
      drawSetupMenu();
      delay(300);
    }

    // Свайп вправо = вход в параметры выбранного пресета
    if (Y > 900) {
      inPresetMenu = true;
      currentParam = 0;
      topParam = 0;
      editingParam = false;
      drawPresetMenu();
      delay(300);
    } else if (Y < 100) {
      drawMenu();
      delay(300);
    }
  }
  else {
    handlePresetParams(X, Y);
  }
}


void drawPresetMenu() {
  lcd.clear();
  for (byte i = 0; i < 2; i++) {
    byte paramIndex = topParam + i;

    if (paramIndex < AUTO_PARAM_COUNT) {
      lcd.setCursor(0, i);

      if (paramIndex == currentParam) {
        lcd.print("- " + autoParams[paramIndex] + ": " + String(presetValues[currentPreset][paramIndex]) + " m");
      } else {
        lcd.print("  " + autoParams[paramIndex] + ": " + String(presetValues[currentPreset][paramIndex]) + " m");
      }
    }
  }
}

void handlePresetParams(unsigned short X, unsigned short Y) {
  char key = customKeypad.getKey();

  if (!editingParam) {
    if (X < 100 && !joyMovedDown) {
      if (currentParam < AUTO_PARAM_COUNT - 1) {
        currentParam++;
        if (currentParam >= topParam + 2) topParam++;
        drawPresetMenu();
      }
      joyMovedDown = true;
    } else if (X > 200 && joyMovedDown) {
      joyMovedDown = false;
    }

    if (X > 900 && !joyMovedUp) {
      if (currentParam > 0) {
        currentParam--;
        if (currentParam < topParam) topParam--;
        drawPresetMenu();
      }
      joyMovedUp = true;
    } else if (X < 800 && joyMovedUp) {
      joyMovedUp = false;
    }

    if (swState) {
      editingParam = true;
      editBuffer = "";
      drawEditScreen();
      delay(300);
    }

    if (Y < 100) {
      inPresetMenu = false;
      drawSetupMenu();
      delay(300);
    }
  } else {
    if (key) {
      if (key >= '0' && key <= '9') {
        editBuffer += key;
        drawEditScreen();
      } else if (key == 'A' && editBuffer.length() > 0) { 
        editBuffer.remove(editBuffer.length() - 1);
        drawEditScreen();
      }
    }

    if (swState && editBuffer.length() > 0) {
      presetValues[currentPreset][currentParam] = editBuffer.toInt();
      editingParam = false;
      drawPresetMenu();
      delay(300);
    }

    if (Y < 100) {
      editingParam = false;
      drawPresetMenu();
      delay(300);
    }
  }
}

// Automatic меню
void handleAutomaticMenu(unsigned short X, unsigned short Y) {
  char key = customKeypad.getKey();

  if (!editingParam) {
    if (X < 100 && !joyMovedDown) {
      if (currentParam < AUTO_PARAM_COUNT - 1) {
        currentParam++;

        if (currentParam >= topParam + 2) 
          topParam++;

        drawAutomaticMenu();
      }
      joyMovedDown = true;
    } else if (X > 200 && joyMovedDown) {
      joyMovedDown = false;
    }

    if (X > 900 && !joyMovedUp) {
      if (currentParam > 0) {
        currentParam--;

        if (currentParam < topParam) 
          topParam--;

        drawAutomaticMenu();
      }
      joyMovedUp = true;
    } else if (X < 800 && joyMovedUp) {
      joyMovedUp = false;
    }

    if (swState) {
      editingParam = true;
      editBuffer = "";
      drawEditScreen();
      delay(300);
    }

    if (Y < 100) {
      drawMenu();
      delay(300);
    }
  } else {
    if (key) {
      if (key >= '0' && key <= '9') {
        editBuffer += key;
        drawEditScreen();
      } else if (key == 'A' && editBuffer.length() > 0) { 
        editBuffer.remove(editBuffer.length() - 1);
        drawEditScreen();
      }
    }

    if (swState && editBuffer.length() > 0) {
      presetValues[activePreset][currentParam] = editBuffer.toInt();
      editingParam = false;
      drawAutomaticMenu();
      delay(300);
    }

    if (Y < 100) {
      editingParam = false;
      drawAutomaticMenu();
      delay(300);
    }
  }
}

void drawAutomaticMenu() {
  lcd.clear();
  for (byte i = 0; i < 2; i++) {
    byte paramIndex = topParam + i;

    if (paramIndex < AUTO_PARAM_COUNT) {
      lcd.setCursor(0, i);

      if (paramIndex == currentParam) {
        lcd.print("- " + autoParams[paramIndex] + ": " + String(presetValues[activePreset][paramIndex]) + " m");
      } else {
        lcd.print("  " + autoParams[paramIndex] + ": " + String(presetValues[activePreset][paramIndex]) + " m");
      }
    }
  }
}

// Общие экраны
void drawEditScreen()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(autoParams[currentParam]);
  lcd.setCursor(0, 1);
  lcd.print("Value: " + editBuffer);
}

void drawSubMenu() {
  lcd.clear();
  switch (currentMenu) {
    case 0:
      drawSetupMenu();   
      break;
    case 1:
      drawAutomaticMenu();
      break;
    case 2:
      drawInfoMenu();
      break;
    case 3:
      lcd.setCursor(0, 0);
      lcd.print("Menu: Manual");
      lcd.setCursor(0, 1);
      lcd.print("<- Back");
      break;
    default:
      lcd.setCursor(0, 0);
      lcd.print("Unknown menu");
      break;
  }
}

void handleInfoMenu(unsigned short X, unsigned short Y) {
  if (X < 100 && !joyMovedDown) {
    if (currentInfo < infoCount - 1) {
      currentInfo++;
      if (currentInfo >= topInfo + 2) topInfo++;
      drawInfoMenu();
    }
    joyMovedDown = true;
  } else if (X > 200 && joyMovedDown) {
    joyMovedDown = false;
  }

  if (X > 900 && !joyMovedUp) {
    if (currentInfo > 0) {
      currentInfo--;
      if (currentInfo < topInfo) topInfo--;
      drawInfoMenu();
    }
    joyMovedUp = true;
  } else if (X < 800 && joyMovedUp) {
    joyMovedUp = false;
  }

  // Выход свайпом влево
  if (Y < 100) {
    drawMenu();
    delay(300);
  }
}

void drawInfoMenu() {
  lcd.clear();
  for (byte i = 0; i < 2; i++) {
    byte infoIndex = topInfo + i;

    if (infoIndex < infoCount) {
      lcd.setCursor(0, i);

      if (infoIndex == currentInfo) 
        lcd.print("- " + infoParams[infoIndex]);

      else lcd.print("  " + infoParams[infoIndex]);
    }
  }
}

// Обработка Manual меню
void handleManualMenu(unsigned short X, unsigned short Y) {
  char key = customKeypad.getKey();

  if (X <= 200 && !joyMovedDown) { 
      if (currentManual < MANUAL_COUNT - 1) 
        currentManual++; 

      if (currentManual >= topManual + 2) 
        topManual++; 
      
      needRefresh = true;
      joyMovedDown = true;
  } else if (X > 200 && joyMovedDown) {
    joyMovedDown = false;
  }

  if (X >= 800 && !joyMovedUp) { 
    if(currentManual > 0) 
      currentManual--; 
    if(currentManual < topManual) 
      topManual--; 
    
    needRefresh = true;
    joyMovedUp = true;
  } else if (X < 800 && joyMovedUp) {
    joyMovedUp = false;
  }

  if (needRefresh) {
    lcd.clear();
    needRefresh = false;
    for(byte i=0;i<2;i++){
      byte idx = topManual+i;
      if(idx < MANUAL_COUNT){
          lcd.setCursor(0,i);
          String val;
          switch(idx){
            case 0: 
              val = lightOn ? "ON" : "OFF"; 
              break;
            case 1:
              val = "RGB";
              break;  
            case 2: 
              val = waterOn? "ON" : "OFF"; 
              break;
          }
          lcd.print((idx==currentManual?"- ":"  ") + manualParams[idx] + ": " + val);
      }
    }
  }
  
  if (Y < 100) { 
    currentMenu = Main;
    needRefresh = true;
    delay(300); 
  }

  if (swState && swState != lastSwState) {
    needRefresh = true;
    switch(currentManual){
      case 0: 
        lightOn = !lightOn; 
        break;
      case 1: 
        rgbCursor = 0; 
        currentMenu = RgbEdit;
        editValue = 4;
        break;  
      case 2: 
        waterOn = !waterOn; 
        break;
    }
  }
}

void drawManualEdit(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(manualParams[currentParam]);
  lcd.setCursor(0,1);
  lcd.print("Value: " + editBufferManual);
}

// Экран редактирования RGB
void drawManualEditRGB() {
  char key = customKeypad.getKey();

  if (joyX <= 200 && !joyMovedDown) { 
    if(rgbCursor < 2 && editValue > 3) 
      rgbCursor++;

    joyMovedDown = true;
    needRefresh = true;
  } else if (joyX > 200 && joyMovedDown) {
    joyMovedDown = false;
  }

  if (joyX >= 800 && !joyMovedUp) {
    if(rgbCursor > 0 && editValue > 3) 
      rgbCursor--;

    joyMovedUp = true;
    needRefresh = true;
  } else if(joyX < 800 && joyMovedUp) {
    joyMovedUp = false;
  }

  // Ввод с клавиатуры
  if (editValue <= 3 && key) {
    needRefresh = true;

    if (key >= '0' && key <= '9') {
      editBuffer += key;
    } else if (key == 'A' && editBuffer.length() > 0) {
      editBuffer.remove(editBuffer.length() - 1);
    }
  }

  // Подтверждение джойстиком
  if (swState && swState != lastSwState) {
    needRefresh = true;

    if (editValue > 3) {
      editValue = rgbCursor;
    } else {
      editValue = 4;

      byte val = constrain(editBuffer.toInt(), 0, 255);

      switch(rgbCursor){
        case 0: 
          R = val; 
          break;
        case 1: 
          G = val; 
          break;
        case 2: 
          B = val; 
          break;
      }

      editBuffer = "";
    }
    
  }

  if (needRefresh) {
    needRefresh = false;
    lcd.clear();
    lcd.setCursor(0, 0);

    for (byte i = 0; i < 2; i ++) {
      lcd.setCursor(0, i);
      byte l = i + (rgbCursor > 1 ? 1 : 0);

      String line;
      if (l == rgbCursor) {
        if (editValue > 3) {
          line = "- ";
        } else {
          line = "* ";
        }
      } else {
        line = "  ";
      }

      switch(l) {
        case 0: 
          line += "R: " + (editValue != l ? String(R) : editBuffer); 
          break;
        case 1: 
          line += "G: " + (editValue != l ? String(G) : editBuffer); 
          break;
        case 2: 
          line += "B: " + (editValue != l ? String(B) : editBuffer); 
          break;
      }
      lcd.print(line);
    }
  }

  // Отмена свайпом влево
  if (joyY < 200) {
    currentMenu = Manual;
    needRefresh = true;
    delay(300);
  } 
}
