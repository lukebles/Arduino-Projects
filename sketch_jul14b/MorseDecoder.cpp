#include "MorseDecoder.h"

MorseDecoder::MorseDecoder(int pin) : button(pin), lastChangeTime(0), messageAvailable(false) {}

void MorseDecoder::update() {
  button.update();


  if (button.wasReleased()) {
    if (button.pressedDuration() > 1000) {
        // message = decodeCharacter(morseCode);
        // messageAvailable = true;
        // Serial.println(message);
    } else {
      if (button.pressedDuration() > 300) {
        morseCode += "-";
      } else {
        morseCode += ".";
      }
      //Serial.println(morseCode);
    }
  }

  if (button.wasPressed()) {
    message = "";
  }

  if (button.pressedDuration()  > 2000){
    if (button.wasReleased() ) {
      message = decodeCharacter(morseCode);
      morseCode = "";
      messageAvailable = true;
    }
  }
}

bool MorseDecoder::hasMessage() {
    return messageAvailable;
}

String MorseDecoder::getMessage() {
    if (messageAvailable) {
        messageAvailable = false;
        return message;
    }
    return "";
}

String MorseDecoder::decodeCharacter(String code) {
    // This function maps Morse code strings to corresponding characters
    if (code == ".-") return "A";
    if (code == "-...") return "B";
    if (code == "-.-.") return "C";
    if (code == "-..") return "D";
    if (code == ".") return "E";
    if (code == "..-.") return "F";
    if (code == "--.") return "G";
    if (code == "....") return "H";
    if (code == "..") return "I";
    if (code == ".---") return "J";
    if (code == "-.-") return "K";
    if (code == ".-..") return "L";
    if (code == "--") return "M";
    if (code == "-.") return "N";
    if (code == "---") return "O";
    if (code == ".--.") return "P";
    if (code == "--.-") return "Q";
    if (code == ".-.") return "R";
    if (code == "...") return "S";
    if (code == "-") return "T";
    if (code == "..-") return "U";
    if (code == "...-") return "V";
    if (code == ".--") return "W";
    if (code == "-..-") return "X";
    if (code == "-.--") return "Y";
    if (code == "--..") return "Z";
    if (code == "-----") return "0";
    if (code == ".----") return "1";
    if (code == "..---") return "2";
    if (code == "...--") return "3";
    if (code == "....-") return "4";
    if (code == ".....") return "5";
    if (code == "-....") return "6";
    if (code == "--...") return "7";
    if (code == "---..") return "8";
    if (code == "----.") return "9";
    if (code == "...---...") return "SOS";
    if (code == "-...-") return "BT";
    if (code == ".-.-.") return "AR";
    // Add more special characters if needed
    return "?";
}
