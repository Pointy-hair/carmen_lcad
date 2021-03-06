'File: TestGPS.spin
CON
  _clkmode = xtal1 + pll16x
  _xinfreq = 5_000_000
  charDegree = $B0

OBJ
  GPS: "GPS"
  Terminal: "FullDuplexSerial"
VAR
  'byte Hour
  'byte Minute
  'byte Second
  byte Time[50]

PUB Start
  Terminal.start (31, 30, 0, 115200)
  Terminal.str (string ("Starting GPS "))
	SendString  (13) 'New Line
	GPS.Start(13,12) 'rx - tx pins
	Terminal.str (string ("Waiting 42sec for GPS cold start "))
	SendString  (13) 'New Line
  'waitcnt (clkfreq*42 + cnt) 'delay 42s
  'Cold start need 42s, Warm 38s, Hot 8s
  waitcnt (clkfreq*2 + cnt) 'delay 2s
  'Hour   := GPS.GetHour
  'Minute := GPS.GetMin
  'Second := GPS.GetSec
  repeat
                                'Terminal.tx (0)
                                '---Time---
                                'Terminal.str (string ("Turn on time (GMT): "))
                                'Terminal.str (GPS.GetTime)
                                'SendFloat2  (Hour)
                                'SendString  (":")
                                'SendFloat2  (Minute)
                                'SendString  (":")
                                'SendFloat2  (Second)
                                'SendString  (13) 'New Line
    Terminal.str (string ("Current Time (GMT): "))
    'GPS.InvalidateData
    Terminal.str (GPS.GetTime)
                                'SendFloat2  (GPS.GetHour)
                                'SendString  (":")
                                'SendFloat2  (GPS.GetMin)
                                'SendString  (":")
                                'SendFloat2  (GPS.GetSec)
                                'SendString  (".")
                                'SendFloat3  (GPS.GetMiliSec)
    SendString  (13) 'New Line
    '---Position X-Y---
    Terminal.str (string ("Current Position  : "))
    Terminal.str (GPS.GetLatitude)
                                'SendDecimal (GPS.GetLatitudeDD) 'degree
                                'SendString  (charDegree)
                                'SendFloat2 (GPS.GetLatitudeMM)  'minute
                                'SendString  (".")
                                'SendFloat4  (GPS.GetLatitudeMMMM)'1/10000 minute
                                'SendString  (GPS.GetLatitudeNS)
                                'SendDecimal (GPS.GetLatitudeNS)
    SendString  (" ")
    Terminal.str (GPS.GetLongitude)
                                'SendDecimal (GPS.GetLongitudeDD)'degree
                                'SendString  (charDegree)
                                'SendFloat2  (GPS.GetLongitudeMM) 'minute
                                'SendString  (".")                                              
                                'SendFloat4  (GPS.GetLongitudeMMMM)'1/10000 minute
                                'SendString  (GPS.GetLongitudeEW)
    SendString  (13) 'New Line
    '---Satelite Number---
    
    Terminal.str(string ("Connected Satellite:"))
    Terminal.str(GPS.GetSatelliteNumber)
    SendString  (13) 'New Line

    '---Precision Rating---
    'From Wikipedia: http://en.wikipedia.org/wiki/Dilution_of_precision_(GPS)
    ' DOP   │ Rating   │ Description
    ' Value │          │
    ' 1     │ Ideal    │ This is the highest possible confidence level to be used for applications demanding the highest possible precision at all times.
    ' 1-2   │ Excellent│ At this confidence level, positional measurements are considered accurate enough to meet all but the most sensitive applications.
    ' 2-5   │ Good     │ Represents a level that marks the minimum appropriate for making business decisions. Positional measurements could be used to make reliable in-route navigation suggestions to the user.
    ' 5-10  │ Moderate │ Positional measurements could be used for calculations, but the fix quality could still be improved. A more open view of the sky is recommended.
    ' 10-20 │ Fair     │ Represents a low confidence level. Positional measurements should be discarded or used only to indicate a very rough estimate of the current location.
    ' >20   │ Poor     │ At this level, measurements are inaccurate by as much as 300 meters with a 6 meter accurate device (50 DOP × 6 meters) and should be discarded.
    Terminal.str (string ("Precision Status  : "))
                                'PrecisionRating(GPS.GetPrecisionDD, GPS.GetPrecisionM)
    Terminal.str (GPS.GetPrecision)
    SendString  (13) 'New Line
    '---Altitude---
        'Note for altitude: the readings changes alot whenever you move few centis
        '                   it is not realible, consider abandon this                      
        'Terminal.str (string ("Altitude (Mean Sea Level)  : "))
        'SendSign    (GPS.GetAltitudeMSLSign)
        'SendDecimal (GPS.GetAltitudeMSLDD)
        'SendString  (".")
        'SendDecimal (GPS.GetAltitudeMSLM)
        'SendString  (" ")
        'SendString  (GPS.GetAltitudeMSLUnit)
        'SendString  (13) 'New Line
        'Terminal.str (string ("Altitude (From Land Level) : "))
        'SendSign    (GPS.GetAltitudeSign)
        'SendDecimal (GPS.GetAltitudeDD)
        'SendString  (".")
        'SendDecimal (GPS.GetAltitudeM)
        'SendString  (" ")
        'SendString  (GPS.GetAltitudeUnit)
        'SendString  (13) 'New Line
    '---Speed & Direction--
    Terminal.str(string ("Speed and direction:"))
    SendDecimal (GPS.GetSpeedDD)  ' in knots, 1 knot = 1.85200 kph = 1.15077945 mph ***from google
    SendString  (".")
    SendDecimal (GPS.GetSpeedMM)
    Terminal.str(string (" knots "))
    SendDecimal (GPS.GetDirectionDD)
    SendString  (".")
    SendDecimal (GPS.GetDirectionMM)
    SendString  (charDegree)
    SendString  (13) 'New Line
    waitcnt (clkfreq + cnt) 'delay 1s
  'GPS.Stop

PRI SendDecimal (DataIn)
  if DataIn == -1 'The data is set to -1 if invalid
    Terminal.str (string ("Invalid"))
  else
    Terminal.dec (DataIn)

PRI SendString (DataIn)
  if DataIn == -1 'The data is set to -1 if invalid
    Terminal.str (string ("Invalid"))
  else
    Terminal.tx (DataIn)

{PRI SendFloat2 (DataIn) 'Send hundreth of decimal
  if DataIn == -1
    Terminal.str (string ("Invalid"))
  elseif DataIn > 99 'overload error
    Terminal.str (string ("##"))
  elseif DataIn > 9  '0.10 ~ 0.99
    Terminal.dec (DataIn)
  elseif DataIn > 0  '0.01 ~ 0.09
    Terminal.str (string ("0"))
    Terminal.dec (DataIn)
  else 'Others, just send all 0s
    Terminal.str (string ("00"))

PRI SendFloat3 (DataIn) 'Send thousandth of decimal
  if DataIn == -1
    Terminal.str (string ("Invalid"))
  elseif DataIn > 999 'overload error
    Terminal.str (string ("###"))
  elseif DataIn > 99  '0.100 ~ 0.999
    Terminal.dec (DataIn)
  elseif DataIn > 9  '0.010 ~ 0.099
    Terminal.str (string ("0"))
    Terminal.dec (DataIn)
  elseif DataIn > 0  '0.001 ~ 0.009
    Terminal.str (string ("00"))
    Terminal.dec (DataIn)
  else 'Others, just send all 0s
    Terminal.str (string ("000"))

PRI SendFloat4 (DataIn) 'Send ten thousandth of decimal
  if DataIn == -1
    Terminal.str (string ("Invalid"))
  elseif DataIn > 9999 'overload error
    Terminal.str (string ("####"))
  elseif DataIn > 999 '0.1000 ~ 0.9999
    Terminal.dec (DataIn)
  elseif DataIn > 99  '0.0100 ~ 0.0999
    Terminal.str (string ("0"))
    Terminal.dec (DataIn)
  elseif DataIn > 9  '0.0010 ~ 0.0099
    Terminal.str (string ("00"))
    Terminal.dec (DataIn)
  elseif DataIn > 0  '0.0001 ~ 0.0009
    Terminal.str (string ("000"))
    Terminal.dec (DataIn)
  else 'Others, just send all 0s
    Terminal.str (string ("0000"))
}
PRI SendSign (DataIn)
  if DataIn == 0
    Terminal.tx (" ")
  elseif DataIn == 1
    Terminal.tx ("-")
  else
    Terminal.str (string ("Invalid"))
  
PRI PrecisionRating (DD,M)
  case DD
    -1,0:
        Terminal.str (string ("Invalid"))
    1:
      if M == 0
        Terminal.str (string ("Ideal"))
      else
        Terminal.str (string ("Excellent"))
    2,3,4:
        Terminal.str (string ("Good"))
    5,6,7,8,9:
        Terminal.str (string ("Moderate"))
    10,11,12,13,14,15,16,17,18,19:
        Terminal.str (string ("Fair"))
    20:
      if M == 0
        Terminal.str (string ("Fair"))
      else
        Terminal.str (string ("Poor"))
    other:
        Terminal.str (string ("Poor"))
    
  
  

'TODO: fix the naming
'      float --> fixed numbering
'      float2 means it will always transmit 2 digits
'TODO: the data are all integer-based for easy transmitting thru normal FullDuplexSerial
'      change to float-based
