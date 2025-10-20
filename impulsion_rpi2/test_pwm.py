import RPi.GPIO as GPIO

import time

GPIO.setmode(GPIO.BOARD)

GPIO.setup(33,GPIO.OUT, initial=GPIO.LOW)

for i in range (100):
  GPIO.output(33,GPIO.HIGH)
  time.sleep(100)
  GPIO.output(33,GPIO.LOW)
  time.sleep(900)

GPIO.cleanup()
