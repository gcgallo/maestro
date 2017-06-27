# Sample code for both the RotaryEncoder class and the Switch class.
# The common pin for the encoder should be wired to ground.
# The sw_pin should be shorted to ground by the switch.

import wiringpi
#import serial
#import subprocess
import libtmux
import io
import time
import RPi.GPIO as GPIO
GPIO.setmode(GPIO.BCM)

server = libtmux.Server()
print(server.list_sessions())
session = server.get_by_id('$0')
window = session.attached_window
print(window.list_panes())
CAN1pane = window.get_by_id('%0')
MENUpane = window.get_by_id('%1')
CAN2pane = window.get_by_id('%2')

serial = wiringpi.serialOpen('/dev/serial0', 9600)

A_PIN  = 23
B_PIN  = 24
SW_PIN = 22

GPIO.setup(A_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)
GPIO.setup(B_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)
GPIO.setup(SW_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)
time_last_refresh = time.time() 
A_set = False
B_set = False
encoderPos = 0
prevPos = 0
sw_state = 0
last_state = None
sel = 2 
mod = 0
CAN1mode_sel = 0
rec_mode_sel = 0
RST="\033[0m"
SEL="\033[1;93;41m"
BLINK="\033[1;93;41;7m"

def refresh():

    print("\033[2J")
    print("\033[H")
    print(SEL if sel == 0 else RST, end="")
    print("    CAN1 Mode    ", end="")
    print(SEL if sel == 1 else RST, end="")
    print("   CAN1 Record   ", end="")
    print(SEL if sel == 2 else RST, end="")
    print("    Playback    ", end="")
    print(SEL if sel == 3 else RST, end="")
    print("   CAN2 Record   ", end="")
    print(SEL if sel == 4 else RST, end="")
    print("    CAN2 Mode    ", end="")
    #print(encoderPos, end=" ")
    #print(mod, end=" ")
    print(RST)
    if mod == 1 or mod == 0:
      for i in range(5):
        show = CAN1mode_sel + i

        if mod == 0:
          print(SEL if i == 0 else SEL, end="")
        else:
          print(BLINK if i == 0 else SEL, end="")
        #if i == 0:

        if show == 0:
          print("        OFF       ", end="")
        if show == 1:
          print("       SNIFF      ", end="")
        if show == 2:
          print("   FILTER INPUT  ", end=" ")
        if show == 3:
          print("   FILTER OUTPUT ", end=" ")
        if show == 4:
          print("      RECORD     ", end=" ")
        if show == 5:
          print("     PLAYBACK    ", end=" ")
        print(RST, end="")
        if mod == 0:
          break 

    if mod == 2 or mod == 0:
      for i in range(5):
        show = rec_mode_sel + i 

        if mod == 0:
          print(SEL if i == 1 else SEL, end="")
          show += 1
        else:
          print(BLINK if i == 1 else SEL, end="")
        #if i == 0:
        if show == 0:
          print(RST, end="")
          print("                 ", end="")
        if show == 1:
          print("     STOPPED     ", end="")
        if show == 2:
          print("      START      ", end="")
        if show == 3:
          print("      STOP       ", end="")
        if show == 4:
          print("      SAVED      ", end=" ")
        print(RST, end="")
        if mod == 0:
          break 


    print(RST)
    #print(ser.name)

def switch_callback(channel):
  global sw_state
  sw_state=1
  #print("normal switch press")

def doEncoderA(channel):
  global A_set
  global B_set
  global encoderPos
  if GPIO.input(A_PIN) != A_set :
    A_set = not A_set
    # adjust counter + if A leads B
    if A_set and not B_set : 
       encoderPos += 1;

def doEncoderB(channel):
  global A_set
  global B_set
  global encoderPos
  if GPIO.input(B_PIN) != B_set : 
    B_set = not B_set
    # adjust counter - 1 if B leads A
    if B_set and not A_set : 
      encoderPos -= 1

def displayMenu(updown):
    global sel
    global CAN1mode_sel
    global rec_mode_sel
    if mod == 0:
        if updown == 1:
          sel = (0 if sel <= 0  else sel - 1)   

        elif updown ==  0:
          sel = (4 if sel >= 4  else sel + 1)   

    elif mod == 1:
        if updown == 1:
          CAN1mode_sel = (0 if CAN1mode_sel <= 0  else CAN1mode_sel - 1)   

        elif updown == 0:
          CAN1mode_sel = (5 if CAN1mode_sel >= 5  else CAN1mode_sel + 1)   

    elif mod == 2:
        if updown == 1:
          rec_mode_sel = (0 if rec_mode_sel <= 0  else rec_mode_sel - 1)   

        elif updown == 0:
          rec_mode_sel = (3 if rec_mode_sel >= 3  else rec_mode_sel + 1)   

    refresh()

def SENDmode():
  #ser.write(b'hello') 
  #wiringpi.serialPuts(serial,"hello")
  if mod == 1:
    wiringpi.serialPuts(serial, "<1")  # CAN1 mode

    if CAN1mode_sel == 0: 
      wiringpi.serialPuts(serial, "O>" ) # set to OFF

    elif CAN1mode_sel == 1:
      wiringpi.serialPuts(serial, "S>")  # set to SNIFF 
      CAN1pane.send_keys('\x01\x04', enter=False) #ctrl-A ctrl-D to detach 
      CAN1pane.send_keys('screen -S filter -X quit')
      time.sleep(1)
      CAN1pane.send_keys('screen -R sniffer')
      time.sleep(1)
      CAN1pane.send_keys('sudo ./can.sh -o 1')
      time.sleep(1)
      CAN1pane.send_keys('sudo ./can.sh -s 1')

    elif CAN1mode_sel == 2:  # set as INPUT to FILTER 
      wiringpi.serialPuts(serial, "I>")
      CAN1pane.send_keys('\x01\x04', enter=False) #ctrl-A ctrl-D to detach 
      time.sleep(1)
      CAN1pane.send_keys('screen -S sniffer -X quit')
      time.sleep(1)
      CAN1pane.send_keys('sudo ./can.sh -d 1')
      time.sleep(1)
      CAN1pane.send_keys('screen -R filter /dev/ttyACM1')

    elif CAN1mode_sel == 3:
      wiringpi.serialPuts(serial, "F>")  # set as OUTPUT of FILTER

    elif CAN1mode_sel == 4:
      wiringpi.serialPuts(serial, "R>")  # set to SNIFF for RECORDING

    elif CAN1mode_sel == 5:
      wiringpi.serialPuts(serial, "P>")  # set to SNIFF for PLAYBACK

GPIO.add_event_detect(SW_PIN, GPIO.RISING, callback=switch_callback, bouncetime = 30)  
GPIO.add_event_detect(A_PIN, GPIO.BOTH, callback=doEncoderA)
GPIO.add_event_detect(B_PIN, GPIO.BOTH, callback=doEncoderB)

while True:
    if sw_state == 1:

      SENDmode()
      #CAN1pane.select_pane()
      #CAN1pane.send_keys('c', enter=False)
      #subprocess.Popen('tmux kill-pane')
  
      if mod != 0:
        mod = 0
      else:
        mod = sel+1
      sw_state = 0

    
    #delta = encoder.get_steps()
    if prevPos != encoderPos :
      if prevPos > encoderPos : 
         updown = 1
      else:
          updown = 0
      prevPos = encoderPos;
      displayMenu(updown)
 
    if time.time() - time_last_refresh > .2 : 
       refresh()
       time_last_refresh = time.time()    

