PORT    ?= /dev/ttyUSB0
FQBN    ?= arduino:avr:nano:cpu=atmega328old
BAUD    ?= 115200
SKETCH  ?= sketch

.PHONY: setup build upload flash monitor log mouse clean ports

setup:
	arduino-cli core update-index
	arduino-cli core install arduino:avr
	arduino-cli lib install "MPU6050" "I2Cdevlib-Core"

ports:
	arduino-cli board list

build:
	arduino-cli compile --fqbn $(FQBN) $(SKETCH)

upload: build
	@echo ">> Power off the HC-05 (pull its VCC) before uploading — it shares TX1/RX0."
	arduino-cli upload -p $(PORT) --fqbn $(FQBN) $(SKETCH)

flash: upload

monitor:
	picocom -b $(BAUD) --imap lfcrlf $(PORT)

# Stream serial to a timestamped CSV in ./data/
log:
	mkdir -p data
	python3 scripts/log.py --port $(PORT) --baud $(BAUD) --out data/mpu-$$(date +%Y%m%d-%H%M%S).csv

mouse:
	python3 scripts/mouse.py --port $(PORT) --baud $(BAUD)

clean:
	rm -rf $(SKETCH)/build
