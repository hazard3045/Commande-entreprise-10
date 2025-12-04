import RPi.GPIO as GPIO

import time

GPIO.setmode(GPIO.BOARD)

GPIO.setup(33,GPIO.OUT, initial=GPIO.LOW)
GPIO.setup(32,GPIO.OUT, initial = GPIO.LOW)

p = GPIO.PWM(32,1)
p.start(10)

i=100

while(1):
  if (i >0):
    GPIO.output(33,GPIO.HIGH)
    time.sleep(100)
    i -=1
  else:
    GPIO.output(33,GPIO.LOW)
  GPIO.output(33,GPIO.LOW)
  time.sleep(900)



GPIO.cleanup()
