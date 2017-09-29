
#include "VRFakeTrackerDevice.h"
#include <math/VRMath.h>

namespace MinVR {

VRFakeTrackerDevice::VRFakeTrackerDevice(const std::string &trackerName,
                                         const std::string &toggleOnOffEventName,
                                         const std::string &rotateEventName,
                                         const std::string &rollEventName,
                                         const std::string &translateEventName,
                                         const std::string &translateZEventName,
                                         float xyScale,
                                         float zScale,
                                         float rotScale,
                                         bool sticky,
                                         VRVector3 startPos,
                                         VRVector3 startDir,
                                         VRVector3 startUp)
{
    _eventName = trackerName + "_Move";
    _toggleEvent = toggleOnOffEventName;
    _rotateOnEvent = rotateEventName + "_Down";
    _rotateOffEvent = rotateEventName + "_Up";
    _translateOnEvent = translateEventName + "_Down";
    _translateOffEvent = translateEventName + "_Up";
    _translateZOnEvent = translateZEventName + "_Down";
    _translateZOffEvent = translateZEventName + "_Up";
    _rollOnEvent = rollEventName + "_Down";
    _rollOffEvent = rollEventName + "_Up";
    _xyScale = xyScale;
    _zScale = zScale;
    _rScale = rotScale;
    _sticky = sticky;

    _tracking = false;
    _state = VRFakeTrackerDevice::XYTranslating;

    _statePos = startPos;

    VRVector3 forward = startDir.normalize();
    VRVector3 x = startUp.cross(forward).normalize();
    VRVector3 up = forward.cross(x);
    std::cout << "StartDir" << startDir << std::endl;
    std::cout << "StartUp" << startUp << std::endl;
    std::cout << "StartPos" << startPos << std::endl;
    std::cout << "forward" << forward << std::endl;
    std::cout << "x" << x  << std::endl;
    std::cout << "up" << up  << std::endl;

    _stateRot = VRMatrix4::fromRowMajorElements( x[0], up[0], forward[0], 0,
                                                 x[1], up[1], forward[1], 0,
                                                 x[2], up[2], forward[2], 0,
                                                 0, 0, 0, 1);
    _transform = VRMatrix4::translation(_statePos) * _stateRot;
    VRMatrix4 xform  = _transform;

    VRDataIndex di;
    di.addData(_eventName + "/Transform", xform);
    _pendingEvents.push(di.serialize(_eventName));

}
    


VRFakeTrackerDevice::~VRFakeTrackerDevice()
{
}


void VRFakeTrackerDevice::onVREvent(const MinVR::VREvent &event)
{
  if (event.getName() == _toggleEvent) {
    _tracking = !_tracking;
    if (_tracking) {
      if (_sticky) {
        _state = VRFakeTrackerDevice::XYTranslating;
      } else {
        _state = VRFakeTrackerDevice::None;
      }
    }
  }
  else if (event.getName() == _translateZOnEvent) {
    if (_state !=  VRFakeTrackerDevice::ZTranslating) {
      _state = VRFakeTrackerDevice::ZTranslating;
    } else {
      _state = VRFakeTrackerDevice::None;
    }
  }
  else if (event.getName() == _translateOnEvent) {
    if (_state != VRFakeTrackerDevice::XYTranslating) {
      _state = VRFakeTrackerDevice::XYTranslating;
    } else {
      _state = VRFakeTrackerDevice::None;
    }
  }
  else if (event.getName() == _rotateOnEvent) {
    if (_state != VRFakeTrackerDevice::Rotating) {
      _state = VRFakeTrackerDevice::Rotating;
    } else {
      _state = VRFakeTrackerDevice::None;
    }
  }
  else if (event.getName() == _rollOnEvent) {
    if (_state != VRFakeTrackerDevice::Rolling) {
      _state = VRFakeTrackerDevice::Rolling;
    } else {
      _state = VRFakeTrackerDevice::None;
    }
  }
  else if (event.getName() == _translateZOffEvent && !_sticky) {
    _state = VRFakeTrackerDevice::None;
  }
  else if (event.getName() == _translateOffEvent && !_sticky) {
    _state = VRFakeTrackerDevice::None;
  }
  else if (event.getName() == _rotateOffEvent && !_sticky) {
    _state = VRFakeTrackerDevice::None;
  }
  else if (event.getName() == _rollOffEvent && !_sticky) {
    _state = VRFakeTrackerDevice::None;
  }
  else if (event.getName() == "Mouse_Move") {
    const float *screenPos = event.getDataAsFloatArray("NormalizedPosition");
    if (screenPos != NULL) {

      // Transform range from [0,1] to [-1,1].
      float mousex = 2.0*(screenPos[0] - 0.5);
      float mousey = 2.0*((1.0-screenPos[1]) - 0.5);

      // If we're not currently tracking, ignore the mouse.
      if (_tracking) {
        float deltaX = mousex - _lastMouseX;
        float deltaY = mousey - _lastMouseY;
            
        if (_state == VRFakeTrackerDevice::ZTranslating) {
          _statePos =  VRVector3(0, 0, _zScale * deltaY) + _statePos;
        }
        else if (_state == VRFakeTrackerDevice::Rotating) {
          VRMatrix4 r = VRMatrix4::rotationY(_rScale * deltaX) * VRMatrix4::rotationX(-_rScale * deltaY);
          _stateRot = r * _stateRot;
        }
        else if (_state == VRFakeTrackerDevice::Rolling) {
          VRMatrix4 r = VRMatrix4::rotationZ(_rScale * deltaX);
          _stateRot = r * _stateRot;
        }
        else if (_state == VRFakeTrackerDevice::XYTranslating){
          _statePos =  _xyScale * VRVector3(deltaX, deltaY, 0) + _statePos;
        }
        
        _transform = VRMatrix4::translation(_statePos) * _stateRot;
        VRMatrix4 xform  = _transform;

        VRDataIndex di;
        di.addData(_eventName + "/Transform", xform);
        _pendingEvents.push(di.serialize(_eventName));
      }
            
      _lastMouseX = mousex;
      _lastMouseY = mousey;
    }
  }
}

  
void VRFakeTrackerDevice::appendNewInputEventsSinceLastCall(VRDataQueue *inputEvents)
{
  while (_pendingEvents.notEmpty()) {
    inputEvents->push(_pendingEvents.getSerializedObject());
    _pendingEvents.pop();
  }
}
  

VRInputDevice*
VRFakeTrackerDevice::create(VRMainInterface *vrMain, VRDataIndex *config, const std::string &nameSpace) {
    std::string devNameSpace = nameSpace;

    // Find and/or set all the default values.
    std::string trackerName = config->getValue("TrackerName", devNameSpace);
    std::string toggleEvent = config->getValue("ToggleOnOffEvent", devNameSpace);
    std::string rotateEvent = config->getValueWithDefault("RotateEvent",
                                                          std::string("Kbdr"),
                                                          devNameSpace);
    std::string translateEvent = config->getValueWithDefault("TranslateEvent",
                                                             std::string("Kbdw"),
                                                             devNameSpace);
    std::string translateZEvent = config->getValueWithDefault("TranslateZEvent",
                                                              std::string("Kbdz"),
                                                              devNameSpace);
    std::string rollEvent = config->getValueWithDefault("RollEvent",
                                                        std::string("Kbde"),
                                                        devNameSpace);
    float xyScale =
      config->getValueWithDefault("XYTranslationScale", 1.0f, devNameSpace);
    float zScale =
      config->getValueWithDefault("ZTranslationScale", 1.0f, devNameSpace);
    float rScale = config->getValueWithDefault("RotationScale",
                                               3.1415926f, devNameSpace);

    int sticky = config->getValueWithDefault("Sticky", 0, devNameSpace);

    int pos[] = {0, 0, -1};
    int dir[] = {0, 0, 1};
    int up[] = {0, 1, 0};
    VRFloatArray defaultPos (pos, pos + sizeof(pos) / sizeof(pos[0]));
    VRFloatArray defaultDir (dir, dir + sizeof(dir) / sizeof(dir[0]));
    VRFloatArray defaultUp (up, up + sizeof(up) / sizeof(up[0]));
    VRVector3 startPos = config->getValueWithDefault("StartingPosition",
                                                     defaultPos, devNameSpace);
    VRVector3 startDir = config->getValueWithDefault("StartingDirection",
                                                     defaultDir, devNameSpace);
    VRVector3 startUp = config->getValueWithDefault("StartingUp",
                                                    defaultUp, devNameSpace);

    // Make a new object.
    VRFakeTrackerDevice *dev = new VRFakeTrackerDevice(trackerName, toggleEvent, rotateEvent, rollEvent, translateEvent, translateZEvent, xyScale, zScale, rScale, sticky, startPos, startDir, startUp);
    vrMain->addEventHandler(dev);

    return dev;
}

  
} // end namespace


