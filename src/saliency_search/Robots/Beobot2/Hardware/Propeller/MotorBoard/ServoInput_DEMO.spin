{{
=========================================================
                    Servo Input Demo        
=========================================================

      ┌──────────────────────────────────────────┐
      │ Copyright (c) 2008 David C. Gregory      │               
      │     See end of file for terms of use.    │               
      └──────────────────────────────────────────┘

See ServoInput.spin for documentation.

}}
CON

  _clkmode = xtal1 + pll16x    'Run at the full 80MHz
  _xinfreq = 5_000_000         '
      
OBJ

  read          : "ServoInput"  
  num           : "Numbers"
  serial : "FullDuplexSerial"    
DAT
  
  pins        LONG 6, 5, 4, 3, 2, 1, 0
  pulseWidths LONG 1, 1, 1, 1, 1, 1, 1

PUB start | counter
    
  num.init
  serial.start(31, 30, 0,38400)
  read.start(@pins,7,@pulseWidths)
  serial.str(string("Start")) 
  repeat
    counter := cnt + 10_000_000  'Run our loop at ~8Hz (80e6/10e6)
    serial.str(string("CH 1 ["))     
    serial.dec(pulseWidths[0])          
    serial.str(string("] |"))
    serial.str(string("CH 2 ["))     
    serial.dec(pulseWidths[1])          
    serial.str(string("] |"))
    serial.str(string("CH 3 ["))     
    serial.dec(pulseWidths[2])          
    serial.str(string("] |"))
    serial.str(string("CH 4 ["))     
    serial.dec(pulseWidths[3])          
    serial.str(string("] |"))
    serial.str(string("CH5E ["))     
    serial.dec(pulseWidths[4])          
    serial.str(string("] |"))
    serial.str(string("CH6VR ["))     
    serial.dec(pulseWidths[5])          
    serial.str(string("] |"))
    serial.str(string("CH7B ["))     
    serial.dec(pulseWidths[6])          
    serial.str(string("] |"))                     
    serial.str(string(10,13))
       
  '    text.str(num.ToStr(pulseWidths[0],num#DEC))
   '   text.str(num.ToStr(pulseWidths[1],num#DEC))
    '  text.str(num.ToStr(pulseWidths[2],num#DEC))
     ' text.str(num.ToStr(pulseWidths[3],num#DEC))              
    '  text.out(13)
      
    waitcnt (counter)

{{
    ┌──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
    │                                                   TERMS OF USE: MIT License                                                  │                                                            
    ├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
    │Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation    │ 
    │files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,    │
    │modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software│
    │is furnished to do so, subject to the following conditions:                                                                   │
    │                                                                                                                              │
    │The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.│
    │                                                                                                                              │
    │THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE          │
    │WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR         │
    │COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,   │
    │ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                         │
    └──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
}}             