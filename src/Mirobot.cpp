#include "Arduino.h"
#include "Mirobot.h"

HotStepper rightMotor(&PORTB, 0b00011101);
HotStepper leftMotor(&PORTD, 0b11110000);

CmdProcessor cmdProcessor;
SerialWebSocket v1ws(Serial);

void sendSerialMsg(ArduinoJson::JsonObject &outMsg){
  outMsg.printTo(Serial);
  Serial.println();
}

void sendSerialMsgV1(ArduinoJson::JsonObject &outMsg){
  v1ws.send(outMsg);
}

Mirobot::Mirobot(){
  blocking = true;
  lastLedChange = millis();
  calibratingSlack = false;
  beepComplete = 0;
}

void Mirobot::begin(unsigned char v){
  version(v);
  // Initialise the steppers
  HotStepper::begin();
  // Set up the pen arm servo
  pinMode(SERVO_PIN, OUTPUT);
  // Set up the collision sensor inputs and state
  pinMode(LEFT_COLLIDE_SENSOR, INPUT_PULLUP);
  pinMode(RIGHT_COLLIDE_SENSOR, INPUT_PULLUP);
  _collideStatus = NORMAL;
  // Initialise the pen arm into the up position
  setPenState(UP);
  // Pull the settings out of memory
  initSettings();
  // Set up the status LED
  pinMode(STATUS_LED, OUTPUT);
}

void Mirobot::begin(){
  begin(2);
}

void Mirobot::enableSerial(){
  // Use non-blocking mode to process serial
  blocking = false;
  // Set up the commands
  initCmds();
  // Set up Serial and add it to be processed
  Serial.begin(57600);
  // Add the output handler for responses
  if(hwVersion == 1){
    cmdProcessor.addOutputHandler(sendSerialMsgV1);
  }else{
    cmdProcessor.addOutputHandler(sendSerialMsg);
  }
  // Enable serial processing
  serialEnabled = true;
}

void Mirobot::initSettings(){
  if(EEPROM.read(EEPROM_OFFSET) == MAGIC_BYTE_1 && EEPROM.read(EEPROM_OFFSET + 1) == MAGIC_BYTE_2 && EEPROM.read(EEPROM_OFFSET + 2) == SETTINGS_VERSION){
    // We've previously written something valid to the EEPROM
    for (unsigned int t=0; t<sizeof(settings); t++){
      *((char*)&settings + t) = EEPROM.read(EEPROM_OFFSET + 2 + t);
    }
    // Sanity check the values to make sure they look correct
    if(settings.settingsVersion == SETTINGS_VERSION &&
       settings.slackCalibration > 50 &&
       settings.moveCalibration > 0.5f &&
       settings.moveCalibration < 1.5f &&
       settings.turnCalibration > 0.5f &&
       settings.turnCalibration < 1.5f){
      // The values look OK so let's leave them as they are
      return;
    }
  }
  // Either this is the first boot or the settings are bad so let's reset them
  settings.settingsVersion = SETTINGS_VERSION;
  settings.slackCalibration = 14;
  settings.moveCalibration = 1.0f;
  settings.turnCalibration = 1.0f;
  saveSettings();
}

void Mirobot::saveSettings(){
  EEPROM.write(EEPROM_OFFSET, MAGIC_BYTE_1);
  EEPROM.write(EEPROM_OFFSET + 1, MAGIC_BYTE_2);
  for (unsigned int t=0; t<sizeof(settings); t++){
    EEPROM.write(EEPROM_OFFSET + 2 + t, *((char*)&settings + t));
  }
}

void Mirobot::initCmds(){
  cmdProcessor.setMirobot(self());
  //             Command name        Handler function             // Returns immediately
  cmdProcessor.addCmd("version",          &Mirobot::_version,          true);
  cmdProcessor.addCmd("ping",             &Mirobot::_ping,             true);
  cmdProcessor.addCmd("uptime",           &Mirobot::_uptime,           true);
  cmdProcessor.addCmd("pause",            &Mirobot::_pause,            true);
  cmdProcessor.addCmd("resume",           &Mirobot::_resume,           true);
  cmdProcessor.addCmd("stop",             &Mirobot::_stop,             true);
  cmdProcessor.addCmd("collideState",     &Mirobot::_collideState,     true);
  cmdProcessor.addCmd("collideNotify",    &Mirobot::_collideNotify,    true);
  cmdProcessor.addCmd("followState",      &Mirobot::_followState,      true);
  cmdProcessor.addCmd("followNotify",     &Mirobot::_followNotify,     true);
  cmdProcessor.addCmd("slackCalibration", &Mirobot::_slackCalibration, true);
  cmdProcessor.addCmd("moveCalibration",  &Mirobot::_moveCalibration,  true);
  cmdProcessor.addCmd("turnCalibration",  &Mirobot::_turnCalibration,  true);
  cmdProcessor.addCmd("calibrateMove",    &Mirobot::_calibrateMove,    true);
  cmdProcessor.addCmd("calibrateTurn",    &Mirobot::_calibrateTurn,    true);
  cmdProcessor.addCmd("forward",          &Mirobot::_forward,          false);
  cmdProcessor.addCmd("back",             &Mirobot::_back,             false);
  cmdProcessor.addCmd("right",            &Mirobot::_right,            false);
  cmdProcessor.addCmd("left",             &Mirobot::_left,             false);
  cmdProcessor.addCmd("penup",            &Mirobot::_penup,            false);
  cmdProcessor.addCmd("pendown",          &Mirobot::_pendown,          false);
  cmdProcessor.addCmd("follow",           &Mirobot::_follow,           false);
  cmdProcessor.addCmd("collide",          &Mirobot::_collide,          false);
  cmdProcessor.addCmd("beep",             &Mirobot::_beep,             false);
  cmdProcessor.addCmd("calibrateSlack",   &Mirobot::_calibrateSlack,   false);
  cmdProcessor.addCmd("arc",              &Mirobot::_arc,              false);
}

void Mirobot::_version(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  outJson["msg"] = versionStr;
}

void Mirobot::_ping(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){}

void Mirobot::_uptime(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  outJson["msg"] = millis();
}

void Mirobot::_pause(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  pause();
}

void Mirobot::_resume(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  resume();
}

void Mirobot::_stop(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  stop();
}

void Mirobot::_collideState(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  switch(collideState()){
    case NONE:
      outJson["msg"] = "none";
      break;
    case LEFT:
      outJson["msg"] = "left";
      break;
    case RIGHT:
      outJson["msg"] = "right";
      break;
    case BOTH:
      outJson["msg"] = "both";
      break;
  }
}

void Mirobot::_collideNotify(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  collideNotify = !!strcmp(inJson["arg"], "false");
}

void Mirobot::_followState(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  outJson["msg"] = followState();
}

void Mirobot::_followNotify(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  followNotify = !!strcmp(inJson["arg"].asString(), "false");
}

void Mirobot::_slackCalibration(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  outJson["msg"] = settings.slackCalibration;
}

void Mirobot::_moveCalibration(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  outJson["msg"] = settings.moveCalibration;
}

void Mirobot::_turnCalibration(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  outJson["msg"] = settings.turnCalibration;
}

void Mirobot::_calibrateMove(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  calibrateMove(atof(inJson["arg"].asString()));
}

void Mirobot::_calibrateTurn(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  calibrateTurn(atof(inJson["arg"].asString()));
}

void Mirobot::_forward(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  forward(atoi(inJson["arg"].asString()));
}

void Mirobot::_back(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  back(atoi(inJson["arg"].asString()));
}

void Mirobot::_right(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  right(atoi(inJson["arg"].asString()));
}

void Mirobot::_left(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  left(atoi(inJson["arg"].asString()));
}

void Mirobot::_penup(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  penup();
}

void Mirobot::_pendown(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  pendown();
}

void Mirobot::_follow(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  follow();
}

void Mirobot::_collide(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  collide();
}

void Mirobot::_beep(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  beep(atoi(inJson["arg"].asString()));
}

void Mirobot::_calibrateSlack(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  calibrateSlack(atoi(inJson["arg"].asString()));
}

void Mirobot::_arc(ArduinoJson::JsonObject &inJson, ArduinoJson::JsonObject &outJson){
  if(inJson["arg"].is<JsonArray&>() && inJson["arg"].size() == 2){
    arc(inJson["arg"][0].as<float>(), inJson["arg"][1].as<float>());
  }
}

void Mirobot::takeUpSlack(byte rightMotorDir, byte leftMotorDir){
  // Take up the slack on each motor
  if(rightMotor.lastDirection != rightMotorDir){
    rightMotor.turn(settings.slackCalibration, rightMotorDir);
  }
  if(leftMotor.lastDirection != leftMotorDir){
    leftMotor.turn(settings.slackCalibration, leftMotorDir);
  }
}

void Mirobot::forward(int distance){
  takeUpSlack(FORWARD, BACKWARD);
  rightMotor.turn(distance * steps_per_mm * settings.moveCalibration, FORWARD);
  leftMotor.turn(distance * steps_per_mm * settings.moveCalibration, BACKWARD);
  wait();
}

void Mirobot::back(int distance){
  takeUpSlack(BACKWARD, FORWARD);
  rightMotor.turn(distance * steps_per_mm * settings.moveCalibration, BACKWARD);
  leftMotor.turn(distance * steps_per_mm * settings.moveCalibration, FORWARD);
  wait();
}

void Mirobot::left(int angle){
  takeUpSlack(FORWARD, FORWARD);
  rightMotor.turn(angle * steps_per_degree * settings.turnCalibration, FORWARD);
  leftMotor.turn(angle * steps_per_degree * settings.turnCalibration, FORWARD);
  wait();
}

void Mirobot::right(int angle){
  takeUpSlack(BACKWARD, BACKWARD);
  rightMotor.turn(angle * steps_per_degree * settings.turnCalibration, BACKWARD);
  leftMotor.turn(angle * steps_per_degree * settings.turnCalibration, BACKWARD);
  wait();
}

void Mirobot::penup(){
  setPenState(UP);
  wait();
}

void Mirobot::pendown(){
  setPenState(DOWN);
  wait();
}

void Mirobot::pause(){
  rightMotor.pause();
  leftMotor.pause();
  paused = true;
}

void Mirobot::resume(){
  rightMotor.resume();
  leftMotor.resume();
  paused = false;
}

void Mirobot::stop(){
  rightMotor.stop();
  leftMotor.stop();
  following = false;
  colliding = false;
  calibratingSlack = false;
}

void Mirobot::follow(){
  following = true;
}

int Mirobot::followState(){
  return analogRead(LEFT_LINE_SENSOR) - analogRead(RIGHT_LINE_SENSOR);
}

void Mirobot::collide(){
  colliding = true;
}

collideState_t Mirobot::collideState(){
  boolean collideLeft = !digitalRead(LEFT_COLLIDE_SENSOR);
  boolean collideRight = !digitalRead(RIGHT_COLLIDE_SENSOR);
  if(collideLeft && collideRight){
    return BOTH;
  }else if(collideLeft){
    return LEFT;
  }else if(collideRight){
    return RIGHT;
  }else{
    return NONE;
  }
}

void Mirobot::beep(int duration){
  tone(SPEAKER_PIN, NOTE_C4, duration);
  beepComplete = millis() + duration;
}

void Mirobot::arc(float angle, float radius){
  // Drawing an arc means drawing three concentric circular arcs with two wheels and a pen at the centre
  // So we need to work out the length of the outer, wheel arcs and then move them by that amount in the same time
  // To calculate the distance we can work out:
  //   circumference = 2 * pi * radius
  //   distance = circumference * (angle / 360)
  // combined:
  //   distance = 2 * pi * radius * (angle / 360)
  //   distance = 2 * 3.141593 * radius * (angle / 360)
  //   distance = 6.283185 * radius * (angle / 360)
  //   distance = 0.017453 * radius * angle
  float right_distance, left_distance;
  float right_rate, left_rate;
  int wheel_distance = 120;

  // extract the sign of the direction (+1 / -1) which will give us the correct distance to turn the steppers
  char dir = (radius > 0) - (radius < 0);

  // work out the distances each wheel has to move
  right_distance = 0.017453 * (radius - (wheel_distance / 2.0)) * angle * dir;
  left_distance = 0.017453 * (radius + (wheel_distance / 2.0)) * angle * dir;

  // work out the rate the wheel should move relative to full speed
  right_rate = abs((right_distance > left_distance) ? 1 : (right_distance / left_distance));
  left_rate = abs((right_distance > left_distance) ? (left_distance / right_distance) : 1);

  // move the wheels
  takeUpSlack((right_distance > 0), (left_distance < 0));
  rightMotor.turn(abs(right_distance) * steps_per_mm * settings.moveCalibration, (right_distance > 0), right_rate);
  leftMotor.turn(abs(left_distance) * steps_per_mm * settings.moveCalibration, (left_distance < 0), left_rate);
  wait();
}

boolean Mirobot::ready(){
  return (rightMotor.ready() && leftMotor.ready() && !servo_pulses_left && beepComplete < millis());
}

void Mirobot::wait(){
  if(blocking){
    while(!ready()){
      if(servo_pulses_left){
        servoHandler();
      }
    }
  }
}

void Mirobot::setPenState(penState_t state){
  penState = state;
  servo_pulses_left = SERVO_PULSES;
  next_servo_pulse = 0;
}

void Mirobot::followHandler(){
  if(rightMotor.ready() && leftMotor.ready()){
    int diff = analogRead(LEFT_LINE_SENSOR) - analogRead(RIGHT_LINE_SENSOR);
    if(diff > 5){
      if(hwVersion == 1){
        right(1);
      }else if(hwVersion == 2){
        left(1);
      }
    }else if(diff < -5){
      if(hwVersion == 1){
        left(1);
      }else if(hwVersion == 2){
        right(1);
      }
    }else{
      forward(5);
    }
  }
}

void Mirobot::collideHandler(){
  boolean collideLeft = !digitalRead(LEFT_COLLIDE_SENSOR);
  boolean collideRight = !digitalRead(RIGHT_COLLIDE_SENSOR);
  if(_collideStatus == NORMAL){
    if(collideLeft){
      _collideStatus = LEFT_REVERSE;
      back(50);
    }else if(collideRight){
      _collideStatus = RIGHT_REVERSE;
      back(50);
    }else{
      forward(10);
    }
  }else if(rightMotor.ready() && leftMotor.ready()){
    switch(_collideStatus){
      case LEFT_REVERSE :
        _collideStatus = LEFT_TURN;
        right(90);
        break;
      case RIGHT_REVERSE :
        _collideStatus = RIGHT_TURN;
        left(90);
        break;
      case LEFT_TURN :
      case RIGHT_TURN :
        _collideStatus = NORMAL;
      case NORMAL :
        break;
    }
  }
}

void Mirobot::ledHandler(){
  long t = millis();
  digitalWrite(STATUS_LED, (!((t / 100) % 10) || !(((t / 100) - 2) % 10)));
}

void Mirobot::servoHandler(){
  if(servo_pulses_left){
    if(micros() >= next_servo_pulse){
      servo_pulses_left--;
      digitalWrite(SERVO_PIN, HIGH);
      if(penState == UP){
        next_servo_pulse = micros() + (12000 - penup_delay);
        delayMicroseconds(penup_delay);
      }else{
        next_servo_pulse = micros() + (12000 - pendown_delay);
        delayMicroseconds(pendown_delay);
      }
      digitalWrite(SERVO_PIN, LOW);
    } 
  }
}

void Mirobot::autoHandler(){
  if(following){
    followHandler();
  }else if(colliding){
    collideHandler();
  }
}

void Mirobot::sensorNotifier(){
  StaticJsonBuffer<60> outBuffer;
  JsonObject& outMsg = outBuffer.createObject();
  if(collideNotify){
    collideState_t currentCollideState = collideState();
    if(currentCollideState != lastCollideState){
      lastCollideState = currentCollideState;
      switch(currentCollideState){
        case BOTH:
          outMsg["msg"] = "both";
          cmdProcessor.notify("collide", outMsg);
          break;
        case LEFT:
          outMsg["msg"] = "left";
          cmdProcessor.notify("collide", outMsg);
          break;
        case RIGHT:
          outMsg["msg"] = "right";
          cmdProcessor.notify("collide", outMsg);
          break;
      }
    }
  }
  if(followNotify){
    int currentFollowState = analogRead(LEFT_LINE_SENSOR) - analogRead(RIGHT_LINE_SENSOR);
    if(currentFollowState != lastFollowState){
      outMsg["msg"] = currentFollowState;
      cmdProcessor.notify("follow", outMsg);
    }
    lastFollowState = currentFollowState;
  }
}

// This allows for runtime configuration of which hardware is used
void Mirobot::version(char v){
  hwVersion = v;
  sprintf(versionStr, "%d.%s", hwVersion, MIROBOT_SUB_VERSION);
  if(v == 1){
    steps_per_mm = STEPS_PER_MM_V1;
    steps_per_degree = STEPS_PER_DEGREE_V1;
    penup_delay = PENUP_DELAY_V1;
    pendown_delay = PENDOWN_DELAY_V1;
    wheel_distance = WHEEL_DISTANCE_V1;
  }else if(v == 2){
    steps_per_mm = STEPS_PER_MM_V2;
    steps_per_degree = STEPS_PER_DEGREE_V2;
    penup_delay = PENUP_DELAY_V2;
    pendown_delay = PENDOWN_DELAY_V2;
    wheel_distance = WHEEL_DISTANCE_V2;
  }
}

void Mirobot::calibrateSlack(unsigned int amount){
  settings.slackCalibration = amount;
  saveSettings();
  calibratingSlack = true;
  rightMotor.turn(1, FORWARD);
  leftMotor.turn(1, BACKWARD);
}

void Mirobot::calibrateMove(float amount){
  settings.moveCalibration = amount;
  saveSettings();
}

void Mirobot::calibrateTurn(float amount){
  settings.turnCalibration = amount;
  saveSettings();
}

void Mirobot::calibrateHandler(){
  if(calibratingSlack && rightMotor.ready() && leftMotor.ready()){
    takeUpSlack((rightMotor.lastDirection == FORWARD ? BACKWARD : FORWARD), (leftMotor.lastDirection == FORWARD ? BACKWARD : FORWARD));
  }
}

void Mirobot::serialHandler(){
  int s;
  if(!serialEnabled) return;
  // process incoming data
  s = Serial.available();
  if (s > 0){
    for(int i = 0; i<s; i++){
      last_char = millis();
      char incomingByte = Serial.read();
      if(hwVersion == 1){
        // Handle the WebSocket parsing over serial for v1
        serial_buffer[serial_buffer_pos++] = incomingByte;
        if(serial_buffer_pos == SERIAL_BUFFER_LENGTH) serial_buffer_pos = 0;
        processState_t res = v1ws.process(serial_buffer, serial_buffer_pos);
        // Handle as a stream of commands
        if(res == SERWS_FRAME_PROCESSED){
          // It's been successfully processed as a line
          cmdProcessor.processMsg(serial_buffer);
          serial_buffer_pos = 0;
        }else if(res == SERWS_HEADERS_PROCESSED || res == SERWS_FRAME_ERROR || res == SERWS_FRAME_EMPTY){
          serial_buffer_pos = 0;
        }
      }else{
        // Handle as a stream of commands
        if((incomingByte == '\r' || incomingByte == '\n') && serial_buffer_pos && cmdProcessor.processMsg(serial_buffer)){
          // It's been successfully processed as a line
          serial_buffer_pos = 0;
        }else{
          // Not a line to process so store for processing
          serial_buffer[serial_buffer_pos++] = incomingByte;
          if(serial_buffer_pos == SERIAL_BUFFER_LENGTH) serial_buffer_pos = 0;
          serial_buffer[serial_buffer_pos] = 0;
        }
      }
    }
  }else{
    //reset the input buffer if nothing is received for 1/2 second to avoid things getting messed up
    if(millis() - last_char >= 500){
      serial_buffer_pos = 0;
    }
  }
}

void Mirobot::checkReady(){
  if(cmdProcessor.in_process && ready()){
    cmdProcessor.sendComplete();
  }
}

void Mirobot::loop(){
  ledHandler();
  servoHandler();
  autoHandler();
  calibrateHandler();
  sensorNotifier();
  serialHandler();
  checkReady();
}
