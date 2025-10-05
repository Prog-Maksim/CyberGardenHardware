#include <Arduino.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
// #include <VirtualWire.h>
#include <IRremote.h>


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
  RgbEdit,
  SetupEdit
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
#define AUTO_PARAM_COUNT 2
const String autoParams[] = {"Light <", "Water <"};
short autoLight = 0;
short autoWater = 0;

#define MAX_PRESETS 3
byte presetValues[MAX_PRESETS][AUTO_PARAM_COUNT] = {
  {5, 10},   // Preset 1
  {3, 7},    // Preset 2
  {0, 0}      // Preset 3 (пустой)
};
byte activePreset = 0; // выбранная предустановка

// Setup меню
byte currentPreset = 0;
byte topPreset = 0;

// Automatic параметры
byte currentParam = 0;     
byte topParam = 0;         
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
#define MANUAL_COUNT 5
const String manualParams[] = {"Light", "Color", "Watering", "Ventil", "Rotor V"};

// Состояние теплицы
bool lightOn = false;
bool waterOn = false;
bool ventilationOn = false;
byte rotorSpeed = 0;
byte R = 0, G = 0, B = 0;

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

#define IR_SEND_PIN 11

void setup()
{
  Serial.begin(9600);
  IrSender.begin(IR_SEND_PIN, ENABLE_LED_FEEDBACK);

  pinMode(JOY_PIN_X, INPUT);
  pinMode(JOY_PIN_Y, INPUT);
  pinMode(JOY_SW_PIN, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
}

void loop()
{
  swState = !digitalRead(JOY_SW_PIN); 
  
  joyX = analogRead(JOY_PIN_X);
  joyY = analogRead(JOY_PIN_Y);

  switch (currentMenu) {
    case Main:
      handleNavigationMenu();
      break;
    case Setup: // Setup
      handleSetupMenu();
      break;
    case Automatic: // Automatic
      handleAutomaticMenu();
      break;
    case Info: // Info
      handleInfoMenu();
      break;
    case Manual: // Manual
      handleManualMenu(); 
      break; 
    case RgbEdit:
      drawManualEditRGB();
      break;
    case SetupEdit:
      handlePresetEdit();
      break;
  }
  
  delay(50);
  lastSwState = swState;
}

// Главное меню
void handleNavigationMenu()
{
  if (joyX <= 200 && !joyMovedDown) {
    if (selection < MENU_COUNT - 1) {
      selection++;
      if (selection >= topMenu + 2) 
        topMenu++;
      needRefresh = true;
    }
    joyMovedDown = true;
  } else if (joyX > 200 && joyMovedDown) {
    joyMovedDown = false;
  }

  if (joyX >= 800 && !joyMovedUp) {
    if (selection > 0) {
      selection--;
      if (selection < topMenu) 
        topMenu--;
      needRefresh = true;
    }
    joyMovedUp = true;
  } else if (joyX < 800 && joyMovedUp) {
    joyMovedUp = false;
  } 

  if (needRefresh) {
    needRefresh = false;
    lcd.clear();
    for (byte i = 0; i < 2; ++i) {
      byte menuIndex = topMenu + i;
      if (menuIndex < MENU_COUNT) {
        lcd.setCursor(0, i);
        lcd.print((menuIndex == selection ? "- " : "  ") + menus[menuIndex]);
      }
    }
  }

  if (swState && swState != lastSwState) {
    needRefresh = true;
    switch (selection) {
      case 0: 
        currentMenu = Setup;
        break;
      case 1: 
        currentMenu = Automatic;
        currentParam = 0;
        editValue = 3;
        break;
      case 2: 
        currentMenu = Info;
        break;
      case 3: 
        currentMenu = Manual;
        editValue = MANUAL_COUNT;
        break;
    }
  }
}

void handleSetupMenu() {
  // Навигация по списку предустановок
  if (joyX <= 200 && !joyMovedDown) {
    if (currentPreset < MAX_PRESETS - 1) {
      currentPreset++;

      if (currentPreset >= topPreset + 2) 
        topPreset++;

      needRefresh = true;
    }

    joyMovedDown = true;
  } else if (joyX > 200 && joyMovedDown) {
    joyMovedDown = false;
  }

  if (joyX >= 900 && !joyMovedUp) {
    if (currentPreset > 0) {
      currentPreset--;
      if (currentPreset < topPreset) topPreset--;
      needRefresh = true;
    }
    joyMovedUp = true;
  } else if (joyX < 800 && joyMovedUp) {
    joyMovedUp = false;
  }

  // Нажатие джойстика = выбор активной предустановки
  if (swState && swState != lastSwState) {
    activePreset = currentPreset; 
    needRefresh = true;
  }

  if (needRefresh) {
    needRefresh = false;

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

  // Свайп вправо = вход в параметры выбранного пресета
  if (joyY > 800) {
    currentParam = 0;
    topParam = 0;
    editValue = 4;
    currentMenu = SetupEdit;
    needRefresh = true;
    delay(300);
  } else if (joyY < 200) {
    needRefresh = true;
    currentMenu = Main;
    delay(300);
  }
}

void handlePresetEdit() {
  char key = customKeypad.getKey();


    if (joyX <= 200 && !joyMovedDown) {
      if (currentParam < AUTO_PARAM_COUNT - 1) {
        currentParam++;

        if (currentParam >= topParam + 2) 
          topParam++;

        needRefresh = true;
      }
      joyMovedDown = true;
    } else if (joyX > 200 && joyMovedDown) {
      joyMovedDown = false;
    }

    if (joyX >= 800 && !joyMovedUp) {
      if (currentParam > 0) {
        currentParam--;

        if (currentParam < topParam) 
          topParam--;

        needRefresh = true;
      }
      joyMovedUp = true;
    } else if (joyX < 800 && joyMovedUp) {
      joyMovedUp = false;
    }

    if (swState && swState != lastSwState) {
      needRefresh = true;
      if (editValue == 4) {
        editValue = currentParam;
        editBuffer = "";
      } else {
        editValue = 4;
        presetValues[currentPreset][currentParam] = editBuffer.toInt();
      }
    }

    if (editValue != 4 && key) {
      needRefresh = true;
      if (key >= '0' && key <= '9') {
        editBuffer += key;
      } else if (key == 'A' && editBuffer.length() > 0) { 
        editBuffer.remove(editBuffer.length() - 1);
      }
    }

    if (needRefresh) {
      needRefresh = false;

      lcd.clear();
      for (byte i = 0; i < 2; i++) {
        byte paramIndex = topParam + i;

        if (paramIndex < AUTO_PARAM_COUNT) {
          lcd.setCursor(0, i);

          if (paramIndex == currentParam) {
            if (editValue == currentParam) {
              lcd.print("* " + autoParams[paramIndex] + ": " + editBuffer + "%");
            } else {
              lcd.print("- " + autoParams[paramIndex] + ": " + String(presetValues[currentPreset][paramIndex]) + "%");
            }
          } else {
            lcd.print("  " + autoParams[paramIndex] + ": " + String(presetValues[currentPreset][paramIndex]) + "%");
          }
        }
      }
    }

    if (joyY < 200) {
      currentMenu = Setup;
      needRefresh = true;
      delay(300);
    }
}

// Automatic меню
void handleAutomaticMenu() {
   char key = customKeypad.getKey();


    if (joyX <= 200 && !joyMovedDown) {
      if (currentParam < AUTO_PARAM_COUNT - 1) {
        currentParam++;

        if (currentParam >= topParam + 2) 
          topParam++;

        needRefresh = true;
      }
      joyMovedDown = true;
    } else if (joyX > 200 && joyMovedDown) {
      joyMovedDown = false;
    }

    if (joyX >= 800 && !joyMovedUp) {
      if (currentParam > 0) {
        currentParam--;

        if (currentParam < topParam) 
          topParam--;

        needRefresh = true;
      }
      joyMovedUp = true;
    } else if (joyX < 800 && joyMovedUp) {
      joyMovedUp = false;
    }

    if (swState && swState != lastSwState) {
      needRefresh = true;
      if (editValue == 3) {
        editValue = currentParam;
        editBuffer = "";
      } else {
        if (editValue == 0) {
          autoLight = editBuffer.toInt();
          IrSender.sendNEC(0x0555 & 0xFF, autoLight, 0); 
        } else {
          autoWater = editBuffer.toInt();
          IrSender.sendNEC(0x0777 & 0xFF, autoWater, 0);
        }
        editValue = 3;
      }
    }

    if (editValue != 3 && key) {
      needRefresh = true;
      if (key >= '0' && key <= '9') {
        editBuffer += key;
      } else if (key == 'A' && editBuffer.length() > 0) { 
        editBuffer.remove(editBuffer.length() - 1);
      }
    }

    if (needRefresh) {
      needRefresh = false;

      lcd.clear();
      for (byte i = 0; i < 2; i++) {
        byte paramIndex = topParam + i;

        if (paramIndex < AUTO_PARAM_COUNT) {
          lcd.setCursor(0, i);

          if (paramIndex == currentParam) {
            if (editValue == currentParam) {
              lcd.print("* " + autoParams[paramIndex] + ": " + editBuffer + "%");
            } else {
              lcd.print("- " + autoParams[paramIndex] + ": " + String(paramIndex == 0 ? autoLight : autoWater) + "%");
            }
          } else {
            lcd.print("  " + autoParams[paramIndex] + ": " + String(paramIndex == 0 ? autoLight : autoWater) + "%");
          }
        }
      }
    }

    if (joyY < 200) {
      currentMenu = Main;
      needRefresh = true;
      delay(300);
    }
}

void handleInfoMenu() {
  if (joyX <= 200 && !joyMovedDown) {
    if (currentInfo < infoCount - 1) {
      currentInfo++;

      if (currentInfo >= topInfo + 2) 
        topInfo++;

      needRefresh = true;
    }
    joyMovedDown = true;
  } else if (joyX > 200 && joyMovedDown) {
    joyMovedDown = false;
  }

  if (joyX >= 800 && !joyMovedUp) {
    if (currentInfo > 0) {
      currentInfo--;

      if (currentInfo < topInfo) 
        topInfo--;

      needRefresh = true;
    }
    joyMovedUp = true;
  } else if (joyX < 800 && joyMovedUp) {
    joyMovedUp = false;
  }

  if (needRefresh) {
    needRefresh = false;
    lcd.clear();
    for (byte i = 0; i < 2; i++) {
      byte infoIndex = topInfo + i;

      if (infoIndex < infoCount) {
        lcd.setCursor(0, i);

        if (infoIndex == currentInfo) 
          lcd.print("- " + infoParams[infoIndex]);
        else 
          lcd.print("  " + infoParams[infoIndex]);
      }
    }
  }

  // Выход свайпом влево
  if (joyY < 100) {
    needRefresh = true;
    currentMenu = Main;
    delay(300);
  }
}

// Обработка Manual меню
void handleManualMenu() {
  char key = customKeypad.getKey();

  if (joyX <= 200 && !joyMovedDown && editValue == MANUAL_COUNT) { 
      if (currentManual < MANUAL_COUNT - 1) 
        currentManual++; 

      if (currentManual >= topManual + 2) 
        topManual++; 
      
      needRefresh = true;
      joyMovedDown = true;
  } else if (joyX > 200 && joyMovedDown) {
    joyMovedDown = false;
  }

  if (joyX >= 800 && !joyMovedUp && editValue == MANUAL_COUNT) { 
    if(currentManual > 0) 
      currentManual--; 
    if(currentManual < topManual) 
      topManual--; 
    
    needRefresh = true;
    joyMovedUp = true;
  } else if (joyX < 800 && joyMovedUp) {
    joyMovedUp = false;
  }

  if (editValue == 4 && key) {
    needRefresh = true;

    if (key >= '0' && key <= '9') {
      editBuffer += key;
    } else if (key == 'A' && editBuffer.length() > 0) {
      editBuffer.remove(editBuffer.length() - 1);
    }
  }

  if (needRefresh) {
    lcd.clear();
    needRefresh = false;
    for(byte i = 0; i < 2; i++) {
      byte idx = topManual + i;
      if (idx < MANUAL_COUNT) {
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
            case 3:
              val = ventilationOn? "ON" : "OFF"; 
              break;
            case 4:
              val = (editValue != 4 ? String(rotorSpeed) : editBuffer) + "%"; 
              break;
          }
          lcd.print((idx==currentManual ? (editValue != 4 ? "- " : "* ") : "  ") + manualParams[idx] + ": " + val);
      }
    }
  }
  
  if (joyY < 200 && editValue == MANUAL_COUNT) { 
    currentMenu = Main;
    needRefresh = true;
    delay(300); 
  }

  if (swState && swState != lastSwState) {
    needRefresh = true;
    switch (currentManual) {
      case 0: 
        lightOn = !lightOn;
        IrSender.sendNEC(0x0444 & 0xFF, lightOn, 0); 
        break;
      case 1: 
        rgbCursor = 0; 
        currentMenu = RgbEdit;
        editValue = 4;
        break;  
      case 2: 
        waterOn = !waterOn; 
        IrSender.sendNEC(0x0666 & 0xFF, waterOn, 0); 
        break;
      case 3: 
        ventilationOn = !ventilationOn; 
        IrSender.sendNEC(0x0888 & 0xFF, ventilationOn, 0); 
        break;
      case 4:
        if (editValue == MANUAL_COUNT) {
          editValue = 4;
          editBuffer = "";
        } else {
          editValue = MANUAL_COUNT;
          byte val = constrain(editBuffer.toInt(), 0, 100);
          rotorSpeed = val;
          IrSender.sendNEC(0x0999 & 0xFF, rotorSpeed, 0); 
        }
        break;
    }
  }
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
          IrSender.sendNEC(0x0111 & 0xFF, val, 0);
          break;
        case 1: 
          G = val; 
          IrSender.sendNEC(0x0222 & 0xFF, val, 0);
          break;
        case 2: 
          B = val; 
          IrSender.sendNEC(0x0333 & 0xFF, val, 0);
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
