#include "Morse.h"

Morse::Morse(uint8_t pin) : _button(pin) {
  _dotDuration = 200; // Durata del punto in ms
  _dashDuration = _dotDuration * 3;
  _charGapDuration = _dotDuration * 3;
  _wordGapDuration = _dotDuration * 7;
  _lastSignalTime = 0;
  _isGap = false;
  _newChar = false;
  _currentChar = "";
}

void Morse::update() {
  _button.update();

  unsigned long currentTime = millis();
  bool signal = _button.isPressed();

  if (signal) {
    _lastSignalTime = currentTime;
    _isGap = false;
  } else if (!_isGap && (currentTime - _lastSignalTime > _charGapDuration)) {
    _isGap = true;
    if (currentTime - _lastSignalTime >= _wordGapDuration) {
      _morseCode += " ";
    } else {
      _morseCode += " ";
    }
  }

  if (_button.wasPressed()) {
    unsigned long pressDuration = _button.pressedDuration();
    if (pressDuration < _dotDuration) {
      _morseCode += ".";
    } else if (pressDuration < _dashDuration) {
      _morseCode += "-";
    }
  }

  if (_isGap && (currentTime - _lastSignalTime > _charGapDuration)) {
    if (_morseCode.length() > 0) {
      _morseCode.trim(); // Usa trim() per rimuovere spazi all'inizio e alla fine
      _currentChar = decodeMorse(_morseCode);
      _morseCode = "";
      _newChar = true;
    }
  }
}

String Morse::getChar() {
  _newChar = false;
  return _currentChar;
}

bool Morse::hasNewChar() {
  return _newChar;
}

String Morse::decodeMorse(String code) {
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
  if (code == ".-.-.-") return ".";
  if (code == "--..--") return ",";
  if (code == "..--..") return "?";
  if (code == ".----.") return "'";
  if (code == "-.-.--") return "!";
  if (code == "-..-.") return "/";
  if (code == "-.--.") return "(";
  if (code == "-.--.-") return ")";
  if (code == ".-...") return "&";
  if (code == "---...") return ":";
  if (code == "-.-.-.") return ";";
  if (code == "-...-") return "=";
  if (code == ".-.-.") return "+";
  if (code == "-....-") return "-";
  if (code == "..--.-") return "_";
  if (code == ".-..-.") return "\"";
  if (code == "...-..-") return "$";
  if (code == ".--.-.") return "@";
  if (code == "...---...") return "SOS";
  if (code == "-...-.-") return "BT";
  if (code == ".-.-") return "AS";
  
  return "";
}
