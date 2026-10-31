# A simple potentiometer volume controller

## Prerequesites
* ESP-IDF v6.1 installed.
* ESP32 or similar microcontroller with potentiometer connected
* libserialport

## Getting Started

Activate ESP-IDF environment
```bash
source /home/username/.espressif/tools/activate_idf_v6.1.sh
```

Build, Flash and Monitor
```bash
idf.py -p PORT flash monitor 
```

## TODOS:

- [] Write potentiometer reader in C
    - [] Allow dynamic number of potentiometer
- [] Write a client daemon listening to the serial port
    - [] Map each slider reading to specific target
        - [] read sink-input ID
        
