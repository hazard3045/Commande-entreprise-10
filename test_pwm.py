import RPi.GPIO as GPIO

import time

GPIO.setmode(GPIO.BOARD)

GPIO.setup(12,GPIO.OUT, initial=GPIO.HIGH)

time.sleep(60)

GPIO.cleanup()
