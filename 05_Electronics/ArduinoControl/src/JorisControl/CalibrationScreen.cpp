/*
 * CalibrationScreen.cpp
 * Class implementation of the calibration screen.
 */

/*
Copyright (c) 2020, Joris Robijn
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following condition is met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <Arduino.h>
#include "CalibrationScreen.h"
#include "MenuScreen.h"

CalibrationScreen::CalibrationScreen()
:
  _step( CSM_ASK_CONTINUE )
{}

void CalibrationScreen::process()
{
  Key pressedKey = keySc.getKey();
  char rotMove = rotEnc.getIncrPos();
  CalibrationScreenStep currStep = _step;
  
  switch( _step ) {
    case CSM_ASK_CONTINUE:
      if( _prevStep != _step ) { // First time in this case
        _subStep = 0;
      }
      switch( pressedKey ) {
        case KEY_3:
          _step = (CalibrationScreenStep) ( (byte)_step + 1 );
          break;
        case KEY_0:
        case KEY_4:
          switchScreen( mainScreen );
          break;
      }
      break;

    case CSM_PARK_MOTOR:
      if( _prevStep != _step ) { // First time in this case
        VmotOverrule = 4; // Let motor run at fixed speed
        _subStep = 0;
      }
      switch( _subStep ) {
        case 0: // Waiting until park input not active
          if( measValues[M_Park] == 0 ) {
            _subStep++;
          }
          break;
        case 1: // Waiting until park input active
          if( measValues[M_Park] == 1 ) {
            // Park is high, stop motor
            VmotOverrule = 0; // Stop the motor
            _step = (CalibrationScreenStep) ( (byte)_step + 1 );
          }
      }
      switch( pressedKey ) {
        case KEY_0:
          _step = CSM_CANCELLED;
          break;
      }

    case CSM_PRESSURE_OFFSETS:
      switch( pressedKey ) {
        case KEY_3:
          settings[S_pOffset] += measValues[M_p];
          settings[S_pQoffset] += measValues[M_pQ];
          _step = (CalibrationScreenStep) ( (byte)_step + 1 );
          break;
        case KEY_0:
          _step = CSM_CANCELLED;
          break;
      }
      break;

    case CSM_MOTOR_PREPARE:
      switch( pressedKey ) {
        case KEY_3:
          _step = (CalibrationScreenStep) ( (byte)_step + 1 );
          break;
        case KEY_0:
          _step = CSM_CANCELLED;
          break;
      }
      break;

    case CSM_VOLTAGE:
      if( _prevStep != _step ) { // First time in this case
        _editValue = measValues[M_Vsup];
        _subStep = 0;
      }
      switch( pressedKey ) {
        case KEY_3:
          if( _subStep > 0 ) { // Only execute if the value was edited
            // Calculate value
            float Vin = measValues[M_Vsup] / settings[S_VsupFactor];
            settings[S_VsupFactor] = _editValue / Vin;
          }
          _step = (CalibrationScreenStep) ( (byte)_step + 1 );
          break;
        case KEY_0:
          _step = CSM_CANCELLED;
          break;
        default:
          // process rotary encoder
          if( rotMove != 0 ) _subStep = 1;
          _editValue += 0.1 * rotMove;
          _editValue = coerce_float( _editValue, 8, 16 );
          break;
      }
      break;

    case CSM_MOTOR_NORMAL:
      if( _prevStep != _step ) { // First time in this case
        VmotOverrule = 4; // Start the motor
        _subStep = 0;
      }
      switch( _subStep ) {
        case 0: // Before measuring, waiting until park input not active
          if( measValues[M_Park] == 0 ) {
            _subStep++;
          }
          break;
        case 1: // Before measuring, waiting until park input active
          if( measValues[M_Park] == 1 ) {
            // Park switch is now active, start measurement!
            _Iavg_normal = 0;
            _avg_counter = 0;
            _subStep = 2;
          }
          break;
        case 2: // Measuring and waiting until park input not active
          _Iavg_normal += measValues[M_Imot];
          _avg_counter ++;
          if( measValues[M_Park] == 0 ) {
            // Park switch not active anymore
            _subStep++;
          }
          break;
        case 3: // Measuring and waiting until park input active
          _Iavg_normal += measValues[M_Imot];
          _avg_counter ++;
          if( measValues[M_Park] == 1 ) {

            // Park switch active, cycle complete!
            _Iavg_normal /= _avg_counter;
            _t_normal = measValues[M_tCycl];
            Serial.print( " _Iavg_normal = " );
            Serial.print( _Iavg_normal );
            Serial.print( " _t_normal = " );
            Serial.print( _t_normal );
            Serial.println();

            // Check if conditions are met
            if( _t_normal > 4 ) {
              // Rotating too slowly, increase speed
              VmotOverrule *= 1.1;
              _Iavg_normal = 0;
              _avg_counter = 0;
              _subStep = 2;
            } else if( _t_normal < 3.3 ) {
              // Rotating too fast, decrease speed
              VmotOverrule *= 0.9;
              _Iavg_normal = 0;
              _avg_counter = 0;
              _subStep = 2;
            } else {
              // Measurement successful
              _step = (CalibrationScreenStep) ( (byte)_step + 1 );
            }
          }
          break;
      }
      switch( pressedKey ) {
        case KEY_0:
          _step = CSM_CANCELLED;
          break;
      }
      break;

    case CSM_MOTOR_SLOWDOWN:
      if( _prevStep != _step ) { // First time in this case
        _subStep = 0;
      }
      switch( _subStep ) {
        case 0: // Before measuring, waiting until park input not active
          if( measValues[M_Park] == 0 ) {
            _subStep++;
          }
          break;
        case 1: // Before measuring, waiting until park input active
          if( measValues[M_Park] == 1 ) {
            // Park switch is now active, start measurement!
            _Iavg_slowdown = 0;
            _avg_counter = 0;
            _subStep = 2;
          }
          break;
        case 2: // Measuring and waiting until park input not active
          _Iavg_slowdown += measValues[M_Imot];
          _avg_counter ++;
          if( measValues[M_Park] == 0 ) {
            // Park switch not active anymore
            _subStep++;
          }
          break;
        case 3: // Measuring and waiting until park input active
          _Iavg_slowdown += measValues[M_Imot];
          _avg_counter ++;
          if( measValues[M_Park] == 1 ) {

            // Park switch active, cycle complete!
            _Iavg_slowdown /= _avg_counter;
            _t_slowdown = measValues[M_tCycl];

            Serial.print( " _Iavg_slowdown = " );
            Serial.print( _Iavg_slowdown );
            Serial.print( " _t_slowdown = " );
            Serial.print( _t_slowdown );
            Serial.println();

            // Check if conditions are met
            if( _Iavg_slowdown < _Iavg_normal * 2 ) {
              // Current not sufficient for measurement
              _Iavg_slowdown = 0;
              _avg_counter = 0;
              _subStep = 2;
            } else if( _Iavg_slowdown > _Iavg_normal * 4 ) {
              // Too much current for measurement
              _Iavg_slowdown = 0;
              _avg_counter = 0;
              _subStep = 2;
            } else {
              // Measurement successful
              // Calculate motor constants Ri and Kv
              float Vmot = VmotOverrule;
              settings[S_Ri] = (_t_slowdown * Vmot - _t_normal * Vmot) / (_Iavg_slowdown * _t_slowdown - _Iavg_normal * _t_normal);
              settings[S_Kv] = 60 / (_t_normal * ( Vmot - _Iavg_normal * settings[S_Ri] ));

              _step = CSM_COMPLETED;
              VmotOverrule = 0; // Stop the motor
            }
          }
          break;
      }
      switch( pressedKey ) {
        case KEY_0:
          _step = CSM_CANCELLED;
          break;
      }
      break;

    case CSM_COMPLETED:
    case CSM_CANCELLED:
      if( _subStep == 0 ) {
        VmotOverrule = 0; // Stop the motor
        _subStep ++;
      }
      switch( pressedKey ) {
        case KEY_0:
        case KEY_3:
        case KEY_4:
        case KEY_ENTER:
          VmotOverrule = NAN; // Motor to normal operation
          switchScreen( mainScreen );
          break;
        case KEY_1:
        case KEY_2:
          VmotOverrule = NAN; // Motor to normal operation
          menuScreen->switchMenu( &calibrationMenu );
          switchScreen( menuScreen );
          break;
      }
      break;
  }
  _prevStep = currStep; // Reason for the currStep variable is that the code above mighgt have changed _step variable
}

void CalibrationScreen::onEnter()
{
  Serial.println( F("CalibrationScreen::onEnter()") );
  lcd.clear();
  _step = CSM_ASK_CONTINUE;
  _prevStep = CSM_COMPLETED; // Force redraw
}

void CalibrationScreen::onLeave()
{
  Serial.println( F("CalibrationScreen::onLeave()") );
}

void CalibrationScreen::draw()
{
  char buf[21];
  int num_rows;
  long start_pos;
  enum : byte { None=0, Cancel=0x01, OK=0x02, OK_Cancel=0x03 } showButtons = None;

  switch( _step ) {
    case CSM_ASK_CONTINUE:
      if( _prevStep != _step ) {
        lcd.clear();
        lcd.printxy( 0, 0, F("CALIBRATION") );
        lcd.printxy( 0, 2, F("Stop the") );
        lcd.printxy( 0, 3, F("machine now?") );
        showButtons = OK_Cancel;
      }
      break;

    case CSM_PARK_MOTOR:
      if( _prevStep != _step ) {
        lcd.clear();
        lcd.printxy( 0, 0, F("CALIBRATION") );
        lcd.printxy( 0, 2, F("Parking...") );
        showButtons = Cancel;
      }
      break;

    case CSM_PRESSURE_OFFSETS:
      if( _prevStep != _step ) {
        lcd.clear();
        lcd.printxy( 0, 0, F("Disconnect") );
        lcd.printxy( 0, 1, F("pressure tubes") );
        lcd.printxy( 0, 2, F("and press OK.") );
        showButtons = OK_Cancel;
      }
      break;

    case CSM_VOLTAGE:
      if( _prevStep != _step ) {
        lcd.clear();
        lcd.printxy( 0, 0, F("Measure") );
        lcd.printxy( 0, 1, F("supply voltage") );
        lcd.printxy( 0, 2, F("and set here:") );
        lcd.blink();
        showButtons = OK_Cancel;
      }
      format_float( buf, _editValue, 4, 1, true, true );
      lcd.printxy( 16, 2, buf );
      lcd.setCursor( 19, 2 ); // Force placing cursor
      break;

    case CSM_MOTOR_PREPARE:
      if( _prevStep != _step ) {
        lcd.noBlink();
        lcd.clear();
        lcd.printxy( 0, 0, F("Disconnect") );
        lcd.printxy( 0, 1, F("motor crank") );
        lcd.printxy( 0, 2, F("and press OK.") );
        lcd.printxy( 0, 3, F("Motor will turn!") );
        showButtons = OK_Cancel;
      }
      break;

    case CSM_MOTOR_NORMAL:
      if( _prevStep != _step ) {
        lcd.clear();
        lcd.printxy( 0, 0, F("Measuring") );
        lcd.printxy( 0, 2, F("...") );
        showButtons = Cancel;
      }
      break;
      
    case CSM_MOTOR_SLOWDOWN:
      if( _prevStep != _step ) {
        lcd.clear();
        lcd.printxy( 3, 0, F("Slow down") );
        lcd.printxy( 1, 1, F("\x7F the motor") );
        lcd.printxy( 1, 2, F("\x7F by hand.") );
        showButtons = Cancel;
      }

      // Draw a graph of motor current
      vgraph.draw( measValues[M_Imot], -_Iavg_normal, 7 * _Iavg_normal, 0, 3, 0 ); // Arrows will be between 2 and 4 * _Iavg_normal motor current

      if( measValues[M_Imot] < 2 * _Iavg_normal ) {
        lcd.printxy( 3, 3, F("Use more force") );
      } else if( measValues[M_Imot] > 4 * _Iavg_normal ) {
        lcd.printxy( 3, 3, F("Use less force") );
      } else {
        lcd.printxy( 3, 3, F("Measuring...  ") );
      }
      break;

    case CSM_COMPLETED:
    case CSM_CANCELLED:
      if( _prevStep != _step ) {
        lcd.clear();
        lcd.printxy( 0, 0, F("Calibration") );
        if( _step == CSM_COMPLETED ) {
          lcd.printxy( 0, 1, F("complete.       info") );
        } else {
          lcd.printxy( 0, 1, F("cancelled.") );
        }
        lcd.printxy( 0, 2, F("Reconnect crank and") );
        lcd.printxy( 0, 3, F("pressure tubes.") );
        showButtons = OK;
      }
      break;

  }
  if( showButtons & Cancel ) {
        lcd.printxy( 14, 0, F("Cancel") );
  }
  if( showButtons & OK ) {
        lcd.printxy( 18, 3, F("OK") );
  }
}