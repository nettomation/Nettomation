/* Copyright (c) 2018 Jiri Ocenasek, http://nettomation.com
 *
 * This file is part of Nettomation.
 *
 * Nettomation is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * For tutorials, donations and proprietary licensing options
 * please visit http://nettomation.com
 */

#ifndef USERPAGES_H_
#define USERPAGES_H_

#include "engine/webcontent.h"
#include <deque>
#include <map>

#define VALVETIME 6000
#define WATERTIME 10000

struct CallbackRecord {
  string callbackName;
  int    index;
  int    destinationIndex;
  string value;
};

class BrewingStep;

class BrewingStatus : public WebContent{
 private:
  string _status;

 public:
 BrewingStatus(Dispatcher* dispatcher, const string& name): WebContent(dispatcher,name) { }

  virtual void render( ostream& stream ) { stream << _status; }
  //  virtual void callback(const string& callbackName, int index1, int index2, const string& value, int session);
  //  virtual void controlLoop();

  void setStatus( const string& status ) { _status = status; invalidate(); } 
};

class BrewingPage : public WebContent{
 private:
  vector<BrewingStep*>   _steps;
  int                    _activeStep;
  deque<CallbackRecord>  _actions;
  bool                   _recipeSaved;
  bool                   _manualMode; // frozen action never finishes

 public:
 BrewingPage(Dispatcher* dispatcher, const string& name): WebContent(dispatcher,name) { _activeStep = -1; _recipeSaved = false; _manualMode = false; }
  virtual ~BrewingPage();

  virtual void render( ostream& stream );
  virtual void callback(const string& callbackName, int index1, int index2, const string& value, int session);
  virtual void controlLoop();
  virtual void drop(const string& sourceName, int sourceIndex1, int sourceIndex2, 
		    const string& destinationName, int destinationIndex1, int destinationIndex2, 
		    int session);
  
  bool manualMode() const { return _manualMode; } 
};

class BrewingStep {
 protected:
  BrewingPage* _parent;
  string       _name;

  // tho following two entries are used to import/export the recipe
  string           _loaderName;
  map<int,string>  _parameters;
  
  // setting parameters has to be done via callbackParameter such that they are registered in _parameters
  virtual void setParameter(int index, const string& value) = 0;
  
 public:
  BrewingStep( BrewingPage* parent, const string& name, const string& loaderName ) { _parent = parent; _name = name; _loaderName = loaderName; }
  virtual void renderDescription( ostream& stream ) = 0; // can call _parent->generateCallback(_name,index);

  virtual void start() = 0;
  virtual bool step( string& status ) = 0;
  virtual void finish() = 0;

  const string& name() { return _name; }

  void callbackParameter(int index, const string& value) { _parameters[index] = value; setParameter(index,value); }
  void exportStep( ostream& stream );
};

class MashingStep : public BrewingStep {
 private:
  long   _durationMinutes;
  double _targetTemp;
  long   _accomplishedMiliseconds;
  long   _lastMs;

  virtual void setParameter(int index, const string& value);
public:
  MashingStep(BrewingPage* parent, const string& name, const string& loaderName );
  virtual void renderDescription( ostream& stream );

  virtual void start();
  virtual bool step( string& status );
  virtual void finish();
};

class BoilingStep : public BrewingStep {
 private:
  long   _durationMinutes;
  long   _accomplishedMiliseconds;
  long   _lastMs;
  double _boilingPowerPercentage;
  long   _stepCounter;

  virtual void setParameter(int index, const string& value);
public:
  BoilingStep(BrewingPage* parent, const string& name, const string& loaderName );
  virtual void renderDescription( ostream& stream );

  virtual void start();
  virtual bool step( string& status );
  virtual void finish();
};

class WaitingStep : public BrewingStep {
 private:
  bool _running;
  long _elapsedMiliseconds;
  long _lastMs;

  virtual void setParameter(int index, const string& value);
 public:
  WaitingStep(BrewingPage* parent, const string& name, const string& loaderName );
  virtual void renderDescription( ostream& stream );

  virtual void start();
  virtual bool step( string& status );
  virtual void finish();
};

class FillMashTunStep : public BrewingStep {
 private:
  double _targetVolume;

  virtual void setParameter(int index, const string& value);
public:
 FillMashTunStep(BrewingPage* parent, const string& name, const string& loaderName ) : BrewingStep(parent,name,loaderName) { _targetVolume = 35.0; }
  virtual void renderDescription( ostream& stream );

  virtual void start();
  virtual bool step( string& status );
  virtual void finish();
};

class FillHLTStep : public BrewingStep {
 private:
  double _targetVolume;
  double _targetTemp;

  virtual void setParameter(int index, const string& value);
 public:
 FillHLTStep(BrewingPage* parent, const string& name, const string& loaderName ) : BrewingStep(parent,name,loaderName) { _targetVolume = 50.0; _targetTemp = 78.0; }
  virtual void renderDescription( ostream& stream );

  virtual void start();
  virtual bool step( string& status );
  virtual void finish();
};

class RestingStep : public BrewingStep {
 private:
  long   _durationMinutes;
  long   _accomplishedMiliseconds;
  long   _lastMs;

  virtual void setParameter(int index, const string& value);
public:
  RestingStep(BrewingPage* parent, const string& name, const string& loaderName );
  virtual void renderDescription( ostream& stream );

  virtual void start();
  virtual bool step( string& status );
  virtual void finish();
};

class WhirlpoolingStep : public BrewingStep {
 private:
  long   _durationMinutes;
  long   _accomplishedMiliseconds;
  long   _lastMs;
  bool   _withCooling;

  virtual void setParameter(int index, const string& value);
public:
  WhirlpoolingStep(BrewingPage* parent, const string& name, const string& loaderName );
  virtual void renderDescription( ostream& stream );

  virtual void start();
  virtual bool step( string& status );
  virtual void finish();
};

class ChillingStep : public BrewingStep {
 private:
  double _valveOpenPercentage;

  virtual void setParameter(int index, const string& value);
 public:
 ChillingStep(BrewingPage* parent, const string& name, const string& loaderName ) : BrewingStep(parent,name,loaderName) { _valveOpenPercentage = 50.0; }
  virtual void renderDescription( ostream& stream );

  virtual void start();
  virtual bool step( string& status );
  virtual void finish();
};

class DrainingStep : public BrewingStep {
 private:
  long   _durationSeconds;
  double _valveOpenPercentage;
  long   _accomplishedMiliseconds;
  long   _lastMs;
  bool   _withStirring;

  virtual void setParameter(int index, const string& value);
public:
 DrainingStep(BrewingPage* parent, const string& name, const string& loaderName ) : BrewingStep(parent,name,loaderName) { _durationSeconds = 900; _valveOpenPercentage = 50.0; _withStirring = false; }
  virtual void renderDescription( ostream& stream );

  virtual void start();
  virtual bool step( string& status );
  virtual void finish();
};

class PumpTheWortStep : public BrewingStep {
 private:
  double _previousVolume;
  long   _lastVolumeSampledMs;
  virtual void setParameter(int index, const string& value) { }
public:
 PumpTheWortStep(BrewingPage* parent, const string& name, const string& loaderName ) : BrewingStep(parent,name,loaderName) { }
  virtual void renderDescription( ostream& stream );

  virtual void start();
  virtual bool step( string& status );
  virtual void finish();
};

class ReturnTheWortStep : public BrewingStep {
 private:
  double _previousVolume;
  long   _lastVolumeSampledMs;
  virtual void setParameter(int index, const string& value) { }
public:
 ReturnTheWortStep(BrewingPage* parent, const string& name, const string& loaderName ) : BrewingStep(parent,name,loaderName) { }
  virtual void renderDescription( ostream& stream );

  virtual void start();
  virtual bool step( string& status );
  virtual void finish();
};

class SpargingStep : public BrewingStep {
 private:
  bool   _spargeAll;
  double _targetVolume;

  double _initialVolume;
  int    _step; // this is a state variable, reset when start() is called

  double _previousVolume;
  long   _lastVolumeSampledMs;
  bool   _wasHLTActive;

  virtual void setParameter(int index, const string& value);
public:
 SpargingStep(BrewingPage* parent, const string& name, const string& loaderName ) : BrewingStep(parent,name,loaderName) { _targetVolume = 20.0; _spargeAll = false; }
  virtual void renderDescription( ostream& stream );

  virtual void start();
  virtual bool step( string& status );
  virtual void finish();
};

class FermentingPage : public WebContent{
 private:
  double _targetTemp;
  double _currentTemp;

  deque<double> _history;
  deque<time_t> _time;

 public:
 FermentingPage(Dispatcher* dispatcher, const string& name): WebContent(dispatcher,name) { _targetTemp = 10.0; }

  virtual void render( ostream& stream );
  virtual void callback(const string& callbackName, int index1, int index2, const string& value, int session);
  virtual void controlLoop();
};

class HLTController : public WebContent{
 private:
  double _targetTemp;
  bool   _active;

 public:
 HLTController(Dispatcher* dispatcher, const string& name): WebContent(dispatcher,name) { _active = false; _targetTemp = 78.0; }

  virtual void render( ostream& stream );
  virtual void controlLoop();

  void activate();
  void deactivate();
  void setTargetTemperature(double temp) { _targetTemp = temp; _active = true; }
  bool isTargetTemperatureReached();
  bool isActive() { return _active; }
};

class ValveController : public WebContent{
  typedef void(*ValveOperation)();
 private:
  deque<pair<ValveOperation,long> > _valveActions;
  bool                              _operationPending;
  long                              _lastMs;

  void unplugBistableValves();

 public:
 ValveController(Dispatcher* dispatcher, const string& name): WebContent(dispatcher,name) { _operationPending = false; _lastMs = 0; }

  virtual void controlLoop();


  static void feedWaterToMashTun();
  static void feedWaterToBoiler();
  static void feedSpargeToMashTun();
  static void preChilling();
  static void preDraining();
  static void startChilling();
  static void startDraining();
  static void feedWortToBoiler();
  static void startWhirlpooling();
  static void startWhirlpoolingWithCooling();
  static void startReturningWortToMashTun();
  static void closeValves();
  
  bool areValvesStabilized();
  bool isWaterLevelStabilized();
  
  void addAction(ValveOperation op, long durationMs = VALVETIME);
};

class PowerMeter : public WebContent{
 private:
  static double _power;
 public:
 PowerMeter(Dispatcher* dispatcher, const string& name): WebContent(dispatcher,name) { }

  virtual void controlLoop();
  virtual void render( ostream& stream );
  static  void isrHandler();
};

class TiltReader : public WebContent{
 private:
  double _temp;
  double _sg;
 public:
 TiltReader(Dispatcher* dispatcher, const string& name): WebContent(dispatcher,name) { }

  virtual void controlLoop();
  virtual void render( ostream& stream );
  double temp() { return _temp; }
  double gravity() { return _sg; }
};

class VolumeReader : public WebContent{
 private:
  double _volumeMashTun;
  double _voltageMashTun;
  double _volumeBoiler;
  double _voltageBoiler;
 public:
 VolumeReader(Dispatcher* dispatcher, const string& name): WebContent(dispatcher,name) { }

  virtual void controlLoop();
  virtual void render( ostream& stream );
  double volumeMashTun() { return _volumeMashTun; }
  double volumeBoiler() { return _volumeBoiler; }
};

class TemperatureReader : public WebContent{
 private:
  double _mashingTemperature;
  double _boilerTemperature;
  double _fermentingTemperature;
  double _auxTemperature;
 public:
 TemperatureReader(Dispatcher* dispatcher, const string& name): WebContent(dispatcher,name) { }

  virtual void controlLoop();
  virtual void render( ostream& stream );
  double getMashingTemperature() { return _mashingTemperature; }
  double getBoilerTemperature() { return _boilerTemperature; }
  double getFermentingTemperature() { return _fermentingTemperature; }
};

#endif
